#include "flexoutput-mapping.h"
#include "flexoutput-source.h"
#include <util/threading.h>
#include <util/platform.h>
#include <util/dstr.h>

// Global mapping system state
static bool mapping_system_initialized = false;
static DARRAY(flexoutput_mapping_rule_t*) mapping_rules;
static pthread_mutex_t rules_mutex;

// Forward declarations
static void cleanup_rule_sources(flexoutput_mapping_rule_t *rule);
static bool is_source_in_rule(const flexoutput_mapping_rule_t *rule, const char *source_name);
static flexoutput_error_t add_source_to_rule(flexoutput_mapping_rule_t *rule, const char *source_name);
static flexoutput_error_t remove_source_from_rule(flexoutput_mapping_rule_t *rule, const char *source_name);

flexoutput_error_t flexoutput_mapping_init(void)
{
    if (mapping_system_initialized) {
        return FLEXOUTPUT_SUCCESS;
    }

    da_init(mapping_rules);
    
    if (pthread_mutex_init(&rules_mutex, NULL) != 0) {
        da_free(mapping_rules);
        return FLEXOUTPUT_ERROR_SYSTEM;
    }

    mapping_system_initialized = true;
    
    FLEXOUTPUT_LOG_INFO("Mapping rule engine initialized");
    return FLEXOUTPUT_SUCCESS;
}

void flexoutput_mapping_cleanup(void)
{
    if (!mapping_system_initialized) {
        return;
    }

    pthread_mutex_lock(&rules_mutex);
    
    // Cleanup all rules
    for (size_t i = 0; i < mapping_rules.num; i++) {
        flexoutput_mapping_destroy_rule(mapping_rules.array[i]);
    }
    da_free(mapping_rules);
    
    pthread_mutex_unlock(&rules_mutex);
    pthread_mutex_destroy(&rules_mutex);

    mapping_system_initialized = false;
    
    FLEXOUTPUT_LOG_INFO("Mapping rule engine cleaned up");
}

flexoutput_mapping_rule_t *flexoutput_mapping_create_rule(const char *output_name,
                                                           flexoutput_mapping_type_t type)
{
    if (!output_name) {
        return NULL;
    }

    flexoutput_mapping_rule_t *rule = bzalloc(sizeof(flexoutput_mapping_rule_t));
    rule->output_name = bstrdup(output_name);
    rule->type = type;
    rule->enabled = true;
    rule->auto_manage = false;
    rule->auto_add_new = false;
    rule->auto_remove_missing = false;
    rule->change_callback = NULL;
    rule->callback_data = NULL;
    
    da_init(rule->source_names);
    da_init(rule->source_type_filters);
    da_init(rule->source_type_include);
    
    if (pthread_mutex_init(&rule->mutex, NULL) != 0) {
        FLEXOUTPUT_SAFE_FREE(rule->output_name);
        da_free(rule->source_names);
        da_free(rule->source_type_filters);
        da_free(rule->source_type_include);
        bfree(rule);
        return NULL;
    }

    // Initialize statistics
    rule->stats.sources_added = 0;
    rule->stats.sources_removed = 0;
    rule->stats.last_update_time = os_gettime_ns();
    rule->stats.rule_changes = 0;

    // Add to global rules list
    pthread_mutex_lock(&rules_mutex);
    da_push_back(mapping_rules, &rule);
    pthread_mutex_unlock(&rules_mutex);

    FLEXOUTPUT_LOG_INFO("Created mapping rule for output: %s (%s)", 
                        output_name, 
                        flexoutput_mapping_get_type_string(type));
    return rule;
}

void flexoutput_mapping_destroy_rule(flexoutput_mapping_rule_t *rule)
{
    if (!rule) {
        return;
    }

    // Remove from global rules list
    pthread_mutex_lock(&rules_mutex);
    for (size_t i = 0; i < mapping_rules.num; i++) {
        if (mapping_rules.array[i] == rule) {
            da_erase(mapping_rules, i);
            break;
        }
    }
    pthread_mutex_unlock(&rules_mutex);

    pthread_mutex_lock(&rule->mutex);

    // Cleanup sources and filters
    cleanup_rule_sources(rule);
    da_free(rule->source_names);
    
    // Cleanup source type filters
    for (size_t i = 0; i < rule->source_type_filters.num; i++) {
        FLEXOUTPUT_SAFE_FREE(rule->source_type_filters.array[i]);
    }
    da_free(rule->source_type_filters);
    da_free(rule->source_type_include);

    FLEXOUTPUT_SAFE_FREE(rule->output_name);
    
    pthread_mutex_unlock(&rule->mutex);
    pthread_mutex_destroy(&rule->mutex);
    
    bfree(rule);
}

flexoutput_mapping_rule_t *flexoutput_mapping_copy_rule(const flexoutput_mapping_rule_t *src)
{
    if (!src) {
        return NULL;
    }

    flexoutput_mapping_rule_t *dst = flexoutput_mapping_create_rule(src->output_name, src->type);
    if (!dst) {
        return NULL;
    }

    pthread_mutex_lock((pthread_mutex_t*)&src->mutex);
    pthread_mutex_lock(&dst->mutex);

    dst->enabled = src->enabled;
    dst->auto_manage = src->auto_manage;
    dst->auto_add_new = src->auto_add_new;
    dst->auto_remove_missing = src->auto_remove_missing;

    // Copy source names
    for (size_t i = 0; i < src->source_names.num; i++) {
        char *name_copy = bstrdup(src->source_names.array[i]);
        da_push_back(dst->source_names, &name_copy);
    }

    // Copy source type filters
    for (size_t i = 0; i < src->source_type_filters.num; i++) {
        char *filter_copy = bstrdup(src->source_type_filters.array[i]);
        bool include = src->source_type_include.array[i];
        da_push_back(dst->source_type_filters, &filter_copy);
        da_push_back(dst->source_type_include, &include);
    }

    // Copy callback fields
    dst->change_callback = src->change_callback;
    dst->callback_data = src->callback_data;

    // Copy statistics
    dst->stats = src->stats;

    pthread_mutex_unlock(&dst->mutex);
    pthread_mutex_unlock((pthread_mutex_t*)&src->mutex);

    return dst;
}

flexoutput_error_t flexoutput_mapping_add_source(flexoutput_mapping_rule_t *rule,
                                                  const char *source_name)
{
    if (!rule || !source_name) {
        return FLEXOUTPUT_ERROR_INVALID_PARAM;
    }

    pthread_mutex_lock(&rule->mutex);
    
    flexoutput_error_t result = add_source_to_rule(rule, source_name);
    
    if (result == FLEXOUTPUT_SUCCESS) {
        rule->stats.sources_added++;
        rule->stats.rule_changes++;
        rule->stats.last_update_time = os_gettime_ns();
        
        // Call change callback if set
        if (rule->change_callback) {
            rule->change_callback(rule, rule->callback_data);
        }
    }
    
    pthread_mutex_unlock(&rule->mutex);

    if (result == FLEXOUTPUT_SUCCESS) {
        FLEXOUTPUT_LOG_DEBUG("Added source '%s' to mapping rule '%s'", 
                             source_name, rule->output_name);
    }

    return result;
}

flexoutput_error_t flexoutput_mapping_remove_source(flexoutput_mapping_rule_t *rule,
                                                     const char *source_name)
{
    if (!rule || !source_name) {
        return FLEXOUTPUT_ERROR_INVALID_PARAM;
    }

    pthread_mutex_lock(&rule->mutex);
    
    flexoutput_error_t result = remove_source_from_rule(rule, source_name);
    
    if (result == FLEXOUTPUT_SUCCESS) {
        rule->stats.sources_removed++;
        rule->stats.rule_changes++;
        rule->stats.last_update_time = os_gettime_ns();
        
        // Call change callback if set
        if (rule->change_callback) {
            rule->change_callback(rule, rule->callback_data);
        }
    }
    
    pthread_mutex_unlock(&rule->mutex);

    if (result == FLEXOUTPUT_SUCCESS) {
        FLEXOUTPUT_LOG_DEBUG("Removed source '%s' from mapping rule '%s'", 
                             source_name, rule->output_name);
    }

    return result;
}

flexoutput_error_t flexoutput_mapping_clear_sources(flexoutput_mapping_rule_t *rule)
{
    if (!rule) {
        return FLEXOUTPUT_ERROR_INVALID_PARAM;
    }

    pthread_mutex_lock(&rule->mutex);
    
    size_t old_count = rule->source_names.num;
    cleanup_rule_sources(rule);
    
    rule->stats.sources_removed += old_count;
    rule->stats.rule_changes++;
    rule->stats.last_update_time = os_gettime_ns();
    
    // Call change callback if set
    if (rule->change_callback) {
        rule->change_callback(rule, rule->callback_data);
    }
    
    pthread_mutex_unlock(&rule->mutex);

    FLEXOUTPUT_LOG_DEBUG("Cleared all sources from mapping rule '%s'", rule->output_name);
    return FLEXOUTPUT_SUCCESS;
}

bool flexoutput_mapping_has_source(const flexoutput_mapping_rule_t *rule,
                                    const char *source_name)
{
    if (!rule || !source_name) {
        return false;
    }

    pthread_mutex_lock((pthread_mutex_t*)&rule->mutex);
    bool result = is_source_in_rule(rule, source_name);
    pthread_mutex_unlock((pthread_mutex_t*)&rule->mutex);

    return result;
}

bool flexoutput_mapping_should_include_source(const flexoutput_mapping_rule_t *rule,
                                               const char *source_name)
{
    if (!rule || !source_name || !rule->enabled) {
        return false;
    }

    pthread_mutex_lock((pthread_mutex_t*)&rule->mutex);

    bool in_rule = is_source_in_rule(rule, source_name);
    bool result = false;

    // Check source type filters first
    obs_weak_source_t *weak_source = flexoutput_source_get_weak_ref(source_name);
    if (weak_source) {
        obs_source_t *source = obs_weak_source_get_source(weak_source);
        if (source) {
            const char *source_type = obs_source_get_id(source);
            if (!flexoutput_mapping_should_include_source_type(rule, source_type)) {
                pthread_mutex_unlock((pthread_mutex_t*)&rule->mutex);
                obs_source_release(source);
                obs_weak_source_release(weak_source);
                return false;
            }
            obs_source_release(source);
        }
        obs_weak_source_release(weak_source);
    }

    // Apply mapping rule logic
    switch (rule->type) {
    case FLEXOUTPUT_MAPPING_WHITELIST:
        result = in_rule; // Include only if explicitly listed
        break;
    case FLEXOUTPUT_MAPPING_BLACKLIST:
        result = !in_rule; // Include unless explicitly excluded
        break;
    default:
        result = false;
        break;
    }

    pthread_mutex_unlock((pthread_mutex_t*)&rule->mutex);
    return result;
}

flexoutput_error_t flexoutput_mapping_get_included_sources(const flexoutput_mapping_rule_t *rule,
                                                            char ***included_sources,
                                                            size_t *count)
{
    if (!rule || !included_sources || !count) {
        return FLEXOUTPUT_ERROR_INVALID_PARAM;
    }

    *included_sources = NULL;
    *count = 0;

    if (!rule->enabled) {
        return FLEXOUTPUT_SUCCESS;
    }

    // Get all available sources
    char **all_sources = NULL;
    size_t all_count = 0;
    flexoutput_error_t result = flexoutput_source_enumerate_all(&all_sources, &all_count);
    if (result != FLEXOUTPUT_SUCCESS) {
        return result;
    }

    // Filter sources based on mapping rule
    DARRAY(char*) included;
    da_init(included);

    for (size_t i = 0; i < all_count; i++) {
        if (flexoutput_mapping_should_include_source(rule, all_sources[i])) {
            char *name_copy = bstrdup(all_sources[i]);
            da_push_back(included, &name_copy);
        }
    }

    // Convert to array
    if (included.num > 0) {
        *included_sources = bmalloc(sizeof(char*) * included.num);
        for (size_t i = 0; i < included.num; i++) {
            (*included_sources)[i] = included.array[i];
        }
        *count = included.num;
    }

    da_free(included);
    flexoutput_source_free_string_array(all_sources, all_count);

    return FLEXOUTPUT_SUCCESS;
}

flexoutput_error_t flexoutput_mapping_get_excluded_sources(const flexoutput_mapping_rule_t *rule,
                                                            char ***excluded_sources,
                                                            size_t *count)
{
    if (!rule || !excluded_sources || !count) {
        return FLEXOUTPUT_ERROR_INVALID_PARAM;
    }

    *excluded_sources = NULL;
    *count = 0;

    if (!rule->enabled) {
        return FLEXOUTPUT_SUCCESS;
    }

    // Get all available sources
    char **all_sources = NULL;
    size_t all_count = 0;
    flexoutput_error_t result = flexoutput_source_enumerate_all(&all_sources, &all_count);
    if (result != FLEXOUTPUT_SUCCESS) {
        return result;
    }

    // Filter sources based on mapping rule
    DARRAY(char*) excluded;
    da_init(excluded);

    for (size_t i = 0; i < all_count; i++) {
        if (!flexoutput_mapping_should_include_source(rule, all_sources[i])) {
            char *name_copy = bstrdup(all_sources[i]);
            da_push_back(excluded, &name_copy);
        }
    }

    // Convert to array
    if (excluded.num > 0) {
        *excluded_sources = bmalloc(sizeof(char*) * excluded.num);
        for (size_t i = 0; i < excluded.num; i++) {
            (*excluded_sources)[i] = excluded.array[i];
        }
        *count = excluded.num;
    }

    da_free(excluded);
    flexoutput_source_free_string_array(all_sources, all_count);

    return FLEXOUTPUT_SUCCESS;
}

void flexoutput_mapping_free_string_array(char **array, size_t count)
{
    if (!array) {
        return;
    }

    for (size_t i = 0; i < count; i++) {
        FLEXOUTPUT_SAFE_FREE(array[i]);
    }
    bfree(array);
}

flexoutput_error_t flexoutput_mapping_validate_rule(const flexoutput_mapping_rule_t *rule)
{
    if (!rule) {
        return FLEXOUTPUT_ERROR_INVALID_PARAM;
    }

    if (!rule->output_name || strlen(rule->output_name) == 0) {
        return FLEXOUTPUT_ERROR_INVALID_CONFIG;
    }

    if (rule->type != FLEXOUTPUT_MAPPING_WHITELIST && 
        rule->type != FLEXOUTPUT_MAPPING_BLACKLIST) {
        return FLEXOUTPUT_ERROR_INVALID_CONFIG;
    }

    return FLEXOUTPUT_SUCCESS;
}

bool flexoutput_mapping_is_rule_empty(const flexoutput_mapping_rule_t *rule)
{
    return rule ? (rule->source_names.num == 0) : true;
}

size_t flexoutput_mapping_get_source_count(const flexoutput_mapping_rule_t *rule)
{
    return rule ? rule->source_names.num : 0;
}

obs_data_t *flexoutput_mapping_save_rule(const flexoutput_mapping_rule_t *rule)
{
    if (!rule) {
        return NULL;
    }

    obs_data_t *data = obs_data_create();
    
    pthread_mutex_lock((pthread_mutex_t*)&rule->mutex);
    
    obs_data_set_string(data, "output_name", rule->output_name);
    obs_data_set_string(data, "type", flexoutput_mapping_get_type_string(rule->type));
    obs_data_set_bool(data, "enabled", rule->enabled);
    obs_data_set_bool(data, "auto_manage", rule->auto_manage);
    obs_data_set_bool(data, "auto_add_new", rule->auto_add_new);
    obs_data_set_bool(data, "auto_remove_missing", rule->auto_remove_missing);

    // Save source names
    obs_data_array_t *sources_array = obs_data_array_create();
    for (size_t i = 0; i < rule->source_names.num; i++) {
        obs_data_t *source_data = obs_data_create();
        obs_data_set_string(source_data, "name", rule->source_names.array[i]);
        obs_data_array_push_back(sources_array, source_data);
        obs_data_release(source_data);
    }
    obs_data_set_array(data, "sources", sources_array);
    obs_data_array_release(sources_array);

    // Save source type filters
    obs_data_array_t *filters_array = obs_data_array_create();
    for (size_t i = 0; i < rule->source_type_filters.num; i++) {
        obs_data_t *filter_data = obs_data_create();
        obs_data_set_string(filter_data, "type", rule->source_type_filters.array[i]);
        obs_data_set_bool(filter_data, "include", rule->source_type_include.array[i]);
        obs_data_array_push_back(filters_array, filter_data);
        obs_data_release(filter_data);
    }
    obs_data_set_array(data, "type_filters", filters_array);
    obs_data_array_release(filters_array);

    pthread_mutex_unlock((pthread_mutex_t*)&rule->mutex);

    return data;
}

flexoutput_mapping_rule_t *flexoutput_mapping_load_rule(obs_data_t *data)
{
    if (!data) {
        return NULL;
    }

    const char *output_name = obs_data_get_string(data, "output_name");
    const char *type_str = obs_data_get_string(data, "type");
    
    if (!output_name || !type_str) {
        return NULL;
    }

    flexoutput_mapping_type_t type = flexoutput_mapping_parse_type_string(type_str);
    flexoutput_mapping_rule_t *rule = flexoutput_mapping_create_rule(output_name, type);
    if (!rule) {
        return NULL;
    }

    pthread_mutex_lock(&rule->mutex);

    rule->enabled = obs_data_get_bool(data, "enabled");
    rule->auto_manage = obs_data_get_bool(data, "auto_manage");
    rule->auto_add_new = obs_data_get_bool(data, "auto_add_new");
    rule->auto_remove_missing = obs_data_get_bool(data, "auto_remove_missing");

    // Load source names
    obs_data_array_t *sources_array = obs_data_get_array(data, "sources");
    if (sources_array) {
        size_t count = obs_data_array_count(sources_array);
        for (size_t i = 0; i < count; i++) {
            obs_data_t *source_data = obs_data_array_item(sources_array, i);
            const char *source_name = obs_data_get_string(source_data, "name");
            if (source_name) {
                add_source_to_rule(rule, source_name);
            }
            obs_data_release(source_data);
        }
        obs_data_array_release(sources_array);
    }

    // Load source type filters
    obs_data_array_t *filters_array = obs_data_get_array(data, "type_filters");
    if (filters_array) {
        size_t count = obs_data_array_count(filters_array);
        for (size_t i = 0; i < count; i++) {
            obs_data_t *filter_data = obs_data_array_item(filters_array, i);
            const char *source_type = obs_data_get_string(filter_data, "type");
            bool include = obs_data_get_bool(filter_data, "include");
            if (source_type) {
                flexoutput_mapping_add_source_type_filter(rule, source_type, include);
            }
            obs_data_release(filter_data);
        }
        obs_data_array_release(filters_array);
    }

    pthread_mutex_unlock(&rule->mutex);

    return rule;
}

flexoutput_error_t flexoutput_mapping_add_source_type_filter(flexoutput_mapping_rule_t *rule,
                                                              const char *source_type,
                                                              bool include)
{
    if (!rule || !source_type) {
        return FLEXOUTPUT_ERROR_INVALID_PARAM;
    }

    pthread_mutex_lock(&rule->mutex);

    // Check if filter already exists
    for (size_t i = 0; i < rule->source_type_filters.num; i++) {
        if (strcmp(rule->source_type_filters.array[i], source_type) == 0) {
            // Update existing filter
            rule->source_type_include.array[i] = include;
            pthread_mutex_unlock(&rule->mutex);
            return FLEXOUTPUT_SUCCESS;
        }
    }

    // Add new filter
    char *type_copy = bstrdup(source_type);
    da_push_back(rule->source_type_filters, &type_copy);
    da_push_back(rule->source_type_include, &include);

    pthread_mutex_unlock(&rule->mutex);

    FLEXOUTPUT_LOG_DEBUG("Added source type filter to rule '%s': %s (%s)", 
                         rule->output_name, source_type, include ? "include" : "exclude");
    return FLEXOUTPUT_SUCCESS;
}

bool flexoutput_mapping_should_include_source_type(const flexoutput_mapping_rule_t *rule,
                                                    const char *source_type)
{
    if (!rule || !source_type) {
        return true; // Default to include if no rule or type
    }

    pthread_mutex_lock((pthread_mutex_t*)&rule->mutex);

    // Check if there are any type filters
    if (rule->source_type_filters.num == 0) {
        pthread_mutex_unlock((pthread_mutex_t*)&rule->mutex);
        return true; // No filters, include all types
    }

    // Check specific type filters
    for (size_t i = 0; i < rule->source_type_filters.num; i++) {
        if (strcmp(rule->source_type_filters.array[i], source_type) == 0) {
            bool include = rule->source_type_include.array[i];
            pthread_mutex_unlock((pthread_mutex_t*)&rule->mutex);
            return include;
        }
    }

    pthread_mutex_unlock((pthread_mutex_t*)&rule->mutex);
    return true; // Type not in filters, default to include
}

const char *flexoutput_mapping_get_type_string(flexoutput_mapping_type_t type)
{
    switch (type) {
    case FLEXOUTPUT_MAPPING_WHITELIST: return "whitelist";
    case FLEXOUTPUT_MAPPING_BLACKLIST: return "blacklist";
    default: return "unknown";
    }
}

flexoutput_mapping_type_t flexoutput_mapping_parse_type_string(const char *type_str)
{
    if (!type_str) {
        return FLEXOUTPUT_MAPPING_WHITELIST;
    }

    if (strcmp(type_str, "whitelist") == 0) {
        return FLEXOUTPUT_MAPPING_WHITELIST;
    } else if (strcmp(type_str, "blacklist") == 0) {
        return FLEXOUTPUT_MAPPING_BLACKLIST;
    }

    return FLEXOUTPUT_MAPPING_WHITELIST; // Default
}

flexoutput_error_t flexoutput_mapping_set_change_callback(flexoutput_mapping_rule_t *rule,
                                                           void (*callback)(const flexoutput_mapping_rule_t *rule, void *data),
                                                           void *data)
{
    if (!rule) {
        return FLEXOUTPUT_ERROR_INVALID_PARAM;
    }

    pthread_mutex_lock(&rule->mutex);
    rule->change_callback = callback;
    rule->callback_data = data;
    pthread_mutex_unlock(&rule->mutex);

    return FLEXOUTPUT_SUCCESS;
}

flexoutput_error_t flexoutput_mapping_get_rule_stats(const flexoutput_mapping_rule_t *rule,
                                                      flexoutput_mapping_stats_t *stats)
{
    if (!rule || !stats) {
        return FLEXOUTPUT_ERROR_INVALID_PARAM;
    }

    pthread_mutex_lock((pthread_mutex_t*)&rule->mutex);
    *stats = rule->stats;
    pthread_mutex_unlock((pthread_mutex_t*)&rule->mutex);

    return FLEXOUTPUT_SUCCESS;
}

// Helper functions
static void cleanup_rule_sources(flexoutput_mapping_rule_t *rule)
{
    if (!rule) {
        return;
    }

    // Free all source names
    for (size_t i = 0; i < rule->source_names.num; i++) {
        FLEXOUTPUT_SAFE_FREE(rule->source_names.array[i]);
    }
    da_resize(rule->source_names, 0);
}

static bool is_source_in_rule(const flexoutput_mapping_rule_t *rule, const char *source_name)
{
    if (!rule || !source_name) {
        return false;
    }

    for (size_t i = 0; i < rule->source_names.num; i++) {
        if (strcmp(rule->source_names.array[i], source_name) == 0) {
            return true;
        }
    }

    return false;
}

static flexoutput_error_t add_source_to_rule(flexoutput_mapping_rule_t *rule, const char *source_name)
{
    if (!rule || !source_name) {
        return FLEXOUTPUT_ERROR_INVALID_PARAM;
    }

    // Check if source already exists
    if (is_source_in_rule(rule, source_name)) {
        return FLEXOUTPUT_ERROR_ALREADY_EXISTS;
    }

    // Add source name
    char *name_copy = bstrdup(source_name);
    da_push_back(rule->source_names, &name_copy);

    return FLEXOUTPUT_SUCCESS;
}

static flexoutput_error_t remove_source_from_rule(flexoutput_mapping_rule_t *rule, const char *source_name)
{
    if (!rule || !source_name) {
        return FLEXOUTPUT_ERROR_INVALID_PARAM;
    }

    for (size_t i = 0; i < rule->source_names.num; i++) {
        if (strcmp(rule->source_names.array[i], source_name) == 0) {
            FLEXOUTPUT_SAFE_FREE(rule->source_names.array[i]);
            da_erase(rule->source_names, i);
            return FLEXOUTPUT_SUCCESS;
        }
    }

    return FLEXOUTPUT_ERROR_NOT_FOUND;
}

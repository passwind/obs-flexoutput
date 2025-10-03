#include "flexoutput-config.h"
#include <util/config-file.h>
#include <util/platform.h>
#include <util/dstr.h>

// Internal configuration storage
static DARRAY(flexoutput_output_config_t*) output_configs;
static DARRAY(flexoutput_mapping_rule_t*) mapping_rules;
static bool config_initialized = false;

// Helper functions
static obs_data_t *output_config_to_data(const flexoutput_output_config_t *config);
static flexoutput_output_config_t *output_config_from_data(obs_data_t *data);
static obs_data_t *mapping_rule_to_data(const flexoutput_mapping_rule_t *rule);
static flexoutput_mapping_rule_t *mapping_rule_from_data(obs_data_t *data);

flexoutput_error_t flexoutput_config_init(void)
{
    if (config_initialized) {
        return FLEXOUTPUT_SUCCESS;
    }

    da_init(output_configs);
    da_init(mapping_rules);
    
    config_initialized = true;
    
    FLEXOUTPUT_LOG_INFO("Configuration system initialized");
    return FLEXOUTPUT_SUCCESS;
}

void flexoutput_config_cleanup(void)
{
    if (!config_initialized) {
        return;
    }

    // Free all output configurations
    for (size_t i = 0; i < output_configs.num; i++) {
        flexoutput_config_free_output_config(output_configs.array[i]);
    }
    da_free(output_configs);

    // Free all mapping rules
    for (size_t i = 0; i < mapping_rules.num; i++) {
        flexoutput_config_free_mapping_rule(mapping_rules.array[i]);
    }
    da_free(mapping_rules);

    config_initialized = false;
    
    FLEXOUTPUT_LOG_INFO("Configuration system cleaned up");
}

flexoutput_error_t flexoutput_config_load(void)
{
    if (!config_initialized) {
        return FLEXOUTPUT_ERROR_NOT_INITIALIZED;
    }

    if (!g_flexoutput_plugin || !g_flexoutput_plugin->config_file_path) {
        return FLEXOUTPUT_ERROR_INVALID_PARAM;
    }

    const char *config_path = g_flexoutput_plugin->config_file_path;
    
    // Check if config file exists
    if (!os_file_exists(config_path)) {
        FLEXOUTPUT_LOG_INFO("Configuration file does not exist, using defaults: %s", config_path);
        return FLEXOUTPUT_SUCCESS;
    }

    // Load JSON data
    obs_data_t *data = obs_data_create_from_json_file(config_path);
    if (!data) {
        FLEXOUTPUT_LOG_ERROR("Failed to load configuration from: %s", config_path);
        return FLEXOUTPUT_ERROR_CONFIG_FAILED;
    }

    // Clear existing configuration
    for (size_t i = 0; i < output_configs.num; i++) {
        flexoutput_config_free_output_config(output_configs.array[i]);
    }
    da_resize(output_configs, 0);

    for (size_t i = 0; i < mapping_rules.num; i++) {
        flexoutput_config_free_mapping_rule(mapping_rules.array[i]);
    }
    da_resize(mapping_rules, 0);

    // Load plugin settings
    bool enabled = obs_data_get_bool(data, "enabled");
    if (g_flexoutput_plugin) {
        g_flexoutput_plugin->enabled = enabled;
    }

    // Load output configurations
    obs_data_array_t *outputs_array = obs_data_get_array(data, "outputs");
    if (outputs_array) {
        size_t count = obs_data_array_count(outputs_array);
        for (size_t i = 0; i < count; i++) {
            obs_data_t *output_data = obs_data_array_item(outputs_array, i);
            flexoutput_output_config_t *config = output_config_from_data(output_data);
            if (config) {
                da_push_back(output_configs, &config);
            }
            obs_data_release(output_data);
        }
        obs_data_array_release(outputs_array);
    }

    // Load mapping rules
    obs_data_array_t *mappings_array = obs_data_get_array(data, "mapping_rules");
    if (mappings_array) {
        size_t count = obs_data_array_count(mappings_array);
        for (size_t i = 0; i < count; i++) {
            obs_data_t *mapping_data = obs_data_array_item(mappings_array, i);
            flexoutput_mapping_rule_t *rule = mapping_rule_from_data(mapping_data);
            if (rule) {
                da_push_back(mapping_rules, &rule);
            }
            obs_data_release(mapping_data);
        }
        obs_data_array_release(mappings_array);
    }

    obs_data_release(data);
    
    FLEXOUTPUT_LOG_INFO("Configuration loaded successfully from: %s", config_path);
    return FLEXOUTPUT_SUCCESS;
}

flexoutput_error_t flexoutput_config_save(void)
{
    if (!config_initialized) {
        return FLEXOUTPUT_ERROR_NOT_INITIALIZED;
    }

    if (!g_flexoutput_plugin || !g_flexoutput_plugin->config_file_path) {
        return FLEXOUTPUT_ERROR_INVALID_PARAM;
    }

    const char *config_path = g_flexoutput_plugin->config_file_path;

    // Create configuration data
    obs_data_t *data = obs_data_create();
    
    // Save plugin settings
    obs_data_set_string(data, "version", FLEXOUTPUT_VERSION);
    obs_data_set_bool(data, "enabled", g_flexoutput_plugin ? g_flexoutput_plugin->enabled : false);

    // Save output configurations
    obs_data_array_t *outputs_array = obs_data_array_create();
    for (size_t i = 0; i < output_configs.num; i++) {
        obs_data_t *output_data = output_config_to_data(output_configs.array[i]);
        obs_data_array_push_back(outputs_array, output_data);
        obs_data_release(output_data);
    }
    obs_data_set_array(data, "outputs", outputs_array);
    obs_data_array_release(outputs_array);

    // Save mapping rules
    obs_data_array_t *mappings_array = obs_data_array_create();
    for (size_t i = 0; i < mapping_rules.num; i++) {
        obs_data_t *mapping_data = mapping_rule_to_data(mapping_rules.array[i]);
        obs_data_array_push_back(mappings_array, mapping_data);
        obs_data_release(mapping_data);
    }
    obs_data_set_array(data, "mapping_rules", mappings_array);
    obs_data_array_release(mappings_array);

    // Save to file
    bool success = obs_data_save_json_safe(data, config_path, "tmp", "bak");
    obs_data_release(data);

    if (!success) {
        FLEXOUTPUT_LOG_ERROR("Failed to save configuration to: %s", config_path);
        return FLEXOUTPUT_ERROR_CONFIG_FAILED;
    }

    FLEXOUTPUT_LOG_INFO("Configuration saved successfully to: %s", config_path);
    return FLEXOUTPUT_SUCCESS;
}

const char *flexoutput_config_get_file_path(void)
{
    return g_flexoutput_plugin ? g_flexoutput_plugin->config_file_path : NULL;
}

flexoutput_error_t flexoutput_config_set_file_path(const char *path)
{
    if (!g_flexoutput_plugin || !path) {
        return FLEXOUTPUT_ERROR_INVALID_PARAM;
    }

    FLEXOUTPUT_SAFE_FREE(g_flexoutput_plugin->config_file_path);
    g_flexoutput_plugin->config_file_path = bstrdup(path);
    
    return FLEXOUTPUT_SUCCESS;
}

bool flexoutput_config_get_enabled(void)
{
    return g_flexoutput_plugin ? g_flexoutput_plugin->enabled : false;
}

void flexoutput_config_set_enabled(bool enabled)
{
    if (g_flexoutput_plugin) {
        g_flexoutput_plugin->enabled = enabled;
    }
}

size_t flexoutput_config_get_output_count(void)
{
    return config_initialized ? output_configs.num : 0;
}

flexoutput_output_config_t *flexoutput_config_get_output(size_t index)
{
    if (!config_initialized || index >= output_configs.num) {
        return NULL;
    }
    return output_configs.array[index];
}

flexoutput_output_config_t *flexoutput_config_get_output_by_name(const char *name)
{
    if (!config_initialized || !name) {
        return NULL;
    }

    for (size_t i = 0; i < output_configs.num; i++) {
        if (strcmp(output_configs.array[i]->name, name) == 0) {
            return output_configs.array[i];
        }
    }
    return NULL;
}

flexoutput_error_t flexoutput_config_add_output(const flexoutput_output_config_t *config)
{
    if (!config_initialized || !config || !config->name) {
        return FLEXOUTPUT_ERROR_INVALID_PARAM;
    }

    // Check if output already exists
    if (flexoutput_config_get_output_by_name(config->name)) {
        return FLEXOUTPUT_ERROR_ALREADY_EXISTS;
    }

    flexoutput_output_config_t *new_config = flexoutput_config_copy_output_config(config);
    if (!new_config) {
        return FLEXOUTPUT_ERROR_MEMORY;
    }

    da_push_back(output_configs, &new_config);
    return FLEXOUTPUT_SUCCESS;
}

flexoutput_error_t flexoutput_config_remove_output(const char *name)
{
    if (!config_initialized || !name) {
        return FLEXOUTPUT_ERROR_INVALID_PARAM;
    }

    for (size_t i = 0; i < output_configs.num; i++) {
        if (strcmp(output_configs.array[i]->name, name) == 0) {
            flexoutput_config_free_output_config(output_configs.array[i]);
            da_erase(output_configs, i);
            return FLEXOUTPUT_SUCCESS;
        }
    }
    return FLEXOUTPUT_ERROR_NOT_FOUND;
}

flexoutput_error_t flexoutput_config_update_output(const char *name, const flexoutput_output_config_t *config)
{
    if (!config_initialized || !name || !config) {
        return FLEXOUTPUT_ERROR_INVALID_PARAM;
    }

    for (size_t i = 0; i < output_configs.num; i++) {
        if (strcmp(output_configs.array[i]->name, name) == 0) {
            flexoutput_config_free_output_config(output_configs.array[i]);
            output_configs.array[i] = flexoutput_config_copy_output_config(config);
            if (!output_configs.array[i]) {
                return FLEXOUTPUT_ERROR_MEMORY;
            }
            return FLEXOUTPUT_SUCCESS;
        }
    }
    return FLEXOUTPUT_ERROR_NOT_FOUND;
}

// Mapping rule functions
size_t flexoutput_config_get_mapping_count(void)
{
    return config_initialized ? mapping_rules.num : 0;
}

flexoutput_mapping_rule_t *flexoutput_config_get_mapping(size_t index)
{
    if (!config_initialized || index >= mapping_rules.num) {
        return NULL;
    }
    return mapping_rules.array[index];
}

flexoutput_mapping_rule_t *flexoutput_config_get_mapping_by_output(const char *output_name)
{
    if (!config_initialized || !output_name) {
        return NULL;
    }

    for (size_t i = 0; i < mapping_rules.num; i++) {
        if (strcmp(mapping_rules.array[i]->output_name, output_name) == 0) {
            return mapping_rules.array[i];
        }
    }
    return NULL;
}

flexoutput_error_t flexoutput_config_add_mapping(const flexoutput_mapping_rule_t *rule)
{
    if (!config_initialized || !rule || !rule->output_name) {
        return FLEXOUTPUT_ERROR_INVALID_PARAM;
    }

    // Check if mapping already exists
    if (flexoutput_config_get_mapping_by_output(rule->output_name)) {
        return FLEXOUTPUT_ERROR_ALREADY_EXISTS;
    }

    flexoutput_mapping_rule_t *new_rule = flexoutput_config_copy_mapping_rule(rule);
    if (!new_rule) {
        return FLEXOUTPUT_ERROR_MEMORY;
    }

    da_push_back(mapping_rules, &new_rule);
    return FLEXOUTPUT_SUCCESS;
}

flexoutput_error_t flexoutput_config_remove_mapping(const char *output_name)
{
    if (!config_initialized || !output_name) {
        return FLEXOUTPUT_ERROR_INVALID_PARAM;
    }

    for (size_t i = 0; i < mapping_rules.num; i++) {
        if (strcmp(mapping_rules.array[i]->output_name, output_name) == 0) {
            flexoutput_config_free_mapping_rule(mapping_rules.array[i]);
            da_erase(mapping_rules, i);
            return FLEXOUTPUT_SUCCESS;
        }
    }
    return FLEXOUTPUT_ERROR_NOT_FOUND;
}

flexoutput_error_t flexoutput_config_update_mapping(const char *output_name, const flexoutput_mapping_rule_t *rule)
{
    if (!config_initialized || !output_name || !rule) {
        return FLEXOUTPUT_ERROR_INVALID_PARAM;
    }

    for (size_t i = 0; i < mapping_rules.num; i++) {
        if (strcmp(mapping_rules.array[i]->output_name, output_name) == 0) {
            flexoutput_config_free_mapping_rule(mapping_rules.array[i]);
            mapping_rules.array[i] = flexoutput_config_copy_mapping_rule(rule);
            if (!mapping_rules.array[i]) {
                return FLEXOUTPUT_ERROR_MEMORY;
            }
            return FLEXOUTPUT_SUCCESS;
        }
    }
    return FLEXOUTPUT_ERROR_NOT_FOUND;
}

// Helper functions for data structures
flexoutput_output_config_t *flexoutput_config_create_output_config(
    const char *name, uint32_t width, uint32_t height, 
    uint32_t fps_num, uint32_t fps_den)
{
    if (!name) {
        return NULL;
    }

    flexoutput_output_config_t *config = bzalloc(sizeof(flexoutput_output_config_t));
    config->name = bstrdup(name);
    config->type = FLEXOUTPUT_OUTPUT_RTMP;  // Default to RTMP stream output
    config->enabled = true;
    
    // Video settings
    config->video.bitrate = 2500;
    config->video.width = width;
    config->video.height = height;
    config->video.fps = (fps_den > 0) ? (fps_num / fps_den) : 30;
    config->video.encoder = bstrdup("obs_x264");
    config->video.preset = bstrdup("veryfast");
    config->video.profile = bstrdup("main");
    
    // Audio settings
    config->audio.bitrate = 160;
    config->audio.sample_rate = 44100;
    config->audio.channels = 2;
    config->audio.encoder = bstrdup("ffmpeg_aac");
    
    // Streaming settings
    config->url = bstrdup("");
    config->key = bstrdup("");
    
    // Recording settings
    config->file_path = bstrdup("");
    config->format = bstrdup("mp4");
    
    // Settings objects
    config->encoder_settings = obs_data_create();
    config->service_settings = obs_data_create();

    return config;
}

void flexoutput_config_free_output_config(flexoutput_output_config_t *config)
{
    if (!config) {
        return;
    }

    FLEXOUTPUT_SAFE_FREE(config->name);
    FLEXOUTPUT_SAFE_FREE(config->video.encoder);
    FLEXOUTPUT_SAFE_FREE(config->video.preset);
    FLEXOUTPUT_SAFE_FREE(config->video.profile);
    FLEXOUTPUT_SAFE_FREE(config->audio.encoder);
    FLEXOUTPUT_SAFE_FREE(config->url);
    FLEXOUTPUT_SAFE_FREE(config->key);
    FLEXOUTPUT_SAFE_FREE(config->file_path);
    FLEXOUTPUT_SAFE_FREE(config->format);
    
    if (config->encoder_settings) {
        obs_data_release(config->encoder_settings);
    }
    if (config->service_settings) {
        obs_data_release(config->service_settings);
    }
    
    bfree(config);
}

flexoutput_output_config_t *flexoutput_config_copy_output_config(const flexoutput_output_config_t *src)
{
    if (!src) {
        return NULL;
    }

    flexoutput_output_config_t *copy = bzalloc(sizeof(flexoutput_output_config_t));
    copy->name = bstrdup(src->name);
    copy->type = src->type;
    copy->enabled = src->enabled;
    
    // Video settings
    copy->video.bitrate = src->video.bitrate;
    copy->video.width = src->video.width;
    copy->video.height = src->video.height;
    copy->video.fps = src->video.fps;
    copy->video.encoder = bstrdup(src->video.encoder);
    copy->video.preset = bstrdup(src->video.preset);
    copy->video.profile = bstrdup(src->video.profile);
    
    // Audio settings
    copy->audio.bitrate = src->audio.bitrate;
    copy->audio.sample_rate = src->audio.sample_rate;
    copy->audio.channels = src->audio.channels;
    copy->audio.encoder = bstrdup(src->audio.encoder);
    
    // Streaming settings
    copy->url = bstrdup(src->url);
    copy->key = bstrdup(src->key);
    
    // Recording settings
    copy->file_path = bstrdup(src->file_path);
    copy->format = bstrdup(src->format);
    
    // Settings objects
    copy->encoder_settings = obs_data_create();
    copy->service_settings = obs_data_create();

    if (src->encoder_settings) {
        obs_data_apply(copy->encoder_settings, src->encoder_settings);
    }
    if (src->service_settings) {
        obs_data_apply(copy->service_settings, src->service_settings);
    }

    return copy;
}

flexoutput_mapping_rule_t *flexoutput_config_create_mapping_rule(
    const char *output_name, flexoutput_mapping_type_t mode)
{
    if (!output_name) {
        return NULL;
    }

    flexoutput_mapping_rule_t *rule = bzalloc(sizeof(flexoutput_mapping_rule_t));
    rule->output_name = bstrdup(output_name);
    rule->type = mode;
    rule->enabled = true;
    rule->auto_manage = false;
    rule->auto_add_new = false;
    rule->auto_remove_missing = false;
    
    da_init(rule->source_names);
    da_init(rule->source_type_filters);
    da_init(rule->source_type_include);
    
    memset(&rule->stats, 0, sizeof(rule->stats));
    rule->change_callback = NULL;
    rule->callback_data = NULL;
    
    if (pthread_mutex_init(&rule->mutex, NULL) != 0) {
        bfree(rule->output_name);
        bfree(rule);
        return NULL;
    }

    return rule;
}

void flexoutput_config_free_mapping_rule(flexoutput_mapping_rule_t *rule)
{
    if (!rule) {
        return;
    }

    FLEXOUTPUT_SAFE_FREE(rule->output_name);
    
    for (size_t i = 0; i < rule->source_names.num; i++) {
        FLEXOUTPUT_SAFE_FREE(rule->source_names.array[i]);
    }
    da_free(rule->source_names);
    
    for (size_t i = 0; i < rule->source_type_filters.num; i++) {
        FLEXOUTPUT_SAFE_FREE(rule->source_type_filters.array[i]);
    }
    da_free(rule->source_type_filters);
    
    da_free(rule->source_type_include);
    
    pthread_mutex_destroy(&rule->mutex);
    
    bfree(rule);
}

flexoutput_mapping_rule_t *flexoutput_config_copy_mapping_rule(const flexoutput_mapping_rule_t *src)
{
    if (!src) {
        return NULL;
    }

    flexoutput_mapping_rule_t *copy = bzalloc(sizeof(flexoutput_mapping_rule_t));
    copy->output_name = bstrdup(src->output_name);
    copy->type = src->type;
    copy->enabled = src->enabled;
    copy->auto_manage = src->auto_manage;
    copy->auto_add_new = src->auto_add_new;
    copy->auto_remove_missing = src->auto_remove_missing;
    
    da_init(copy->source_names);
    for (size_t i = 0; i < src->source_names.num; i++) {
        char *source_name = bstrdup(src->source_names.array[i]);
        da_push_back(copy->source_names, &source_name);
    }
    
    da_init(copy->source_type_filters);
    for (size_t i = 0; i < src->source_type_filters.num; i++) {
        char *filter = bstrdup(src->source_type_filters.array[i]);
        da_push_back(copy->source_type_filters, &filter);
    }
    
    da_init(copy->source_type_include);
    for (size_t i = 0; i < src->source_type_include.num; i++) {
        bool include = src->source_type_include.array[i];
        da_push_back(copy->source_type_include, &include);
    }
    
    copy->stats = src->stats;
    copy->change_callback = src->change_callback;
    copy->callback_data = src->callback_data;
    
    if (pthread_mutex_init(&copy->mutex, NULL) != 0) {
        // Handle mutex initialization failure
        da_free(copy->source_names);
        da_free(copy->source_type_filters);
        da_free(copy->source_type_include);
        bfree(copy->output_name);
        bfree(copy);
        return NULL;
    }

    return copy;
}

flexoutput_error_t flexoutput_config_mapping_add_source(
    flexoutput_mapping_rule_t *rule, const char *source_name)
{
    if (!rule || !source_name) {
        return FLEXOUTPUT_ERROR_INVALID_PARAM;
    }

    // Check if source already exists
    if (flexoutput_config_mapping_has_source(rule, source_name)) {
        return FLEXOUTPUT_ERROR_ALREADY_EXISTS;
    }

    char *name_copy = bstrdup(source_name);
    da_push_back(rule->source_names, &name_copy);
    
    return FLEXOUTPUT_SUCCESS;
}

flexoutput_error_t flexoutput_config_mapping_remove_source(
    flexoutput_mapping_rule_t *rule, const char *source_name)
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

bool flexoutput_config_mapping_has_source(
    const flexoutput_mapping_rule_t *rule, const char *source_name)
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

// Helper functions for serialization
static obs_data_t *output_config_to_data(const flexoutput_output_config_t *config)
{
    if (!config) {
        return NULL;
    }

    obs_data_t *data = obs_data_create();
    obs_data_set_string(data, "name", config->name);
    obs_data_set_int(data, "type", config->type);
    obs_data_set_bool(data, "enabled", config->enabled);
    
    // Video settings
    obs_data_set_int(data, "video_bitrate", config->video.bitrate);
    obs_data_set_int(data, "width", config->video.width);
    obs_data_set_int(data, "height", config->video.height);
    obs_data_set_int(data, "fps", config->video.fps);
    obs_data_set_string(data, "encoder_id", config->video.encoder);
    obs_data_set_string(data, "preset", config->video.preset);
    obs_data_set_string(data, "profile", config->video.profile);
    
    // Audio settings
    obs_data_set_int(data, "audio_bitrate", config->audio.bitrate);
    obs_data_set_int(data, "audio_sample_rate", config->audio.sample_rate);
    obs_data_set_int(data, "audio_channels", config->audio.channels);
    obs_data_set_string(data, "audio_encoder", config->audio.encoder);
    
    // Streaming settings
    obs_data_set_string(data, "url", config->url);
    obs_data_set_string(data, "key", config->key);
    
    // Recording settings
    obs_data_set_string(data, "file_path", config->file_path);
    obs_data_set_string(data, "format", config->format);

    if (config->encoder_settings) {
        obs_data_set_obj(data, "encoder_settings", config->encoder_settings);
    }
    if (config->service_settings) {
        obs_data_set_obj(data, "service_settings", config->service_settings);
    }

    return data;
}

static flexoutput_output_config_t *output_config_from_data(obs_data_t *data)
{
    if (!data) {
        return NULL;
    }

    const char *name = obs_data_get_string(data, "name");
    if (!name || !*name) {
        return NULL;
    }

    flexoutput_output_config_t *config = bzalloc(sizeof(flexoutput_output_config_t));
    config->name = bstrdup(name);
    config->type = (flexoutput_output_type_t)obs_data_get_int(data, "type");
    if (config->type != FLEXOUTPUT_OUTPUT_RTMP && config->type != FLEXOUTPUT_OUTPUT_FILE) {
        config->type = FLEXOUTPUT_OUTPUT_RTMP;  // Default fallback
    }
    config->enabled = obs_data_get_bool(data, "enabled");
    
    // Video settings
    config->video.bitrate = (uint32_t)obs_data_get_int(data, "video_bitrate");
    config->video.width = (uint32_t)obs_data_get_int(data, "width");
    config->video.height = (uint32_t)obs_data_get_int(data, "height");
    config->video.fps = (uint32_t)obs_data_get_int(data, "fps");
    // Handle legacy fps_num/fps_den format
    if (config->video.fps == 0) {
        uint32_t fps_num = (uint32_t)obs_data_get_int(data, "fps_num");
        uint32_t fps_den = (uint32_t)obs_data_get_int(data, "fps_den");
        config->video.fps = (fps_den > 0) ? (fps_num / fps_den) : 30;
    }
    config->video.encoder = bstrdup(obs_data_get_string(data, "encoder_id"));
    config->video.preset = bstrdup(obs_data_get_string(data, "preset"));
    config->video.profile = bstrdup(obs_data_get_string(data, "profile"));
    
    // Audio settings
    config->audio.bitrate = (uint32_t)obs_data_get_int(data, "audio_bitrate");
    config->audio.sample_rate = (uint32_t)obs_data_get_int(data, "audio_sample_rate");
    config->audio.channels = (uint32_t)obs_data_get_int(data, "audio_channels");
    if (config->audio.channels == 0) {
        config->audio.channels = 2; // Default to stereo
    }
    config->audio.encoder = bstrdup(obs_data_get_string(data, "audio_encoder"));
    
    // Streaming settings
    config->url = bstrdup(obs_data_get_string(data, "url"));
    config->key = bstrdup(obs_data_get_string(data, "key"));
    
    // Recording settings
    config->file_path = bstrdup(obs_data_get_string(data, "file_path"));
    config->format = bstrdup(obs_data_get_string(data, "format"));

    config->encoder_settings = obs_data_create();
    obs_data_t *encoder_settings = obs_data_get_obj(data, "encoder_settings");
    if (encoder_settings) {
        obs_data_apply(config->encoder_settings, encoder_settings);
        obs_data_release(encoder_settings);
    }

    config->service_settings = obs_data_create();
    obs_data_t *service_settings = obs_data_get_obj(data, "service_settings");
    if (service_settings) {
        obs_data_apply(config->service_settings, service_settings);
        obs_data_release(service_settings);
    }

    return config;
}

static obs_data_t *mapping_rule_to_data(const flexoutput_mapping_rule_t *rule)
{
    if (!rule) {
        return NULL;
    }

    obs_data_t *data = obs_data_create();
    obs_data_set_string(data, "output_name", rule->output_name);
    obs_data_set_int(data, "mode", rule->type);
    obs_data_set_bool(data, "enabled", rule->enabled);
    obs_data_set_bool(data, "auto_manage", rule->auto_manage);
    obs_data_set_bool(data, "auto_add_new", rule->auto_add_new);
    obs_data_set_bool(data, "auto_remove_missing", rule->auto_remove_missing);

    obs_data_array_t *sources_array = obs_data_array_create();
    for (size_t i = 0; i < rule->source_names.num; i++) {
        obs_data_t *source_data = obs_data_create();
        obs_data_set_string(source_data, "name", rule->source_names.array[i]);
        obs_data_array_push_back(sources_array, source_data);
        obs_data_release(source_data);
    }
    obs_data_set_array(data, "sources", sources_array);
    obs_data_array_release(sources_array);
    
    obs_data_array_t *filters_array = obs_data_array_create();
    for (size_t i = 0; i < rule->source_type_filters.num; i++) {
        obs_data_t *filter_data = obs_data_create();
        obs_data_set_string(filter_data, "filter", rule->source_type_filters.array[i]);
        obs_data_set_bool(filter_data, "include", rule->source_type_include.array[i]);
        obs_data_array_push_back(filters_array, filter_data);
        obs_data_release(filter_data);
    }
    obs_data_set_array(data, "type_filters", filters_array);
    obs_data_array_release(filters_array);

    return data;
}

static flexoutput_mapping_rule_t *mapping_rule_from_data(obs_data_t *data)
{
    if (!data) {
        return NULL;
    }

    const char *output_name = obs_data_get_string(data, "output_name");
    if (!output_name || !*output_name) {
        return NULL;
    }

    flexoutput_mapping_rule_t *rule = bzalloc(sizeof(flexoutput_mapping_rule_t));
    rule->output_name = bstrdup(output_name);
    rule->type = (flexoutput_mapping_type_t)obs_data_get_int(data, "mode");
    rule->enabled = obs_data_get_bool(data, "enabled");
    rule->auto_manage = obs_data_get_bool(data, "auto_manage");
    rule->auto_add_new = obs_data_get_bool(data, "auto_add_new");
    rule->auto_remove_missing = obs_data_get_bool(data, "auto_remove_missing");
    
    da_init(rule->source_names);
    da_init(rule->source_type_filters);
    da_init(rule->source_type_include);
    
    memset(&rule->stats, 0, sizeof(rule->stats));
    rule->change_callback = NULL;
    rule->callback_data = NULL;
    
    if (pthread_mutex_init(&rule->mutex, NULL) != 0) {
        bfree(rule->output_name);
        bfree(rule);
        return NULL;
    }

    obs_data_array_t *sources_array = obs_data_get_array(data, "sources");
    if (sources_array) {
        size_t count = obs_data_array_count(sources_array);
        for (size_t i = 0; i < count; i++) {
            obs_data_t *source_data = obs_data_array_item(sources_array, i);
            const char *source_name = obs_data_get_string(source_data, "name");
            if (source_name && *source_name) {
                char *name_copy = bstrdup(source_name);
                da_push_back(rule->source_names, &name_copy);
            }
            obs_data_release(source_data);
        }
        obs_data_array_release(sources_array);
    }
    
    obs_data_array_t *filters_array = obs_data_get_array(data, "type_filters");
    if (filters_array) {
        size_t count = obs_data_array_count(filters_array);
        for (size_t i = 0; i < count; i++) {
            obs_data_t *filter_data = obs_data_array_item(filters_array, i);
            const char *filter = obs_data_get_string(filter_data, "filter");
            bool include = obs_data_get_bool(filter_data, "include");
            if (filter && *filter) {
                char *filter_copy = bstrdup(filter);
                da_push_back(rule->source_type_filters, &filter_copy);
                da_push_back(rule->source_type_include, &include);
            }
            obs_data_release(filter_data);
        }
        obs_data_array_release(filters_array);
    }

    return rule;
}

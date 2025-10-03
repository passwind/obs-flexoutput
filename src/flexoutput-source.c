#include "flexoutput-source.h"
#include <obs-frontend-api.h>

// Internal state
static bool source_system_initialized = false;
static DARRAY(source_event_callback_t) event_callbacks;
static DARRAY(void*) event_user_data;

// Forward declarations
static void source_created_handler(void *data, calldata_t *cd);
static void source_destroyed_handler(void *data, calldata_t *cd);
static void source_renamed_handler(void *data, calldata_t *cd);
static void source_activated_handler(void *data, calldata_t *cd);
static void source_deactivated_handler(void *data, calldata_t *cd);
static void notify_source_event(source_event_type_t type, obs_source_t *source, 
                                const char *old_name);

// Helper callback for list updates
static bool list_update_callback(void *data, obs_source_t *source)
{
    struct enum_data {
        source_list_t *list;
    };
    
    struct enum_data *ed = (struct enum_data*)data;
    
    if (flexoutput_source_matches_filter(source, ed->list->filter)) {
        flexoutput_source_info_t *info = flexoutput_source_get_info_from_source(source);
        if (info) {
            da_push_back(ed->list->sources, &info);
        }
    }
    
    return true;
}

flexoutput_error_t flexoutput_source_init(void)
{
    if (source_system_initialized) {
        return FLEXOUTPUT_SUCCESS;
    }

    da_init(event_callbacks);
    da_init(event_user_data);

    // Register for source events
    signal_handler_t *handler = obs_get_signal_handler();
    if (handler) {
        signal_handler_connect(handler, "source_create", source_created_handler, NULL);
        signal_handler_connect(handler, "source_destroy", source_destroyed_handler, NULL);
        signal_handler_connect(handler, "source_rename", source_renamed_handler, NULL);
        signal_handler_connect(handler, "source_activate", source_activated_handler, NULL);
        signal_handler_connect(handler, "source_deactivate", source_deactivated_handler, NULL);
    }

    source_system_initialized = true;
    
    FLEXOUTPUT_LOG_INFO("Source management system initialized");
    return FLEXOUTPUT_SUCCESS;
}

void flexoutput_source_cleanup(void)
{
    if (!source_system_initialized) {
        return;
    }

    // Disconnect signal handlers
    signal_handler_t *handler = obs_get_signal_handler();
    if (handler) {
        signal_handler_disconnect(handler, "source_create", source_created_handler, NULL);
        signal_handler_disconnect(handler, "source_destroy", source_destroyed_handler, NULL);
        signal_handler_disconnect(handler, "source_rename", source_renamed_handler, NULL);
        signal_handler_disconnect(handler, "source_activate", source_activated_handler, NULL);
        signal_handler_disconnect(handler, "source_deactivate", source_deactivated_handler, NULL);
    }

    da_free(event_callbacks);
    da_free(event_user_data);

    source_system_initialized = false;
    
    FLEXOUTPUT_LOG_INFO("Source management system cleaned up");
}

flexoutput_error_t flexoutput_source_enumerate(source_enum_callback_t callback, void *data)
{
    if (!callback) {
        return FLEXOUTPUT_ERROR_INVALID_PARAM;
    }

    obs_enum_sources(callback, data);
    return FLEXOUTPUT_SUCCESS;
}

// Helper structure for type filtering
struct type_filter_data {
    const char *type_filter;
    source_enum_callback_t callback;
    void *user_data;
};

// Helper callback function for type filtering
static bool type_filter_callback(void *data, obs_source_t *source)
{
    struct type_filter_data *ed = (struct type_filter_data*)data;
    
    if (ed->type_filter) {
        const char *source_type = obs_source_get_id(source);
        if (!source_type || strcmp(source_type, ed->type_filter) != 0) {
            return true; // Continue enumeration
        }
    }
    
    return ed->callback(ed->user_data, source);
}

flexoutput_error_t flexoutput_source_enumerate_by_type(const char *type, 
                                                       source_enum_callback_t callback, 
                                                       void *data)
{
    if (!callback) {
        return FLEXOUTPUT_ERROR_INVALID_PARAM;
    }

    struct type_filter_data filter_data = {type, callback, data};

    obs_enum_sources(type_filter_callback, &filter_data);
    return FLEXOUTPUT_SUCCESS;
}

// Helper structure for collecting source names
struct source_names_data {
    char **names;
    size_t count;
    size_t capacity;
};

// Helper callback function for collecting source names
static bool collect_source_names_callback(void *data, obs_source_t *source)
{
    struct source_names_data *snd = (struct source_names_data*)data;
    
    const char *name = obs_source_get_name(source);
    if (!name) {
        return true; // Continue enumeration
    }
    
    // Resize array if needed
    if (snd->count >= snd->capacity) {
        size_t new_capacity = snd->capacity == 0 ? 16 : snd->capacity * 2;
        char **new_names = brealloc(snd->names, new_capacity * sizeof(char*));
        if (!new_names) {
            return false; // Stop enumeration on memory error
        }
        snd->names = new_names;
        snd->capacity = new_capacity;
    }
    
    // Copy source name
    snd->names[snd->count] = bstrdup(name);
    if (!snd->names[snd->count]) {
        return false; // Stop enumeration on memory error
    }
    
    snd->count++;
    return true; // Continue enumeration
}

flexoutput_error_t flexoutput_source_enumerate_all(char ***sources, size_t *count)
{
    if (!sources || !count) {
        return FLEXOUTPUT_ERROR_INVALID_PARAM;
    }
    
    *sources = NULL;
    *count = 0;
    
    struct source_names_data snd = {NULL, 0, 0};
    
    obs_enum_sources(collect_source_names_callback, &snd);
    
    if (snd.count == 0) {
        // No sources found, return empty array
        *sources = NULL;
        *count = 0;
        return FLEXOUTPUT_SUCCESS;
    }
    
    // Resize to exact size
    if (snd.count < snd.capacity) {
        char **final_names = brealloc(snd.names, snd.count * sizeof(char*));
        if (final_names) {
            snd.names = final_names;
        }
    }
    
    *sources = snd.names;
    *count = snd.count;
    
    return FLEXOUTPUT_SUCCESS;
}

void flexoutput_source_free_string_array(char **sources, size_t count)
{
    if (!sources) {
        return;
    }
    
    for (size_t i = 0; i < count; i++) {
        if (sources[i]) {
            bfree(sources[i]);
        }
    }
    
    bfree(sources);
}

flexoutput_source_info_t *flexoutput_source_get_info(const char *name)
{
    if (!name) {
        return NULL;
    }

    obs_source_t *source = obs_get_source_by_name(name);
    if (!source) {
        return NULL;
    }

    flexoutput_source_info_t *info = flexoutput_source_get_info_from_source(source);
    obs_source_release(source);
    
    return info;
}

flexoutput_source_info_t *flexoutput_source_get_info_from_source(obs_source_t *source)
{
    if (!source) {
        return NULL;
    }

    flexoutput_source_info_t *info = bzalloc(sizeof(flexoutput_source_info_t));
    
    info->name = bstrdup(obs_source_get_name(source));
    info->type = bstrdup(obs_source_get_id(source));
    info->display_name = bstrdup(obs_source_get_display_name(obs_source_get_id(source)));
    
    info->capabilities = obs_source_get_output_flags(source);
    info->has_video = (info->capabilities & OBS_SOURCE_VIDEO) != 0;
    info->has_audio = (info->capabilities & OBS_SOURCE_AUDIO) != 0;
    info->is_scene = obs_source_is_scene(source);
    info->is_group = obs_source_is_group(source);
    
    if (info->has_video) {
        info->width = obs_source_get_width(source);
        info->height = obs_source_get_height(source);
    }
    
    info->active = obs_source_active(source);
    info->showing = obs_source_showing(source);
    info->enabled = obs_source_enabled(source);
    info->muted = obs_source_muted(source);
    
    if (info->has_audio) {
        info->volume = obs_source_get_volume(source);
    }

    return info;
}

void flexoutput_source_free_info(flexoutput_source_info_t *info)
{
    if (!info) {
        return;
    }

    FLEXOUTPUT_SAFE_FREE(info->name);
    FLEXOUTPUT_SAFE_FREE(info->type);
    FLEXOUTPUT_SAFE_FREE(info->display_name);
    
    bfree(info);
}

bool flexoutput_source_exists(const char *name)
{
    if (!name) {
        return false;
    }

    obs_source_t *source = obs_get_source_by_name(name);
    if (source) {
        obs_source_release(source);
        return true;
    }
    return false;
}

bool flexoutput_source_is_active(const char *name)
{
    if (!name) {
        return false;
    }

    obs_source_t *source = obs_get_source_by_name(name);
    if (!source) {
        return false;
    }

    bool active = obs_source_active(source);
    obs_source_release(source);
    
    return active;
}

obs_source_t *flexoutput_source_get_ref(const char *name)
{
    if (!name) {
        return NULL;
    }

    return obs_get_source_by_name(name);
}

obs_weak_source_t *flexoutput_source_get_weak_ref(const char *name)
{
    if (!name) {
        return NULL;
    }

    obs_source_t *source = obs_get_source_by_name(name);
    if (!source) {
        return NULL;
    }

    obs_weak_source_t *weak_ref = obs_source_get_weak_source(source);
    obs_source_release(source);
    
    return weak_ref;
}

flexoutput_error_t flexoutput_source_register_events(source_event_callback_t callback, 
                                                      void *user_data)
{
    if (!callback) {
        return FLEXOUTPUT_ERROR_INVALID_PARAM;
    }

    da_push_back(event_callbacks, &callback);
    da_push_back(event_user_data, &user_data);
    
    return FLEXOUTPUT_SUCCESS;
}

flexoutput_error_t flexoutput_source_unregister_events(source_event_callback_t callback)
{
    if (!callback) {
        return FLEXOUTPUT_ERROR_INVALID_PARAM;
    }

    for (size_t i = 0; i < event_callbacks.num; i++) {
        if (event_callbacks.array[i] == callback) {
            da_erase(event_callbacks, i);
            da_erase(event_user_data, i);
            return FLEXOUTPUT_SUCCESS;
        }
    }
    
    return FLEXOUTPUT_ERROR_NOT_FOUND;
}

bool flexoutput_source_matches_filter(obs_source_t *source, source_filter_type_t filter)
{
    if (!source) {
        return false;
    }

    uint32_t caps = obs_source_get_output_flags(source);
    
    switch (filter) {
    case SOURCE_FILTER_ALL:
        return true;
        
    case SOURCE_FILTER_VIDEO_ONLY:
        return (caps & OBS_SOURCE_VIDEO) && !(caps & OBS_SOURCE_AUDIO);
        
    case SOURCE_FILTER_AUDIO_ONLY:
        return (caps & OBS_SOURCE_AUDIO) && !(caps & OBS_SOURCE_VIDEO);
        
    case SOURCE_FILTER_COMPOSITE:
        return (caps & OBS_SOURCE_COMPOSITE) != 0;
        
    case SOURCE_FILTER_SCENE:
        return obs_source_is_scene(source);
        
    case SOURCE_FILTER_TRANSITION:
        return obs_source_get_type(source) == OBS_SOURCE_TYPE_TRANSITION;
        
    case SOURCE_FILTER_FILTER:
        return (caps & OBS_SOURCE_CAP_DISABLED) == 0 && 
               obs_source_get_type(source) == OBS_SOURCE_TYPE_FILTER;
        
    default:
        return false;
    }
}

uint32_t flexoutput_source_get_capabilities(obs_source_t *source)
{
    return source ? obs_source_get_output_flags(source) : 0;
}

bool flexoutput_source_has_video(obs_source_t *source)
{
    return source && (obs_source_get_output_flags(source) & OBS_SOURCE_VIDEO) != 0;
}

bool flexoutput_source_has_audio(obs_source_t *source)
{
    return source && (obs_source_get_output_flags(source) & OBS_SOURCE_AUDIO) != 0;
}

bool flexoutput_source_get_dimensions(obs_source_t *source, uint32_t *width, uint32_t *height)
{
    if (!source || !width || !height) {
        return false;
    }

    if (!flexoutput_source_has_video(source)) {
        return false;
    }

    *width = obs_source_get_width(source);
    *height = obs_source_get_height(source);
    
    return *width > 0 && *height > 0;
}

source_list_t *flexoutput_source_create_list(source_filter_type_t filter, bool auto_update)
{
    source_list_t *list = bzalloc(sizeof(source_list_t));
    da_init(list->sources);
    list->filter = filter;
    list->auto_update = auto_update;
    
    // Initial population
    flexoutput_source_update_list(list);
    
    return list;
}

void flexoutput_source_free_list(source_list_t *list)
{
    if (!list) {
        return;
    }

    for (size_t i = 0; i < list->sources.num; i++) {
        flexoutput_source_free_info(list->sources.array[i]);
    }
    da_free(list->sources);
    
    bfree(list);
}

flexoutput_error_t flexoutput_source_update_list(source_list_t *list)
{
    if (!list) {
        return FLEXOUTPUT_ERROR_INVALID_PARAM;
    }

    // Clear existing sources
    for (size_t i = 0; i < list->sources.num; i++) {
        flexoutput_source_free_info(list->sources.array[i]);
    }
    da_resize(list->sources, 0);

    // Enumerate and filter sources
    struct enum_data {
        source_list_t *list;
    } data = {list};

    obs_enum_sources(list_update_callback, &data);
    
    return FLEXOUTPUT_SUCCESS;
}

size_t flexoutput_source_list_count(const source_list_t *list)
{
    return list ? list->sources.num : 0;
}

flexoutput_source_info_t *flexoutput_source_list_get(const source_list_t *list, size_t index)
{
    if (!list || index >= list->sources.num) {
        return NULL;
    }
    return list->sources.array[index];
}

flexoutput_source_info_t *flexoutput_source_list_find(const source_list_t *list, const char *name)
{
    if (!list || !name) {
        return NULL;
    }

    for (size_t i = 0; i < list->sources.num; i++) {
        if (strcmp(list->sources.array[i]->name, name) == 0) {
            return list->sources.array[i];
        }
    }
    return NULL;
}

bool flexoutput_source_validate_for_output(const char *source_name, 
                                            const flexoutput_output_config_t *output_config,
                                            char **error_msg)
{
    if (!source_name || !output_config) {
        if (error_msg) {
            *error_msg = bstrdup("Invalid parameters");
        }
        return false;
    }

    obs_source_t *source = obs_get_source_by_name(source_name);
    if (!source) {
        if (error_msg) {
            *error_msg = bstrdup("Source not found");
        }
        return false;
    }

    bool valid = flexoutput_source_is_compatible(source, output_config);
    
    if (!valid && error_msg) {
        *error_msg = bstrdup("Source is not compatible with output configuration");
    }
    
    obs_source_release(source);
    return valid;
}

bool flexoutput_source_is_compatible(obs_source_t *source, 
                                     const flexoutput_output_config_t *output_config)
{
    if (!source || !output_config) {
        return false;
    }

    // Check if source has video output for video outputs
    if (output_config->video.width > 0 && output_config->video.height > 0) {
        if (!flexoutput_source_has_video(source)) {
            return false;
        }
    }

    // Additional compatibility checks can be added here
    // For example, format compatibility, resolution limits, etc.
    
    return true;
}

const char *flexoutput_source_get_type_string(obs_source_t *source)
{
    return source ? obs_source_get_id(source) : NULL;
}

char *flexoutput_source_get_display_name(obs_source_t *source)
{
    if (!source) {
        return NULL;
    }

    const char *id = obs_source_get_id(source);
    const char *display_name = obs_source_get_display_name(id);
    
    return bstrdup(display_name ? display_name : id);
}

flexoutput_source_info_t *flexoutput_source_copy_info(const flexoutput_source_info_t *src)
{
    if (!src) {
        return NULL;
    }

    flexoutput_source_info_t *copy = bzalloc(sizeof(flexoutput_source_info_t));
    
    copy->name = bstrdup(src->name);
    copy->type = bstrdup(src->type);
    copy->display_name = bstrdup(src->display_name);
    copy->capabilities = src->capabilities;
    copy->has_video = src->has_video;
    copy->has_audio = src->has_audio;
    copy->is_scene = src->is_scene;
    copy->is_group = src->is_group;
    copy->width = src->width;
    copy->height = src->height;
    copy->active = src->active;
    copy->showing = src->showing;
    copy->enabled = src->enabled;
    copy->muted = src->muted;
    copy->volume = src->volume;

    return copy;
}

int flexoutput_source_compare_info(const flexoutput_source_info_t *a, 
                                   const flexoutput_source_info_t *b)
{
    if (!a && !b) return 0;
    if (!a) return -1;
    if (!b) return 1;
    
    return strcmp(a->name, b->name);
}

// Signal handlers
static void source_created_handler(void *data, calldata_t *cd)
{
    UNUSED_PARAMETER(data);
    
    obs_source_t *source = (obs_source_t*)calldata_ptr(cd, "source");
    if (source) {
        notify_source_event(SOURCE_EVENT_CREATED, source, NULL);
    }
}

static void source_destroyed_handler(void *data, calldata_t *cd)
{
    UNUSED_PARAMETER(data);
    
    obs_source_t *source = (obs_source_t*)calldata_ptr(cd, "source");
    if (source) {
        notify_source_event(SOURCE_EVENT_DESTROYED, source, NULL);
    }
}

static void source_renamed_handler(void *data, calldata_t *cd)
{
    UNUSED_PARAMETER(data);
    
    obs_source_t *source = (obs_source_t*)calldata_ptr(cd, "source");
    const char *old_name = calldata_string(cd, "prev_name");
    
    if (source) {
        notify_source_event(SOURCE_EVENT_RENAMED, source, old_name);
    }
}

static void source_activated_handler(void *data, calldata_t *cd)
{
    UNUSED_PARAMETER(data);
    
    obs_source_t *source = (obs_source_t*)calldata_ptr(cd, "source");
    if (source) {
        notify_source_event(SOURCE_EVENT_ACTIVATED, source, NULL);
    }
}

static void source_deactivated_handler(void *data, calldata_t *cd)
{
    UNUSED_PARAMETER(data);
    
    obs_source_t *source = (obs_source_t*)calldata_ptr(cd, "source");
    if (source) {
        notify_source_event(SOURCE_EVENT_DEACTIVATED, source, NULL);
    }
}

static void notify_source_event(source_event_type_t type, obs_source_t *source, 
                                const char *old_name)
{
    if (!source) {
        return;
    }

    source_event_t event = {0};
    event.type = type;
    event.source_name = bstrdup(obs_source_get_name(source));
    event.old_name = old_name ? bstrdup(old_name) : NULL;
    event.source = source;

    for (size_t i = 0; i < event_callbacks.num; i++) {
        event.user_data = event_user_data.array[i];
        event_callbacks.array[i](&event);
    }

    FLEXOUTPUT_SAFE_FREE(event.source_name);
    FLEXOUTPUT_SAFE_FREE(event.old_name);
}

#pragma once

#include "flexoutput-types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Source Management Module
 * 
 * This module handles source enumeration, monitoring, and reference management.
 * It provides functionality to:
 * - Enumerate all available sources in OBS
 * - Monitor source creation/destruction events
 * - Manage source references for safe access
 * - Track source properties and state changes
 */

// Source enumeration and discovery
typedef bool (*source_enum_callback_t)(void *data, obs_source_t *source);

/**
 * Initialize the source management system
 * @return Error code
 */
flexoutput_error_t flexoutput_source_init(void);

/**
 * Cleanup the source management system
 */
void flexoutput_source_cleanup(void);

/**
 * Enumerate all available sources
 * @param callback Function to call for each source
 * @param data User data to pass to callback
 * @return Error code
 */
flexoutput_error_t flexoutput_source_enumerate(source_enum_callback_t callback, void *data);

/**
 * Enumerate sources by type
 * @param type Source type filter (NULL for all types)
 * @param callback Function to call for each source
 * @param data User data to pass to callback
 * @return Error code
 */
flexoutput_error_t flexoutput_source_enumerate_by_type(const char *type, 
                                                       source_enum_callback_t callback, 
                                                       void *data);

/**
 * Enumerate all sources and return their names as an array
 * @param sources Output array of source names (must be freed with flexoutput_source_free_string_array)
 * @param count Output count of sources
 * @return Error code
 */
flexoutput_error_t flexoutput_source_enumerate_all(char ***sources, size_t *count);

/**
 * Free source names array returned by flexoutput_source_enumerate_all
 * @param sources Array of source names
 * @param count Number of sources
 */
void flexoutput_source_free_string_array(char **sources, size_t count);

/**
 * Get source information by name
 * @param name Source name
 * @return Source info structure (must be freed with flexoutput_source_free_info)
 */
flexoutput_source_info_t *flexoutput_source_get_info(const char *name);

/**
 * Get source information by source reference
 * @param source Source reference
 * @return Source info structure (must be freed with flexoutput_source_free_info)
 */
flexoutput_source_info_t *flexoutput_source_get_info_from_source(obs_source_t *source);

/**
 * Free source information structure
 * @param info Source info to free
 */
void flexoutput_source_free_info(flexoutput_source_info_t *info);

/**
 * Check if source exists
 * @param name Source name
 * @return true if source exists
 */
bool flexoutput_source_exists(const char *name);

/**
 * Check if source is valid and active
 * @param name Source name
 * @return true if source is valid and active
 */
bool flexoutput_source_is_active(const char *name);

/**
 * Get source reference (adds reference count)
 * @param name Source name
 * @return Source reference (must be released with obs_source_release)
 */
obs_source_t *flexoutput_source_get_ref(const char *name);

/**
 * Get weak source reference (does not add reference count)
 * @param name Source name
 * @return Weak source reference
 */
obs_weak_source_t *flexoutput_source_get_weak_ref(const char *name);

// Source monitoring and events
typedef enum {
    SOURCE_EVENT_CREATED,
    SOURCE_EVENT_DESTROYED,
    SOURCE_EVENT_RENAMED,
    SOURCE_EVENT_ACTIVATED,
    SOURCE_EVENT_DEACTIVATED,
    SOURCE_EVENT_PROPERTIES_CHANGED
} source_event_type_t;

typedef struct {
    source_event_type_t type;
    char *source_name;
    char *old_name;  // For rename events
    obs_source_t *source;
    void *user_data;
} source_event_t;

typedef void (*source_event_callback_t)(const source_event_t *event);

/**
 * Register for source events
 * @param callback Callback function for events
 * @param user_data User data to pass to callback
 * @return Error code
 */
flexoutput_error_t flexoutput_source_register_events(source_event_callback_t callback, 
                                                      void *user_data);

/**
 * Unregister from source events
 * @param callback Callback function to unregister
 * @return Error code
 */
flexoutput_error_t flexoutput_source_unregister_events(source_event_callback_t callback);

// Source filtering and validation
typedef enum {
    SOURCE_FILTER_ALL,
    SOURCE_FILTER_VIDEO_ONLY,
    SOURCE_FILTER_AUDIO_ONLY,
    SOURCE_FILTER_COMPOSITE,
    SOURCE_FILTER_SCENE,
    SOURCE_FILTER_TRANSITION,
    SOURCE_FILTER_FILTER
} source_filter_type_t;

/**
 * Check if source matches filter criteria
 * @param source Source to check
 * @param filter Filter type
 * @return true if source matches filter
 */
bool flexoutput_source_matches_filter(obs_source_t *source, source_filter_type_t filter);

/**
 * Get source capabilities
 * @param source Source to check
 * @return Source capabilities flags
 */
uint32_t flexoutput_source_get_capabilities(obs_source_t *source);

/**
 * Check if source has video output
 * @param source Source to check
 * @return true if source has video
 */
bool flexoutput_source_has_video(obs_source_t *source);

/**
 * Check if source has audio output
 * @param source Source to check
 * @return true if source has audio
 */
bool flexoutput_source_has_audio(obs_source_t *source);

/**
 * Get source video dimensions
 * @param source Source to check
 * @param width Output width
 * @param height Output height
 * @return true if dimensions are valid
 */
bool flexoutput_source_get_dimensions(obs_source_t *source, uint32_t *width, uint32_t *height);

// Source list management
typedef struct {
    DARRAY(flexoutput_source_info_t*) sources;
    source_filter_type_t filter;
    bool auto_update;
} source_list_t;

/**
 * Create a source list
 * @param filter Filter type for sources
 * @param auto_update Whether to automatically update the list
 * @return Source list (must be freed with flexoutput_source_free_list)
 */
source_list_t *flexoutput_source_create_list(source_filter_type_t filter, bool auto_update);

/**
 * Free a source list
 * @param list Source list to free
 */
void flexoutput_source_free_list(source_list_t *list);

/**
 * Update source list (refresh from current sources)
 * @param list Source list to update
 * @return Error code
 */
flexoutput_error_t flexoutput_source_update_list(source_list_t *list);

/**
 * Get source count in list
 * @param list Source list
 * @return Number of sources
 */
size_t flexoutput_source_list_count(const source_list_t *list);

/**
 * Get source info by index
 * @param list Source list
 * @param index Source index
 * @return Source info (do not free, owned by list)
 */
flexoutput_source_info_t *flexoutput_source_list_get(const source_list_t *list, size_t index);

/**
 * Find source in list by name
 * @param list Source list
 * @param name Source name
 * @return Source info (do not free, owned by list)
 */
flexoutput_source_info_t *flexoutput_source_list_find(const source_list_t *list, const char *name);

// Source validation for mapping
/**
 * Validate source for output mapping
 * @param source_name Source name
 * @param output_config Output configuration
 * @param error_msg Output error message (optional)
 * @return true if source is valid for mapping
 */
bool flexoutput_source_validate_for_output(const char *source_name, 
                                            const flexoutput_output_config_t *output_config,
                                            char **error_msg);

/**
 * Check if source is compatible with output format
 * @param source Source to check
 * @param output_config Output configuration
 * @return true if compatible
 */
bool flexoutput_source_is_compatible(obs_source_t *source, 
                                     const flexoutput_output_config_t *output_config);

// Utility functions
/**
 * Get source type string
 * @param source Source
 * @return Source type string (do not free)
 */
const char *flexoutput_source_get_type_string(obs_source_t *source);

/**
 * Get source display name
 * @param source Source
 * @return Display name (must be freed)
 */
char *flexoutput_source_get_display_name(obs_source_t *source);

/**
 * Copy source info structure
 * @param src Source info to copy
 * @return Copied source info (must be freed)
 */
flexoutput_source_info_t *flexoutput_source_copy_info(const flexoutput_source_info_t *src);

/**
 * Compare two source info structures
 * @param a First source info
 * @param b Second source info
 * @return 0 if equal, <0 if a < b, >0 if a > b
 */
int flexoutput_source_compare_info(const flexoutput_source_info_t *a, 
                                   const flexoutput_source_info_t *b);

#ifdef __cplusplus
}
#endif

/*
OBS FlexOutput Plugin
Copyright (C) 2024 FlexOutput Team

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program. If not, see <https://www.gnu.org/licenses/>
*/

#include <obs-module.h>
#include <obs-frontend-api.h>
#include <plugin-support.h>
#include <util/threading.h>
#include <util/platform.h>

#include "flexoutput-types.h"
#include "flexoutput-config.h"
#include "flexoutput-source.h"
#include "flexoutput-video.h"
#include "flexoutput-audio.h"
#include "flexoutput-output.h"
#include "flexoutput-mapping.h"

#ifdef ENABLE_FRONTEND_API
#include "flexoutput-ui.h"
#endif

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")

// Global plugin instance
flexoutput_plugin_t *g_flexoutput_plugin = NULL;

// Forward declarations
static void flexoutput_frontend_event_callback(enum obs_frontend_event event, void *data);
static void flexoutput_source_event_callback(void *data, calldata_t *cd);
static void flexoutput_output_event_callback(void *data, calldata_t *cd);
static bool flexoutput_initialize_plugin(void);
static void flexoutput_cleanup_plugin(void);
static void flexoutput_update_source_mappings(void);
static void flexoutput_handle_source_create(const char *source_name);
static void flexoutput_handle_source_destroy(const char *source_name);
static void flexoutput_handle_source_rename(const char *old_name, const char *new_name);

bool obs_module_load(void)
{
    obs_log(LOG_INFO, "Loading OBS FlexOutput Plugin (version %s)", PLUGIN_VERSION);

    // Initialize plugin
    if (!flexoutput_initialize_plugin()) {
        obs_log(LOG_ERROR, "Failed to initialize FlexOutput plugin");
        return false;
    }

    // Register frontend callbacks
    obs_frontend_add_event_callback(flexoutput_frontend_event_callback, NULL);

    // Register source signal callbacks
    signal_handler_t *source_handler = obs_get_signal_handler();
    signal_handler_connect(source_handler, "source_create", flexoutput_source_event_callback, NULL);
    signal_handler_connect(source_handler, "source_destroy", flexoutput_source_event_callback, NULL);
    signal_handler_connect(source_handler, "source_rename", flexoutput_source_event_callback, NULL);

    // Register output signal callbacks
    signal_handler_t *output_handler = obs_output_get_signal_handler(NULL);
    if (output_handler) {
        signal_handler_connect(output_handler, "start", flexoutput_output_event_callback, NULL);
        signal_handler_connect(output_handler, "stop", flexoutput_output_event_callback, NULL);
        signal_handler_connect(output_handler, "stopping", flexoutput_output_event_callback, NULL);
    }

#ifdef ENABLE_FRONTEND_API
    // Initialize UI if frontend API is available
    if (obs_frontend_get_main_window()) {
        flexoutput_ui_init();
        obs_log(LOG_INFO, "FlexOutput UI initialized");
    }
#endif

    obs_log(LOG_INFO, "OBS FlexOutput Plugin loaded successfully");
    return true;
}

void obs_module_unload(void)
{
    obs_log(LOG_INFO, "Unloading OBS FlexOutput Plugin");

#ifdef ENABLE_FRONTEND_API
    // Cleanup UI
    flexoutput_ui_cleanup();
#endif

    // Disconnect signal handlers
    signal_handler_t *source_handler = obs_get_signal_handler();
    signal_handler_disconnect(source_handler, "source_create", flexoutput_source_event_callback, NULL);
    signal_handler_disconnect(source_handler, "source_destroy", flexoutput_source_event_callback, NULL);
    signal_handler_disconnect(source_handler, "source_rename", flexoutput_source_event_callback, NULL);

    signal_handler_t *output_handler = obs_output_get_signal_handler(NULL);
    if (output_handler) {
        signal_handler_disconnect(output_handler, "start", flexoutput_output_event_callback, NULL);
        signal_handler_disconnect(output_handler, "stop", flexoutput_output_event_callback, NULL);
        signal_handler_disconnect(output_handler, "stopping", flexoutput_output_event_callback, NULL);
    }

    // Remove frontend callbacks
    obs_frontend_remove_event_callback(flexoutput_frontend_event_callback, NULL);

    // Cleanup plugin
    flexoutput_cleanup_plugin();

    obs_log(LOG_INFO, "OBS FlexOutput Plugin unloaded");
}

// Plugin initialization
static bool flexoutput_initialize_plugin(void)
{
    // Create global plugin instance
    g_flexoutput_plugin = bzalloc(sizeof(flexoutput_plugin_t));
    if (!g_flexoutput_plugin) {
        obs_log(LOG_ERROR, "Failed to allocate plugin instance");
        return false;
    }

    // Initialize plugin state
    g_flexoutput_plugin->initialized = false;
    g_flexoutput_plugin->running = false;
    da_init(g_flexoutput_plugin->outputs);
    da_init(g_flexoutput_plugin->mapping_rules);

    if (pthread_mutex_init(&g_flexoutput_plugin->mutex, NULL) != 0) {
        obs_log(LOG_ERROR, "Failed to initialize plugin mutex");
        bfree(g_flexoutput_plugin);
        g_flexoutput_plugin = NULL;
        return false;
    }

    // Initialize all subsystems
    flexoutput_error_t result;

    // Initialize configuration system
    result = flexoutput_config_init();
    if (result != FLEXOUTPUT_SUCCESS) {
        obs_log(LOG_ERROR, "Failed to initialize configuration system: %d", result);
        goto cleanup;
    }

    // Initialize source management
    result = flexoutput_source_init();
    if (result != FLEXOUTPUT_SUCCESS) {
        obs_log(LOG_ERROR, "Failed to initialize source management: %d", result);
        goto cleanup;
    }

    // Initialize video rendering
    result = flexoutput_video_init();
    if (result != FLEXOUTPUT_SUCCESS) {
        obs_log(LOG_ERROR, "Failed to initialize video rendering: %d", result);
        goto cleanup;
    }

    // Initialize audio mixing
    result = flexoutput_audio_init();
    if (result != FLEXOUTPUT_SUCCESS) {
        obs_log(LOG_ERROR, "Failed to initialize audio mixing: %d", result);
        goto cleanup;
    }

    // Initialize output management
    result = flexoutput_output_init();
    if (result != FLEXOUTPUT_SUCCESS) {
        obs_log(LOG_ERROR, "Failed to initialize output management: %d", result);
        goto cleanup;
    }

    // Initialize mapping rules
    result = flexoutput_mapping_init();
    if (result != FLEXOUTPUT_SUCCESS) {
        obs_log(LOG_ERROR, "Failed to initialize mapping rules: %d", result);
        goto cleanup;
    }

    // Load configuration
    result = flexoutput_config_load();
    if (result != FLEXOUTPUT_SUCCESS) {
        obs_log(LOG_WARNING, "Failed to load configuration, using defaults: %d", result);
        // Continue with default configuration
    }

    g_flexoutput_plugin->initialized = true;
    obs_log(LOG_INFO, "FlexOutput plugin initialized successfully");
    return true;

cleanup:
    flexoutput_cleanup_plugin();
    return false;
}

// Plugin cleanup
static void flexoutput_cleanup_plugin(void)
{
    if (!g_flexoutput_plugin) {
        return;
    }

    pthread_mutex_lock(&g_flexoutput_plugin->mutex);

    // Stop all outputs
    for (size_t i = 0; i < g_flexoutput_plugin->outputs.num; i++) {
        flexoutput_output_instance_t *output = g_flexoutput_plugin->outputs.array[i];
        if (output && flexoutput_output_is_active(output)) {
            flexoutput_output_stop(output);
        }
    }

    // Save configuration before cleanup
    if (g_flexoutput_plugin->initialized) {
        flexoutput_config_save();
    }

    pthread_mutex_unlock(&g_flexoutput_plugin->mutex);

    // Cleanup all subsystems
    flexoutput_mapping_cleanup();
    flexoutput_output_cleanup();
    flexoutput_audio_cleanup();
    flexoutput_video_cleanup();
    flexoutput_source_cleanup();
    flexoutput_config_cleanup();

    // Cleanup plugin instance
    pthread_mutex_destroy(&g_flexoutput_plugin->mutex);
    da_free(g_flexoutput_plugin->outputs);
    da_free(g_flexoutput_plugin->mapping_rules);
    bfree(g_flexoutput_plugin);
    g_flexoutput_plugin = NULL;

    obs_log(LOG_INFO, "FlexOutput plugin cleaned up");
}

// Frontend event callback
static void flexoutput_frontend_event_callback(enum obs_frontend_event event, void *data)
{
    UNUSED_PARAMETER(data);

    if (!g_flexoutput_plugin || !g_flexoutput_plugin->initialized) {
        return;
    }

    switch (event) {
    case OBS_FRONTEND_EVENT_STREAMING_STARTING:
        obs_log(LOG_DEBUG, "Frontend streaming starting");
        break;
    case OBS_FRONTEND_EVENT_STREAMING_STARTED:
        obs_log(LOG_DEBUG, "Frontend streaming started");
        break;
    case OBS_FRONTEND_EVENT_STREAMING_STOPPING:
        obs_log(LOG_DEBUG, "Frontend streaming stopping");
        break;
    case OBS_FRONTEND_EVENT_STREAMING_STOPPED:
        obs_log(LOG_DEBUG, "Frontend streaming stopped");
        break;
    case OBS_FRONTEND_EVENT_SCENE_CHANGED:
        obs_log(LOG_DEBUG, "Scene changed, updating source mappings");
        flexoutput_update_source_mappings();
        break;
    case OBS_FRONTEND_EVENT_SCENE_LIST_CHANGED:
        obs_log(LOG_DEBUG, "Scene list changed, updating source mappings");
        flexoutput_update_source_mappings();
        break;
    case OBS_FRONTEND_EVENT_EXIT:
        obs_log(LOG_DEBUG, "OBS exiting, saving configuration");
        flexoutput_config_save();
        break;
    default:
        break;
    }
}

// Source event callback
static void flexoutput_source_event_callback(void *data, calldata_t *cd)
{
    UNUSED_PARAMETER(data);

    if (!g_flexoutput_plugin || !g_flexoutput_plugin->initialized) {
        return;
    }

    const char *signal_name = calldata_string(cd, "signal");
    obs_source_t *source = calldata_ptr(cd, "source");

    if (!signal_name || !source) {
        return;
    }

    const char *source_name = obs_source_get_name(source);
    if (!source_name) {
        return;
    }

    if (strcmp(signal_name, "source_create") == 0) {
        flexoutput_handle_source_create(source_name);
    } else if (strcmp(signal_name, "source_destroy") == 0) {
        flexoutput_handle_source_destroy(source_name);
    } else if (strcmp(signal_name, "source_rename") == 0) {
        const char *prev_name = calldata_string(cd, "prev_name");
        if (prev_name) {
            flexoutput_handle_source_rename(prev_name, source_name);
        }
    }
}

// Output event callback
static void flexoutput_output_event_callback(void *data, calldata_t *cd)
{
    UNUSED_PARAMETER(data);

    if (!g_flexoutput_plugin || !g_flexoutput_plugin->initialized) {
        return;
    }

    const char *signal_name = calldata_string(cd, "signal");
    obs_output_t *output = calldata_ptr(cd, "output");

    if (!signal_name || !output) {
        return;
    }

    obs_log(LOG_DEBUG, "Output event: %s", signal_name);

    // Handle output events for statistics and monitoring
    if (strcmp(signal_name, "start") == 0) {
        obs_log(LOG_INFO, "Output started");
    } else if (strcmp(signal_name, "stop") == 0) {
        obs_log(LOG_INFO, "Output stopped");
    } else if (strcmp(signal_name, "stopping") == 0) {
        obs_log(LOG_INFO, "Output stopping");
    }
}

// Update source mappings
static void flexoutput_update_source_mappings(void)
{
    if (!g_flexoutput_plugin || !g_flexoutput_plugin->initialized) {
        return;
    }

    pthread_mutex_lock(&g_flexoutput_plugin->mutex);

    // Update all active outputs with current source mappings
    for (size_t i = 0; i < g_flexoutput_plugin->outputs.num; i++) {
        flexoutput_output_instance_t *output = g_flexoutput_plugin->outputs.array[i];
        if (output && flexoutput_output_is_active(output)) {
            // Get mapping rule for this output
            flexoutput_mapping_rule_t *rule = NULL;
            for (size_t j = 0; j < g_flexoutput_plugin->mapping_rules.num; j++) {
                flexoutput_mapping_rule_t *current_rule = g_flexoutput_plugin->mapping_rules.array[j];
                if (current_rule && strcmp(current_rule->output_name, output->config.name) == 0) {
                    rule = current_rule;
                    break;
                }
            }

            if (rule) {
                // Update video sources
                char **included_sources = NULL;
                size_t source_count = 0;
                if (flexoutput_mapping_get_included_sources(rule, &included_sources, &source_count) == FLEXOUTPUT_SUCCESS) {
                    // Update video rendering context
                    if (output->video_context) {
                        flexoutput_video_clear_sources(output->video_context);
                        for (size_t k = 0; k < source_count; k++) {
                            obs_weak_source_t *weak_source = flexoutput_source_get_weak_ref(included_sources[k]);
                            if (weak_source) {
                                obs_source_t *source = obs_weak_source_get_source(weak_source);
                                if (source && obs_source_get_type(source) == OBS_SOURCE_TYPE_INPUT) {
                                    flexoutput_video_add_source(output->video_context, included_sources[k]);
                                }
                                if (source) {
                                    obs_source_release(source);
                                }
                                obs_weak_source_release(weak_source);
                            }
                        }
                    }

                    // Update audio mixing context
                    if (output->audio_context) {
                        flexoutput_audio_clear_sources(output->audio_context);
                        for (size_t k = 0; k < source_count; k++) {
                            obs_weak_source_t *weak_source = flexoutput_source_get_weak_ref(included_sources[k]);
                            if (weak_source) {
                                obs_source_t *source = obs_weak_source_get_source(weak_source);
                                if (source && obs_source_get_type(source) == OBS_SOURCE_TYPE_INPUT) {
                                    flexoutput_audio_add_source(output->audio_context, included_sources[k], 1.0f);
                                }
                                if (source) {
                                    obs_source_release(source);
                                }
                                obs_weak_source_release(weak_source);
                            }
                        }
                    }

                    flexoutput_mapping_free_string_array(included_sources, source_count);
                }
            }
        }
    }

    pthread_mutex_unlock(&g_flexoutput_plugin->mutex);
}

// Handle source creation
static void flexoutput_handle_source_create(const char *source_name)
{
    obs_log(LOG_DEBUG, "Source created: %s", source_name);
    flexoutput_update_source_mappings();
}

// Handle source destruction
static void flexoutput_handle_source_destroy(const char *source_name)
{
    obs_log(LOG_DEBUG, "Source destroyed: %s", source_name);
    
    // Remove source from all mapping rules
    pthread_mutex_lock(&g_flexoutput_plugin->mutex);
    for (size_t i = 0; i < g_flexoutput_plugin->mapping_rules.num; i++) {
        flexoutput_mapping_rule_t *rule = g_flexoutput_plugin->mapping_rules.array[i];
        if (rule && flexoutput_mapping_has_source(rule, source_name)) {
            flexoutput_mapping_remove_source(rule, source_name);
        }
    }
    pthread_mutex_unlock(&g_flexoutput_plugin->mutex);
    
    flexoutput_update_source_mappings();
}

// Handle source rename
static void flexoutput_handle_source_rename(const char *old_name, const char *new_name)
{
    obs_log(LOG_DEBUG, "Source renamed: %s -> %s", old_name, new_name);
    
    // Update source name in all mapping rules
    pthread_mutex_lock(&g_flexoutput_plugin->mutex);
    for (size_t i = 0; i < g_flexoutput_plugin->mapping_rules.num; i++) {
        flexoutput_mapping_rule_t *rule = g_flexoutput_plugin->mapping_rules.array[i];
        if (rule && flexoutput_mapping_has_source(rule, old_name)) {
            flexoutput_mapping_remove_source(rule, old_name);
            flexoutput_mapping_add_source(rule, new_name);
        }
    }
    pthread_mutex_unlock(&g_flexoutput_plugin->mutex);
    
    flexoutput_update_source_mappings();
}

// Public API functions for UI and external access
flexoutput_plugin_t *flexoutput_get_plugin_instance(void)
{
    return g_flexoutput_plugin;
}

flexoutput_error_t flexoutput_create_output(const char *name, const flexoutput_output_config_t *config)
{
    if (!g_flexoutput_plugin || !g_flexoutput_plugin->initialized || !name || !config) {
        return FLEXOUTPUT_ERROR_INVALID_PARAM;
    }

    pthread_mutex_lock(&g_flexoutput_plugin->mutex);

    // Check if output already exists
    for (size_t i = 0; i < g_flexoutput_plugin->outputs.num; i++) {
        flexoutput_output_instance_t *output = g_flexoutput_plugin->outputs.array[i];
        if (output && strcmp(output->config.name, name) == 0) {
            pthread_mutex_unlock(&g_flexoutput_plugin->mutex);
            return FLEXOUTPUT_ERROR_ALREADY_EXISTS;
        }
    }

    // Create new output instance
    flexoutput_output_instance_t *output = flexoutput_output_create(config);
    if (!output) {
        pthread_mutex_unlock(&g_flexoutput_plugin->mutex);
        return FLEXOUTPUT_ERROR_SYSTEM;
    }

    // Add to plugin outputs
    da_push_back(g_flexoutput_plugin->outputs, &output);

    pthread_mutex_unlock(&g_flexoutput_plugin->mutex);

    obs_log(LOG_INFO, "Created output: %s", name);
    return FLEXOUTPUT_SUCCESS;
}

flexoutput_error_t flexoutput_destroy_output(const char *name)
{
    if (!g_flexoutput_plugin || !g_flexoutput_plugin->initialized || !name) {
        return FLEXOUTPUT_ERROR_INVALID_PARAM;
    }

    pthread_mutex_lock(&g_flexoutput_plugin->mutex);

    // Find and remove output
    for (size_t i = 0; i < g_flexoutput_plugin->outputs.num; i++) {
        flexoutput_output_instance_t *output = g_flexoutput_plugin->outputs.array[i];
        if (output && strcmp(output->config.name, name) == 0) {
            flexoutput_output_destroy(output);
            da_erase(g_flexoutput_plugin->outputs, i);
            pthread_mutex_unlock(&g_flexoutput_plugin->mutex);
            obs_log(LOG_INFO, "Destroyed output: %s", name);
            return FLEXOUTPUT_SUCCESS;
        }
    }

    pthread_mutex_unlock(&g_flexoutput_plugin->mutex);
    return FLEXOUTPUT_ERROR_NOT_FOUND;
}

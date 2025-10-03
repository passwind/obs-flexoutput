#pragma once

#include "flexoutput-types.h"
#include <obs.h>
#include <obs-output.h>
#include <obs-encoder.h>

#ifdef __cplusplus
extern "C" {
#endif

// Output Management Module API
// Handles creation, management, and control of OBS outputs and encoders

/**
 * Initialize the output management system
 * @return Error code
 */
flexoutput_error_t flexoutput_output_init(void);

/**
 * Cleanup the output management system
 */
void flexoutput_output_cleanup(void);

/**
 * Create a new output instance
 * @param config Output configuration
 * @return New output instance or NULL on failure
 */
flexoutput_output_instance_t *flexoutput_output_create(const flexoutput_output_config_t *config);

/**
 * Destroy an output instance
 * @param instance Output instance to destroy
 */
void flexoutput_output_destroy(flexoutput_output_instance_t *instance);

/**
 * Start an output
 * @param instance Output instance
 * @return Error code
 */
flexoutput_error_t flexoutput_output_start(flexoutput_output_instance_t *instance);

/**
 * Stop an output
 * @param instance Output instance
 * @return Error code
 */
flexoutput_error_t flexoutput_output_stop(flexoutput_output_instance_t *instance);

/**
 * Force stop an output
 * @param instance Output instance
 * @return Error code
 */
flexoutput_error_t flexoutput_output_force_stop(flexoutput_output_instance_t *instance);

/**
 * Check if output is active
 * @param instance Output instance
 * @return True if active
 */
bool flexoutput_output_is_active(const flexoutput_output_instance_t *instance);

/**
 * Get output status
 * @param instance Output instance
 * @return Output status
 */
flexoutput_output_status_t flexoutput_output_get_status(const flexoutput_output_instance_t *instance);

/**
 * Update output configuration
 * @param instance Output instance
 * @param config New configuration
 * @return Error code
 */
flexoutput_error_t flexoutput_output_update_config(flexoutput_output_instance_t *instance,
                                                    const flexoutput_output_config_t *config);

/**
 * Get output statistics
 * @param instance Output instance
 * @param stats Output statistics structure to fill
 * @return Error code
 */
flexoutput_error_t flexoutput_output_get_stats(const flexoutput_output_instance_t *instance,
                                                flexoutput_output_stats_t *stats);

/**
 * Reset output statistics
 * @param instance Output instance
 * @return Error code
 */
flexoutput_error_t flexoutput_output_reset_stats(flexoutput_output_instance_t *instance);

// Encoder Management Functions

/**
 * Create video encoder for output
 * @param instance Output instance
 * @param encoder_id Encoder ID (e.g., "obs_x264", "ffmpeg_nvenc")
 * @param settings Encoder settings
 * @return Error code
 */
flexoutput_error_t flexoutput_output_create_video_encoder(flexoutput_output_instance_t *instance,
                                                          const char *encoder_id,
                                                          obs_data_t *settings);

/**
 * Create audio encoder for output
 * @param instance Output instance
 * @param encoder_id Encoder ID (e.g., "ffmpeg_aac")
 * @param settings Encoder settings
 * @return Error code
 */
flexoutput_error_t flexoutput_output_create_audio_encoder(flexoutput_output_instance_t *instance,
                                                          const char *encoder_id,
                                                          obs_data_t *settings);

/**
 * Update video encoder settings
 * @param instance Output instance
 * @param settings New encoder settings
 * @return Error code
 */
flexoutput_error_t flexoutput_output_update_video_encoder(flexoutput_output_instance_t *instance,
                                                          obs_data_t *settings);

/**
 * Update audio encoder settings
 * @param instance Output instance
 * @param settings New encoder settings
 * @return Error code
 */
flexoutput_error_t flexoutput_output_update_audio_encoder(flexoutput_output_instance_t *instance,
                                                          obs_data_t *settings);

/**
 * Get video encoder settings
 * @param instance Output instance
 * @return Encoder settings (caller must release)
 */
obs_data_t *flexoutput_output_get_video_encoder_settings(const flexoutput_output_instance_t *instance);

/**
 * Get audio encoder settings
 * @param instance Output instance
 * @return Encoder settings (caller must release)
 */
obs_data_t *flexoutput_output_get_audio_encoder_settings(const flexoutput_output_instance_t *instance);

// Output Service Management

/**
 * Create output service (for streaming)
 * @param instance Output instance
 * @param service_id Service ID (e.g., "rtmp_common")
 * @param settings Service settings
 * @return Error code
 */
flexoutput_error_t flexoutput_output_create_service(flexoutput_output_instance_t *instance,
                                                    const char *service_id,
                                                    obs_data_t *settings);

/**
 * Update output service settings
 * @param instance Output instance
 * @param settings New service settings
 * @return Error code
 */
flexoutput_error_t flexoutput_output_update_service(flexoutput_output_instance_t *instance,
                                                    obs_data_t *settings);

/**
 * Get output service settings
 * @param instance Output instance
 * @return Service settings (caller must release)
 */
obs_data_t *flexoutput_output_get_service_settings(const flexoutput_output_instance_t *instance);

// Output Event Callbacks

/**
 * Set output event callbacks
 * @param instance Output instance
 * @param callbacks Callback structure
 * @param data User data for callbacks
 * @return Error code
 */
flexoutput_error_t flexoutput_output_set_callbacks(flexoutput_output_instance_t *instance,
                                                    const flexoutput_output_callbacks_t *callbacks,
                                                    void *data);

// Output Management Utilities

/**
 * Get all available output types
 * @param types Array to store output type IDs
 * @param count Number of types
 * @return Error code
 */
flexoutput_error_t flexoutput_output_get_available_types(char ***types, size_t *count);

/**
 * Get all available video encoders
 * @param encoders Array to store encoder IDs
 * @param count Number of encoders
 * @return Error code
 */
flexoutput_error_t flexoutput_output_get_available_video_encoders(char ***encoders, size_t *count);

/**
 * Get all available audio encoders
 * @param encoders Array to store encoder IDs
 * @param count Number of encoders
 * @return Error code
 */
flexoutput_error_t flexoutput_output_get_available_audio_encoders(char ***encoders, size_t *count);

/**
 * Get all available services
 * @param services Array to store service IDs
 * @param count Number of services
 * @return Error code
 */
flexoutput_error_t flexoutput_output_get_available_services(char ***services, size_t *count);

/**
 * Free string array returned by get_available_* functions
 * @param array String array to free
 * @param count Number of strings
 */
void flexoutput_output_free_string_array(char **array, size_t count);

/**
 * Check if output type is supported
 * @param type_id Output type ID
 * @return True if supported
 */
bool flexoutput_output_is_type_supported(const char *type_id);

/**
 * Check if video encoder is supported
 * @param encoder_id Encoder ID
 * @return True if supported
 */
bool flexoutput_output_is_video_encoder_supported(const char *encoder_id);

/**
 * Check if audio encoder is supported
 * @param encoder_id Encoder ID
 * @return True if supported
 */
bool flexoutput_output_is_audio_encoder_supported(const char *encoder_id);

/**
 * Check if service is supported
 * @param service_id Service ID
 * @return True if supported
 */
bool flexoutput_output_is_service_supported(const char *service_id);

// Output Configuration Helpers

/**
 * Create default output configuration
 * @param name Output name
 * @param type Output type (FLEXOUTPUT_OUTPUT_RTMP or FLEXOUTPUT_OUTPUT_FILE)
 * @return New configuration (caller must free)
 */
flexoutput_output_config_t *flexoutput_output_create_default_config(const char *name,
                                                                     flexoutput_output_type_t type);

/**
 * Copy output configuration
 * @param src Source configuration
 * @return New configuration copy (caller must free)
 */
flexoutput_output_config_t *flexoutput_output_copy_config(const flexoutput_output_config_t *src);

/**
 * Free output configuration
 * @param config Configuration to free
 */
void flexoutput_output_free_config(flexoutput_output_config_t *config);

/**
 * Validate output configuration
 * @param config Configuration to validate
 * @return Error code
 */
flexoutput_error_t flexoutput_output_validate_config(const flexoutput_output_config_t *config);

// Output Format Utilities

/**
 * Get optimal video format for output
 * @param config Output configuration
 * @return Optimal video format
 */
enum video_format flexoutput_output_get_optimal_video_format(const flexoutput_output_config_t *config);

/**
 * Get optimal audio format for output
 * @param config Output configuration
 * @return Optimal audio format
 */
enum audio_format flexoutput_output_get_optimal_audio_format(const flexoutput_output_config_t *config);

/**
 * Check if video format is supported by output
 * @param config Output configuration
 * @param format Video format to check
 * @return True if supported
 */
bool flexoutput_output_is_video_format_supported(const flexoutput_output_config_t *config,
                                                  enum video_format format);

/**
 * Check if audio format is supported by output
 * @param config Output configuration
 * @param format Audio format to check
 * @return True if supported
 */
bool flexoutput_output_is_audio_format_supported(const flexoutput_output_config_t *config,
                                                  enum audio_format format);

// Performance and Monitoring

/**
 * Get output performance metrics
 * @param instance Output instance
 * @param metrics Performance metrics structure to fill
 * @return Error code
 */
flexoutput_error_t flexoutput_output_get_performance_metrics(const flexoutput_output_instance_t *instance,
                                                             flexoutput_performance_metrics_t *metrics);

/**
 * Reset performance metrics
 * @param instance Output instance
 * @return Error code
 */
flexoutput_error_t flexoutput_output_reset_performance_metrics(flexoutput_output_instance_t *instance);

/**
 * Set performance monitoring callback
 * @param instance Output instance
 * @param callback Performance callback function
 * @param data User data for callback
 * @return Error code
 */
flexoutput_error_t flexoutput_output_set_performance_callback(flexoutput_output_instance_t *instance,
                                                              void (*callback)(const flexoutput_performance_metrics_t *metrics, void *data),
                                                              void *data);

// Advanced Output Control

/**
 * Pause output (if supported)
 * @param instance Output instance
 * @return Error code
 */
flexoutput_error_t flexoutput_output_pause(flexoutput_output_instance_t *instance);

/**
 * Resume output (if supported)
 * @param instance Output instance
 * @return Error code
 */
flexoutput_error_t flexoutput_output_resume(flexoutput_output_instance_t *instance);

/**
 * Check if output is paused
 * @param instance Output instance
 * @return True if paused
 */
bool flexoutput_output_is_paused(const flexoutput_output_instance_t *instance);

/**
 * Set output delay (in milliseconds)
 * @param instance Output instance
 * @param delay_ms Delay in milliseconds
 * @return Error code
 */
flexoutput_error_t flexoutput_output_set_delay(flexoutput_output_instance_t *instance,
                                                uint32_t delay_ms);

/**
 * Get output delay
 * @param instance Output instance
 * @return Delay in milliseconds
 */
uint32_t flexoutput_output_get_delay(const flexoutput_output_instance_t *instance);

// Output State Management

/**
 * Save output state to data
 * @param instance Output instance
 * @return State data (caller must release)
 */
obs_data_t *flexoutput_output_save_state(const flexoutput_output_instance_t *instance);

/**
 * Load output state from data
 * @param instance Output instance
 * @param data State data
 * @return Error code
 */
flexoutput_error_t flexoutput_output_load_state(flexoutput_output_instance_t *instance,
                                                 obs_data_t *data);

/**
 * Get output name
 * @param instance Output instance
 * @return Output name (do not free)
 */
const char *flexoutput_output_get_name(const flexoutput_output_instance_t *instance);

/**
 * Get output type
 * @param instance Output instance
 * @return Output type
 */
flexoutput_output_type_t flexoutput_output_get_type(const flexoutput_output_instance_t *instance);

/**
 * Get output configuration
 * @param instance Output instance
 * @return Output configuration (do not free)
 */
const flexoutput_output_config_t *flexoutput_output_get_config(const flexoutput_output_instance_t *instance);

// High-level API functions

/**
 * Create a new output with the given configuration
 * @param name Output name
 * @param config Output configuration
 * @return Error code
 */
flexoutput_error_t flexoutput_create_output(const char *name, const flexoutput_output_config_t *config);

/**
 * Destroy an output by name
 * @param name Output name
 * @return Error code
 */
flexoutput_error_t flexoutput_destroy_output(const char *name);

#ifdef __cplusplus
}
#endif

#include "flexoutput-output.h"
#include "flexoutput-video.h"
#include "flexoutput-audio.h"
#include <util/threading.h>
#include <util/platform.h>
#include <util/dstr.h>

// Global output system state
static bool output_system_initialized = false;
static DARRAY(flexoutput_output_instance_t*) output_instances;
static pthread_mutex_t instances_mutex;

// Forward declarations
static void output_signal_handler(void *data, calldata_t *cd);
static void encoder_signal_handler(void *data, calldata_t *cd);
static void cleanup_instance_encoders(flexoutput_output_instance_t *instance);
static void cleanup_instance_output(flexoutput_output_instance_t *instance);
static flexoutput_error_t setup_output_encoders(flexoutput_output_instance_t *instance);
static flexoutput_error_t setup_output_service(flexoutput_output_instance_t *instance);
static void assign_config_embed(flexoutput_output_config_t *dst, const flexoutput_output_config_t *src);
static void free_config_fields(flexoutput_output_config_t *config);

flexoutput_error_t flexoutput_output_init(void)
{
    if (output_system_initialized) {
        return FLEXOUTPUT_SUCCESS;
    }

    da_init(output_instances);
    
    if (pthread_mutex_init(&instances_mutex, NULL) != 0) {
        da_free(output_instances);
        return FLEXOUTPUT_ERROR_SYSTEM;
    }

    output_system_initialized = true;
    
    FLEXOUTPUT_LOG_INFO("Output management system initialized");
    return FLEXOUTPUT_SUCCESS;
}

void flexoutput_output_cleanup(void)
{
    if (!output_system_initialized) {
        return;
    }

    pthread_mutex_lock(&instances_mutex);
    
    // Cleanup all instances
    for (size_t i = 0; i < output_instances.num; i++) {
        flexoutput_output_destroy(output_instances.array[i]);
    }
    da_free(output_instances);
    
    pthread_mutex_unlock(&instances_mutex);
    pthread_mutex_destroy(&instances_mutex);

    output_system_initialized = false;
    
    FLEXOUTPUT_LOG_INFO("Output management system cleaned up");
}

flexoutput_output_instance_t *flexoutput_output_create(const flexoutput_output_config_t *config)
{
    if (!config || !config->name) {
        return NULL;
    }

    flexoutput_output_instance_t *instance = bzalloc(sizeof(flexoutput_output_instance_t));
    assign_config_embed(&instance->config, config);
    instance->video_context = NULL;
    instance->audio_context = NULL;
    instance->status = FLEXOUTPUT_OUTPUT_STOPPED;

    if (pthread_mutex_init(&instance->mutex, NULL) != 0) {
        free_config_fields(&instance->config);
        bfree(instance);
        return NULL;
    }

    // Initialize statistics
    instance->stats.start_time = 0;
    instance->stats.total_frames = 0;
    instance->stats.dropped_frames = 0;
    instance->stats.total_bytes = 0;
    instance->stats.bitrate = 0.0f;
    instance->stats.fps = 0.0f;

    // Create output based on type
    const char *output_id = NULL;
    switch (config->type) {
    case FLEXOUTPUT_OUTPUT_RTMP:
        output_id = "rtmp_output";
        break;
    case FLEXOUTPUT_OUTPUT_FILE:
        output_id = "ffmpeg_muxer";
        break;
    case FLEXOUTPUT_OUTPUT_UDP:
    case FLEXOUTPUT_OUTPUT_SRT:
    case FLEXOUTPUT_OUTPUT_WEBRTC:
    case FLEXOUTPUT_OUTPUT_CUSTOM:
        FLEXOUTPUT_LOG_ERROR("Output type %d not yet implemented", config->type);
        goto error;
    default:
        FLEXOUTPUT_LOG_ERROR("Unknown output type: %d", config->type);
        goto error;
    }

    instance->output = obs_output_create(output_id, config->name, NULL, NULL);
    if (!instance->output) {
        FLEXOUTPUT_LOG_ERROR("Failed to create output: %s", output_id);
        goto error;
    }

    // Setup encoders
    if (setup_output_encoders(instance) != FLEXOUTPUT_SUCCESS) {
        FLEXOUTPUT_LOG_ERROR("Failed to setup encoders for output: %s", config->name);
        goto error;
    }

    // Setup service for streaming
    if (config->type == FLEXOUTPUT_OUTPUT_RTMP) {
        if (setup_output_service(instance) != FLEXOUTPUT_SUCCESS) {
            FLEXOUTPUT_LOG_ERROR("Failed to setup service for output: %s", config->name);
            goto error;
        }
    }

    // Connect signals
    signal_handler_t *signal = obs_output_get_signal_handler(instance->output);
    signal_handler_connect(signal, "start", output_signal_handler, instance);
    signal_handler_connect(signal, "stop", output_signal_handler, instance);
    signal_handler_connect(signal, "pause", output_signal_handler, instance);
    signal_handler_connect(signal, "unpause", output_signal_handler, instance);

    // Add to global instances list
    pthread_mutex_lock(&instances_mutex);
    da_push_back(output_instances, &instance);
    pthread_mutex_unlock(&instances_mutex);

    FLEXOUTPUT_LOG_INFO("Created output instance: %s (%s)",
                        config->name,
                        config->type == FLEXOUTPUT_OUTPUT_RTMP ? "Stream" : "Record");
    return instance;

error:
    if (instance->output) {
        obs_output_release(instance->output);
    }
    cleanup_instance_encoders(instance);
    free_config_fields(&instance->config);
    pthread_mutex_destroy(&instance->mutex);
    bfree(instance);
    return NULL;
}

void flexoutput_output_destroy(flexoutput_output_instance_t *instance)
{
    if (!instance) {
        return;
    }

    // Stop output first
    flexoutput_output_force_stop(instance);

    // Remove from global instances list
    pthread_mutex_lock(&instances_mutex);
    for (size_t i = 0; i < output_instances.num; i++) {
        if (output_instances.array[i] == instance) {
            da_erase(output_instances, i);
            break;
        }
    }
    pthread_mutex_unlock(&instances_mutex);

    pthread_mutex_lock(&instance->mutex);

    // Cleanup output and encoders
    cleanup_instance_output(instance);
    cleanup_instance_encoders(instance);

    // Free configuration fields for embedded config
    free_config_fields(&instance->config);
    
    pthread_mutex_unlock(&instance->mutex);
    pthread_mutex_destroy(&instance->mutex);
    
    bfree(instance);
}

flexoutput_error_t flexoutput_output_start(flexoutput_output_instance_t *instance)
{
    if (!instance) {
        return FLEXOUTPUT_ERROR_INVALID_PARAM;
    }

    pthread_mutex_lock(&instance->mutex);

    if (instance->status == FLEXOUTPUT_OUTPUT_STARTING || 
        instance->status == FLEXOUTPUT_OUTPUT_ACTIVE) {
        pthread_mutex_unlock(&instance->mutex);
        return FLEXOUTPUT_ERROR_ALREADY_ACTIVE;
    }

    if (!instance->output || !instance->video_encoder || !instance->audio_encoder) {
        pthread_mutex_unlock(&instance->mutex);
        return FLEXOUTPUT_ERROR_NOT_CONFIGURED;
    }

    // Set encoders
    obs_output_set_video_encoder(instance->output, instance->video_encoder);
    obs_output_set_audio_encoder(instance->output, instance->audio_encoder, 0);

    // Set service for streaming
    if (instance->config.type == FLEXOUTPUT_OUTPUT_RTMP && instance->service) {
        obs_output_set_service(instance->output, instance->service);
    }

    // Start output
    bool success = obs_output_start(instance->output);
    if (!success) {
        pthread_mutex_unlock(&instance->mutex);
        FLEXOUTPUT_LOG_ERROR("Failed to start output: %s", instance->config.name);
        return FLEXOUTPUT_ERROR_START_FAILED;
    }

    instance->status = FLEXOUTPUT_OUTPUT_STARTING;
    instance->stats.start_time = os_gettime_ns();
    
    pthread_mutex_unlock(&instance->mutex);

    FLEXOUTPUT_LOG_INFO("Started output: %s", instance->config.name);
    return FLEXOUTPUT_SUCCESS;
}

flexoutput_error_t flexoutput_output_stop(flexoutput_output_instance_t *instance)
{
    if (!instance) {
        return FLEXOUTPUT_ERROR_INVALID_PARAM;
    }

    pthread_mutex_lock(&instance->mutex);

    if (instance->status == FLEXOUTPUT_OUTPUT_STOPPED || 
        instance->status == FLEXOUTPUT_OUTPUT_STOPPING) {
        pthread_mutex_unlock(&instance->mutex);
        return FLEXOUTPUT_ERROR_NOT_ACTIVE;
    }

    if (instance->output) {
        obs_output_stop(instance->output);
        instance->status = FLEXOUTPUT_OUTPUT_STOPPING;
    }

    pthread_mutex_unlock(&instance->mutex);

    FLEXOUTPUT_LOG_INFO("Stopping output: %s", instance->config.name);
    return FLEXOUTPUT_SUCCESS;
}

flexoutput_error_t flexoutput_output_force_stop(flexoutput_output_instance_t *instance)
{
    if (!instance) {
        return FLEXOUTPUT_ERROR_INVALID_PARAM;
    }

    pthread_mutex_lock(&instance->mutex);

    if (instance->output) {
        obs_output_force_stop(instance->output);
        instance->status = FLEXOUTPUT_OUTPUT_STOPPED;
    }

    pthread_mutex_unlock(&instance->mutex);

    FLEXOUTPUT_LOG_INFO("Force stopped output: %s", instance->config.name);
    return FLEXOUTPUT_SUCCESS;
}

bool flexoutput_output_is_active(const flexoutput_output_instance_t *instance)
{
    if (!instance) {
        return false;
    }

    return instance->status == FLEXOUTPUT_OUTPUT_ACTIVE ||
           instance->status == FLEXOUTPUT_OUTPUT_STARTING;
}

flexoutput_output_status_t flexoutput_output_get_status(const flexoutput_output_instance_t *instance)
{
    return instance ? instance->status : FLEXOUTPUT_OUTPUT_STOPPED;
}

flexoutput_error_t flexoutput_output_update_config(flexoutput_output_instance_t *instance,
                                                    const flexoutput_output_config_t *config)
{
    if (!instance || !config) {
        return FLEXOUTPUT_ERROR_INVALID_PARAM;
    }

    pthread_mutex_lock(&instance->mutex);

    // Stop output if active
    bool was_active = flexoutput_output_is_active(instance);
    if (was_active) {
        obs_output_force_stop(instance->output);
    }

    // Update embedded configuration
    free_config_fields(&instance->config);
    assign_config_embed(&instance->config, config);

    // Recreate encoders with new settings
    cleanup_instance_encoders(instance);
    flexoutput_error_t result = setup_output_encoders(instance);

    // Restart if was active
    if (was_active && result == FLEXOUTPUT_SUCCESS) {
        obs_output_start(instance->output);
    }

    pthread_mutex_unlock(&instance->mutex);

    FLEXOUTPUT_LOG_INFO("Updated configuration for output: %s", config->name);
    return result;
}

flexoutput_error_t flexoutput_output_get_stats(const flexoutput_output_instance_t *instance,
                                                flexoutput_output_stats_t *stats)
{
    if (!instance || !stats) {
        return FLEXOUTPUT_ERROR_INVALID_PARAM;
    }

    pthread_mutex_lock((pthread_mutex_t*)&instance->mutex);
    *stats = instance->stats;
    
    // Update real-time stats if output is active
    if (instance->output && flexoutput_output_is_active(instance)) {
        stats->total_frames = obs_output_get_total_frames(instance->output);
        stats->dropped_frames = obs_output_get_frames_dropped(instance->output);
        stats->total_bytes = obs_output_get_total_bytes(instance->output);
        
        // Calculate FPS and bitrate
        uint64_t current_time = os_gettime_ns();
        if (stats->start_time > 0) {
            double elapsed_seconds = (current_time - stats->start_time) / 1000000000.0;
            if (elapsed_seconds > 0) {
                stats->fps = stats->total_frames / elapsed_seconds;
                stats->bitrate = (stats->total_bytes * 8.0) / elapsed_seconds / 1000.0; // kbps
            }
        }
    }
    
    pthread_mutex_unlock((pthread_mutex_t*)&instance->mutex);
    return FLEXOUTPUT_SUCCESS;
}

// Embedded-config helpers
static void assign_config_embed(flexoutput_output_config_t *dst, const flexoutput_output_config_t *src)
{
    if (!dst || !src) {
        return;
    }

    // Free existing to avoid leaks
    free_config_fields(dst);

    dst->name = src->name ? bstrdup(src->name) : NULL;
    dst->type = src->type;
    dst->enabled = src->enabled;

    dst->video.bitrate = src->video.bitrate;
    dst->video.width = src->video.width;
    dst->video.height = src->video.height;
    dst->video.fps = src->video.fps;
    dst->video.encoder = src->video.encoder ? bstrdup(src->video.encoder) : NULL;
    dst->video.preset = src->video.preset ? bstrdup(src->video.preset) : NULL;
    dst->video.profile = src->video.profile ? bstrdup(src->video.profile) : NULL;

    dst->audio.bitrate = src->audio.bitrate;
    dst->audio.sample_rate = src->audio.sample_rate;
    dst->audio.channels = src->audio.channels;
    dst->audio.encoder = src->audio.encoder ? bstrdup(src->audio.encoder) : NULL;

    dst->url = src->url ? bstrdup(src->url) : NULL;
    dst->key = src->key ? bstrdup(src->key) : NULL;
    dst->file_path = src->file_path ? bstrdup(src->file_path) : NULL;
    dst->format = src->format ? bstrdup(src->format) : NULL;
}

static void free_config_fields(flexoutput_output_config_t *config)
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
}

flexoutput_error_t flexoutput_output_reset_stats(flexoutput_output_instance_t *instance)
{
    if (!instance) {
        return FLEXOUTPUT_ERROR_INVALID_PARAM;
    }

    pthread_mutex_lock(&instance->mutex);
    
    instance->stats.start_time = os_gettime_ns();
    instance->stats.total_frames = 0;
    instance->stats.dropped_frames = 0;
    instance->stats.total_bytes = 0;
    instance->stats.bitrate = 0.0f;
    instance->stats.fps = 0.0f;
    
    pthread_mutex_unlock(&instance->mutex);
    return FLEXOUTPUT_SUCCESS;
}

flexoutput_error_t flexoutput_output_create_video_encoder(flexoutput_output_instance_t *instance,
                                                          const char *encoder_id,
                                                          obs_data_t *settings)
{
    if (!instance || !encoder_id) {
        return FLEXOUTPUT_ERROR_INVALID_PARAM;
    }

    pthread_mutex_lock(&instance->mutex);

    // Release existing encoder
    if (instance->video_encoder) {
        obs_encoder_release(instance->video_encoder);
        instance->video_encoder = NULL;
    }

    // Create new encoder
    instance->video_encoder = obs_video_encoder_create(encoder_id, 
                                                       instance->config.name, 
                                                       settings, NULL);
    
    if (!instance->video_encoder) {
        pthread_mutex_unlock(&instance->mutex);
        FLEXOUTPUT_LOG_ERROR("Failed to create video encoder: %s", encoder_id);
        return FLEXOUTPUT_ERROR_ENCODER_FAILED;
    }

    // Note: Encoder signal handlers not available in current OBS version
    // TODO: Implement encoder monitoring through alternative means

    pthread_mutex_unlock(&instance->mutex);

    FLEXOUTPUT_LOG_INFO("Created video encoder for output %s: %s", 
                        instance->config.name, encoder_id);
    return FLEXOUTPUT_SUCCESS;
}

flexoutput_error_t flexoutput_output_create_audio_encoder(flexoutput_output_instance_t *instance,
                                                          const char *encoder_id,
                                                          obs_data_t *settings)
{
    if (!instance || !encoder_id) {
        return FLEXOUTPUT_ERROR_INVALID_PARAM;
    }

    pthread_mutex_lock(&instance->mutex);

    // Release existing encoder
    if (instance->audio_encoder) {
        obs_encoder_release(instance->audio_encoder);
        instance->audio_encoder = NULL;
    }

    // Create new encoder
    instance->audio_encoder = obs_audio_encoder_create(encoder_id, 
                                                       instance->config.name, 
                                                       settings, 0, NULL);
    
    if (!instance->audio_encoder) {
        pthread_mutex_unlock(&instance->mutex);
        FLEXOUTPUT_LOG_ERROR("Failed to create audio encoder: %s", encoder_id);
        return FLEXOUTPUT_ERROR_ENCODER_FAILED;
    }

    // Note: Encoder signal handlers not available in current OBS version
    // TODO: Implement encoder monitoring through alternative means

    pthread_mutex_unlock(&instance->mutex);

    FLEXOUTPUT_LOG_INFO("Created audio encoder for output %s: %s", 
                        instance->config.name, encoder_id);
    return FLEXOUTPUT_SUCCESS;
}

flexoutput_error_t flexoutput_output_create_service(flexoutput_output_instance_t *instance,
                                                    const char *service_id,
                                                    obs_data_t *settings)
{
    if (!instance || !service_id) {
        return FLEXOUTPUT_ERROR_INVALID_PARAM;
    }

    pthread_mutex_lock(&instance->mutex);

    // Release existing service
    if (instance->service) {
        obs_service_release(instance->service);
        instance->service = NULL;
    }

    // Create new service
    instance->service = obs_service_create(service_id, instance->config.name, settings, NULL);
    
    if (!instance->service) {
        pthread_mutex_unlock(&instance->mutex);
        FLEXOUTPUT_LOG_ERROR("Failed to create service: %s", service_id);
        return FLEXOUTPUT_ERROR_SERVICE_FAILED;
    }

    pthread_mutex_unlock(&instance->mutex);

    FLEXOUTPUT_LOG_INFO("Created service for output %s: %s", 
                        instance->config.name, service_id);
    return FLEXOUTPUT_SUCCESS;
}

// Configuration helpers
flexoutput_output_config_t *flexoutput_output_create_default_config(const char *name,
                                                                     flexoutput_output_type_t type)
{
    if (!name) {
        return NULL;
    }

    flexoutput_output_config_t *config = bzalloc(sizeof(flexoutput_output_config_t));
    config->name = bstrdup(name);
    config->type = type;
    config->enabled = true;
    
    // Set default video settings
    config->video.bitrate = 2500;
    config->video.width = 1920;
    config->video.height = 1080;
    config->video.fps = 30;
    config->video.encoder = bstrdup("obs_x264");
    config->video.preset = bstrdup("veryfast");
    config->video.profile = bstrdup("main");
    
    // Set default audio settings
    config->audio.bitrate = 160;
    config->audio.sample_rate = 44100;
    config->audio.channels = 2;
    config->audio.encoder = bstrdup("ffmpeg_aac");
    
    // Set default output settings based on type
    if (type == FLEXOUTPUT_OUTPUT_RTMP) {
        config->url = bstrdup("rtmp://live.twitch.tv/live/");
        config->key = bstrdup("");
    } else {
        config->file_path = bstrdup("recording.mp4");
        config->format = bstrdup("mp4");
    }

    return config;
}

flexoutput_output_config_t *flexoutput_output_copy_config(const flexoutput_output_config_t *src)
{
    if (!src) {
        return NULL;
    }

    flexoutput_output_config_t *dst = bzalloc(sizeof(flexoutput_output_config_t));
    
    dst->name = src->name ? bstrdup(src->name) : NULL;
    dst->type = src->type;
    dst->enabled = src->enabled;
    
    dst->video.bitrate = src->video.bitrate;
    dst->video.width = src->video.width;
    dst->video.height = src->video.height;
    dst->video.fps = src->video.fps;
    dst->video.encoder = src->video.encoder ? bstrdup(src->video.encoder) : NULL;
    dst->video.preset = src->video.preset ? bstrdup(src->video.preset) : NULL;
    dst->video.profile = src->video.profile ? bstrdup(src->video.profile) : NULL;
    
    dst->audio.bitrate = src->audio.bitrate;
    dst->audio.sample_rate = src->audio.sample_rate;
    dst->audio.channels = src->audio.channels;
    dst->audio.encoder = src->audio.encoder ? bstrdup(src->audio.encoder) : NULL;
    
    dst->url = src->url ? bstrdup(src->url) : NULL;
    dst->key = src->key ? bstrdup(src->key) : NULL;
    dst->file_path = src->file_path ? bstrdup(src->file_path) : NULL;
    dst->format = src->format ? bstrdup(src->format) : NULL;

    return dst;
}

void flexoutput_output_free_config(flexoutput_output_config_t *config)
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
    
    bfree(config);
}

flexoutput_error_t flexoutput_output_validate_config(const flexoutput_output_config_t *config)
{
    if (!config || !config->name) {
        return FLEXOUTPUT_ERROR_INVALID_PARAM;
    }

    // Validate video settings
    if (config->video.bitrate <= 0 || config->video.bitrate > 50000) {
        return FLEXOUTPUT_ERROR_INVALID_CONFIG;
    }
    
    if (config->video.width <= 0 || config->video.height <= 0) {
        return FLEXOUTPUT_ERROR_INVALID_CONFIG;
    }
    
    if (config->video.fps <= 0) {
        return FLEXOUTPUT_ERROR_INVALID_CONFIG;
    }

    // Validate audio settings
    if (config->audio.bitrate <= 0 || config->audio.bitrate > 320) {
        return FLEXOUTPUT_ERROR_INVALID_CONFIG;
    }
    
    if (config->audio.sample_rate != 44100 && config->audio.sample_rate != 48000) {
        return FLEXOUTPUT_ERROR_INVALID_CONFIG;
    }

    // Validate type-specific settings
    if (config->type == FLEXOUTPUT_OUTPUT_RTMP) {
        if (!config->url || strlen(config->url) == 0) {
            return FLEXOUTPUT_ERROR_INVALID_CONFIG;
        }
    } else if (config->type == FLEXOUTPUT_OUTPUT_FILE) {
        if (!config->file_path || strlen(config->file_path) == 0) {
            return FLEXOUTPUT_ERROR_INVALID_CONFIG;
        }
    }

    return FLEXOUTPUT_SUCCESS;
}

// Helper functions
static void output_signal_handler(void *data, calldata_t *cd)
{
    flexoutput_output_instance_t *instance = (flexoutput_output_instance_t*)data;
    const char *signal = calldata_string(cd, "signal");
    
    if (!instance || !signal) {
        return;
    }

    pthread_mutex_lock(&instance->mutex);
    
    if (strcmp(signal, "start") == 0) {
        instance->status = FLEXOUTPUT_OUTPUT_ACTIVE;
        FLEXOUTPUT_LOG_INFO("Output started: %s", instance->config.name);
    } else if (strcmp(signal, "stop") == 0) {
        instance->status = FLEXOUTPUT_OUTPUT_STOPPED;
        FLEXOUTPUT_LOG_INFO("Output stopped: %s", instance->config.name);
    } else if (strcmp(signal, "pause") == 0) {
        instance->status = FLEXOUTPUT_OUTPUT_PAUSED;
        FLEXOUTPUT_LOG_INFO("Output paused: %s", instance->config.name);
    } else if (strcmp(signal, "unpause") == 0) {
        instance->status = FLEXOUTPUT_OUTPUT_ACTIVE;
        FLEXOUTPUT_LOG_INFO("Output unpaused: %s", instance->config.name);
    }
    
    pthread_mutex_unlock(&instance->mutex);

    // Call user callback if set
    if (instance->callbacks.status_changed) {
        instance->callbacks.status_changed(instance, instance->status, instance->callback_data);
    }
}

static void encoder_signal_handler(void *data, calldata_t *cd)
{
    flexoutput_output_instance_t *instance = (flexoutput_output_instance_t*)data;
    const char *signal = calldata_string(cd, "signal");
    
    if (!instance || !signal) {
        return;
    }

    FLEXOUTPUT_LOG_DEBUG("Encoder signal for output %s: %s", instance->config.name, signal);
}

static void cleanup_instance_encoders(flexoutput_output_instance_t *instance)
{
    if (!instance) {
        return;
    }

    if (instance->video_encoder) {
        obs_encoder_release(instance->video_encoder);
        instance->video_encoder = NULL;
    }
    
    if (instance->audio_encoder) {
        obs_encoder_release(instance->audio_encoder);
        instance->audio_encoder = NULL;
    }
}

static void cleanup_instance_output(flexoutput_output_instance_t *instance)
{
    if (!instance) {
        return;
    }

    if (instance->output) {
        obs_output_release(instance->output);
        instance->output = NULL;
    }
    
    if (instance->service) {
        obs_service_release(instance->service);
        instance->service = NULL;
    }
}

static flexoutput_error_t setup_output_encoders(flexoutput_output_instance_t *instance)
{
    if (!instance) {
        return FLEXOUTPUT_ERROR_INVALID_PARAM;
    }

    // Create video encoder settings
    obs_data_t *video_settings = obs_data_create();
    obs_data_set_int(video_settings, "bitrate", instance->config.video.bitrate);
    obs_data_set_string(video_settings, "rate_control", "CBR");
    obs_data_set_string(video_settings, "preset", instance->config.video.preset ? instance->config.video.preset : "veryfast");
    obs_data_set_string(video_settings, "profile", instance->config.video.profile ? instance->config.video.profile : "main");
    obs_data_set_string(video_settings, "tune", "zerolatency");
    
    flexoutput_error_t result = flexoutput_output_create_video_encoder(instance,
                                                                       instance->config.video.encoder,
                                                                       video_settings);
    obs_data_release(video_settings);
    
    if (result != FLEXOUTPUT_SUCCESS) {
        return result;
    }

    // Create audio encoder settings
    obs_data_t *audio_settings = obs_data_create();
    obs_data_set_int(audio_settings, "bitrate", instance->config.audio.bitrate);
    
    result = flexoutput_output_create_audio_encoder(instance,
                                                    instance->config.audio.encoder,
                                                    audio_settings);
    obs_data_release(audio_settings);
    
    return result;
}

static flexoutput_error_t setup_output_service(flexoutput_output_instance_t *instance)
{
    if (!instance || instance->config.type != FLEXOUTPUT_OUTPUT_RTMP) {
        return FLEXOUTPUT_ERROR_INVALID_PARAM;
    }

    // Create service settings
    obs_data_t *service_settings = obs_data_create();
    obs_data_set_string(service_settings, "server", instance->config.url);
    obs_data_set_string(service_settings, "key", instance->config.key);
    
    flexoutput_error_t result = flexoutput_output_create_service(instance,
                                                                 "rtmp_common",
                                                                 service_settings);
    obs_data_release(service_settings);
    
    return result;
}

// Utility functions
bool flexoutput_output_is_type_supported(const char *type_id)
{
    if (!type_id) {
        return false;
    }

    return strcmp(type_id, "rtmp_output") == 0 ||
           strcmp(type_id, "ffmpeg_muxer") == 0;
}

bool flexoutput_output_is_video_encoder_supported(const char *encoder_id)
{
    if (!encoder_id) {
        return false;
    }

    return strcmp(encoder_id, "obs_x264") == 0 ||
           strcmp(encoder_id, "ffmpeg_nvenc") == 0 ||
           strcmp(encoder_id, "ffmpeg_vaapi") == 0;
}

bool flexoutput_output_is_audio_encoder_supported(const char *encoder_id)
{
    if (!encoder_id) {
        return false;
    }

    return strcmp(encoder_id, "ffmpeg_aac") == 0 ||
           strcmp(encoder_id, "ffmpeg_opus") == 0;
}

enum video_format flexoutput_output_get_optimal_video_format(const flexoutput_output_config_t *config)
{
    UNUSED_PARAMETER(config);
    return VIDEO_FORMAT_NV12; // Most compatible format
}

enum audio_format flexoutput_output_get_optimal_audio_format(const flexoutput_output_config_t *config)
{
    UNUSED_PARAMETER(config);
    return AUDIO_FORMAT_FLOAT; // Best quality
}

const char *flexoutput_output_get_name(const flexoutput_output_instance_t *instance)
{
    return instance ? instance->config.name : NULL;
}

flexoutput_output_type_t flexoutput_output_get_type(const flexoutput_output_instance_t *instance)
{
    return instance ? instance->config.type : FLEXOUTPUT_OUTPUT_RTMP;
}

const flexoutput_output_config_t *flexoutput_output_get_config(const flexoutput_output_instance_t *instance)
{
    return instance ? &instance->config : NULL;
}

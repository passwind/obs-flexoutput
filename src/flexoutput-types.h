#pragma once

#include <obs.h>
#include <obs-module.h>
#include <util/threading.h>
#include <util/darray.h>
#include <media-io/video-io.h>
#include <media-io/audio-io.h>
#include <callback/signal.h>

#ifdef __cplusplus
extern "C" {
#endif

// Plugin version and constants
#define FLEXOUTPUT_VERSION "1.0.0"
#define FLEXOUTPUT_MAX_OUTPUTS 16
#define FLEXOUTPUT_MAX_SOURCES 64
#define FLEXOUTPUT_CONFIG_FILE "flexoutput.json"
#define MAX_AV_PLANES 8

// Forward declarations
struct flexoutput_plugin;
struct flexoutput_source_info;
struct flexoutput_output_instance;
typedef struct flexoutput_output_instance flexoutput_output_instance_t;
struct video_render_context;
typedef struct video_render_context video_render_context_t;
struct audio_mix_context;
typedef struct audio_mix_context audio_mix_context_t;
struct flexoutput_mapping_rule;

/**
 * Output types supported by FlexOutput
 */
typedef enum {
    FLEXOUTPUT_OUTPUT_RTMP = 0,
    FLEXOUTPUT_OUTPUT_FILE = 1,
    FLEXOUTPUT_OUTPUT_UDP = 2,
    FLEXOUTPUT_OUTPUT_SRT = 3,
    FLEXOUTPUT_OUTPUT_WEBRTC = 4,
    FLEXOUTPUT_OUTPUT_CUSTOM = 5
} flexoutput_output_type_t;

/**
 * Video configuration structure
 */
typedef struct {
    uint32_t width;
    uint32_t height;
    uint32_t fps;
    uint32_t bitrate;
    char *encoder;
    char *preset;
    char *profile;
} flexoutput_video_config_t;

/**
 * Audio configuration structure
 */
typedef struct {
    uint32_t sample_rate;
    uint32_t bitrate;
    uint32_t channels;
    char *encoder;
} flexoutput_audio_config_t;

// Source information structure
typedef struct flexoutput_source_info {
    char *name;
    char *type;
    char *display_name;
    obs_source_t *source;
    uint32_t capabilities;
    bool has_video;
    bool has_audio;
    bool is_scene;
    bool is_group;
    uint32_t width;
    uint32_t height;
    bool active;
    bool showing;
    bool enabled;
    bool muted;
    float volume;
    bool monitored;
} flexoutput_source_info_t;

// Output status
typedef enum {
    FLEXOUTPUT_OUTPUT_STOPPED = 0,
    FLEXOUTPUT_OUTPUT_STARTING = 1,
    FLEXOUTPUT_OUTPUT_ACTIVE = 2,
    FLEXOUTPUT_OUTPUT_STOPPING = 3,
    FLEXOUTPUT_OUTPUT_PAUSED = 4
} flexoutput_output_status_t;

// Output statistics
typedef struct {
    uint64_t start_time;
    uint64_t total_frames;
    uint64_t dropped_frames;
    uint64_t total_bytes;
    float bitrate;
    float fps;
} flexoutput_output_stats_t;

// Performance metrics
typedef struct flexoutput_performance_metrics {
    double cpu_usage;
    double gpu_usage;
    double render_ms;
    double encode_ms;
} flexoutput_performance_metrics_t;

// Output configuration
typedef struct {
    char *name;
    flexoutput_output_type_t type;
    bool enabled;
    
    // Video and Audio configurations
    flexoutput_video_config_t video;
    flexoutput_audio_config_t audio;
    
    // Streaming
    char *server;
    char *url;
    char *key;
    // Recording
    char *file_path;
    char *format;
    
    // Advanced settings
    uint32_t keyframe_interval;
    bool use_cbr;
    char *custom_settings;
    
    // Settings
    obs_data_t *encoder_settings;
    obs_data_t *service_settings;
} flexoutput_output_config_t;

// Output callbacks
typedef struct {
    void (*status_changed)(flexoutput_output_instance_t *instance,
                           flexoutput_output_status_t status,
                           void *data);
    void (*performance_updated)(const flexoutput_performance_metrics_t *metrics,
                                void *data);
} flexoutput_output_callbacks_t;

// Output instance
struct flexoutput_output_instance {
    // Configuration
    flexoutput_output_config_t config;
    
    // Contexts
    video_render_context_t *video_context;
    audio_mix_context_t *audio_context;
    
    // OBS objects
    obs_output_t *output;
    obs_service_t *service;
    obs_encoder_t *video_encoder;
    obs_encoder_t *audio_encoder;
    
    // State
    pthread_mutex_t mutex;
    flexoutput_output_status_t status;
    flexoutput_output_stats_t stats;
    
    // Callbacks
    flexoutput_output_callbacks_t callbacks;
    void *callback_data;
};

// Main plugin state
typedef struct flexoutput_plugin {
    bool initialized;
    bool enabled;
    bool running;
    
    // Configuration
    obs_data_t *config_data;
    char *config_file_path;
    
    // Source management
    DARRAY(flexoutput_source_info_t*) available_sources;
    
    // Output management
    DARRAY(flexoutput_output_instance_t*) outputs;
    DARRAY(struct flexoutput_mapping_rule*) mapping_rules;
    pthread_mutex_t mutex;
    
    // Rendering resources
    gs_effect_t *default_effect;
    
    // Thread synchronization
    pthread_mutex_t config_mutex;
    pthread_mutex_t render_mutex;
    pthread_mutex_t audio_mutex;
    
//    // Callbacks
//    void *signals;  // signal_handler_t pointer stored as void*
//    
    // UI
    void *ui_data;
    
} flexoutput_plugin_t;

// Audio frame structure for mixing
typedef struct {
    uint8_t *data[MAX_AV_PLANES];
    uint32_t frames;
    uint64_t timestamp;
    enum speaker_layout speakers;
    enum audio_format format;
    uint32_t samples_per_sec;
} flexoutput_audio_frame_t;

// Video frame structure for rendering
typedef struct {
    gs_texture_t *texture;
    uint32_t width;
    uint32_t height;
    uint64_t timestamp;
    enum video_format format;
    size_t size;        // Total frame data size in bytes
    uint32_t linesize;  // Bytes per line
    uint8_t *data;      // Raw frame data
} flexoutput_video_frame_t;

// Error codes
typedef enum {
    FLEXOUTPUT_SUCCESS = 0,
    FLEXOUTPUT_ERROR_INVALID_PARAM,
    FLEXOUTPUT_ERROR_MEMORY,
    FLEXOUTPUT_ERROR_NOT_FOUND,
    FLEXOUTPUT_ERROR_ALREADY_EXISTS,
    FLEXOUTPUT_ERROR_INIT_FAILED,
    FLEXOUTPUT_ERROR_NOT_INITIALIZED,
    FLEXOUTPUT_ERROR_OPERATION_FAILED,
    FLEXOUTPUT_ERROR_SYSTEM,
    FLEXOUTPUT_ERROR_NOT_CONFIGURED,
    FLEXOUTPUT_ERROR_ALREADY_ACTIVE,
    FLEXOUTPUT_ERROR_START_FAILED,
    FLEXOUTPUT_ERROR_ENCODER_FAILED,
    FLEXOUTPUT_ERROR_SERVICE_FAILED,
    FLEXOUTPUT_ERROR_RENDER_FAILED,
    FLEXOUTPUT_ERROR_NOT_ACTIVE,
    FLEXOUTPUT_ERROR_NO_SOURCES,
    FLEXOUTPUT_ERROR_INVALID_CONFIG,
    FLEXOUTPUT_ERROR_INVALID_SOURCE,
    FLEXOUTPUT_ERROR_CONFIG_FAILED,
    FLEXOUTPUT_ERROR_UNSUPPORTED_FORMAT,
    FLEXOUTPUT_ERROR_RESAMPLE_FAILED
} flexoutput_error_t;

// Mapping types
typedef enum {
    FLEXOUTPUT_MAPPING_WHITELIST = 0,
    FLEXOUTPUT_MAPPING_BLACKLIST
} flexoutput_mapping_type_t;

// Mapping merge types
typedef enum {
    FLEXOUTPUT_MAPPING_MERGE_UNION = 0,
    FLEXOUTPUT_MAPPING_MERGE_INTERSECTION,
    FLEXOUTPUT_MAPPING_MERGE_DIFFERENCE
} flexoutput_mapping_merge_type_t;

// Mapping statistics
typedef struct {
    size_t sources_added;
    size_t sources_removed;
    uint64_t last_update_time;
    size_t rule_changes;
} flexoutput_mapping_stats_t;

// Mapping rule structure
typedef struct flexoutput_mapping_rule {
    char *output_name;
    flexoutput_mapping_type_t type;
    bool enabled;
    bool auto_manage;
    bool auto_add_new;
    bool auto_remove_missing;
    DARRAY(char*) source_names;
    DARRAY(char*) source_type_filters;
    DARRAY(bool) source_type_include;
    flexoutput_mapping_stats_t stats;
    void (*change_callback)(const struct flexoutput_mapping_rule *rule, void *data);
    void *callback_data;
    pthread_mutex_t mutex;
} flexoutput_mapping_rule_t;

// Mapping operation types
typedef enum {
    FLEXOUTPUT_MAPPING_OP_ADD = 0,
    FLEXOUTPUT_MAPPING_OP_REMOVE
} flexoutput_mapping_operation_t;

// Mapping import operation types
typedef enum {
    FLEXOUTPUT_MAPPING_IMPORT_REPLACE = 0,
    FLEXOUTPUT_MAPPING_IMPORT_MERGE,
    FLEXOUTPUT_MAPPING_IMPORT_APPEND
} flexoutput_mapping_import_operation_t;

// Global plugin instance
extern flexoutput_plugin_t *g_flexoutput_plugin;

// Plugin API functions
flexoutput_plugin_t *flexoutput_get_plugin_instance(void);

// Utility macros
#define FLEXOUTPUT_LOG(level, format, ...) \
    blog(level, "[FlexOutput] " format, ##__VA_ARGS__)

#define FLEXOUTPUT_LOG_INFO(format, ...) \
    FLEXOUTPUT_LOG(LOG_INFO, format, ##__VA_ARGS__)

#define FLEXOUTPUT_LOG_WARNING(format, ...) \
    FLEXOUTPUT_LOG(LOG_WARNING, format, ##__VA_ARGS__)

#define FLEXOUTPUT_LOG_ERROR(format, ...) \
    FLEXOUTPUT_LOG(LOG_ERROR, format, ##__VA_ARGS__)

#define FLEXOUTPUT_LOG_DEBUG(format, ...) \
    FLEXOUTPUT_LOG(LOG_DEBUG, format, ##__VA_ARGS__)

// Memory management helpers
#define FLEXOUTPUT_SAFE_FREE(ptr) \
    do { \
        if (ptr) { \
            bfree(ptr); \
            ptr = NULL; \
        } \
    } while (0)

#define FLEXOUTPUT_SAFE_RELEASE(ptr) \
    do { \
        if (ptr) { \
            obs_source_release(ptr); \
            ptr = NULL; \
        } \
    } while (0)

#ifdef __cplusplus
}
#endif

#include "flexoutput-audio.h"
#include "flexoutput-source.h"
#include <util/threading.h>
#include <util/platform.h>
#include <util/circlebuf.h>

#include "plugin-support.h"

// Global audio system state
static bool audio_system_initialized = false;
static DARRAY(audio_mix_context_t*) mix_contexts;
static pthread_mutex_t contexts_mutex;

// Forward declarations
static void cleanup_context_sources(audio_mix_context_t *context);
static flexoutput_error_t resample_audio_data(audio_mix_context_t *context,
                                               const struct audio_data *input,
                                               struct audio_data *output);

flexoutput_error_t flexoutput_audio_init(void)
{
    if (audio_system_initialized) {
        return FLEXOUTPUT_SUCCESS;
    }

    da_init(mix_contexts);
    
    if (pthread_mutex_init(&contexts_mutex, NULL) != 0) {
        da_free(mix_contexts);
        return FLEXOUTPUT_ERROR_SYSTEM;
    }

    audio_system_initialized = true;
    
    FLEXOUTPUT_LOG_INFO("Audio mixing system initialized");
    return FLEXOUTPUT_SUCCESS;
}

void flexoutput_audio_cleanup(void)
{
    if (!audio_system_initialized) {
        return;
    }

    pthread_mutex_lock(&contexts_mutex);
    
    // Cleanup all contexts
    for (size_t i = 0; i < mix_contexts.num; i++) {
        flexoutput_audio_destroy_context(mix_contexts.array[i]);
    }
    da_free(mix_contexts);
    
    pthread_mutex_unlock(&contexts_mutex);
    pthread_mutex_destroy(&contexts_mutex);

    audio_system_initialized = false;
    
    FLEXOUTPUT_LOG_INFO("Audio mixing system cleaned up");
}

audio_mix_context_t *flexoutput_audio_create_context(const char *output_name,
                                                     uint32_t sample_rate,
                                                     enum audio_format format,
                                                     enum speaker_layout speakers)
{
    if (!output_name || sample_rate == 0) {
        return NULL;
    }

    audio_mix_context_t *context = bzalloc(sizeof(audio_mix_context_t));
    context->output_name = bstrdup(output_name);
    context->sample_rate = sample_rate;
    context->format = format;
    context->speakers = speakers;
    
    // Initialize audio info
    context->audio_info.samples_per_sec = sample_rate;
    context->audio_info.format = format;
    context->audio_info.speakers = speakers;
    
    da_init(context->source_names);
    da_init(context->source_refs);
    da_init(context->source_volumes);
    da_init(context->source_muted);
    
    da_init(context->audio_buffer);
    
    if (pthread_mutex_init(&context->mutex, NULL) != 0) {
        FLEXOUTPUT_SAFE_FREE(context->output_name);
        da_free(context->source_names);
        da_free(context->source_refs);
        da_free(context->source_volumes);
        da_free(context->source_muted);
        da_free(context->audio_buffer);
        bfree(context);
        return NULL;
    }

    // Create resampler if needed
    struct resample_info src_info = {0};
    struct resample_info dst_info = {0};
    
    src_info.samples_per_sec = 44100; // Default source rate
    src_info.format = AUDIO_FORMAT_FLOAT_PLANAR;
    src_info.speakers = SPEAKERS_STEREO;
    
    dst_info.samples_per_sec = sample_rate;
    dst_info.format = format;
    dst_info.speakers = speakers;
    
    context->resampler = audio_resampler_create(&dst_info, &src_info);

    // Add to global contexts list
    pthread_mutex_lock(&contexts_mutex);
    da_push_back(mix_contexts, &context);
    pthread_mutex_unlock(&contexts_mutex);

    FLEXOUTPUT_LOG_INFO("Created audio mix context for output: %s (%dHz, %s, %s)", 
                        output_name, sample_rate,
                        flexoutput_audio_get_format_string(format),
                        flexoutput_audio_get_speaker_string(speakers));
    return context;
}

void flexoutput_audio_destroy_context(audio_mix_context_t *context)
{
    if (!context) {
        return;
    }

    // Stop mixing first
    flexoutput_audio_stop_mixing(context);

    // Remove from global contexts list
    pthread_mutex_lock(&contexts_mutex);
    for (size_t i = 0; i < mix_contexts.num; i++) {
        if (mix_contexts.array[i] == context) {
            da_erase(mix_contexts, i);
            break;
        }
    }
    pthread_mutex_unlock(&contexts_mutex);

    pthread_mutex_lock(&context->mutex);

    // Cleanup sources
    cleanup_context_sources(context);
    da_free(context->source_names);
    da_free(context->source_refs);
    da_free(context->source_volumes);
    da_free(context->source_muted);

    // Cleanup audio resources
    da_free(context->audio_buffer);
    
    if (context->resampler) {
        audio_resampler_destroy(context->resampler);
    }

    FLEXOUTPUT_SAFE_FREE(context->output_name);
    
    pthread_mutex_unlock(&context->mutex);
    pthread_mutex_destroy(&context->mutex);
    
    bfree(context);
}

flexoutput_error_t flexoutput_audio_add_source(audio_mix_context_t *context,
                                                const char *source_name,
                                                float volume)
{
    if (!context || !source_name || volume < 0.0f || volume > 1.0f) {
        return FLEXOUTPUT_ERROR_INVALID_PARAM;
    }

    pthread_mutex_lock(&context->mutex);

    // Check if source already exists
    for (size_t i = 0; i < context->source_names.num; i++) {
        if (strcmp(context->source_names.array[i], source_name) == 0) {
            pthread_mutex_unlock(&context->mutex);
            return FLEXOUTPUT_ERROR_ALREADY_EXISTS;
        }
    }

    // Get source reference
    obs_source_t *source = flexoutput_source_get_ref(source_name);
    if (!source) {
        pthread_mutex_unlock(&context->mutex);
        return FLEXOUTPUT_ERROR_NOT_FOUND;
    }

    // Check if source has audio
    if (!flexoutput_source_has_audio(source)) {
        obs_source_release(source);
        pthread_mutex_unlock(&context->mutex);
        return FLEXOUTPUT_ERROR_INVALID_SOURCE;
    }

    // Add to arrays
    char *name_copy = bstrdup(source_name);
    bool muted = false;
    
    da_push_back(context->source_names, &name_copy);
    da_push_back(context->source_refs, &source);
    da_push_back(context->source_volumes, &volume);
    da_push_back(context->source_muted, &muted);

    pthread_mutex_unlock(&context->mutex);

    FLEXOUTPUT_LOG_DEBUG("Added audio source '%s' to mix context '%s' (volume: %.2f)", 
                         source_name, context->output_name, volume);
    return FLEXOUTPUT_SUCCESS;
}

flexoutput_error_t flexoutput_audio_remove_source(audio_mix_context_t *context,
                                                   const char *source_name)
{
    if (!context || !source_name) {
        return FLEXOUTPUT_ERROR_INVALID_PARAM;
    }

    pthread_mutex_lock(&context->mutex);

    for (size_t i = 0; i < context->source_names.num; i++) {
        if (strcmp(context->source_names.array[i], source_name) == 0) {
            // Release source reference
            obs_source_release(context->source_refs.array[i]);
            
            // Free name and remove from arrays
            FLEXOUTPUT_SAFE_FREE(context->source_names.array[i]);
            da_erase(context->source_names, i);
            da_erase(context->source_refs, i);
            da_erase(context->source_volumes, i);
            da_erase(context->source_muted, i);
            
            pthread_mutex_unlock(&context->mutex);
            
            FLEXOUTPUT_LOG_DEBUG("Removed audio source '%s' from mix context '%s'", 
                                 source_name, context->output_name);
            return FLEXOUTPUT_SUCCESS;
        }
    }

    pthread_mutex_unlock(&context->mutex);
    return FLEXOUTPUT_ERROR_NOT_FOUND;
}

flexoutput_error_t flexoutput_audio_clear_sources(audio_mix_context_t *context)
{
    if (!context) {
        return FLEXOUTPUT_ERROR_INVALID_PARAM;
    }

    pthread_mutex_lock(&context->mutex);
    cleanup_context_sources(context);
    pthread_mutex_unlock(&context->mutex);

    FLEXOUTPUT_LOG_DEBUG("Cleared all audio sources from mix context '%s'", context->output_name);
    return FLEXOUTPUT_SUCCESS;
}

flexoutput_error_t flexoutput_audio_update_sources(audio_mix_context_t *context,
                                                    const flexoutput_mapping_rule_t *mapping_rule)
{
    if (!context || !mapping_rule) {
        return FLEXOUTPUT_ERROR_INVALID_PARAM;
    }

    pthread_mutex_lock(&context->mutex);

    // Clear existing sources
    cleanup_context_sources(context);

    // Add sources based on mapping rule
    for (size_t i = 0; i < mapping_rule->source_names.num; i++) {
        const char *source_name = mapping_rule->source_names.array[i];
        obs_source_t *source = flexoutput_source_get_ref(source_name);
        
        if (source && flexoutput_source_has_audio(source)) {
            char *name_copy = bstrdup(source_name);
            float volume = 1.0f; // Default volume
            bool muted = false;
            
            da_push_back(context->source_names, &name_copy);
            da_push_back(context->source_refs, &source);
            da_push_back(context->source_volumes, &volume);
            da_push_back(context->source_muted, &muted);
        } else if (source) {
            obs_source_release(source);
        }
    }

    pthread_mutex_unlock(&context->mutex);

    FLEXOUTPUT_LOG_DEBUG("Updated audio sources for mix context '%s' based on mapping rule", 
                         context->output_name);
    return FLEXOUTPUT_SUCCESS;
}

flexoutput_error_t flexoutput_audio_set_source_volume(audio_mix_context_t *context,
                                                       const char *source_name,
                                                       float volume)
{
    if (!context || !source_name || volume < 0.0f || volume > 1.0f) {
        return FLEXOUTPUT_ERROR_INVALID_PARAM;
    }

    pthread_mutex_lock(&context->mutex);

    for (size_t i = 0; i < context->source_names.num; i++) {
        if (strcmp(context->source_names.array[i], source_name) == 0) {
            context->source_volumes.array[i] = volume;
            pthread_mutex_unlock(&context->mutex);
            return FLEXOUTPUT_SUCCESS;
        }
    }

    pthread_mutex_unlock(&context->mutex);
    return FLEXOUTPUT_ERROR_NOT_FOUND;
}

flexoutput_error_t flexoutput_audio_get_source_volume(audio_mix_context_t *context,
                                                       const char *source_name,
                                                       float *volume)
{
    if (!context || !source_name || !volume) {
        return FLEXOUTPUT_ERROR_INVALID_PARAM;
    }

    pthread_mutex_lock(&context->mutex);

    for (size_t i = 0; i < context->source_names.num; i++) {
        if (strcmp(context->source_names.array[i], source_name) == 0) {
            *volume = context->source_volumes.array[i];
            pthread_mutex_unlock(&context->mutex);
            return FLEXOUTPUT_SUCCESS;
        }
    }

    pthread_mutex_unlock(&context->mutex);
    return FLEXOUTPUT_ERROR_NOT_FOUND;
}

flexoutput_error_t flexoutput_audio_set_source_muted(audio_mix_context_t *context,
                                                      const char *source_name,
                                                      bool muted)
{
    if (!context || !source_name) {
        return FLEXOUTPUT_ERROR_INVALID_PARAM;
    }

    pthread_mutex_lock(&context->mutex);

    for (size_t i = 0; i < context->source_names.num; i++) {
        if (strcmp(context->source_names.array[i], source_name) == 0) {
            context->source_muted.array[i] = muted;
            pthread_mutex_unlock(&context->mutex);
            return FLEXOUTPUT_SUCCESS;
        }
    }

    pthread_mutex_unlock(&context->mutex);
    return FLEXOUTPUT_ERROR_NOT_FOUND;
}

flexoutput_error_t flexoutput_audio_get_source_muted(audio_mix_context_t *context,
                                                      const char *source_name,
                                                      bool *muted)
{
    if (!context || !source_name || !muted) {
        return FLEXOUTPUT_ERROR_INVALID_PARAM;
    }

    pthread_mutex_lock(&context->mutex);

    for (size_t i = 0; i < context->source_names.num; i++) {
        if (strcmp(context->source_names.array[i], source_name) == 0) {
            *muted = context->source_muted.array[i];
            pthread_mutex_unlock(&context->mutex);
            return FLEXOUTPUT_SUCCESS;
        }
    }

    pthread_mutex_unlock(&context->mutex);
    return FLEXOUTPUT_ERROR_NOT_FOUND;
}

flexoutput_error_t flexoutput_audio_start_mixing(audio_mix_context_t *context)
{
    if (!context) {
        return FLEXOUTPUT_ERROR_INVALID_PARAM;
    }

    pthread_mutex_lock(&context->mutex);
    
    if (context->active) {
        pthread_mutex_unlock(&context->mutex);
        return FLEXOUTPUT_ERROR_ALREADY_ACTIVE;
    }

    context->active = true;
    context->frames_mixed = 0;
    context->last_mix_time = os_gettime_ns();
    
    pthread_mutex_unlock(&context->mutex);

    FLEXOUTPUT_LOG_INFO("Started audio mixing for context '%s'", context->output_name);
    return FLEXOUTPUT_SUCCESS;
}

flexoutput_error_t flexoutput_audio_stop_mixing(audio_mix_context_t *context)
{
    if (!context) {
        return FLEXOUTPUT_ERROR_INVALID_PARAM;
    }

    pthread_mutex_lock(&context->mutex);
    context->active = false;
    pthread_mutex_unlock(&context->mutex);

    FLEXOUTPUT_LOG_INFO("Stopped audio mixing for context '%s'", context->output_name);
    return FLEXOUTPUT_SUCCESS;
}

bool flexoutput_audio_is_mixing(const audio_mix_context_t *context)
{
    return context ? context->active : false;
}

flexoutput_error_t flexoutput_audio_set_frame_callback(audio_mix_context_t *context,
                                                        void (*callback)(const flexoutput_audio_frame_t *frame, void *data),
                                                        void *data)
{
    if (!context) {
        return FLEXOUTPUT_ERROR_INVALID_PARAM;
    }

    pthread_mutex_lock(&context->mutex);
    context->audio_ready_callback = callback;
    context->callback_data = data;
    pthread_mutex_unlock(&context->mutex);

    return FLEXOUTPUT_SUCCESS;
}

flexoutput_error_t flexoutput_audio_mix_frame(audio_mix_context_t *context,
                                               flexoutput_audio_frame_t *frame)
{
    if (!context || !frame) {
        return FLEXOUTPUT_ERROR_INVALID_PARAM;
    }

    if (!context->active) {
        return FLEXOUTPUT_ERROR_NOT_ACTIVE;
    }

    pthread_mutex_lock(&context->mutex);

    if (context->source_refs.num == 0) {
        pthread_mutex_unlock(&context->mutex);
        return FLEXOUTPUT_ERROR_NO_SOURCES;
    }

    uint32_t channels = flexoutput_audio_get_speaker_count(context->speakers);
    uint32_t frames_per_buffer = 1024; // Standard audio buffer size
    
    // Allocate frame data if needed
    size_t frame_size = flexoutput_audio_calculate_frame_size(frames_per_buffer, 
                                                              channels, 
                                                              context->format);
    
    if (!frame->data[0]) {
        FLEXOUTPUT_SAFE_FREE(frame->data[0]);
        frame->data[0] = bmalloc(frame_size);
    }

    // Initialize frame properties
    frame->samples_per_sec = context->sample_rate;
    frame->format = context->format;
    frame->speakers = context->speakers;
    frame->frames = frames_per_buffer;
    frame->timestamp = os_gettime_ns();

    // Clear output buffer
    flexoutput_audio_clear_buffer(frame->data[0], frames_per_buffer, channels, context->format);

    // Mix audio from all sources
    for (size_t i = 0; i < context->source_refs.num; i++) {
        obs_source_t *source = context->source_refs.array[i];
        float volume = context->source_volumes.array[i];
        bool muted = context->source_muted.array[i];
        
        if (!source || !obs_source_active(source) || muted || volume <= 0.0f) {
            continue;
        }

        // Skip audio mixing for now - this requires proper audio callback setup
        // TODO: Implement proper audio source mixing using OBS audio callbacks
        // For now, we'll just apply silence or a basic tone
        (void)volume; // Suppress unused variable warning
    }

    // Update statistics
    context->frames_mixed++;
    context->last_mix_time = os_gettime_ns();

    // Call frame ready callback if set
    if (context->audio_ready_callback) {
        context->audio_ready_callback(frame, context->callback_data);
    }

    pthread_mutex_unlock(&context->mutex);

    return FLEXOUTPUT_SUCCESS;
}

flexoutput_error_t flexoutput_audio_get_frame(audio_mix_context_t *context,
                                               flexoutput_audio_frame_t *frame)
{
    return flexoutput_audio_mix_frame(context, frame);
}

flexoutput_audio_frame_t *flexoutput_audio_create_frame(uint32_t sample_rate,
                                                         enum audio_format format,
                                                         enum speaker_layout speakers,
                                                         uint32_t frames)
{
    if (sample_rate == 0 || frames == 0) {
        return NULL;
    }

    flexoutput_audio_frame_t *frame = bzalloc(sizeof(flexoutput_audio_frame_t));
    frame->samples_per_sec = sample_rate;
    frame->format = format;
    frame->speakers = speakers;
    frame->frames = frames;
    
    uint32_t channels = flexoutput_audio_get_speaker_count(speakers);
    size_t frame_size = flexoutput_audio_calculate_frame_size(frames, channels, format);
    frame->data[0] = bmalloc(frame_size);
    frame->timestamp = os_gettime_ns();

    return frame;
}

void flexoutput_audio_free_frame(flexoutput_audio_frame_t *frame)
{
    if (!frame) {
        return;
    }

    for (int i = 0; i < MAX_AV_PLANES; i++) {
        FLEXOUTPUT_SAFE_FREE(frame->data[i]);
    }
    bfree(frame);
}

flexoutput_error_t flexoutput_audio_copy_frame(flexoutput_audio_frame_t *dst,
                                                const flexoutput_audio_frame_t *src)
{
    if (!dst || !src || !src->data[0]) {
        return FLEXOUTPUT_ERROR_INVALID_PARAM;
    }

    // Calculate frame size
    uint32_t channels = flexoutput_audio_get_speaker_count(src->speakers);
    size_t frame_size = flexoutput_audio_calculate_frame_size(src->frames, channels, src->format);

    // Allocate destination data if needed
    if (!dst->data[0]) {
        dst->data[0] = bmalloc(frame_size);
    }

    // Copy frame properties
    dst->samples_per_sec = src->samples_per_sec;
    dst->format = src->format;
    dst->speakers = src->speakers;
    dst->frames = src->frames;
    dst->timestamp = src->timestamp;

    // Copy frame data
    memcpy(dst->data[0], src->data[0], frame_size);

    return FLEXOUTPUT_SUCCESS;
}

flexoutput_error_t flexoutput_audio_mix_buffers(void *output,
                                                 const void *input,
                                                 uint32_t frames,
                                                 uint32_t channels,
                                                 float volume,
                                                 enum audio_format format)
{
    if (!output || !input || frames == 0 || channels == 0) {
        return FLEXOUTPUT_ERROR_INVALID_PARAM;
    }

    switch (format) {
    case AUDIO_FORMAT_FLOAT:
    case AUDIO_FORMAT_FLOAT_PLANAR: {
        float *out = (float*)output;
        const float *in = (const float*)input;
        size_t samples = frames * channels;
        
        for (size_t i = 0; i < samples; i++) {
            out[i] += in[i] * volume;
            // Clamp to prevent overflow
            if (out[i] > 1.0f) out[i] = 1.0f;
            else if (out[i] < -1.0f) out[i] = -1.0f;
        }
        break;
    }
    case AUDIO_FORMAT_16BIT:
    case AUDIO_FORMAT_16BIT_PLANAR: {
        int16_t *out = (int16_t*)output;
        const int16_t *in = (const int16_t*)input;
        size_t samples = frames * channels;
        
        for (size_t i = 0; i < samples; i++) {
            int32_t mixed = out[i] + (int32_t)(in[i] * volume);
            // Clamp to prevent overflow
            if (mixed > 32767) mixed = 32767;
            else if (mixed < -32768) mixed = -32768;
            out[i] = (int16_t)mixed;
        }
        break;
    }
    case AUDIO_FORMAT_32BIT:
    case AUDIO_FORMAT_32BIT_PLANAR: {
        int32_t *out = (int32_t*)output;
        const int32_t *in = (const int32_t*)input;
        size_t samples = frames * channels;
        
        for (size_t i = 0; i < samples; i++) {
            int64_t mixed = out[i] + (int64_t)(in[i] * volume);
            // Clamp to prevent overflow
            if (mixed > 2147483647LL) mixed = 2147483647LL;
            else if (mixed < -2147483648LL) mixed = -2147483648LL;
            out[i] = (int32_t)mixed;
        }
        break;
    }
    default:
        return FLEXOUTPUT_ERROR_UNSUPPORTED_FORMAT;
    }

    return FLEXOUTPUT_SUCCESS;
}

flexoutput_error_t flexoutput_audio_apply_volume(void *buffer,
                                                  uint32_t frames,
                                                  uint32_t channels,
                                                  float volume,
                                                  enum audio_format format)
{
    if (!buffer || frames == 0 || channels == 0) {
        return FLEXOUTPUT_ERROR_INVALID_PARAM;
    }

    if (volume == 1.0f) {
        return FLEXOUTPUT_SUCCESS; // No change needed
    }

    switch (format) {
    case AUDIO_FORMAT_FLOAT:
    case AUDIO_FORMAT_FLOAT_PLANAR: {
        float *data = (float*)buffer;
        size_t samples = frames * channels;
        
        for (size_t i = 0; i < samples; i++) {
            data[i] *= volume;
        }
        break;
    }
    case AUDIO_FORMAT_16BIT:
    case AUDIO_FORMAT_16BIT_PLANAR: {
        int16_t *data = (int16_t*)buffer;
        size_t samples = frames * channels;
        
        for (size_t i = 0; i < samples; i++) {
            data[i] = (int16_t)(data[i] * volume);
        }
        break;
    }
    case AUDIO_FORMAT_32BIT:
    case AUDIO_FORMAT_32BIT_PLANAR: {
        int32_t *data = (int32_t*)buffer;
        size_t samples = frames * channels;
        
        for (size_t i = 0; i < samples; i++) {
            data[i] = (int32_t)(data[i] * volume);
        }
        break;
    }
    default:
        return FLEXOUTPUT_ERROR_UNSUPPORTED_FORMAT;
    }

    return FLEXOUTPUT_SUCCESS;
}

flexoutput_error_t flexoutput_audio_clear_buffer(void *buffer,
                                                  uint32_t frames,
                                                  uint32_t channels,
                                                  enum audio_format format)
{
    if (!buffer || frames == 0 || channels == 0) {
        return FLEXOUTPUT_ERROR_INVALID_PARAM;
    }

    size_t size = flexoutput_audio_calculate_frame_size(frames, channels, format);
    memset(buffer, 0, size);

    return FLEXOUTPUT_SUCCESS;
}

flexoutput_error_t flexoutput_audio_get_format_info(enum audio_format format,
                                                     uint32_t *bytes_per_sample,
                                                     bool *is_float)
{
    if (!bytes_per_sample || !is_float) {
        return FLEXOUTPUT_ERROR_INVALID_PARAM;
    }

    switch (format) {
    case AUDIO_FORMAT_FLOAT:
    case AUDIO_FORMAT_FLOAT_PLANAR:
        *bytes_per_sample = 4;
        *is_float = true;
        break;
    case AUDIO_FORMAT_16BIT:
    case AUDIO_FORMAT_16BIT_PLANAR:
        *bytes_per_sample = 2;
        *is_float = false;
        break;
    case AUDIO_FORMAT_32BIT:
    case AUDIO_FORMAT_32BIT_PLANAR:
        *bytes_per_sample = 4;
        *is_float = false;
        break;
    default:
        return FLEXOUTPUT_ERROR_UNSUPPORTED_FORMAT;
    }

    return FLEXOUTPUT_SUCCESS;
}

uint32_t flexoutput_audio_get_speaker_count(enum speaker_layout speakers)
{
    switch (speakers) {
    case SPEAKERS_MONO: return 1;
    case SPEAKERS_STEREO: return 2;
    case SPEAKERS_2POINT1: return 3;
    case SPEAKERS_4POINT0: return 4;
    case SPEAKERS_4POINT1: return 5;
    case SPEAKERS_5POINT1: return 6;
    case SPEAKERS_7POINT1: return 8;
    default: return 2; // Default to stereo
    }
}

size_t flexoutput_audio_calculate_frame_size(uint32_t frames,
                                              uint32_t channels,
                                              enum audio_format format)
{
    uint32_t bytes_per_sample;
    bool is_float;
    
    if (flexoutput_audio_get_format_info(format, &bytes_per_sample, &is_float) != FLEXOUTPUT_SUCCESS) {
        return 0;
    }

    return frames * channels * bytes_per_sample;
}

enum audio_format flexoutput_audio_get_optimal_format(const flexoutput_output_config_t *config)
{
    UNUSED_PARAMETER(config);
    
    // Default to float for best quality
    return AUDIO_FORMAT_FLOAT;
}

bool flexoutput_audio_is_format_supported(enum audio_format format)
{
    switch (format) {
    case AUDIO_FORMAT_FLOAT:
    case AUDIO_FORMAT_FLOAT_PLANAR:
    case AUDIO_FORMAT_16BIT:
    case AUDIO_FORMAT_16BIT_PLANAR:
    case AUDIO_FORMAT_32BIT:
    case AUDIO_FORMAT_32BIT_PLANAR:
        return true;
    default:
        return false;
    }
}

const char *flexoutput_audio_get_format_string(enum audio_format format)
{
    switch (format) {
    case AUDIO_FORMAT_FLOAT: return "Float";
    case AUDIO_FORMAT_FLOAT_PLANAR: return "Float Planar";
    case AUDIO_FORMAT_16BIT: return "16-bit";
    case AUDIO_FORMAT_16BIT_PLANAR: return "16-bit Planar";
    case AUDIO_FORMAT_32BIT: return "32-bit";
    case AUDIO_FORMAT_32BIT_PLANAR: return "32-bit Planar";
    default: return "Unknown";
    }
}

const char *flexoutput_audio_get_speaker_string(enum speaker_layout speakers)
{
    switch (speakers) {
    case SPEAKERS_MONO: return "Mono";
    case SPEAKERS_STEREO: return "Stereo";
    case SPEAKERS_2POINT1: return "2.1";
    case SPEAKERS_4POINT0: return "4.0";
    case SPEAKERS_4POINT1: return "4.1";
    case SPEAKERS_5POINT1: return "5.1";
    case SPEAKERS_7POINT1: return "7.1";
    default: return "Unknown";
    }
}

// Helper functions
static void cleanup_context_sources(audio_mix_context_t *context)
{
    if (!context) {
        return;
    }

    // Release all source references
    for (size_t i = 0; i < context->source_refs.num; i++) {
        obs_source_release(context->source_refs.array[i]);
    }
    da_resize(context->source_refs, 0);

    // Free all source names
    for (size_t i = 0; i < context->source_names.num; i++) {
        FLEXOUTPUT_SAFE_FREE(context->source_names.array[i]);
    }
    da_resize(context->source_names, 0);
    
    // Clear volume and mute arrays
    da_resize(context->source_volumes, 0);
    da_resize(context->source_muted, 0);
}

static flexoutput_error_t resample_audio_data(audio_mix_context_t *context,
                                               const struct audio_data *input,
                                               struct audio_data *output)
{
    if (!context || !input || !output || !context->resampler) {
        return FLEXOUTPUT_ERROR_INVALID_PARAM;
    }

    // Use resampler to convert audio format
    uint8_t *resampled_data[MAX_AV_PLANES] = {0};
    uint32_t resampled_frames = 0;
    uint64_t ts_offset = 0;
    
    bool success = audio_resampler_resample(context->resampler,
                                            resampled_data,
                                            &resampled_frames,
                                            &ts_offset,
                                            (const uint8_t *const *)input->data,
                                            input->frames);
    
    if (!success || resampled_frames == 0) {
        return FLEXOUTPUT_ERROR_RESAMPLE_FAILED;
    }

    // Copy resampled data to output
    if (output->data[0] && resampled_data[0]) {
        size_t copy_size = resampled_frames * flexoutput_audio_get_speaker_count(context->speakers);
        uint32_t bytes_per_sample;
        bool is_float;
        
        if (flexoutput_audio_get_format_info(context->format, &bytes_per_sample, &is_float) == FLEXOUTPUT_SUCCESS) {
            copy_size *= bytes_per_sample;
            memcpy(output->data[0], resampled_data[0], copy_size);
            output->frames = resampled_frames;
        }
    }

    return FLEXOUTPUT_SUCCESS;
}

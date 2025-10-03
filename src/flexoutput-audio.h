#pragma once

#include "flexoutput-types.h"
#include <media-io/audio-resampler.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Audio Mixing Engine
 * 
 * This module handles audio mixing from multiple sources for each output.
 * It provides functionality to:
 * - Mix audio from multiple sources
 * - Handle different sample rates and formats
 * - Apply volume controls and effects
 * - Manage audio buffers efficiently
 * - Support real-time audio processing
 */

// Audio mixing context
struct audio_mix_context {
    char *output_name;
    
    // Audio format
    struct audio_output_info audio_info;
    uint32_t sample_rate;
    enum audio_format format;
    enum speaker_layout speakers;
    
    // Source management
    DARRAY(char*) source_names;
    DARRAY(obs_source_t*) source_refs;
    DARRAY(float) source_volumes;
    DARRAY(bool) source_muted;
    
    // Audio processing
    DARRAY(uint8_t) audio_buffer;
    audio_resampler_t *resampler;
    
    // Mixing state
    bool active;
    bool mixing;
    uint64_t frames_mixed;
    uint64_t last_mix_time;
    
    // Threading
    pthread_mutex_t mutex;
    
    // Callbacks
    void (*audio_ready_callback)(const flexoutput_audio_frame_t *frame, void *data);
    void *callback_data;
};

/**
 * Initialize the audio mixing system
 * @return Error code
 */
flexoutput_error_t flexoutput_audio_init(void);

/**
 * Cleanup the audio mixing system
 */
void flexoutput_audio_cleanup(void);

/**
 * Create an audio mixing context
 * @param output_name Output name
 * @param sample_rate Sample rate (e.g., 44100, 48000)
 * @param format Audio format
 * @param speakers Speaker layout
 * @return Mixing context (must be freed with flexoutput_audio_destroy_context)
 */
audio_mix_context_t *flexoutput_audio_create_context(const char *output_name,
                                                     uint32_t sample_rate,
                                                     enum audio_format format,
                                                     enum speaker_layout speakers);

/**
 * Destroy an audio mixing context
 * @param context Context to destroy
 */
void flexoutput_audio_destroy_context(audio_mix_context_t *context);

/**
 * Add source to mixing context
 * @param context Mixing context
 * @param source_name Source name to add
 * @param volume Initial volume (0.0 to 1.0)
 * @return Error code
 */
flexoutput_error_t flexoutput_audio_add_source(audio_mix_context_t *context,
                                                const char *source_name,
                                                float volume);

/**
 * Remove source from mixing context
 * @param context Mixing context
 * @param source_name Source name to remove
 * @return Error code
 */
flexoutput_error_t flexoutput_audio_remove_source(audio_mix_context_t *context,
                                                   const char *source_name);

/**
 * Clear all sources from mixing context
 * @param context Mixing context
 * @return Error code
 */
flexoutput_error_t flexoutput_audio_clear_sources(audio_mix_context_t *context);

/**
 * Update sources in mixing context based on mapping rules
 * @param context Mixing context
 * @param mapping_rule Mapping rule to apply
 * @return Error code
 */
flexoutput_error_t flexoutput_audio_update_sources(audio_mix_context_t *context,
                                                    const flexoutput_mapping_rule_t *mapping_rule);

/**
 * Set source volume
 * @param context Mixing context
 * @param source_name Source name
 * @param volume Volume (0.0 to 1.0)
 * @return Error code
 */
flexoutput_error_t flexoutput_audio_set_source_volume(audio_mix_context_t *context,
                                                       const char *source_name,
                                                       float volume);

/**
 * Get source volume
 * @param context Mixing context
 * @param source_name Source name
 * @param volume Output volume
 * @return Error code
 */
flexoutput_error_t flexoutput_audio_get_source_volume(audio_mix_context_t *context,
                                                       const char *source_name,
                                                       float *volume);

/**
 * Set source mute state
 * @param context Mixing context
 * @param source_name Source name
 * @param muted Mute state
 * @return Error code
 */
flexoutput_error_t flexoutput_audio_set_source_muted(audio_mix_context_t *context,
                                                      const char *source_name,
                                                      bool muted);

/**
 * Get source mute state
 * @param context Mixing context
 * @param source_name Source name
 * @param muted Output mute state
 * @return Error code
 */
flexoutput_error_t flexoutput_audio_get_source_muted(audio_mix_context_t *context,
                                                      const char *source_name,
                                                      bool *muted);

/**
 * Start mixing for context
 * @param context Mixing context
 * @return Error code
 */
flexoutput_error_t flexoutput_audio_start_mixing(audio_mix_context_t *context);

/**
 * Stop mixing for context
 * @param context Mixing context
 * @return Error code
 */
flexoutput_error_t flexoutput_audio_stop_mixing(audio_mix_context_t *context);

/**
 * Check if context is actively mixing
 * @param context Mixing context
 * @return true if mixing
 */
bool flexoutput_audio_is_mixing(const audio_mix_context_t *context);

/**
 * Set audio ready callback
 * @param context Mixing context
 * @param callback Callback function
 * @param data User data for callback
 * @return Error code
 */
flexoutput_error_t flexoutput_audio_set_frame_callback(audio_mix_context_t *context,
                                                        void (*callback)(const flexoutput_audio_frame_t *frame, void *data),
                                                        void *data);

/**
 * Mix audio from all sources
 * @param context Mixing context
 * @param frame Output audio frame
 * @return Error code
 */
flexoutput_error_t flexoutput_audio_mix_frame(audio_mix_context_t *context,
                                               flexoutput_audio_frame_t *frame);

/**
 * Get current mixed audio frame
 * @param context Mixing context
 * @param frame Output audio frame
 * @return Error code
 */
flexoutput_error_t flexoutput_audio_get_frame(audio_mix_context_t *context,
                                               flexoutput_audio_frame_t *frame);

// Audio frame management
/**
 * Create an audio frame
 * @param sample_rate Sample rate
 * @param format Audio format
 * @param speakers Speaker layout
 * @param frames Number of frames
 * @return Audio frame (must be freed with flexoutput_audio_free_frame)
 */
flexoutput_audio_frame_t *flexoutput_audio_create_frame(uint32_t sample_rate,
                                                         enum audio_format format,
                                                         enum speaker_layout speakers,
                                                         uint32_t frames);

/**
 * Free an audio frame
 * @param frame Frame to free
 */
void flexoutput_audio_free_frame(flexoutput_audio_frame_t *frame);

/**
 * Copy audio frame data
 * @param dst Destination frame
 * @param src Source frame
 * @return Error code
 */
flexoutput_error_t flexoutput_audio_copy_frame(flexoutput_audio_frame_t *dst,
                                                const flexoutput_audio_frame_t *src);

/**
 * Convert audio frame format
 * @param frame Frame to convert
 * @param new_format Target format
 * @param new_sample_rate Target sample rate
 * @param new_speakers Target speaker layout
 * @return Error code
 */
flexoutput_error_t flexoutput_audio_convert_frame(flexoutput_audio_frame_t *frame,
                                                   enum audio_format new_format,
                                                   uint32_t new_sample_rate,
                                                   enum speaker_layout new_speakers);

// Audio processing utilities
/**
 * Mix two audio buffers
 * @param output Output buffer
 * @param input Input buffer to mix
 * @param frames Number of frames
 * @param channels Number of channels
 * @param volume Volume multiplier (0.0 to 1.0)
 * @param format Audio format
 * @return Error code
 */
flexoutput_error_t flexoutput_audio_mix_buffers(void *output,
                                                 const void *input,
                                                 uint32_t frames,
                                                 uint32_t channels,
                                                 float volume,
                                                 enum audio_format format);

/**
 * Apply volume to audio buffer
 * @param buffer Audio buffer
 * @param frames Number of frames
 * @param channels Number of channels
 * @param volume Volume multiplier (0.0 to 1.0)
 * @param format Audio format
 * @return Error code
 */
flexoutput_error_t flexoutput_audio_apply_volume(void *buffer,
                                                  uint32_t frames,
                                                  uint32_t channels,
                                                  float volume,
                                                  enum audio_format format);

/**
 * Clear audio buffer (fill with silence)
 * @param buffer Audio buffer
 * @param frames Number of frames
 * @param channels Number of channels
 * @param format Audio format
 * @return Error code
 */
flexoutput_error_t flexoutput_audio_clear_buffer(void *buffer,
                                                  uint32_t frames,
                                                  uint32_t channels,
                                                  enum audio_format format);

/**
 * Get audio format info
 * @param format Audio format
 * @param bytes_per_sample Output bytes per sample
 * @param is_float Output whether format is floating point
 * @return Error code
 */
flexoutput_error_t flexoutput_audio_get_format_info(enum audio_format format,
                                                     uint32_t *bytes_per_sample,
                                                     bool *is_float);

/**
 * Get speaker count for layout
 * @param speakers Speaker layout
 * @return Number of speakers/channels
 */
uint32_t flexoutput_audio_get_speaker_count(enum speaker_layout speakers);

/**
 * Calculate audio frame size in bytes
 * @param frames Number of frames
 * @param channels Number of channels
 * @param format Audio format
 * @return Frame size in bytes
 */
size_t flexoutput_audio_calculate_frame_size(uint32_t frames,
                                              uint32_t channels,
                                              enum audio_format format);

/**
 * Get optimal audio format for output configuration
 * @param config Output configuration
 * @return Optimal audio format
 */
enum audio_format flexoutput_audio_get_optimal_format(const flexoutput_output_config_t *config);

/**
 * Check if audio format is supported
 * @param format Audio format to check
 * @return true if supported
 */
bool flexoutput_audio_is_format_supported(enum audio_format format);

/**
 * Get audio format string
 * @param format Audio format
 * @return Format string (do not free)
 */
const char *flexoutput_audio_get_format_string(enum audio_format format);

/**
 * Get speaker layout string
 * @param speakers Speaker layout
 * @return Layout string (do not free)
 */
const char *flexoutput_audio_get_speaker_string(enum speaker_layout speakers);

// Performance monitoring
typedef struct {
    uint64_t frames_mixed;
    uint64_t frames_dropped;
    double average_mix_time;
    double peak_mix_time;
    uint64_t total_mix_time;
    uint32_t buffer_underruns;
    uint32_t buffer_overruns;
} audio_performance_stats_t;

/**
 * Get performance statistics for context
 * @param context Mixing context
 * @param stats Output statistics
 * @return Error code
 */
flexoutput_error_t flexoutput_audio_get_stats(const audio_mix_context_t *context,
                                               audio_performance_stats_t *stats);

/**
 * Reset performance statistics
 * @param context Mixing context
 * @return Error code
 */
flexoutput_error_t flexoutput_audio_reset_stats(audio_mix_context_t *context);

// Advanced audio features
/**
 * Set audio effects chain
 * @param context Mixing context
 * @param effects Array of effect names
 * @param effect_count Number of effects
 * @return Error code
 */
flexoutput_error_t flexoutput_audio_set_effects(audio_mix_context_t *context,
                                                 const char **effects,
                                                 size_t effect_count);

/**
 * Set master volume for context
 * @param context Mixing context
 * @param volume Master volume (0.0 to 1.0)
 * @return Error code
 */
flexoutput_error_t flexoutput_audio_set_master_volume(audio_mix_context_t *context,
                                                       float volume);

/**
 * Get master volume for context
 * @param context Mixing context
 * @param volume Output master volume
 * @return Error code
 */
flexoutput_error_t flexoutput_audio_get_master_volume(audio_mix_context_t *context,
                                                       float *volume);

#ifdef __cplusplus
}
#endif

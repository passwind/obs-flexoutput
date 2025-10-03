#pragma once

#include "flexoutput-types.h"
#include <graphics/graphics.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Video Rendering Engine
 * 
 * This module handles video rendering using gs_texrender for multiple sources.
 * It provides functionality to:
 * - Create and manage rendering contexts for each output
 * - Composite multiple sources into a single output frame
 * - Handle different video formats and resolutions
 * - Manage GPU resources efficiently
 * - Support real-time rendering with minimal latency
 */

// Video rendering context
struct video_render_context {
    char *output_name;
    uint32_t width;
    uint32_t height;
    enum gs_color_format format;
    
    gs_texrender_t *texrender;
    gs_stagesurf_t *stagesurface;
    
    // Source composition
    DARRAY(char*) source_names;
    DARRAY(obs_source_t*) source_refs;
    
    // Rendering state
    bool active;
    bool rendering;
    uint64_t frame_count;
    uint64_t last_render_time;
    
    // Threading
    pthread_mutex_t mutex;
    
    // Callbacks
    void (*frame_ready_callback)(const flexoutput_video_frame_t *frame, void *data);
    void *callback_data;
};

/**
 * Initialize the video rendering system
 * @return Error code
 */
flexoutput_error_t flexoutput_video_init(void);

/**
 * Cleanup the video rendering system
 */
void flexoutput_video_cleanup(void);

/**
 * Create a video rendering context
 * @param output_name Output name
 * @param width Output width
 * @param height Output height
 * @param format Color format
 * @return Rendering context (must be freed with flexoutput_video_destroy_context)
 */
video_render_context_t *flexoutput_video_create_context(const char *output_name,
                                                        uint32_t width,
                                                        uint32_t height,
                                                        enum gs_color_format format);

/**
 * Destroy a video rendering context
 * @param context Context to destroy
 */
void flexoutput_video_destroy_context(video_render_context_t *context);

/**
 * Add source to rendering context
 * @param context Rendering context
 * @param source_name Source name to add
 * @return Error code
 */
flexoutput_error_t flexoutput_video_add_source(video_render_context_t *context,
                                                const char *source_name);

/**
 * Remove source from rendering context
 * @param context Rendering context
 * @param source_name Source name to remove
 * @return Error code
 */
flexoutput_error_t flexoutput_video_remove_source(video_render_context_t *context,
                                                   const char *source_name);

/**
 * Clear all sources from rendering context
 * @param context Rendering context
 * @return Error code
 */
flexoutput_error_t flexoutput_video_clear_sources(video_render_context_t *context);

/**
 * Update sources in rendering context based on mapping rules
 * @param context Rendering context
 * @param mapping_rule Mapping rule to apply
 * @return Error code
 */
flexoutput_error_t flexoutput_video_update_sources(video_render_context_t *context,
                                                    const flexoutput_mapping_rule_t *mapping_rule);

/**
 * Start rendering for context
 * @param context Rendering context
 * @return Error code
 */
flexoutput_error_t flexoutput_video_start_rendering(video_render_context_t *context);

/**
 * Stop rendering for context
 * @param context Rendering context
 * @return Error code
 */
flexoutput_error_t flexoutput_video_stop_rendering(video_render_context_t *context);

/**
 * Check if context is actively rendering
 * @param context Rendering context
 * @return true if rendering
 */
bool flexoutput_video_is_rendering(const video_render_context_t *context);

/**
 * Set frame ready callback
 * @param context Rendering context
 * @param callback Callback function
 * @param data User data for callback
 * @return Error code
 */
flexoutput_error_t flexoutput_video_set_frame_callback(video_render_context_t *context,
                                                       void (*callback)(const flexoutput_video_frame_t *frame, void *data),
                                                       void *data);

/**
 * Render a single frame
 * @param context Rendering context
 * @param frame Output frame data
 * @return Error code
 */
flexoutput_error_t flexoutput_video_render_frame(video_render_context_t *context,
                                                  flexoutput_video_frame_t *frame);

/**
 * Get current frame from context
 * @param context Rendering context
 * @param frame Output frame data
 * @return Error code
 */
flexoutput_error_t flexoutput_video_get_frame(video_render_context_t *context,
                                               flexoutput_video_frame_t *frame);

// Frame management
/**
 * Create a video frame
 * @param width Frame width
 * @param height Frame height
 * @param format Color format
 * @return Video frame (must be freed with flexoutput_video_free_frame)
 */
flexoutput_video_frame_t *flexoutput_video_create_frame(uint32_t width,
                                                         uint32_t height,
                                                         enum gs_color_format format);

/**
 * Free a video frame
 * @param frame Frame to free
 */
void flexoutput_video_free_frame(flexoutput_video_frame_t *frame);

/**
 * Copy video frame data
 * @param dst Destination frame
 * @param src Source frame
 * @return Error code
 */
flexoutput_error_t flexoutput_video_copy_frame(flexoutput_video_frame_t *dst,
                                                const flexoutput_video_frame_t *src);

/**
 * Convert frame format
 * @param frame Frame to convert
 * @param new_format Target format
 * @return Error code
 */
flexoutput_error_t flexoutput_video_convert_frame(flexoutput_video_frame_t *frame,
                                                   enum gs_color_format new_format);

// Source composition
/**
 * Composite sources into output frame
 * @param context Rendering context
 * @param sources Array of source references
 * @param source_count Number of sources
 * @param output_frame Output frame
 * @return Error code
 */
flexoutput_error_t flexoutput_video_composite_sources(video_render_context_t *context,
                                                       obs_source_t **sources,
                                                       size_t source_count,
                                                       flexoutput_video_frame_t *output_frame);

/**
 * Render source to texture
 * @param source Source to render
 * @param texrender Texture render object
 * @param width Output width
 * @param height Output height
 * @return Error code
 */
flexoutput_error_t flexoutput_video_render_source(obs_source_t *source,
                                                   gs_texrender_t *texrender,
                                                   uint32_t width,
                                                   uint32_t height);

/**
 * Blend multiple textures
 * @param textures Array of textures to blend
 * @param texture_count Number of textures
 * @param output_texture Output texture
 * @param blend_mode Blend mode
 * @return Error code
 */
flexoutput_error_t flexoutput_video_blend_textures(gs_texture_t **textures,
                                                    size_t texture_count,
                                                    gs_texture_t *output_texture,
                                                    enum gs_blend_type blend_mode);

// Utility functions
/**
 * Get optimal format for output configuration
 * @param config Output configuration
 * @return Optimal color format
 */
enum gs_color_format flexoutput_video_get_optimal_format(const flexoutput_output_config_t *config);

/**
 * Check if format is supported
 * @param format Color format to check
 * @return true if supported
 */
bool flexoutput_video_is_format_supported(enum gs_color_format format);

/**
 * Get format string
 * @param format Color format
 * @return Format string (do not free)
 */
const char *flexoutput_video_get_format_string(enum gs_color_format format);

/**
 * Calculate frame size in bytes
 * @param width Frame width
 * @param height Frame height
 * @param format Color format
 * @return Frame size in bytes
 */
size_t flexoutput_video_calculate_frame_size(uint32_t width,
                                              uint32_t height,
                                              enum gs_color_format format);

/**
 * Get bytes per pixel for format
 * @param format Color format
 * @return Bytes per pixel
 */
uint32_t flexoutput_video_get_bytes_per_pixel(enum gs_color_format format);

// Performance monitoring
typedef struct {
    uint64_t frames_rendered;
    uint64_t frames_dropped;
    double average_render_time;
    double peak_render_time;
    uint64_t total_render_time;
    uint32_t current_fps;
    uint32_t target_fps;
} video_performance_stats_t;

/**
 * Get performance statistics for context
 * @param context Rendering context
 * @param stats Output statistics
 * @return Error code
 */
flexoutput_error_t flexoutput_video_get_stats(const video_render_context_t *context,
                                               video_performance_stats_t *stats);

/**
 * Reset performance statistics
 * @param context Rendering context
 * @return Error code
 */
flexoutput_error_t flexoutput_video_reset_stats(video_render_context_t *context);

// Advanced rendering features
/**
 * Set custom shader for rendering
 * @param context Rendering context
 * @param shader_path Path to shader file
 * @return Error code
 */
flexoutput_error_t flexoutput_video_set_custom_shader(video_render_context_t *context,
                                                       const char *shader_path);

/**
 * Apply color correction
 * @param context Rendering context
 * @param brightness Brightness adjustment (-1.0 to 1.0)
 * @param contrast Contrast adjustment (-1.0 to 1.0)
 * @param saturation Saturation adjustment (-1.0 to 1.0)
 * @param gamma Gamma adjustment (0.1 to 3.0)
 * @return Error code
 */
flexoutput_error_t flexoutput_video_set_color_correction(video_render_context_t *context,
                                                          float brightness,
                                                          float contrast,
                                                          float saturation,
                                                          float gamma);

/**
 * Set scaling filter
 * @param context Rendering context
 * @param filter Scaling filter type
 * @return Error code
 */
flexoutput_error_t flexoutput_video_set_scaling_filter(video_render_context_t *context,
                                                        enum obs_scale_type filter);

#ifdef __cplusplus
}
#endif

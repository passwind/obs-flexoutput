#include "flexoutput-video.h"
#include "flexoutput-source.h"
#include <util/threading.h>
#include <util/platform.h>

// Global video system state
static bool video_system_initialized = false;
static DARRAY(video_render_context_t*) render_contexts;
static pthread_mutex_t contexts_mutex;

// Forward declarations
static void video_render_thread(void *data);
static void cleanup_context_sources(video_render_context_t *context);
static void update_context_sources(video_render_context_t *context);
static enum video_format gs_color_format_to_video_format(enum gs_color_format format);

flexoutput_error_t flexoutput_video_init(void)
{
    if (video_system_initialized) {
        return FLEXOUTPUT_SUCCESS;
    }

    da_init(render_contexts);
    
    if (pthread_mutex_init(&contexts_mutex, NULL) != 0) {
        da_free(render_contexts);
        return FLEXOUTPUT_ERROR_MEMORY;
    }

    video_system_initialized = true;
    
    FLEXOUTPUT_LOG_INFO("Video rendering system initialized");
    return FLEXOUTPUT_SUCCESS;
}

void flexoutput_video_cleanup(void)
{
    if (!video_system_initialized) {
        return;
    }

    pthread_mutex_lock(&contexts_mutex);
    
    // Cleanup all contexts
    for (size_t i = 0; i < render_contexts.num; i++) {
        flexoutput_video_destroy_context(render_contexts.array[i]);
    }
    da_free(render_contexts);
    
    pthread_mutex_unlock(&contexts_mutex);
    pthread_mutex_destroy(&contexts_mutex);

    video_system_initialized = false;
    
    FLEXOUTPUT_LOG_INFO("Video rendering system cleaned up");
}

video_render_context_t *flexoutput_video_create_context(const char *output_name,
                                                        uint32_t width,
                                                        uint32_t height,
                                                        enum gs_color_format format)
{
    if (!output_name || width == 0 || height == 0) {
        return NULL;
    }

    video_render_context_t *context = bzalloc(sizeof(video_render_context_t));
    context->output_name = bstrdup(output_name);
    context->width = width;
    context->height = height;
    context->format = format;
    
    da_init(context->source_names);
    da_init(context->source_refs);
    
    if (pthread_mutex_init(&context->mutex, NULL) != 0) {
        FLEXOUTPUT_SAFE_FREE(context->output_name);
        da_free(context->source_names);
        da_free(context->source_refs);
        bfree(context);
        return NULL;
    }

    // Create graphics resources
    obs_enter_graphics();
    
    context->texrender = gs_texrender_create(format, GS_ZS_NONE);
    if (!context->texrender) {
        FLEXOUTPUT_LOG_ERROR("Failed to create texrender for output: %s", output_name);
        obs_leave_graphics();
        pthread_mutex_destroy(&context->mutex);
        FLEXOUTPUT_SAFE_FREE(context->output_name);
        da_free(context->source_names);
        da_free(context->source_refs);
        bfree(context);
        return NULL;
    }

    context->stagesurface = gs_stagesurface_create(width, height, format);
    if (!context->stagesurface) {
        FLEXOUTPUT_LOG_ERROR("Failed to create stage surface for output: %s", output_name);
        gs_texrender_destroy(context->texrender);
        obs_leave_graphics();
        pthread_mutex_destroy(&context->mutex);
        FLEXOUTPUT_SAFE_FREE(context->output_name);
        da_free(context->source_names);
        da_free(context->source_refs);
        bfree(context);
        return NULL;
    }
    
    obs_leave_graphics();

    // Add to global contexts list
    pthread_mutex_lock(&contexts_mutex);
    da_push_back(render_contexts, &context);
    pthread_mutex_unlock(&contexts_mutex);

    FLEXOUTPUT_LOG_INFO("Created video render context for output: %s (%dx%d)", 
                        output_name, width, height);
    return context;
}

void flexoutput_video_destroy_context(video_render_context_t *context)
{
    if (!context) {
        return;
    }

    // Stop rendering first
    flexoutput_video_stop_rendering(context);

    // Remove from global contexts list
    pthread_mutex_lock(&contexts_mutex);
    for (size_t i = 0; i < render_contexts.num; i++) {
        if (render_contexts.array[i] == context) {
            da_erase(render_contexts, i);
            break;
        }
    }
    pthread_mutex_unlock(&contexts_mutex);

    pthread_mutex_lock(&context->mutex);

    // Cleanup sources
    cleanup_context_sources(context);
    da_free(context->source_names);
    da_free(context->source_refs);

    // Cleanup graphics resources
    obs_enter_graphics();
    if (context->texrender) {
        gs_texrender_destroy(context->texrender);
    }
    if (context->stagesurface) {
        gs_stagesurface_destroy(context->stagesurface);
    }
    obs_leave_graphics();

    FLEXOUTPUT_SAFE_FREE(context->output_name);
    
    pthread_mutex_unlock(&context->mutex);
    pthread_mutex_destroy(&context->mutex);
    
    bfree(context);
}

flexoutput_error_t flexoutput_video_add_source(video_render_context_t *context,
                                                const char *source_name)
{
    if (!context || !source_name) {
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

    // Add to arrays
    char *name_copy = bstrdup(source_name);
    da_push_back(context->source_names, &name_copy);
    da_push_back(context->source_refs, &source);

    pthread_mutex_unlock(&context->mutex);

    FLEXOUTPUT_LOG_DEBUG("Added source '%s' to render context '%s'", 
                         source_name, context->output_name);
    return FLEXOUTPUT_SUCCESS;
}

flexoutput_error_t flexoutput_video_remove_source(video_render_context_t *context,
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
            
            pthread_mutex_unlock(&context->mutex);
            
            FLEXOUTPUT_LOG_DEBUG("Removed source '%s' from render context '%s'", 
                                 source_name, context->output_name);
            return FLEXOUTPUT_SUCCESS;
        }
    }

    pthread_mutex_unlock(&context->mutex);
    return FLEXOUTPUT_ERROR_NOT_FOUND;
}

flexoutput_error_t flexoutput_video_clear_sources(video_render_context_t *context)
{
    if (!context) {
        return FLEXOUTPUT_ERROR_INVALID_PARAM;
    }

    pthread_mutex_lock(&context->mutex);
    cleanup_context_sources(context);
    pthread_mutex_unlock(&context->mutex);

    FLEXOUTPUT_LOG_DEBUG("Cleared all sources from render context '%s'", context->output_name);
    return FLEXOUTPUT_SUCCESS;
}

flexoutput_error_t flexoutput_video_update_sources(video_render_context_t *context,
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
        
        if (source) {
            char *name_copy = bstrdup(source_name);
            da_push_back(context->source_names, &name_copy);
            da_push_back(context->source_refs, &source);
        }
    }

    pthread_mutex_unlock(&context->mutex);

    FLEXOUTPUT_LOG_DEBUG("Updated sources for render context '%s' based on mapping rule", 
                         context->output_name);
    return FLEXOUTPUT_SUCCESS;
}

flexoutput_error_t flexoutput_video_start_rendering(video_render_context_t *context)
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
    context->frame_count = 0;
    context->last_render_time = os_gettime_ns();
    
    pthread_mutex_unlock(&context->mutex);

    FLEXOUTPUT_LOG_INFO("Started rendering for context '%s'", context->output_name);
    return FLEXOUTPUT_SUCCESS;
}

flexoutput_error_t flexoutput_video_stop_rendering(video_render_context_t *context)
{
    if (!context) {
        return FLEXOUTPUT_ERROR_INVALID_PARAM;
    }

    pthread_mutex_lock(&context->mutex);
    context->active = false;
    pthread_mutex_unlock(&context->mutex);

    FLEXOUTPUT_LOG_INFO("Stopped rendering for context '%s'", context->output_name);
    return FLEXOUTPUT_SUCCESS;
}

bool flexoutput_video_is_rendering(const video_render_context_t *context)
{
    return context ? context->active : false;
}

flexoutput_error_t flexoutput_video_set_frame_callback(video_render_context_t *context,
                                                       void (*callback)(const flexoutput_video_frame_t *frame, void *data),
                                                       void *data)
{
    if (!context) {
        return FLEXOUTPUT_ERROR_INVALID_PARAM;
    }

    pthread_mutex_lock(&context->mutex);
    context->frame_ready_callback = callback;
    context->callback_data = data;
    pthread_mutex_unlock(&context->mutex);

    FLEXOUTPUT_LOG_DEBUG("Video frame rendered for output: %s", context->output_name);
    return FLEXOUTPUT_SUCCESS;
}

flexoutput_error_t flexoutput_video_render_frame(video_render_context_t *context,
                                                  flexoutput_video_frame_t *frame)
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

    obs_enter_graphics();

    // Begin rendering
    if (!gs_texrender_begin(context->texrender, context->width, context->height)) {
        obs_leave_graphics();
        pthread_mutex_unlock(&context->mutex);
        return FLEXOUTPUT_ERROR_RENDER_FAILED;
    }

    // Set up viewport
    gs_viewport_push();
    gs_projection_push();
    gs_matrix_push();
    gs_matrix_identity();
    gs_ortho(0.0f, (float)context->width, 0.0f, (float)context->height, -100.0f, 100.0f);
    gs_set_viewport(0, 0, context->width, context->height);

    // Clear background
    struct vec4 clear_color = {0.0f, 0.0f, 0.0f, 1.0f};
    gs_clear(GS_CLEAR_COLOR, &clear_color, 0.0f, 0);

    // Render each source
    for (size_t i = 0; i < context->source_refs.num; i++) {
        obs_source_t *source = context->source_refs.array[i];
        if (!source || !obs_source_active(source)) {
            continue;
        }

        // Get source dimensions
        uint32_t source_width = obs_source_get_width(source);
        uint32_t source_height = obs_source_get_height(source);
        
        if (source_width == 0 || source_height == 0) {
            continue;
        }

        // Calculate scaling to fit output
        float scale_x = (float)context->width / (float)source_width;
        float scale_y = (float)context->height / (float)source_height;
        float scale = fminf(scale_x, scale_y);

        uint32_t scaled_width = (uint32_t)(source_width * scale);
        uint32_t scaled_height = (uint32_t)(source_height * scale);
        
        // Center the source
        uint32_t x_offset = (context->width - scaled_width) / 2;
        uint32_t y_offset = (context->height - scaled_height) / 2;

        // Render source
        gs_matrix_push();
        gs_matrix_translate3f((float)x_offset, (float)y_offset, 0.0f);
        gs_matrix_scale3f(scale, scale, 1.0f);
        
        obs_source_video_render(source);
        
        gs_matrix_pop();
    }

    // Restore graphics state
    gs_matrix_pop();
    gs_projection_pop();
    gs_viewport_pop();

    // End rendering
    gs_texrender_end(context->texrender);

    // Get rendered texture
    gs_texture_t *texture = gs_texrender_get_texture(context->texrender);
    if (!texture) {
        obs_leave_graphics();
        pthread_mutex_unlock(&context->mutex);
        return FLEXOUTPUT_ERROR_RENDER_FAILED;
    }

    // Copy to stage surface for CPU access
    gs_stage_texture(context->stagesurface, texture);

    // Map stage surface and copy data
    uint8_t *data;
    uint32_t linesize;
    if (gs_stagesurface_map(context->stagesurface, &data, &linesize)) {
        // Calculate frame size
        size_t frame_size = flexoutput_video_calculate_frame_size(context->width, 
                                                                  context->height, 
                                                                  context->format);
        
        // Allocate frame data if needed
        if (!frame->data || frame->size < frame_size) {
            FLEXOUTPUT_SAFE_FREE(frame->data);
            frame->data = bmalloc(frame_size);
            frame->size = frame_size;
        }

        // Copy frame data
        frame->width = context->width;
        frame->height = context->height;
        frame->format = gs_color_format_to_video_format(context->format);
        frame->linesize = linesize;
        frame->timestamp = os_gettime_ns();

        // Copy line by line to handle different line sizes
        uint32_t bytes_per_pixel = flexoutput_video_get_bytes_per_pixel(context->format);
        uint32_t row_size = context->width * bytes_per_pixel;
        
        for (uint32_t y = 0; y < context->height; y++) {
            memcpy((uint8_t*)frame->data + y * row_size,
                   data + y * linesize,
                   row_size);
        }

        gs_stagesurface_unmap(context->stagesurface);
    } else {
        obs_leave_graphics();
        pthread_mutex_unlock(&context->mutex);
        return FLEXOUTPUT_ERROR_RENDER_FAILED;
    }

    obs_leave_graphics();

    // Update statistics
    context->frame_count++;
    context->last_render_time = os_gettime_ns();

    // Call frame ready callback if set
    if (context->frame_ready_callback) {
        context->frame_ready_callback(frame, context->callback_data);
    }

    pthread_mutex_unlock(&context->mutex);

    return FLEXOUTPUT_SUCCESS;
}

flexoutput_error_t flexoutput_video_get_frame(video_render_context_t *context,
                                               flexoutput_video_frame_t *frame)
{
    return flexoutput_video_render_frame(context, frame);
}

flexoutput_video_frame_t *flexoutput_video_create_frame(uint32_t width,
                                                         uint32_t height,
                                                         enum gs_color_format format)
{
    if (width == 0 || height == 0) {
        return NULL;
    }

    flexoutput_video_frame_t *frame = bzalloc(sizeof(flexoutput_video_frame_t));
    frame->width = width;
    frame->height = height;
    frame->format = gs_color_format_to_video_format(format);
    frame->size = flexoutput_video_calculate_frame_size(width, height, format);
    frame->linesize = width * flexoutput_video_get_bytes_per_pixel(format);
    frame->data = bmalloc(frame->size);
    frame->timestamp = os_gettime_ns();

    return frame;
}

void flexoutput_video_free_frame(flexoutput_video_frame_t *frame)
{
    if (!frame) {
        return;
    }

    FLEXOUTPUT_SAFE_FREE(frame->data);
    bfree(frame);
}

flexoutput_error_t flexoutput_video_copy_frame(flexoutput_video_frame_t *dst,
                                                const flexoutput_video_frame_t *src)
{
    if (!dst || !src || !src->data) {
        return FLEXOUTPUT_ERROR_INVALID_PARAM;
    }

    // Allocate destination data if needed
    if (!dst->data || dst->size < src->size) {
        FLEXOUTPUT_SAFE_FREE(dst->data);
        dst->data = bmalloc(src->size);
        dst->size = src->size;
    }

    // Copy frame properties
    dst->width = src->width;
    dst->height = src->height;
    dst->format = src->format;
    dst->linesize = src->linesize;
    dst->timestamp = src->timestamp;

    // Copy frame data
    memcpy(dst->data, src->data, src->size);

    return FLEXOUTPUT_SUCCESS;
}

enum gs_color_format flexoutput_video_get_optimal_format(const flexoutput_output_config_t *config)
{
    UNUSED_PARAMETER(config);
    
    // Default to BGRA for compatibility
    return GS_BGRA;
}

bool flexoutput_video_is_format_supported(enum gs_color_format format)
{
    switch (format) {
    case GS_BGRA:
    case GS_BGRX:
    case GS_RGBA:
        return true;
    default:
        return false;
    }
}

const char *flexoutput_video_get_format_string(enum gs_color_format format)
{
    switch (format) {
    case GS_BGRA: return "BGRA";
    case GS_BGRX: return "BGRX";
    case GS_RGBA: return "RGBA";
    default: return "Unknown";
    }
}

size_t flexoutput_video_calculate_frame_size(uint32_t width,
                                              uint32_t height,
                                              enum gs_color_format format)
{
    return width * height * flexoutput_video_get_bytes_per_pixel(format);
}

uint32_t flexoutput_video_get_bytes_per_pixel(enum gs_color_format format)
{
    switch (format) {
    case GS_BGRA:
    case GS_BGRX:
    case GS_RGBA:
        return 4;
    default:
        return 4; // Default to 4 bytes
    }
}

static enum video_format gs_color_format_to_video_format(enum gs_color_format gs_format)
{
    switch (gs_format) {
    case GS_BGRA:
        return VIDEO_FORMAT_BGRA;
    case GS_BGRX:
        return VIDEO_FORMAT_BGRX;
    case GS_RGBA:
        return VIDEO_FORMAT_RGBA;
    case GS_R8:
        return VIDEO_FORMAT_Y800;
    case GS_RG16:
        return VIDEO_FORMAT_YVYU;
    case GS_RGBA16:
        return VIDEO_FORMAT_RGBA;
    case GS_R16:
        return VIDEO_FORMAT_Y800;
    case GS_RGBA16F:
        return VIDEO_FORMAT_RGBA;
    case GS_RGBA32F:
        return VIDEO_FORMAT_RGBA;
    case GS_RG32F:
        return VIDEO_FORMAT_YVYU;
    case GS_R32F:
        return VIDEO_FORMAT_Y800;
    default:
        return VIDEO_FORMAT_BGRA; // Default fallback
    }
}

// Helper functions
static void cleanup_context_sources(video_render_context_t *context)
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
}

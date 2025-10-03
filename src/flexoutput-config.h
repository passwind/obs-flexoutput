#pragma once

#include "flexoutput-types.h"

#ifdef __cplusplus
extern "C" {
#endif

// Configuration management functions

/**
 * Initialize configuration system
 * @return FLEXOUTPUT_SUCCESS on success, error code on failure
 */
flexoutput_error_t flexoutput_config_init(void);

/**
 * Cleanup configuration system
 */
void flexoutput_config_cleanup(void);

/**
 * Load configuration from file
 * @return FLEXOUTPUT_SUCCESS on success, error code on failure
 */
flexoutput_error_t flexoutput_config_load(void);

/**
 * Save configuration to file
 * @return FLEXOUTPUT_SUCCESS on success, error code on failure
 */
flexoutput_error_t flexoutput_config_save(void);

/**
 * Get configuration file path
 * @return Configuration file path (caller should not free)
 */
const char *flexoutput_config_get_file_path(void);

/**
 * Set configuration file path
 * @param path New configuration file path
 * @return FLEXOUTPUT_SUCCESS on success, error code on failure
 */
flexoutput_error_t flexoutput_config_set_file_path(const char *path);

/**
 * Get plugin enabled state
 * @return true if enabled, false otherwise
 */
bool flexoutput_config_get_enabled(void);

/**
 * Set plugin enabled state
 * @param enabled New enabled state
 */
void flexoutput_config_set_enabled(bool enabled);

/**
 * Get output configuration count
 * @return Number of configured outputs
 */
size_t flexoutput_config_get_output_count(void);

/**
 * Get output configuration by index
 * @param index Output index
 * @return Output configuration or NULL if not found
 */
flexoutput_output_config_t *flexoutput_config_get_output(size_t index);

/**
 * Get output configuration by name
 * @param name Output name
 * @return Output configuration or NULL if not found
 */
flexoutput_output_config_t *flexoutput_config_get_output_by_name(const char *name);

/**
 * Add output configuration
 * @param config Output configuration to add
 * @return FLEXOUTPUT_SUCCESS on success, error code on failure
 */
flexoutput_error_t flexoutput_config_add_output(const flexoutput_output_config_t *config);

/**
 * Remove output configuration
 * @param name Output name to remove
 * @return FLEXOUTPUT_SUCCESS on success, error code on failure
 */
flexoutput_error_t flexoutput_config_remove_output(const char *name);

/**
 * Update output configuration
 * @param name Output name to update
 * @param config New configuration
 * @return FLEXOUTPUT_SUCCESS on success, error code on failure
 */
flexoutput_error_t flexoutput_config_update_output(const char *name, const flexoutput_output_config_t *config);

/**
 * Get mapping rule count
 * @return Number of mapping rules
 */
size_t flexoutput_config_get_mapping_count(void);

/**
 * Get mapping rule by index
 * @param index Mapping rule index
 * @return Mapping rule or NULL if not found
 */
flexoutput_mapping_rule_t *flexoutput_config_get_mapping(size_t index);

/**
 * Get mapping rule by output name
 * @param output_name Output name
 * @return Mapping rule or NULL if not found
 */
flexoutput_mapping_rule_t *flexoutput_config_get_mapping_by_output(const char *output_name);

/**
 * Add mapping rule
 * @param rule Mapping rule to add
 * @return FLEXOUTPUT_SUCCESS on success, error code on failure
 */
flexoutput_error_t flexoutput_config_add_mapping(const flexoutput_mapping_rule_t *rule);

/**
 * Remove mapping rule
 * @param output_name Output name
 * @return FLEXOUTPUT_SUCCESS on success, error code on failure
 */
flexoutput_error_t flexoutput_config_remove_mapping(const char *output_name);

/**
 * Update mapping rule
 * @param output_name Output name
 * @param rule New mapping rule
 * @return FLEXOUTPUT_SUCCESS on success, error code on failure
 */
flexoutput_error_t flexoutput_config_update_mapping(const char *output_name, const flexoutput_mapping_rule_t *rule);

/**
 * Reset configuration to defaults
 */
void flexoutput_config_reset_to_defaults(void);

/**
 * Validate configuration
 * @return FLEXOUTPUT_SUCCESS if valid, error code if invalid
 */
flexoutput_error_t flexoutput_config_validate(void);

/**
 * Export configuration to JSON string
 * @return JSON string (caller must free) or NULL on error
 */
char *flexoutput_config_export_json(void);

/**
 * Import configuration from JSON string
 * @param json_str JSON string to import
 * @return FLEXOUTPUT_SUCCESS on success, error code on failure
 */
flexoutput_error_t flexoutput_config_import_json(const char *json_str);

// Helper functions for configuration data structures

/**
 * Create output configuration
 * @param name Output name
 * @param width Video width
 * @param height Video height
 * @param fps_num FPS numerator
 * @param fps_den FPS denominator
 * @return New output configuration (caller must free)
 */
flexoutput_output_config_t *flexoutput_config_create_output_config(
    const char *name, uint32_t width, uint32_t height, 
    uint32_t fps_num, uint32_t fps_den);

/**
 * Free output configuration
 * @param config Configuration to free
 */
void flexoutput_config_free_output_config(flexoutput_output_config_t *config);

/**
 * Copy output configuration
 * @param src Source configuration
 * @return New configuration copy (caller must free)
 */
flexoutput_output_config_t *flexoutput_config_copy_output_config(const flexoutput_output_config_t *src);

/**
 * Create mapping rule
 * @param output_name Output name
 * @param mode Mapping mode (whitelist/blacklist)
 * @return New mapping rule (caller must free)
 */
flexoutput_mapping_rule_t *flexoutput_config_create_mapping_rule(
    const char *output_name, flexoutput_mapping_type_t mode);

/**
 * Free mapping rule
 * @param rule Rule to free
 */
void flexoutput_config_free_mapping_rule(flexoutput_mapping_rule_t *rule);

/**
 * Copy mapping rule
 * @param src Source rule
 * @return New rule copy (caller must free)
 */
flexoutput_mapping_rule_t *flexoutput_config_copy_mapping_rule(const flexoutput_mapping_rule_t *src);

/**
 * Add source to mapping rule
 * @param rule Mapping rule
 * @param source_name Source name to add
 * @return FLEXOUTPUT_SUCCESS on success, error code on failure
 */
flexoutput_error_t flexoutput_config_mapping_add_source(
    flexoutput_mapping_rule_t *rule, const char *source_name);

/**
 * Remove source from mapping rule
 * @param rule Mapping rule
 * @param source_name Source name to remove
 * @return FLEXOUTPUT_SUCCESS on success, error code on failure
 */
flexoutput_error_t flexoutput_config_mapping_remove_source(
    flexoutput_mapping_rule_t *rule, const char *source_name);

/**
 * Check if source is in mapping rule
 * @param rule Mapping rule
 * @param source_name Source name to check
 * @return true if source is in rule, false otherwise
 */
bool flexoutput_config_mapping_has_source(
    const flexoutput_mapping_rule_t *rule, const char *source_name);

#ifdef __cplusplus
}
#endif

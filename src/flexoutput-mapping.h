#pragma once

#include "flexoutput-types.h"
#include <obs.h>

#ifdef __cplusplus
extern "C" {
#endif

// Mapping Rule Engine API
// Handles source-to-output mapping logic with whitelist/blacklist support

/**
 * Initialize the mapping rule engine
 * @return Error code
 */
flexoutput_error_t flexoutput_mapping_init(void);

/**
 * Cleanup the mapping rule engine
 */
void flexoutput_mapping_cleanup(void);

/**
 * Create a new mapping rule
 * @param output_name Output name for this rule
 * @param type Mapping rule type (whitelist or blacklist)
 * @return New mapping rule or NULL on failure
 */
flexoutput_mapping_rule_t *flexoutput_mapping_create_rule(const char *output_name,
                                                           flexoutput_mapping_type_t type);

/**
 * Destroy a mapping rule
 * @param rule Mapping rule to destroy
 */
void flexoutput_mapping_destroy_rule(flexoutput_mapping_rule_t *rule);

/**
 * Copy a mapping rule
 * @param src Source mapping rule
 * @return New mapping rule copy (caller must free)
 */
flexoutput_mapping_rule_t *flexoutput_mapping_copy_rule(const flexoutput_mapping_rule_t *src);

/**
 * Add source to mapping rule
 * @param rule Mapping rule
 * @param source_name Source name to add
 * @return Error code
 */
flexoutput_error_t flexoutput_mapping_add_source(flexoutput_mapping_rule_t *rule,
                                                  const char *source_name);

/**
 * Remove source from mapping rule
 * @param rule Mapping rule
 * @param source_name Source name to remove
 * @return Error code
 */
flexoutput_error_t flexoutput_mapping_remove_source(flexoutput_mapping_rule_t *rule,
                                                     const char *source_name);

/**
 * Clear all sources from mapping rule
 * @param rule Mapping rule
 * @return Error code
 */
flexoutput_error_t flexoutput_mapping_clear_sources(flexoutput_mapping_rule_t *rule);

/**
 * Check if source is in mapping rule
 * @param rule Mapping rule
 * @param source_name Source name to check
 * @return True if source is in rule
 */
bool flexoutput_mapping_has_source(const flexoutput_mapping_rule_t *rule,
                                    const char *source_name);

/**
 * Check if source should be included in output based on mapping rule
 * @param rule Mapping rule
 * @param source_name Source name to check
 * @return True if source should be included
 */
bool flexoutput_mapping_should_include_source(const flexoutput_mapping_rule_t *rule,
                                               const char *source_name);

/**
 * Get all sources that should be included for an output
 * @param rule Mapping rule
 * @param included_sources Array to store included source names
 * @param count Number of included sources
 * @return Error code
 */
flexoutput_error_t flexoutput_mapping_get_included_sources(const flexoutput_mapping_rule_t *rule,
                                                            char ***included_sources,
                                                            size_t *count);

/**
 * Get all sources that should be excluded for an output
 * @param rule Mapping rule
 * @param excluded_sources Array to store excluded source names
 * @param count Number of excluded sources
 * @return Error code
 */
flexoutput_error_t flexoutput_mapping_get_excluded_sources(const flexoutput_mapping_rule_t *rule,
                                                            char ***excluded_sources,
                                                            size_t *count);

/**
 * Free string array returned by get_*_sources functions
 * @param array String array to free
 * @param count Number of strings
 */
void flexoutput_mapping_free_string_array(char **array, size_t count);

// Mapping Rule Validation

/**
 * Validate mapping rule
 * @param rule Mapping rule to validate
 * @return Error code
 */
flexoutput_error_t flexoutput_mapping_validate_rule(const flexoutput_mapping_rule_t *rule);

/**
 * Check if mapping rule is empty (no sources)
 * @param rule Mapping rule
 * @return True if empty
 */
bool flexoutput_mapping_is_rule_empty(const flexoutput_mapping_rule_t *rule);

/**
 * Get number of sources in mapping rule
 * @param rule Mapping rule
 * @return Number of sources
 */
size_t flexoutput_mapping_get_source_count(const flexoutput_mapping_rule_t *rule);

// Mapping Rule Serialization

/**
 * Save mapping rule to obs_data
 * @param rule Mapping rule to save
 * @return obs_data object (caller must release)
 */
obs_data_t *flexoutput_mapping_save_rule(const flexoutput_mapping_rule_t *rule);

/**
 * Load mapping rule from obs_data
 * @param data obs_data object containing rule data
 * @return New mapping rule (caller must free)
 */
flexoutput_mapping_rule_t *flexoutput_mapping_load_rule(obs_data_t *data);

// Advanced Mapping Operations

/**
 * Merge two mapping rules
 * @param rule1 First mapping rule
 * @param rule2 Second mapping rule
 * @param merge_type How to merge (union, intersection, etc.)
 * @return New merged mapping rule (caller must free)
 */
flexoutput_mapping_rule_t *flexoutput_mapping_merge_rules(const flexoutput_mapping_rule_t *rule1,
                                                           const flexoutput_mapping_rule_t *rule2,
                                                           flexoutput_mapping_merge_type_t merge_type);

/**
 * Invert mapping rule (whitelist becomes blacklist and vice versa)
 * @param rule Mapping rule to invert
 * @return New inverted mapping rule (caller must free)
 */
flexoutput_mapping_rule_t *flexoutput_mapping_invert_rule(const flexoutput_mapping_rule_t *rule);

/**
 * Filter mapping rule based on source criteria
 * @param rule Original mapping rule
 * @param filter_func Filter function
 * @param filter_data User data for filter function
 * @return New filtered mapping rule (caller must free)
 */
flexoutput_mapping_rule_t *flexoutput_mapping_filter_rule(const flexoutput_mapping_rule_t *rule,
                                                           bool (*filter_func)(const char *source_name, void *data),
                                                           void *filter_data);

// Source Type Filtering

/**
 * Add source type filter to mapping rule
 * @param rule Mapping rule
 * @param source_type Source type to filter (e.g., "window_capture", "dshow_input")
 * @param include True to include this type, false to exclude
 * @return Error code
 */
flexoutput_error_t flexoutput_mapping_add_source_type_filter(flexoutput_mapping_rule_t *rule,
                                                              const char *source_type,
                                                              bool include);

/**
 * Remove source type filter from mapping rule
 * @param rule Mapping rule
 * @param source_type Source type to remove from filter
 * @return Error code
 */
flexoutput_error_t flexoutput_mapping_remove_source_type_filter(flexoutput_mapping_rule_t *rule,
                                                                 const char *source_type);

/**
 * Clear all source type filters from mapping rule
 * @param rule Mapping rule
 * @return Error code
 */
flexoutput_error_t flexoutput_mapping_clear_source_type_filters(flexoutput_mapping_rule_t *rule);

/**
 * Check if source type should be included based on type filters
 * @param rule Mapping rule
 * @param source_type Source type to check
 * @return True if source type should be included
 */
bool flexoutput_mapping_should_include_source_type(const flexoutput_mapping_rule_t *rule,
                                                    const char *source_type);

// Dynamic Source Management

/**
 * Update mapping rule based on current available sources
 * @param rule Mapping rule to update
 * @param auto_add_new True to automatically add new sources (for whitelist mode)
 * @param auto_remove_missing True to automatically remove missing sources
 * @return Error code
 */
flexoutput_error_t flexoutput_mapping_update_from_sources(flexoutput_mapping_rule_t *rule,
                                                           bool auto_add_new,
                                                           bool auto_remove_missing);

/**
 * Set automatic source management for mapping rule
 * @param rule Mapping rule
 * @param auto_manage True to enable automatic management
 * @param auto_add_new True to automatically add new sources
 * @param auto_remove_missing True to automatically remove missing sources
 * @return Error code
 */
flexoutput_error_t flexoutput_mapping_set_auto_management(flexoutput_mapping_rule_t *rule,
                                                           bool auto_manage,
                                                           bool auto_add_new,
                                                           bool auto_remove_missing);

/**
 * Check if automatic source management is enabled
 * @param rule Mapping rule
 * @return True if automatic management is enabled
 */
bool flexoutput_mapping_is_auto_management_enabled(const flexoutput_mapping_rule_t *rule);

// Mapping Rule Events

/**
 * Set mapping rule change callback
 * @param rule Mapping rule
 * @param callback Callback function
 * @param data User data for callback
 * @return Error code
 */
flexoutput_error_t flexoutput_mapping_set_change_callback(flexoutput_mapping_rule_t *rule,
                                                           void (*callback)(const flexoutput_mapping_rule_t *rule, void *data),
                                                           void *data);

// Utility Functions

/**
 * Get mapping type string
 * @param type Mapping type
 * @return Type string
 */
const char *flexoutput_mapping_get_type_string(flexoutput_mapping_type_t type);

/**
 * Parse mapping type from string
 * @param type_str Type string
 * @return Mapping type
 */
flexoutput_mapping_type_t flexoutput_mapping_parse_type_string(const char *type_str);

/**
 * Get mapping merge type string
 * @param merge_type Merge type
 * @return Merge type string
 */
const char *flexoutput_mapping_get_merge_type_string(flexoutput_mapping_merge_type_t merge_type);

/**
 * Parse mapping merge type from string
 * @param merge_type_str Merge type string
 * @return Mapping merge type
 */
flexoutput_mapping_merge_type_t flexoutput_mapping_parse_merge_type_string(const char *merge_type_str);

/**
 * Print mapping rule information (for debugging)
 * @param rule Mapping rule
 * @param buffer Buffer to write to
 * @param buffer_size Size of buffer
 * @return Number of characters written
 */
size_t flexoutput_mapping_print_rule_info(const flexoutput_mapping_rule_t *rule,
                                           char *buffer,
                                           size_t buffer_size);

// Batch Operations

/**
 * Apply mapping rule to multiple sources at once
 * @param rule Mapping rule
 * @param source_names Array of source names
 * @param source_count Number of sources
 * @param operation Operation to perform (add or remove)
 * @return Error code
 */
flexoutput_error_t flexoutput_mapping_batch_operation(flexoutput_mapping_rule_t *rule,
                                                       const char **source_names,
                                                       size_t source_count,
                                                       flexoutput_mapping_operation_t operation);

/**
 * Import sources from another mapping rule
 * @param dst_rule Destination mapping rule
 * @param src_rule Source mapping rule
 * @param operation How to import (replace, merge, etc.)
 * @return Error code
 */
flexoutput_error_t flexoutput_mapping_import_sources(flexoutput_mapping_rule_t *dst_rule,
                                                      const flexoutput_mapping_rule_t *src_rule,
                                                      flexoutput_mapping_import_operation_t operation);

// Performance and Statistics

/**
 * Get mapping rule statistics
 * @param rule Mapping rule
 * @param stats Statistics structure to fill
 * @return Error code
 */
flexoutput_error_t flexoutput_mapping_get_rule_stats(const flexoutput_mapping_rule_t *rule,
                                                      flexoutput_mapping_stats_t *stats);

/**
 * Reset mapping rule statistics
 * @param rule Mapping rule
 * @return Error code
 */
flexoutput_error_t flexoutput_mapping_reset_rule_stats(flexoutput_mapping_rule_t *rule);

#ifdef __cplusplus
}
#endif

#pragma once

#include <stdlib.h>
#include <stdio.h>

#include "common.h"

typedef struct bijson_array_analysis {
	// Opaque structure, do not access.
	size_t v[6];
} bijson_array_analysis_t;

typedef struct bijson_object_analysis {
	// Opaque structure, do not access.
	size_t v[10];
} bijson_object_analysis_t;

typedef enum bijson_value_type {
	// Basic bijson types
	bijson_value_type_null,
	bijson_value_type_false,
	bijson_value_type_true,
	bijson_value_type_decimal,
	bijson_value_type_string,
	bijson_value_type_array,
	bijson_value_type_object,

	// Additional JSON compatible convenience types
	bijson_value_type_integer,
	bijson_value_type_iee754_2008_float,
	bijson_value_type_iee754_2008_decimal_float,
	bijson_value_type_iee754_2008_decimal_float_dpd,

	// Outright JSON incompatible types
	bijson_value_type_bytes,
	bijson_value_type_undefined,
	bijson_value_type_snan,
	bijson_value_type_qnan,
	bijson_value_type_inf,
} bijson_value_type_t;

extern bijson_error_t bijson_open_filename(bijson_t *bijson, const char *filename);

extern bijson_error_t bijson_get_value_type(const bijson_t *bijson, bijson_value_type_t *result);

extern bijson_error_t bijson_array_count(const bijson_t *bijson, size_t *result);
extern bijson_error_t bijson_array_get_index(
	const bijson_t *bijson,
	size_t index,
	bijson_t *result
);
extern bijson_error_t bijson_array_analyze(
	const bijson_t *bijson,
	bijson_array_analysis_t *result
);
extern bijson_error_t bijson_analyzed_array_get_index(
	const bijson_array_analysis_t *analysis,
	size_t index,
	bijson_t *result
);
extern bijson_error_t bijson_analyzed_array_count(
	const bijson_array_analysis_t *analysis,
	size_t *result
);

extern bijson_error_t bijson_object_count(const bijson_t *bijson, size_t *result);
extern bijson_error_t bijson_object_get_index(
	const bijson_t *bijson,
	size_t index,
	const void **key_buffer_result,
	size_t *key_size_result,
	bijson_t *value_result
);
extern bijson_error_t bijson_object_analyze(
	const bijson_t *bijson,
	bijson_object_analysis_t *result
);
extern bijson_error_t bijson_analyzed_object_get_index(
	const bijson_object_analysis_t *analysis,
	size_t index,
	const void **key_buffer_result,
	size_t *key_size_result,
	bijson_t *value_result
);
extern bijson_error_t bijson_analyzed_object_get_key(
	const bijson_object_analysis_t *analysis,
	const char *key,
	size_t len,
	bijson_t *result
);
extern bijson_error_t bijson_object_get_key(
	const bijson_t *bijson,
	const char *key,
	size_t len,
	bijson_t *result
);
extern bijson_error_t bijson_analyzed_object_get_key_range(
	const bijson_object_analysis_t *analysis,
	const char *key,
	size_t len,
	size_t *start_index,
	size_t *end_index
);
extern bijson_error_t bijson_object_get_key_range(
	const bijson_t *bijson,
	const char *key,
	size_t len,
	size_t *start_index,
	size_t *end_index
);

extern bijson_error_t bijson_analyzed_object_count(
	const bijson_object_analysis_t *analysis,
	size_t *result
);

extern bijson_error_t bijson_to_json(
	const bijson_t *bijson,
	bijson_output_callback_t callback,
	void *callback_data
);
extern bijson_error_t bijson_to_json_FILE(const bijson_t *bijson, FILE *file);
extern bijson_error_t bijson_to_json_fd(const bijson_t *bijson, int fd);
extern bijson_error_t bijson_to_json_malloc(
	const bijson_t *bijson,
	const void **result_buffer,
	size_t *result_size
);
extern bijson_error_t bijson_to_json_tempfile(
	const bijson_t *bijson,
	const void **result_buffer,
	size_t *result_size
);
extern bijson_error_t bijson_to_json_bytecounter(
	const bijson_t *bijson,
	size_t *result_size
);
extern bijson_error_t bijson_to_json_filename(const bijson_t *bijson, const char *filename);

extern void bijson_free(bijson_t *bijson);
extern void bijson_close(bijson_t *bijson);

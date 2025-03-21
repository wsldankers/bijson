#pragma once

#include <stdlib.h>
#include <stdio.h>

#include "common.h"

typedef struct bijson {
	const void *buffer;
	size_t size;
} bijson_t;


typedef struct bijson_array_analysis {
	// Opaque structure, do not access.
	union { unsigned char *b; size_t l; } v[6];
} bijson_array_analysis_t;

typedef struct bijson_object_analysis {
	// Opaque structure, do not access.
	union { unsigned char *b; size_t l; } v[10];
} bijson_object_analysis_t;

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
	void **result_buffer,
    size_t *result_size
);
extern bijson_error_t bijson_to_json_bytecounter(
	const bijson_t *bijson,
	void **result_buffer,
    size_t *result_size
);

#pragma once

#include <stdlib.h>
#include <stdio.h>

#include "common.h"

typedef struct bijson {
	const void *buffer;
	size_t size;
} bijson_t;

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

#pragma once

#include <stdlib.h>
#include <stdio.h>

#include <bijson/common.h>

typedef struct bijson {
	const void *buffer;
	size_t size;
} bijson_t;

extern bool bijson_to_json(const bijson_t *bijson, bijson_output_callback callback, void *callback_data);
extern bool bijson_to_json_FILE(const bijson_t *bijson, FILE *file);
extern bool bijson_to_json_fd(const bijson_t *bijson, int fd);
extern bool bijson_to_json_malloc(
	const bijson_t *bijson,
	void **result_buffer,
    size_t *result_size
);
extern bool bijson_to_json_bytecounter(
	const bijson_t *bijson,
	void **result_buffer,
    size_t *result_size
);

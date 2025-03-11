#pragma once

#include <stdlib.h>

typedef bool (*bijson_output_callback)(const void *data, size_t len, void *userdata);

typedef struct bijson {
	const void *buffer;
	size_t size;
} bijson_t;

extern bool bijson_to_json(const bijson_t *bijson, bijson_output_callback callback, void *userdata);
extern bool bijson_stdio_output_callback(const void *data, size_t len, void *userdata);

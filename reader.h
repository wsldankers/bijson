#pragma once

#include <stdlib.h>

typedef bool (*bijson_output_callback)(const void *data, size_t len, void *userdata);

typedef struct bijson {
    const void *buffer;
    size_t size;
} bijson_t;

#pragma once

#include "../writer.h"

// Arrays and objects share the same container struct
typedef struct _bijson_container {
	size_t spool_size;
	size_t output_size;
} _bijson_container_t;

static const _bijson_container_t _bijson_container_0 = {0};

__attribute__((pure))
extern size_t _bijson_writer_size_container(bijson_writer_t *writer, size_t spool_offset);

extern void _bijson_container_restore_expect(bijson_writer_t *writer);
__attribute__((pure))
extern size_t _bijson_writer_size_value(bijson_writer_t *writer, size_t spool_offset);

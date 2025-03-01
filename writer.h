#pragma once

#include <stdbool.h>
#include <stdlib.h>

#include "buffer.h"

typedef uint8_t _bijson_spool_type_t;
extern const _bijson_spool_type_t _bijson_spool_type_scalar;
extern const _bijson_spool_type_t _bijson_spool_type_object;
extern const _bijson_spool_type_t _bijson_spool_type_array;

#define _BIJSON_CHECK(ok) do { if(!(ok)) { writer->failed = true; return false; }} while(false)

typedef struct bijson_writer {
	// The spool contains values, each starting with a _bijson_spool_type_t,
	// then a size_t tobject_item denotes the length of the value in its spooled form
	// (not including the type byte or the size field). Everything after tobject_item
	// is type dependent.
	_bijson_buffer_t spool;
	// Stack contains offsets into the spool for both previous and current
	// containers. Also serves as memory space for self-contained
	// computations.
	_bijson_buffer_t stack;
	size_t current_container;
	bool failed;
} bijson_writer_t;

extern const bijson_writer_t _bijson_writer_0;

// If data == NULL: write a zeroed region of length len (or seek, if appropriate).
typedef bool (*_bijson_writer_write_func_t)(void *write_data, const void *data, size_t len);

extern size_t _bijson_writer_size_value(bijson_writer_t *writer, size_t spool_offset);

static inline bool _bijson_writer_write_minimal_int(_bijson_writer_write_func_t write, void *write_data, uint64_t u, size_t nbytes) {
	uint8_t buf[8];
	for(size_t z = 0; z < nbytes; z++) {
		buf[z] = u & UINT8_C(0xFF);
		u >>= 8;
	}
	return write(write_data, buf, nbytes);
}

static inline bool _bijson_writer_write_compact_int(_bijson_writer_write_func_t write, void *write_data, uint64_t u, uint8_t width) {
	return _bijson_writer_write_minimal_int(write, write_data, u, 1 << width);
}

extern bool _bijson_writer_write_value(bijson_writer_t *writer, _bijson_writer_write_func_t write, void *write_data, const char *spool);

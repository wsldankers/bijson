#pragma once

#include <stdbool.h>
#include <stdlib.h>

#include "buffer.h"
#include "common.h"

// These values actually end up on the spool:
typedef enum _bijson_spool_type_t {
	_bijson_spool_type_scalar,
	_bijson_spool_type_object,
	_bijson_spool_type_array,
} _bijson_spool_type_t;

// These values are for use in writer->expect.
// The first three values are valid for writer->expect_after_value.
typedef enum _bijson_writer_expect {
	_bijson_writer_expect_none,
	_bijson_writer_expect_key,
	_bijson_writer_expect_value,
	_bijson_writer_expect_more_key,
	_bijson_writer_expect_more_string,
	_bijson_writer_expect_more_bytes,
	_bijson_writer_expect_more_decimal_string,
} _bijson_writer_expect_t;

// Use in public functions:
#define _BIJSON_WRITER_ERROR_RETURN(x) _BIJSON_ERROR_CLEANUP_AND_RETURN(x, writer->failed = true)

typedef struct bijson_writer {
	// The spool contains values, each starting with a _bijson_spool_type_t,
	// then a size_t tobject_item denotes the length of the value in its spooled form
	// (not including the type byte or the size field). Everything after the size field
	// is type dependent.
	_bijson_buffer_t spool;
	// Stack contains offsets into the spool for both previous and current
	// containers. Also serves as memory space for self-contained
	// computations.
	_bijson_buffer_t stack;
	size_t current_container;
	_bijson_writer_expect_t expect;
	// What to put into `expect` after writing a value.
	// Doubles as a way to see if we're inside an array or object (or neither).
	_bijson_writer_expect_t expect_after_value;
	bool failed;
} bijson_writer_t;

// Check whether the writer is ready to accept a new value:
static inline bijson_error_t _bijson_writer_check_expect_value(bijson_writer_t *writer) {
	return writer->expect == _bijson_writer_expect_value
		? NULL
		: writer->expect == _bijson_writer_expect_key
			? bijson_error_key_expected
			: bijson_error_unmatched_end;
}

// If data == NULL: write a zeroed region of length len (or seek, if appropriate).
typedef bijson_error_t (*_bijson_writer_write_func_t)(void *write_data, const void *data, size_t len);
extern bijson_error_t _bijson_writer_bytecounter_writer(void *write_data, const void *data, size_t len);

static inline bijson_error_t _bijson_writer_write_minimal_int(_bijson_writer_write_func_t write, void *write_data, uint64_t u, size_t nbytes) {
	byte buf[8];
	for(size_t z = 0; z < nbytes; z++) {
		buf[z] = u & BYTE_C(0xFF);
		u >>= 8;
	}
	return write(write_data, buf, nbytes);
}

static inline bijson_error_t _bijson_writer_write_compact_int(_bijson_writer_write_func_t write, void *write_data, uint64_t u, byte width) {
	return _bijson_writer_write_minimal_int(write, write_data, u, 1 << width);
}

extern bijson_error_t _bijson_writer_write_value(bijson_writer_t *writer, _bijson_writer_write_func_t write, void *write_data, const byte *spool);

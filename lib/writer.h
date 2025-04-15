#pragma once

#include <stdbool.h>
#include <stdlib.h>

#include "../include/writer.h"

#include "common.h"
#include "io.h"
#include "writer/buffer.h"

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

struct bijson_writer {
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
};

static inline byte_compute_t _bijson_optimal_storage_size(uint64_t len) {
	if(len > UINT64_C(65535))
		return len > UINT64_C(4294967295) ? BYTE_C(3) : BYTE_C(2);
	else
		return len > UINT64_C(255) ? BYTE_C(1) : BYTE_C(0);
}

static inline byte_compute_t _bijson_optimal_storage_size1(uint64_t len) {
	if(len > UINT64_C(65536))
		return len > UINT64_C(4294967296) ? BYTE_C(3) : BYTE_C(2);
	else
		return len > UINT64_C(256) ? BYTE_C(1) : BYTE_C(0);
}

static inline size_t _bijson_optimal_storage_size_bytes(byte_compute_t storage_size) {
	return (size_t)1 << storage_size;
}

static inline size_t _bijson_fit_uint64(uint64_t value) {
	return value > UINT64_C(0xFFFFFFFFF)
		? value > UINT64_C(0xFFFFFFFFFFFFF)
			? value > UINT64_C(0xFFFFFFFFFFFFFFF)
				? 8
				: 7
			: value > UINT64_C(0xFFFFFFFFFF)
				? 6
				: 5
		: value > UINT64_C(0xFFFF)
			? value > UINT64_C(0xFFFFFF)
				? 4
				: 3
			: value > UINT64_C(0xFF)
				? 2
				: 1;
}

// Check whether the writer is ready to accept a new value:
static inline bijson_error_t _bijson_writer_check_expect_value(bijson_writer_t *writer) {
	return writer->expect == _bijson_writer_expect_value
		? NULL
		: writer->expect == _bijson_writer_expect_key
			? bijson_error_key_expected
			: bijson_error_unmatched_end;
}

static inline bijson_error_t _bijson_writer_write_minimal_int(bijson_output_callback_t write, void *write_data, uint64_t u, size_t nbytes) {
	byte_t buf[8];
	for(size_t z = 0; z < nbytes; z++) {
		buf[z] = u & UINT64_C(0xFF);
		u >>= 8U;
	}
	return write(write_data, buf, nbytes);
}

static inline bijson_error_t _bijson_writer_write_compact_int(bijson_output_callback_t write, void *write_data, uint64_t u, unsigned int width) {
	return _bijson_writer_write_minimal_int(write, write_data, u, 1 << width);
}

extern bijson_error_t _bijson_writer_write_value(
	bijson_writer_t *writer,
	bijson_output_callback_t write,
	void *write_data,
	const byte_t *spool
);

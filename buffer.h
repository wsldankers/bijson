#pragma once

#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>

#include <bijson/common.h>

#include "common.h"

typedef struct _bijson_buffer {
	// minibuffer must be the first item for alignment reasons
	uint8_t _minibuffer[sizeof(size_t) * SIZE_C(4)];
	void *_buffer;
	size_t _size;
	size_t used;
	int _fd;
#ifndef NDEBUG
	bool _finalized;
	bool _failed;
#endif
} _bijson_buffer_t;

#define _bijson_buffer_0 ((struct _bijson_buffer){._fd = -1})

static inline const size_t _bijson_buffer_offset(_bijson_buffer_t *buffer, const char *pointer) {
	assert(!buffer->_failed);
	assert(buffer->_finalized);
	return pointer - (const char *)buffer->_buffer;
}

extern void _bijson_buffer_init(_bijson_buffer_t *buffer);
extern void _bijson_buffer_wipe(_bijson_buffer_t *buffer);
extern void _bijson_buffer_read(_bijson_buffer_t *buffer, size_t offset, void *data, size_t len);
extern void *_bijson_buffer_access(_bijson_buffer_t *buffer, size_t offset, size_t len);
extern void _bijson_buffer_write(_bijson_buffer_t *buffer, size_t offset, const void *data, size_t len);
extern const char *_bijson_buffer_finalize(_bijson_buffer_t *buffer);
extern bijson_error_t _bijson_buffer_append(_bijson_buffer_t *buffer, const void *data, size_t len);
extern bijson_error_t _bijson_buffer_push(_bijson_buffer_t *buffer, const void *data, size_t len, void *result);
extern void _bijson_buffer_pop(_bijson_buffer_t *buffer, void *data, size_t len);

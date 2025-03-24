#pragma once

#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include <bijson/common.h>

#include "common.h"

typedef struct _bijson_buffer {
	// minibuffer must be the first item for alignment reasons
	byte _minibuffer[sizeof(size_t) * SIZE_C(4)];
	byte *_buffer;
	size_t _size;
	size_t used;
	int _fd;
#ifndef NDEBUG
	bool _finalized;
	bool _failed;
#endif
} _bijson_buffer_t;

#define _bijson_buffer_0 ((struct _bijson_buffer){._fd = -1})

static inline const size_t _bijson_buffer_offset(_bijson_buffer_t *buffer, const byte *pointer) {
	assert(!buffer->_failed);
	assert(buffer->_finalized);
	return pointer - (const byte *)buffer->_buffer;
}

extern void _bijson_buffer_init(_bijson_buffer_t *buffer);
extern void _bijson_buffer_wipe(_bijson_buffer_t *buffer);
extern const byte *_bijson_buffer_finalize(_bijson_buffer_t *buffer);
extern bijson_error_t _bijson_buffer_ensure_space(_bijson_buffer_t *buffer, size_t required);

static inline void *_bijson_buffer_access(_bijson_buffer_t *buffer, size_t offset, size_t len) {
	assert(!buffer->_failed);
	assert(offset + len <= buffer->used);
	return buffer->_buffer + offset;
}

static inline void _bijson_buffer_read(_bijson_buffer_t *buffer, size_t offset, void *data, size_t len) {
	assert(!buffer->_failed);
	assert(data);
	const void *offset_buffer = _bijson_buffer_access(buffer, offset, len);
	memcpy(data, offset_buffer, len);
}

static inline byte _bijson_buffer_read_byte(_bijson_buffer_t *buffer, size_t offset) {
	byte byte;
	_bijson_buffer_read(buffer, offset, &byte, sizeof byte);
	return byte;
}

static inline size_t _bijson_buffer_read_size(_bijson_buffer_t *buffer, size_t offset) {
	size_t size;
	_bijson_buffer_read(buffer, offset, &size, sizeof size);
	return size;
}

static inline void _bijson_buffer_write(_bijson_buffer_t *buffer, size_t offset, const void *data, size_t len) {
	assert(!buffer->_failed);
	assert(!buffer->_finalized);
	assert(offset + len <= buffer->used);
	assert(data || !len);
	memcpy(buffer->_buffer + offset, data, len);
}

static inline void _bijson_buffer_write_byte(_bijson_buffer_t *buffer, size_t offset, byte byte) {
	_bijson_buffer_write(buffer, offset, &byte, sizeof byte);
}

static inline void _bijson_buffer_write_size(_bijson_buffer_t *buffer, size_t offset, size_t size) {
	_bijson_buffer_write(buffer, offset, &size, sizeof size);
}

static inline bijson_error_t _bijson_buffer_push(_bijson_buffer_t *buffer, const void *data, size_t len) {
	assert(!buffer->_failed);
	assert(!buffer->_finalized);
	assert(data || !len);
	_BIJSON_ERROR_RETURN(_bijson_buffer_ensure_space(buffer, len));
	size_t old_used = buffer->used;
	memcpy(buffer->_buffer + old_used, data, len);
	buffer->used = old_used + len;
	return NULL;
}

static inline bijson_error_t _bijson_buffer_push_byte(_bijson_buffer_t *buffer, byte byte) {
	return _bijson_buffer_push(buffer, &byte, sizeof byte);
}

static inline bijson_error_t _bijson_buffer_push_size(_bijson_buffer_t *buffer, size_t size) {
	return _bijson_buffer_push(buffer, &size, sizeof size);
}

static inline void _bijson_buffer_pop(_bijson_buffer_t *buffer, void *data, size_t len) {
	assert(!buffer->_failed);
	assert(!buffer->_finalized);
	assert(len <= buffer->used);
	size_t new_used = buffer->used - len;
	if(data)
		_bijson_buffer_read(buffer, new_used, data, len);
	buffer->used = new_used;
}

static inline byte _bijson_buffer_pop_byte(_bijson_buffer_t *buffer) {
	byte byte;
	_bijson_buffer_pop(buffer, &byte, sizeof byte);
	return byte;
}

static inline size_t _bijson_buffer_pop_size(_bijson_buffer_t *buffer) {
	size_t size;
	_bijson_buffer_pop(buffer, &size, sizeof size);
	return size;
}

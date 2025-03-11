#pragma once

#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>

typedef struct _bijson_buffer {
	void *_buffer;
	size_t _size;
	size_t used;
	int _fd;
	bool _finalized;
	bool _failed;
} _bijson_buffer_t;

extern const _bijson_buffer_t _bijson_buffer_0;

static inline const size_t _bijson_buffer_offset(_bijson_buffer_t *buffer, const char *pointer) {
	if(buffer->_failed || !buffer->_finalized)
		return SIZE_MAX;
	return pointer - (const char *)buffer->_buffer;
}

extern bool _bijson_buffer_alloc(_bijson_buffer_t *buffer);
extern void _bijson_buffer_free(_bijson_buffer_t *buffer);
extern bool _bijson_buffer_read(_bijson_buffer_t *buffer, size_t offset, void *data, size_t len);
extern const void *_bijson_buffer_access(_bijson_buffer_t *buffer, size_t offset, size_t len);
extern bool _bijson_buffer_write(_bijson_buffer_t *buffer, size_t offset, const void *data, size_t len);
extern const char *_bijson_buffer_finalize(_bijson_buffer_t *buffer);
extern void *_bijson_buffer_push(_bijson_buffer_t *buffer, const void *data, size_t len);
extern bool _bijson_buffer_pop(_bijson_buffer_t *buffer, void *data, size_t len);

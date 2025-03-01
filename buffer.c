#include "buffer.h"
#include "common.h"

#include <string.h>
#include <stdlib.h>
#include <stdint.h>

const _bijson_buffer_t _bijson_buffer_0 = {0};

bool _bijson_buffer_alloc(_bijson_buffer_t *buffer) {
	*buffer = _bijson_buffer_0;
	buffer->_size = 65536;
	buffer->_buffer = xalloc(buffer->_size);
	return true;
}

void _bijson_buffer_free(_bijson_buffer_t *buffer) {
	xfree(buffer->_buffer);
	*buffer = _bijson_buffer_0;
}

bool _bijson_buffer_read(_bijson_buffer_t *buffer, size_t offset, void *data, size_t len) {
	if(buffer->_failed)
		return false;
	memcpy(data, buffer->_buffer + offset, len);
	return true;
}

bool _bijson_buffer_write(_bijson_buffer_t *buffer, size_t offset, const void *data, size_t len) {
	if(buffer->_failed || buffer->_finalized)
		return false;
	if(data)
		memcpy(buffer->_buffer + offset, data, len);
	else
		memset(buffer->_buffer + offset, '\0', len);
	return true;
}

const char *_bijson_buffer_finalize(_bijson_buffer_t *buffer) {
	if(buffer->_failed)
		return NULL;
	buffer->_finalized = true;
	return buffer->_buffer;
}

void *_bijson_buffer_push(_bijson_buffer_t *buffer, const void *data, size_t len) {
	if(buffer->_failed || buffer->_finalized)
		return false;
	size_t used = buffer->used;
	if(data && !_bijson_buffer_write(buffer, used, data, len))
		return NULL;
	buffer->used = used + len;
	return buffer->_buffer + used;
}

bool _bijson_buffer_pop(_bijson_buffer_t *buffer, void *data, size_t len) {
	if(buffer->_failed || buffer->_finalized)
		return false;
	buffer->used -= len;
	if(data && !_bijson_buffer_read(buffer, buffer->used, data, len))
		return false;
	return true;
}

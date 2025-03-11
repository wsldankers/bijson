#undef _GNU_SOURCE
#define _GNU_SOURCE

#include "buffer.h"
#include "common.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>

const _bijson_buffer_t _bijson_buffer_0 = {._fd = -1};

#define _BIJSON_SMALL_BUFFER SIZE_C(4096)
#define _BIJSON_MEDIUM_BUFFER SIZE_C(65536)
#define _BIJSON_LARGE_BUFFER SIZE_C(1048576)

#define _BIJSON_MAX_BUFFER_EXTENSION SIZE_C(16777216)

bool _bijson_buffer_alloc(_bijson_buffer_t *buffer) {
	*buffer = _bijson_buffer_0;
	buffer->_size = _BIJSON_SMALL_BUFFER;
	buffer->_buffer = malloc(buffer->_size);
	return buffer->_buffer;
}

static bool _bijson_buffer_ensure_space(_bijson_buffer_t *buffer, size_t required) {
	if(buffer->_failed)
		return false;

	size_t old_size = buffer->_size;
	if(required <= old_size)
		return true;

	int fd = buffer->_fd;
	bool was_malloced = fd == -1;

	if(was_malloced && required <= _BIJSON_LARGE_BUFFER) {
		size_t new_size = required < _BIJSON_MEDIUM_BUFFER
			? _BIJSON_MEDIUM_BUFFER
			: _BIJSON_LARGE_BUFFER;
		void *new_buffer = realloc(buffer->_buffer, new_size);
		if(!new_buffer) {
			buffer->_failed = true;
			return false;
		}
		buffer->_size = new_size;
		buffer->_buffer = new_buffer;
	} else {
		size_t new_size;
		if(required > _BIJSON_MAX_BUFFER_EXTENSION) {
			// round to next multiple of _BIJSON_MAX_BUFFER_EXTENSION
			new_size = (required - SIZE_C(1) + _BIJSON_MAX_BUFFER_EXTENSION) / _BIJSON_MAX_BUFFER_EXTENSION * _BIJSON_MAX_BUFFER_EXTENSION;
		} else {
			new_size = old_size << 1;
			while(new_size < required)
				new_size <<= 1;
		}
		if(was_malloced) {
			const char *tmpdir = getenv("TMPDIR");
			if(!tmpdir)
				tmpdir = "/tmp";
			fd = open(tmpdir, O_RDWR|O_TMPFILE|O_CLOEXEC, 0600);
			if(fd == -1) {
				buffer->_failed = true;
				return false;
			}
			buffer->_fd = fd;
			if(posix_fallocate(fd, 0, new_size) == -1) {
				close(fd);
				buffer->_failed = true;
				return false;
			}
		} else {
			if(posix_fallocate(fd, old_size, new_size - old_size) == -1) {
				close(fd);
				buffer->_failed = true;
				return false;
			}
		}
		void *new_buffer = was_malloced
			? mmap(NULL, new_size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0)
			: mremap(buffer->_buffer, old_size, new_size, MREMAP_MAYMOVE, fd, 0);

		if(new_buffer == MAP_FAILED) {
			close(fd);
			buffer->_failed = true;
			return false;
		}

		if(was_malloced) {
			memcpy(new_buffer, buffer->_buffer, buffer->used);
			free(buffer->_buffer);
		}

		buffer->_fd = fd;
		buffer->_buffer = new_buffer;
		buffer->_size = new_size;
	}

	return true;
}

void _bijson_buffer_free(_bijson_buffer_t *buffer) {
	int fd = buffer->_fd;
	if(fd == -1) {
		free(buffer->_buffer);
	} else {
		close(fd);
		munmap(buffer->_buffer, buffer->_size);
	}
#ifndef NDEBUG
	*buffer = _bijson_buffer_0;
#endif
}

const void *_bijson_buffer_access(_bijson_buffer_t *buffer, size_t offset, size_t len) {
	// This function may fail because the underlying implementation does not use mmap().
	// Always have a fallback that uses _bijson_buffer_read().
	if(buffer->_failed)
		return NULL;
	// fprintf(stderr, "offset=%zu len=%zu used=%zu\n", offset, len, buffer->used);
	assert(offset + len <= buffer->used);
	if(offset + len > buffer->used)
		return NULL;
	return buffer->_buffer + offset;
}

bool _bijson_buffer_read(_bijson_buffer_t *buffer, size_t offset, void *data, size_t len) {
	assert(data);
	if(!data)
		return false;
	const void *offset_buffer = _bijson_buffer_access(buffer, offset, len);
	if(!offset_buffer)
		return false;
	memcpy(data, offset_buffer, len);
	return true;
}

bool _bijson_buffer_write(_bijson_buffer_t *buffer, size_t offset, const void *data, size_t len) {
	if(buffer->_failed || buffer->_finalized)
		return false;
	if(offset + len > buffer->used)
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
	size_t old_used = buffer->used;
	size_t new_used = old_used + len;
	if(!_bijson_buffer_ensure_space(buffer, new_used))
		return NULL;
	if(data)
		memcpy(buffer->_buffer + old_used, data, len);
	buffer->used = new_used;
	return buffer->_buffer + old_used;
}

bool _bijson_buffer_pop(_bijson_buffer_t *buffer, void *data, size_t len) {
	if(buffer->_failed || buffer->_finalized)
		return false;
	size_t new_used = buffer->used - len;
	if(data && !_bijson_buffer_read(buffer, new_used, data, len))
		return false;
	buffer->used = new_used;
	return true;
}

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

#define _BIJSON_SMALL_BUFFER SIZE_C(4096)
#define _BIJSON_MEDIUM_BUFFER SIZE_C(65536)
#define _BIJSON_LARGE_BUFFER SIZE_C(1048576)

#define _BIJSON_MAX_BUFFER_EXTENSION SIZE_C(134217728)

void _bijson_buffer_init(_bijson_buffer_t *buffer) {
	*buffer = _bijson_buffer_0;
	buffer->_size = sizeof buffer->_minibuffer;
	buffer->_buffer = buffer->_minibuffer;
}

void _bijson_buffer_wipe(_bijson_buffer_t *buffer) {
	int fd = buffer->_fd;
	if(fd == -1) {
		if(buffer->_buffer != buffer->_minibuffer)
			free(buffer->_buffer);
	} else {
		close(fd);
		munmap(buffer->_buffer, buffer->_size);
	}
	IF_DEBUG(memset(buffer, 'A', sizeof *buffer));
}

const byte *_bijson_buffer_finalize(_bijson_buffer_t *buffer) {
	assert(!buffer->_failed);
	IF_DEBUG(buffer->_finalized = true);
	return buffer->_buffer;
}

bijson_error_t _bijson_buffer_ensure_space(_bijson_buffer_t *buffer, size_t required) {
	assert(!buffer->_failed);

	size_t used = buffer->used;
	if(SIZE_MAX - required < used)
		return bijson_error_out_of_virtual_memory;
	required += used;

	size_t old_size = buffer->_size;
	if(required <= old_size)
		return NULL;

	int fd = buffer->_fd;
	bool was_malloced = fd == -1;

	if(was_malloced && required <= _BIJSON_LARGE_BUFFER) {
		size_t new_size = required <= _BIJSON_SMALL_BUFFER
			? _BIJSON_SMALL_BUFFER
			: required <= _BIJSON_MEDIUM_BUFFER
				? _BIJSON_MEDIUM_BUFFER
				: _BIJSON_LARGE_BUFFER;

		void *new_buffer = buffer->_buffer == buffer->_minibuffer
			? malloc(new_size)
			: realloc(buffer->_buffer, new_size);
		if(!new_buffer) {
			IF_DEBUG(buffer->_failed = true);
			return bijson_error_system;
		}
		if(buffer->_buffer == buffer->_minibuffer)
			memcpy(new_buffer, buffer->_minibuffer, old_size);
		buffer->_size = new_size;
		buffer->_buffer = new_buffer;
	} else {
		// assert(false);
		size_t new_size;
		if(required > _BIJSON_MAX_BUFFER_EXTENSION) {
			// round to next multiple of _BIJSON_MAX_BUFFER_EXTENSION
			new_size = (required - SIZE_C(1) + _BIJSON_MAX_BUFFER_EXTENSION) / _BIJSON_MAX_BUFFER_EXTENSION * _BIJSON_MAX_BUFFER_EXTENSION;
		} else {
			new_size = old_size << 1U;
			while(new_size < required)
				new_size <<= 1U;
		}
		if(was_malloced) {
			const char *tmpdir = getenv("BIJSON_TMPDIR");
			if(!tmpdir)
				tmpdir = getenv("TMPDIR");
			if(!tmpdir)
				tmpdir = "/tmp";
			fd = open(tmpdir, O_RDWR|O_TMPFILE|O_CLOEXEC, 0600);
			if(fd == -1) {
				IF_DEBUG(buffer->_failed = true);
				return bijson_error_system;
			}
			if(posix_fallocate(fd, 0, new_size) == -1) {
				close(fd);
				IF_DEBUG(buffer->_failed = true);
				return bijson_error_system;
			}
		} else {
			if(posix_fallocate(fd, old_size, new_size - old_size) == -1) {
				IF_DEBUG(buffer->_failed = true);
				return bijson_error_system;
			}
		}
		void *new_buffer = was_malloced
			? mmap(NULL, new_size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0)
			: mremap(buffer->_buffer, old_size, new_size, MREMAP_MAYMOVE, fd, 0);

		if(new_buffer == MAP_FAILED) {
			if(was_malloced)
				close(fd);
			IF_DEBUG(buffer->_failed = true);
			return bijson_error_system;
		}

		if(was_malloced) {
			memcpy(new_buffer, buffer->_buffer, buffer->used);
			if(buffer->_buffer != buffer->_minibuffer)
				free(buffer->_buffer);
			buffer->_fd = fd;
		}

		buffer->_buffer = new_buffer;
		buffer->_size = new_size;
	}

	return NULL;
}

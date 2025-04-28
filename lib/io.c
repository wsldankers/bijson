#undef _GNU_SOURCE
#define _GNU_SOURCE

#include <stdbool.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/uio.h>

#include "io.h"
#include "common.h"

static const byte_t _bijson_nul_bytes[4096];

bijson_error_t _bijson_io_write_nul_bytes(bijson_output_callback_t write, void *write_data, size_t len) {
	while(len) {
		size_t chunk = _bijson_size_min(len, sizeof _bijson_nul_bytes);
		_BIJSON_RETURN_ON_ERROR(write(write_data, _bijson_nul_bytes, chunk));
		len -= chunk;
	}
	return NULL;
}

// Must be a power of two.
#define _BIJSON_WRITE_TO_FD_MAX_BUFFER SIZE_C(1048576)

typedef struct _bijson_buffer_write_to_fd_state {
	byte_t *buffer;
	size_t size;
	size_t fill;
	size_t written;
	int fd;
	bool nonblocking;
} _bijson_buffer_write_to_fd_state_t;

static bijson_error_t _bijson_io_write_to_fd_output_callback(void *write_data, const void *data, size_t len) {
	_bijson_buffer_write_to_fd_state_t state = *(_bijson_buffer_write_to_fd_state_t *)write_data;
	size_t required = state.fill + len;
	if(required <= state.size) {
		memcpy(state.buffer + state.fill, data, len);
		((_bijson_buffer_write_to_fd_state_t *)write_data)->fill = required;
	} else {
		if(required <= _BIJSON_WRITE_TO_FD_MAX_BUFFER) {
			size_t new_size = state.size;
			while(new_size < required)
				new_size <<= 1U;
			byte_t *new_buffer = realloc(state.buffer, new_size);
			if(new_buffer) {
				memcpy(new_buffer + state.fill, data, len);
				((_bijson_buffer_write_to_fd_state_t *)write_data)->buffer = new_buffer;
				((_bijson_buffer_write_to_fd_state_t *)write_data)->size = new_size;
				((_bijson_buffer_write_to_fd_state_t *)write_data)->fill = required;
				return NULL;
			}
		}
		((_bijson_buffer_write_to_fd_state_t *)write_data)->written = state.written + state.fill + len;
		if(state.fill) {
			struct iovec vec[] = {
				{state.buffer, state.fill},
				{_bijson_no_const(data), len},
			};
			for(;;) {
				if(state.nonblocking) {
					struct pollfd poll_fd = {state.fd, POLLOUT};
					int ret = poll(&poll_fd, 1, -1);
					if(ret == -1) {
						if(errno != EINTR)
							_BIJSON_RETURN_ERROR(bijson_error_system);
						continue;
					}
					if(!ret || !poll_fd.revents)
						continue;
					if(poll_fd.revents != POLLOUT)
						_BIJSON_RETURN_ERROR(bijson_error_system);
				}
				size_t written = (size_t)writev(state.fd, vec, 2);
				if(written == SIZE_MAX) {
					if(errno == EWOULDBLOCK || errno == EAGAIN)
						state.nonblocking = ((_bijson_buffer_write_to_fd_state_t *)write_data)->nonblocking = true;
					else if(errno != EINTR)
						_BIJSON_RETURN_ERROR(bijson_error_system);
					continue;
				}
				if(written >= vec[0].iov_len) {
					written -= vec[0].iov_len;
					data = (const byte_t *)data + written;
					len -= written;
					break;
				}
				vec[0].iov_base = (byte_t *)vec[0].iov_base + written;
				vec[0].iov_len -= written;
			}
			((_bijson_buffer_write_to_fd_state_t *)write_data)->fill = SIZE_C(0);
		}
		while(len) {
			if(state.nonblocking) {
				struct pollfd poll_fd = {state.fd, POLLOUT};
				int ret = poll(&poll_fd, 1, -1);
				if(ret == -1) {
					if(errno != EINTR)
						_BIJSON_RETURN_ERROR(bijson_error_system);
					continue;
				}
				if(!ret || !poll_fd.revents)
					continue;
				if(poll_fd.revents != POLLOUT)
					_BIJSON_RETURN_ERROR(bijson_error_system);
			}
			size_t written = (size_t)write(state.fd, data, len);
			if(written == SIZE_MAX) {
				if(errno == EWOULDBLOCK || errno == EAGAIN)
					state.nonblocking = ((_bijson_buffer_write_to_fd_state_t *)write_data)->nonblocking = true;
				else if(errno != EINTR)
					_BIJSON_RETURN_ERROR(bijson_error_system);
				continue;
			}
			len -= written;
			data = (const char *)data + written;
		}
	}

	return NULL;
}

bijson_error_t _bijson_io_write_to_fd(
	_bijson_output_action_callback_t action_callback,
	void *action_callback_data,
	int fd,
	size_t *result_size
) {
	_bijson_buffer_write_to_fd_state_t state = {.fd = fd, .size = SIZE_C(4096)};
	state.buffer = malloc(state.size);
	if(!state.buffer)
		_BIJSON_RETURN_ERROR(bijson_error_system);
	bijson_error_t error = action_callback(action_callback_data, _bijson_io_write_to_fd_output_callback, &state);
	if(!error) {
		byte_t *buffer = state.buffer;
		state.written += state.fill;
		while(state.fill) {
			if(state.nonblocking) {
				struct pollfd poll_fd = {state.fd, POLLOUT};
				int ret = poll(&poll_fd, 1, -1);
				if(ret == -1) {
					if(errno != EINTR) {
						error = bijson_error_system;
						break;
					}
					continue;
				}
				if(!ret || !poll_fd.revents)
					continue;
				if(poll_fd.revents != POLLOUT) {
					error = bijson_error_system;
					break;
				}
			}
			size_t written = (size_t)write(state.fd, buffer, state.fill);
			if(written == SIZE_MAX) {
				if(errno == EWOULDBLOCK || errno == EAGAIN) {
					state.nonblocking = true;
				} else if(errno != EINTR) {
					error = bijson_error_system;
					break;
				}
				continue;
			}
			state.fill -= written;
			buffer += written;
		}
	}
	if(!error && result_size)
		*result_size = state.written;
	free(state.buffer);
	return error;
}

typedef struct _bijson_buffer_write_to_FILE_state {
	FILE *file;
	size_t written;
} _bijson_buffer_write_to_FILE_state_t;

static bijson_error_t _bijson_io_write_to_FILE_output_callback(void *callback_data, const void *data, size_t len) {
	((_bijson_buffer_write_to_FILE_state_t *)callback_data)->written += len;
	FILE *file = ((_bijson_buffer_write_to_FILE_state_t *)callback_data)->file;
	return fwrite(data, SIZE_C(1), len, file) == len
		? NULL
		: bijson_error_system;
}

bijson_error_t _bijson_io_write_to_FILE(
	_bijson_output_action_callback_t action_callback,
	void *action_callback_data,
	FILE *file,
	size_t *result_size
) {
	if(!file)
		_BIJSON_RETURN_ERROR(bijson_error_parameter_is_null);

	_bijson_buffer_write_to_FILE_state_t state = {.file = file};

	bijson_error_t error = action_callback(
		action_callback_data,
		_bijson_io_write_to_FILE_output_callback,
		&state
	);

	if(!error && result_size)
		*result_size = state.written;

	return error;
}

typedef struct _bijson_io_write_to_malloc_state {
	byte_t *buffer;
	size_t size;
	size_t written;
} _bijson_io_write_to_malloc_state_t;

static bijson_error_t _bijson_io_write_to_malloc_output_callback(void *write_data, const void *data, size_t len) {
	_bijson_io_write_to_malloc_state_t state = *(_bijson_io_write_to_malloc_state_t *)write_data;
	if(state.buffer && state.size > state.written) {
		size_t remainder = state.size - state.written;
		memcpy(state.buffer + state.written, data, _bijson_size_min(len, remainder));
	}
	((_bijson_io_write_to_malloc_state_t *)write_data)->written = state.written + len;
	return NULL;
}

bijson_error_t _bijson_io_write_to_malloc(
	_bijson_output_action_callback_t action_callback,
	void *action_callback_data,
	const void **result_buffer,
	size_t *result_size
) {
	_bijson_io_write_to_malloc_state_t state = {.size = 4096};
	state.buffer = malloc(state.size);
	if(!state.buffer)
		_BIJSON_RETURN_ERROR(bijson_error_system);

	_BIJSON_CLEANUP_AND_RETURN_ON_ERROR(action_callback(
		action_callback_data,
		_bijson_io_write_to_malloc_output_callback,
		&state
	), free(state.buffer));

	if(state.written > state.size) {
		free(state.buffer);
		state = (_bijson_io_write_to_malloc_state_t){
			.buffer = malloc(state.written),
			.size = state.written,
		};
		if(!state.buffer)
			_BIJSON_RETURN_ERROR(bijson_error_system);

		_BIJSON_CLEANUP_AND_RETURN_ON_ERROR(action_callback(
			action_callback_data,
			_bijson_io_write_to_malloc_output_callback,
			&state
		), free(state.buffer));
	} else if(state.written != state.size) {
		void *new_buffer = realloc(state.buffer, state.written);
		if(new_buffer)
			state.buffer = new_buffer;
	}

	*result_buffer = state.buffer;
	*result_size = state.written;

	return NULL;
}

bijson_error_t _bijson_io_bytecounter_output_callback(void *write_data, const void *data, size_t len) {
	*(size_t *)write_data += len;
	return NULL;
}

bijson_error_t _bijson_io_write_bytecounter(
	_bijson_output_action_callback_t action_callback,
	void *action_callback_data,
	size_t *result_size
) {
	return action_callback(
		action_callback_data,
		_bijson_io_bytecounter_output_callback,
		result_size
	);
}

bijson_error_t _bijson_io_write_to_filename(
	_bijson_output_action_callback_t action_callback,
	void *action_callback_data,
	const char *filename,
	size_t *result_size
) {
	int fd = open(filename, O_WRONLY|O_CREAT|O_TRUNC|O_NOCTTY|O_CLOEXEC, 0666);
	if(fd == -1)
		_BIJSON_RETURN_ERROR(bijson_error_system);

	bijson_error_t error = _bijson_io_write_to_fd(action_callback, action_callback_data, fd, result_size);

	if(!error && fsync(fd) == -1)
		error = bijson_error_system;

	close(fd);

	return error;
}

bijson_error_t _bijson_io_write_to_tempfile(
	_bijson_output_action_callback_t action_callback,
	void *action_callback_data,
	const void **result_buffer,
	size_t *result_size
) {
	if(!result_buffer || !result_size)
		_BIJSON_RETURN_ERROR(bijson_error_parameter_is_null);

	const char *tmpdir = getenv("BIJSON_TMPDIR");
	if(!tmpdir)
		tmpdir = getenv("TMPDIR");
	if(!tmpdir)
		tmpdir = "/tmp";

	int fd = open(tmpdir, O_RDWR|O_TMPFILE|O_CLOEXEC, 0600);
	if(fd == -1)
		_BIJSON_RETURN_ERROR(bijson_error_system);

	size_t size;
	bijson_error_t error = _bijson_io_write_to_fd(action_callback, action_callback_data, fd, &size);

	if(!error) {
		void *buffer = mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0);
		if(buffer == MAP_FAILED) {
			error = bijson_error_system;
		} else if(!buffer) {
			// mmap() can technically return NULL
			error = bijson_error_unsupported_feature;
			munmap(buffer, size);
		} else {
			*result_buffer = buffer;
			*result_size = size;
		}
	}

	close(fd);

	return error;
}

bijson_error_t _bijson_io_read_from_filename(
	bijson_input_action_t action,
	const void *action_data,
	const char *filename
) {
	if(!filename)
		_BIJSON_RETURN_ERROR(bijson_error_parameter_is_null);
	int fd = open(filename, O_RDONLY|O_NOCTTY|O_CLOEXEC);
	if(fd == -1)
		_BIJSON_RETURN_ERROR(bijson_error_system);
	bijson_error_t error = bijson_error_system;
	void *buffer = MAP_FAILED;
	size_t size = 0;
	do {
		struct stat st;
		if(fstat(fd, &st) == -1)
			break;
		if(st.st_size <= 0) {
			error = bijson_error_file_format_error;
			break;
		}
		if((uintmax_t)st.st_size > (uintmax_t)SIZE_MAX) {
			error = bijson_error_out_of_virtual_memory;
			break;
		}
		size = (size_t)st.st_size;
		buffer = mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0);
	} while(false);
	close(fd);

	do {
		if(buffer == MAP_FAILED)
			break;
		if(!buffer) {
			// mmap() can technically return NULL
			error = bijson_error_unsupported_feature;
		} else if(action) {
			error = action(_bijson_no_const(action_data), buffer, size);
		} else if(action_data) {
			error = NULL;
			bijson_t *bijson = _bijson_no_const(action_data);
			bijson->buffer = buffer;
			bijson->size = size;
			break;
		} else {
			error = bijson_error_parameter_is_null;
		}
		munmap(buffer, size);
	} while(false);

	return error;
}

void _bijson_io_close(bijson_t *bijson) {
	if(!bijson || !bijson->buffer || bijson->buffer == MAP_FAILED || !bijson->size)
		return;
	munmap(_bijson_no_const(bijson->buffer), bijson->size);
	bijson->buffer = NULL;
	bijson->size = 0;
}

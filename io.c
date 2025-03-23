#include <stdbool.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/uio.h>
#include <poll.h>
#include <errno.h>

#include "io.h"
#include "common.h"

typedef struct _bijson_buffer_write_to_fd_state {
	void *buffer;
	size_t size;
	size_t fill;
	int fd;
	bool nonblocking;
} _bijson_buffer_write_to_fd_state_t;

static const char _bijson_nul_bytes[4096];

static bijson_error_t _bijson_io_write_to_fd_output_callback(void *write_data, const void *data, size_t len) {
	_bijson_buffer_write_to_fd_state_t state = *(_bijson_buffer_write_to_fd_state_t *)write_data;
	if(data) {
		if(state.fill + len <= state.size) {
			memcpy(state.buffer + state.fill, data, len);
			((_bijson_buffer_write_to_fd_state_t *)write_data)->fill = state.fill + len;
		} else {
			if(state.fill) {
				struct iovec vec[] = {
					{state.buffer, state.fill},
					{(void *)data, len},
				};
				for(;;) {
					if(state.nonblocking) {
						struct pollfd poll_fd = {state.fd, POLLOUT};
						int ret = poll(&poll_fd, 1, -1);
						if(ret == -1) {
							if(errno != EINTR)
								return bijson_error_system;
							continue;
						}
						if(!ret || !poll_fd.revents)
							continue;
						if(poll_fd.revents != POLLOUT)
							return bijson_error_system;
					}
					ssize_t written = writev(state.fd, vec, 2);
					if(written == (ssize_t)-1) {
						if(errno == EWOULDBLOCK || errno == EAGAIN)
							state.nonblocking = ((_bijson_buffer_write_to_fd_state_t *)write_data)->nonblocking = true;
						else if(errno != EINTR)
							return bijson_error_system;
						continue;
					}
					if(written >= vec[0].iov_len) {
						written -= vec[0].iov_len;
						data = (const char *)data + written;
						len -= written;
						break;
					}
					vec[0].iov_base += written;
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
							return bijson_error_system;
						continue;
					}
					if(!ret || !poll_fd.revents)
						continue;
					if(poll_fd.revents != POLLOUT)
						return bijson_error_system;
				}
				ssize_t written = write(state.fd, data, len);
				if(written == (ssize_t)-1) {
					if(errno == EWOULDBLOCK || errno == EAGAIN)
						state.nonblocking = ((_bijson_buffer_write_to_fd_state_t *)write_data)->nonblocking = true;
					else if(errno != EINTR)
						return bijson_error_system;
					continue;
				}
				len -= written;
				data = (const char *)data + written;
			}
		}
	} else {
		while(len) {
			size_t chunk = _bijson_size_min(len, sizeof _bijson_nul_bytes);
			_BIJSON_ERROR_RETURN(_bijson_io_write_to_fd_output_callback(write_data, _bijson_nul_bytes, chunk));
			len -= chunk;
		}
	}

	return NULL;
}

bijson_error_t _bijson_io_write_to_fd(bijson_output_action_callback_t action_callback, void *action_callback_data, int fd) {
	_bijson_buffer_write_to_fd_state_t state = {.fd = fd, .size = SIZE_C(1048576)};
	state.buffer = malloc(state.size);
	if(!state.buffer)
		return bijson_error_system;
	bijson_error_t error = action_callback(action_callback_data, _bijson_io_write_to_fd_output_callback, &state);
	unsigned char *buffer = state.buffer;
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
		ssize_t written = write(state.fd, buffer, state.fill);
		if(written == (ssize_t)-1) {
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
	free(state.buffer);
	return error;
}

static bijson_error_t _bijson_io_write_to_FILE_output_callback(void *callback_data, const void *data, size_t len) {
	return fwrite(data, sizeof *data, len, callback_data) == len * sizeof *data
		? NULL
		: bijson_error_system;
}

bijson_error_t _bijson_io_write_to_FILE(
	bijson_output_action_callback_t action_callback,
	void *action_callback_data,
	FILE *file
) {
	return action_callback(
		action_callback_data,
		_bijson_io_write_to_FILE_output_callback,
		file
	);
}

typedef struct _bijson_io_write_to_malloc_state {
	void *buffer;
	size_t size;
	size_t written;
} _bijson_io_write_to_malloc_state_t;

static bijson_error_t _bijson_io_write_to_malloc_output_callback(void *write_data, const void *data, size_t len) {
	_bijson_io_write_to_malloc_state_t state = *(_bijson_io_write_to_malloc_state_t *)write_data;
	if(state.buffer && state.size > state.written) {
		size_t remainder = state.size - state.written;
		if(data)
			memcpy(state.buffer + state.written, data, len < remainder ? len : remainder);
		else
			memset(state.buffer + state.written, '\0', len < remainder ? len : remainder);
	}
	((_bijson_io_write_to_malloc_state_t *)write_data)->written = state.written + len;
	return NULL;
}

bijson_error_t _bijson_io_write_to_malloc(
	bijson_output_action_callback_t action_callback,
	void *action_callback_data,
	void **result_buffer,
	size_t *result_size
) {
	_bijson_io_write_to_malloc_state_t state = {.size = 4096};
	state.buffer = malloc(state.size);
	if(!state.buffer)
		return bijson_error_system;

	_BIJSON_ERROR_CLEANUP_AND_RETURN(action_callback(
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
			return bijson_error_system;

		_BIJSON_ERROR_CLEANUP_AND_RETURN(action_callback(
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
	bijson_output_action_callback_t action_callback,
	void *action_callback_data,
	size_t *result_size
) {
	return action_callback(
		action_callback_data,
		_bijson_io_write_to_malloc_output_callback,
		result_size
	);
}

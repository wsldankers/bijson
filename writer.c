#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>

#include <bijson/writer.h>

#include "buffer.h"
#include "common.h"
#include "container.h"
#include "string.h"
#include "writer.h"

const _bijson_spool_type_t _bijson_spool_type_scalar = UINT8_C(0);
const _bijson_spool_type_t _bijson_spool_type_object = UINT8_C(1);
const _bijson_spool_type_t _bijson_spool_type_array = UINT8_C(2);

static const bijson_writer_t _bijson_writer_0 = {{0}};

void bijson_writer_free(bijson_writer_t *writer) {
	_bijson_buffer_free(&writer->spool);
	_bijson_buffer_free(&writer->stack);
	_bijson_xfree(writer);
}

bijson_writer_t *bijson_writer_alloc(void) {
	bijson_writer_t *writer = _bijson_xalloc(sizeof *writer);
	*writer = _bijson_writer_0;
	if(!_bijson_buffer_alloc(&writer->spool))
		return bijson_writer_free(writer), NULL;
	if(!_bijson_buffer_alloc(&writer->stack))
		return bijson_writer_free(writer), NULL;
	return writer;
}

typedef size_t (*_bijson_writer_size_type_func_t)(bijson_writer_t *writer, size_t spool_offset);

static size_t _bijson_writer_size_scalar(bijson_writer_t *writer, size_t spool_offset) {
	size_t spool_size;
	_BIJSON_CHECK(_bijson_buffer_read(&writer->spool, spool_offset, &spool_size, sizeof spool_size));
	return spool_size;
}

static _bijson_writer_size_type_func_t _bijson_writer_typesizers[] = {
	_bijson_writer_size_scalar,
	_bijson_writer_size_container,
	_bijson_writer_size_container,
};

size_t _bijson_writer_size_value(bijson_writer_t *writer, size_t spool_offset) {
	_bijson_spool_type_t spool_type;
	_BIJSON_CHECK_OR_RETURN(_bijson_buffer_read(&writer->spool, spool_offset, &spool_type, sizeof spool_type), SIZE_MAX);
	assert(spool_type < orz(_bijson_writer_typesizers));
	_bijson_writer_size_type_func_t typesizer = _bijson_writer_typesizers[spool_type];
	return typesizer(writer, spool_offset + sizeof spool_type);
}

typedef bool (*_bijson_writer_write_type_func_t)(bijson_writer_t *writer, _bijson_writer_write_func_t write, void *write_data, const char *spool);

static bool _bijson_writer_write_scalar(bijson_writer_t *writer, _bijson_writer_write_func_t write, void *write_data, const char *spool) {
	size_t spool_size;
	memcpy(&spool_size, spool, sizeof spool_size);
	_BIJSON_CHECK(write(write_data, spool + sizeof spool_size, spool_size));
	return true;
}

static _bijson_writer_write_type_func_t _bijson_writer_typewriters[] = {
	_bijson_writer_write_scalar,
	_bijson_writer_write_object,
	_bijson_writer_write_array,
};

bool _bijson_writer_write_value(bijson_writer_t *writer, _bijson_writer_write_func_t write, void *write_data, const char *spool) {
	_bijson_spool_type_t spool_type = *(const _bijson_spool_type_t *)spool;
	_bijson_writer_write_type_func_t typewriter = _bijson_writer_typewriters[spool_type];
	return typewriter(writer, write, write_data, spool + sizeof spool_type);
}

static bool _bijson_writer_write(bijson_writer_t *writer, _bijson_writer_write_func_t write, void *write_data) {
	const char *spool = _bijson_buffer_finalize(&writer->spool);
	_BIJSON_CHECK(spool);
	return _bijson_writer_write_value(writer, write, write_data, spool);
}

bool _bijson_writer_bytecounter_writer(void *write_data, const void *data, size_t len) {
	*(size_t *)write_data += len;
	return true;
}

static const char _bijson_nul_bytes[4096];

static bool _bijson_write_to_fd(void *write_data, const void *data, size_t len) {
	int fd = *(int *)write_data;
	if(data) {
		while(len) {
			ssize_t written = write(fd, data, len);
			if(written < 0)
				return false;
			len -= written;
			data = (const char *)data + written;
		}
	} else {
		while(len) {
			ssize_t written = write(fd, _bijson_nul_bytes, _bijson_size_min(len, sizeof _bijson_nul_bytes));
			if(written < 0)
				return false;
			len -= written;
		}
	}
	return true;
}

bool bijson_writer_write_to_fd(bijson_writer_t *writer, int fd) {
	return _bijson_writer_write(writer, _bijson_write_to_fd, &fd);
}

typedef struct _bijson_buffer_write_state {
	void *buffer;
	size_t size;
	size_t written;
} _bijson_buffer_write_state_t;

static bool _bijson_write_to_buffer(void *write_data, const void *data, size_t len) {
	_bijson_buffer_write_state_t state = *(_bijson_buffer_write_state_t *)write_data;
	if(state.buffer && state.size > state.written) {
		size_t remainder = state.size - state.written;
		memcpy(state.buffer + state.written, data, len < remainder ? len : remainder);
	}
	((_bijson_buffer_write_state_t *)write_data)->written = state.written + len;
	return true;
}

bool bijson_writer_write_to_buffer(bijson_writer_t *writer, void **result_buffer, size_t *result_size) {
	_bijson_buffer_write_state_t state = {.size = 4096};
	state.buffer = malloc(state.size);
	if(!state.buffer)
		return false;

	if(!(_bijson_writer_write(writer, _bijson_write_to_buffer, &state))) {
		free(state.buffer);
		return false;
	}

	if(state.written <= state.size) {
		if(state.written < state.size) {
			void *new_buffer = realloc(state.buffer, state.written);
			if(new_buffer)
				state.buffer = new_buffer;
		}
	} else {
		free(state.buffer);
		state = (_bijson_buffer_write_state_t){
			.buffer = malloc(state.written),
			.size = state.written,
		};
		if(!state.buffer)
			return false;
		if(!(_bijson_writer_write(writer, _bijson_write_to_buffer, &state))) {
			free(state.buffer);
			return false;
		}
	}

	*result_buffer = state.buffer;
	*result_size = state.written;

	return true;
}

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
#include "io.h"

const _bijson_spool_type_t _bijson_spool_type_scalar = UINT8_C(0);
const _bijson_spool_type_t _bijson_spool_type_array = UINT8_C(1);
const _bijson_spool_type_t _bijson_spool_type_object = UINT8_C(2);

static const bijson_writer_t _bijson_writer_0 = {{0}};

void bijson_writer_free(bijson_writer_t *writer) {
	_bijson_buffer_wipe(&writer->spool);
	_bijson_buffer_wipe(&writer->stack);
	_bijson_xfree(writer);
}

bijson_writer_t *bijson_writer_alloc(void) {
	bijson_writer_t *writer = _bijson_xalloc(sizeof *writer);
	*writer = _bijson_writer_0;
	if(_bijson_buffer_init(&writer->spool))
		return bijson_writer_free(writer), NULL;
	if(_bijson_buffer_init(&writer->stack))
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
	_bijson_writer_write_array,
	_bijson_writer_write_object,
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

typedef struct _bijson_writer_write_state {
	bijson_writer_t *writer;
} _bijson_writer_write_state_t;

bool _bijson_writer_write_callback(
	void *action_callback_data,
	bijson_output_callback_t output_callback,
	void *output_callback_data
) {
	return _bijson_writer_write(
		((_bijson_writer_write_state_t *)action_callback_data)->writer,
		output_callback,
		output_callback_data
	);
}

bool bijson_writer_write_to_fd(bijson_writer_t *writer, int fd) {
	_bijson_writer_write_state_t state = {writer};
	return _bijson_io_write_to_fd(_bijson_writer_write_callback, &state, fd);
}

bool bijson_writer_write_to_FILE(bijson_writer_t *writer, FILE *file) {
	_bijson_writer_write_state_t state = {writer};
	return _bijson_io_write_to_FILE(_bijson_writer_write_callback, &state, file);
}

bool bijson_writer_write_to_malloc(
	bijson_writer_t *writer,
	void **result_buffer,
	size_t *result_size
) {
	_bijson_writer_write_state_t state = {writer};
	return _bijson_io_write_to_malloc(
		_bijson_writer_write_callback,
		&state,
		result_buffer,
		result_size
	);
}

bool bijson_writer_write_bytecounter(
	bijson_writer_t *writer,
	void **result_buffer,
	size_t *result_size
) {
	_bijson_writer_write_state_t state = {writer};
	return _bijson_io_write_bytecounter(
		_bijson_writer_write_callback,
		&state,
		result_size
	);
}

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

#define _bijson_writer_0 ((bijson_writer_t){ \
	.spool = _bijson_buffer_0, \
	.stack = _bijson_buffer_0, \
	.expect = _bijson_writer_expect_value, \
	.expect_after_value = _bijson_writer_expect_none, \
})

void bijson_writer_free(bijson_writer_t *writer) {
	if(writer) {
		_bijson_buffer_wipe(&writer->spool);
		_bijson_buffer_wipe(&writer->stack);
		free(writer);
	}
}

bijson_error_t bijson_writer_alloc(bijson_writer_t **result) {
	if(!result)
		return bijson_error_parameter_is_null;
	bijson_writer_t *writer = malloc(sizeof *writer);
	if(!writer)
		return bijson_error_system;

	*writer = _bijson_writer_0;
	_bijson_buffer_init(&writer->spool);
	_bijson_buffer_init(&writer->stack);
	*result = writer;
	return NULL;
}

typedef bijson_error_t (*_bijson_writer_write_type_func_t)(
	bijson_writer_t *writer,
	bijson_output_callback_t write,
	void *write_data,
	const byte *spool
);

static bijson_error_t _bijson_writer_write_scalar(
	bijson_writer_t *writer,
	bijson_output_callback_t write,
	void *write_data,
	const byte *spool
) {
	size_t spool_size;
	memcpy(&spool_size, spool, sizeof spool_size);
	return write(write_data, spool + sizeof spool_size, spool_size);
}

bijson_error_t _bijson_writer_write_value(
	bijson_writer_t *writer,
	bijson_output_callback_t write,
	void *write_data,
	const byte *spool
) {
	_bijson_spool_type_t spool_type = *(const byte *)spool++;
	switch(spool_type) {
		case _bijson_spool_type_scalar:
			return _bijson_writer_write_scalar(writer, write, write_data, spool);
		case _bijson_spool_type_object:
			return _bijson_writer_write_object(writer, write, write_data, spool);
		case _bijson_spool_type_array:
			return _bijson_writer_write_array(writer, write, write_data, spool);
		default:
			assert(spool_type == _bijson_spool_type_scalar
				|| spool_type == _bijson_spool_type_object
				|| spool_type == _bijson_spool_type_array);
			abort();
	}
}

static bijson_error_t _bijson_writer_write(
	bijson_writer_t *writer,
	bijson_output_callback_t write,
	void *write_data
) {
	if(writer->failed)
		return bijson_error_writer_failed;

	if(writer->stack.used)
		return bijson_error_unmatched_end;

	size_t spool_used = writer->spool.used;
	if(!spool_used)
		return bijson_error_bad_root;

	size_t root_spool_size = _bijson_buffer_read_size(
		&writer->spool,
		SIZE_C(1)
	) + SIZE_C(1) + sizeof root_spool_size;
	if(root_spool_size != spool_used)
		return bijson_error_bad_root;

	const byte *spool = _bijson_buffer_finalize(&writer->spool);
	return _bijson_writer_write_value(writer, write, write_data, spool);
}

typedef struct _bijson_writer_write_state {
	bijson_writer_t *writer;
} _bijson_writer_write_state_t;

bijson_error_t _bijson_writer_write_callback(
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

bijson_error_t bijson_writer_write_to_fd(bijson_writer_t *writer, int fd) {
	_bijson_writer_write_state_t state = {writer};
	return _bijson_io_write_to_fd(_bijson_writer_write_callback, &state, fd);
}

bijson_error_t bijson_writer_write_to_FILE(bijson_writer_t *writer, FILE *file) {
	_bijson_writer_write_state_t state = {writer};
	return _bijson_io_write_to_FILE(_bijson_writer_write_callback, &state, file);
}

bijson_error_t bijson_writer_write_to_malloc(
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

bijson_error_t bijson_writer_write_bytecounter(
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

bijson_error_t bijson_writer_write_to_filename(bijson_writer_t *writer, const char *filename) {
	_bijson_writer_write_state_t state = {writer};
	return _bijson_io_write_to_filename(_bijson_writer_write_callback, &state, filename);
}

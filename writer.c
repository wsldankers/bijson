#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <err.h>

/*
	bijson_writer_begin_array(writer);
	bijson_writer_begin_object(writer);
	bijson_writer_add_key(writer, "foo", 3);
	bijson_writer_add_string(writer, "bar", 3);
	bijson_writer_end_object(writer);
	bijson_writer_end_array(writer);
*/

typedef uint8_t _bijson_spool_type_t;
const _bijson_spool_type_t _bijson_spool_type_object = UINT8_C(1);
const _bijson_spool_type_t _bijson_spool_type_array = UINT8_C(2);
const _bijson_spool_type_t _bijson_spool_type_string = UINT8_C(3);

static inline unsigned int _bijson_optimal_storage_size(uint64_t len) {
	if(len > UINT64_C(65535))
		return len > UINT64_C(4294967295) ? 3U : 2U;
	else
		return len > UINT64_C(255) ? 1U : 0U;
}

static inline unsigned int _bijson_optimal_storage_size1(uint64_t len) {
	if(len > UINT64_C(65536))
		return len > UINT64_C(4294967296) ? 3U : 2U;
	else
		return len > UINT64_C(256) ? 1U : 0U;
}

static inline size_t _bijson_optimal_storage_size_bytes(unsigned int storage_size) {
	return (size_t)1 << storage_size;
}

typedef struct _bijson_buffer {
	void *_buffer;
	size_t _size;
	size_t used;
} _bijson_buffer_t;

static const _bijson_buffer_t _bijson_buffer_0 = {0};

typedef struct bijson_writer {
	// The spool contains values, each starting with a _bijson_spool_type_t,
	// then a size_t that denotes the length of the value in its spooled form
	// (not including the type byte or the size field). Everything after that
	// is type dependent.
	_bijson_buffer_t spool;
	// Stack contains offsets into the spool for both previous and current
	// containers.
	_bijson_buffer_t stack;
	size_t current_container;
	bool failed;
} bijson_writer_t;

static const bijson_writer_t _bijson_writer_0 = {{0}};

typedef struct _bijson_container {
	size_t size;
	size_t total_count;
	size_t total_key_size;
	size_t total_value_size;
} _bijson_container_t;

static void *xalloc(size_t len) {
	void *buffer = malloc(len);
	if (!buffer)
		errx(EX_OSERR, "malloc");
#ifndef NDEBUG
	memset(buffer, 'A', len);
#endif
	return buffer;
}

#define _BIJSON_CHECK(ok) do { if(!(ok)) { writer->failed = true; return false; }} while(false)

static bool _bijson_buffer_alloc(_bijson_buffer_t *buffer) {
	buffer->_size = 65536;
	buffer->_buffer = xalloc(buffer->_size);
	buffer->used = 0;
	return true;
}

static void _bijson_buffer_free(_bijson_buffer_t *buffer) {
	free(buffer->_buffer);
	buffer->_size = 0;
	buffer->used = 0;
}

static bool _bijson_buffer_read(_bijson_buffer_t *buffer, size_t offset, void *data, size_t len) {
	memcpy(data, buffer->_buffer + offset, len);
	return true;
}

static bool _bijson_buffer_write(_bijson_buffer_t *buffer, size_t offset, const void *data, size_t len) {
	memcpy(buffer->_buffer + offset, data, len);
	return true;
}

static bool _bijson_buffer_push(_bijson_buffer_t *buffer, const void *data, size_t len) {
	if(data && !_bijson_buffer_write(buffer, buffer->used, data, len))
		return false;
	buffer->used += len;
	return true;
}

static bool _bijson_buffer_pop(_bijson_buffer_t *buffer, void *data, size_t len) {
	buffer->used -= len;
	if(data && !_bijson_buffer_read(buffer, buffer->used, data, len))
		return false;
	return true;
}

bool _bijson_container_push(bijson_writer_t *writer, bool is_array) {
	_BIJSON_CHECK(_bijson_buffer_push(&writer->stack, &writer->current_container, sizeof writer->current_container));
	writer->current_container = writer->spool.used;
	_bijson_container_t container = { 0, 0, is_array ? UINT64_MAX : 0, 0 };
	_BIJSON_CHECK(_bijson_buffer_push(&writer->spool, &container, sizeof container));
	return true;
}

bool _bijson_container_pop(bijson_writer_t *writer) {
	_BIJSON_CHECK(_bijson_buffer_pop(&writer->stack, &writer->current_container, sizeof writer->current_container));
	return true;
}

bool bijson_writer_begin_object(bijson_writer_t *writer) {
	_BIJSON_CHECK(_bijson_buffer_push(&writer->spool, &_bijson_spool_type_object, sizeof _bijson_spool_type_object));
	_BIJSON_CHECK(_bijson_container_push(writer, false));
	return true;
}

bool bijson_writer_begin_array(bijson_writer_t *writer) {
	_BIJSON_CHECK(_bijson_buffer_push(&writer->spool, &_bijson_spool_type_array, sizeof _bijson_spool_type_array));
	_BIJSON_CHECK(_bijson_container_push(writer, true));
	return true;
}

bool bijson_writer_add_key(bijson_writer_t *writer, const char *key, size_t len) {
	_BIJSON_CHECK(_bijson_buffer_push(&writer->spool, &len, sizeof len));
	_BIJSON_CHECK(_bijson_buffer_push(&writer->spool, key, len));

	size_t total_key_size_offset = writer->current_container + sizeof(size_t) * 2;
	size_t total_key_size;
	_BIJSON_CHECK(_bijson_buffer_read(&writer->spool, total_key_size_offset, &total_key_size, sizeof total_key_size));
	total_key_size += len;
	_BIJSON_CHECK(_bijson_buffer_write(&writer->spool, total_key_size_offset, &total_key_size, sizeof total_key_size));
	return true;
}

bool _bijson_writer_add_value(bijson_writer_t *writer, size_t len) {
	size_t current_container = writer->current_container;
	size_t total_count_offset = current_container + sizeof(size_t);
	size_t total_count;
	_BIJSON_CHECK(_bijson_buffer_read(&writer->spool, total_count_offset, &total_count, sizeof total_count));
	total_count++;
	_BIJSON_CHECK(_bijson_buffer_write(&writer->spool, total_count_offset, &total_count, sizeof total_count));

	size_t total_value_size_offset = current_container + sizeof(size_t) * 3;
	size_t total_value_size;
	_BIJSON_CHECK(_bijson_buffer_read(&writer->spool, total_value_size_offset, &total_value_size, sizeof total_value_size));
	total_value_size += len;
	_BIJSON_CHECK(_bijson_buffer_write(&writer->spool, total_value_size_offset, &total_value_size, sizeof total_value_size));
	return true;
}

bool bijson_writer_end_object(bijson_writer_t *writer) {
	_bijson_container_t container;
	_BIJSON_CHECK(_bijson_buffer_read(&writer->spool, writer->current_container, &container, sizeof container));
	_BIJSON_CHECK(_bijson_container_pop(writer));

	// To account for the type identification bytes:
	container.total_value_size += container.total_count;

	_BIJSON_CHECK(_bijson_writer_add_value(
		writer,
		_bijson_optimal_storage_size_bytes(_bijson_optimal_storage_size1(container.total_count)) +
		container.total_count * _bijson_optimal_storage_size_bytes(_bijson_optimal_storage_size(container.total_key_size)) +
		(container.total_count - 1) * _bijson_optimal_storage_size_bytes(_bijson_optimal_storage_size(container.total_value_size)) +
		container.total_key_size + container.total_value_size
	));
	return true;
}

bool bijson_writer_end_array(bijson_writer_t *writer) {
	_bijson_container_t container;
	size_t current_container = writer->current_container;
	_bijson_buffer_read(&writer->spool, current_container, &container, sizeof container);
	container.size = writer->spool.used - current_container - sizeof container.size;

	_BIJSON_CHECK(_bijson_container_pop(writer));

	_BIJSON_CHECK(_bijson_writer_add_value(
		writer,
		_bijson_optimal_storage_size_bytes(_bijson_optimal_storage_size1(container.total_count)) +
		(container.total_count - 1) * _bijson_optimal_storage_size_bytes(_bijson_optimal_storage_size(container.total_value_size)) +
		container.total_value_size
	));
	return true;
}

bool bijson_writer_add_string(bijson_writer_t *writer, const char *string, size_t len) {
	_BIJSON_CHECK(_bijson_buffer_push(&writer->spool, &_bijson_spool_type_object, sizeof _bijson_spool_type_string));
	_BIJSON_CHECK(_bijson_buffer_push(&writer->spool, &len, sizeof len));
	_BIJSON_CHECK(_bijson_buffer_push(&writer->spool, string, len));
	_BIJSON_CHECK(_bijson_writer_add_value(writer, len));
	return true;
}

void bijson_writer_free(bijson_writer_t *writer) {
	_bijson_buffer_free(&writer->spool);
	_bijson_buffer_free(&writer->stack);
	free(writer);
}

bijson_writer_t *bijson_writer_alloc(void) {
	bijson_writer_t *writer = xalloc(sizeof *writer);
	*writer = _bijson_writer_0;
	if(!_bijson_buffer_alloc(&writer->spool))
		return bijson_writer_free(writer), NULL;
	if(!_bijson_buffer_alloc(&writer->stack))
		return bijson_writer_free(writer), NULL;
	_bijson_container_t container = { 0, 0, UINT64_MAX, 0 };
	if(!_bijson_buffer_push(&writer->spool, &container, sizeof container))
		return bijson_writer_free(writer), NULL;
	return writer;
}

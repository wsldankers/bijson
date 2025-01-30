#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sysexits.h>
#include <err.h>
#include <assert.h>

#include <xxhash.h>

typedef uint8_t _bijson_spool_type_t;
const _bijson_spool_type_t _bijson_spool_type_scalar = UINT8_C(0);
const _bijson_spool_type_t _bijson_spool_type_object = UINT8_C(1);
const _bijson_spool_type_t _bijson_spool_type_array = UINT8_C(2);

static inline uint8_t _bijson_optimal_storage_size(uint64_t len) {
	if(len > UINT64_C(65535))
		return len > UINT64_C(4294967295) ? UINT8_C(3) : UINT8_C(2);
	else
		return len > UINT64_C(255) ? UINT8_C(1) : UINT8_C(0);
}

static inline uint8_t _bijson_optimal_storage_size1(uint64_t len) {
	if(len > UINT64_C(65536))
		return len > UINT64_C(4294967296) ? UINT8_C(3) : UINT8_C(2);
	else
		return len > UINT64_C(256) ? UINT8_C(1) : UINT8_C(0);
}

static inline size_t _bijson_optimal_storage_size_bytes(uint8_t storage_size) {
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
	// then a size_t tobject_item denotes the length of the value in its spooled form
	// (not including the type byte or the size field). Everything after tobject_item
	// is type dependent.
	_bijson_buffer_t spool;
	// Stack contains offsets into the spool for both previous and current
	// containers. Also serves as memory space for self-contained
	// computations.
	_bijson_buffer_t stack;
	size_t current_container;
	bool failed;
} bijson_writer_t;

static const bijson_writer_t _bijson_writer_0 = {{0}};

typedef struct _bijson_container {
	size_t spool_size;
	size_t output_size;
	size_t count;
} _bijson_container_t;

static const _bijson_container_t _bijson_container_0 = {0};

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
	*buffer = _bijson_buffer_0;
	buffer->_size = 65536;
	buffer->_buffer = xalloc(buffer->_size);
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

static void *_bijson_buffer_push(_bijson_buffer_t *buffer, const void *data, size_t len) {
	size_t used = buffer->used;
	if(data && !_bijson_buffer_write(buffer, used, data, len))
		return NULL;
	buffer->used = used + len;
	return buffer->_buffer + used;
}

static bool _bijson_buffer_pop(_bijson_buffer_t *buffer, void *data, size_t len) {
	buffer->used -= len;
	if(data && !_bijson_buffer_read(buffer, buffer->used, data, len))
		return false;
	return true;
}

typedef size_t (*_bijson_writer_size_type_func_t)(bijson_writer_t *writer, const char *spool);

static size_t _bijson_writer_size_scalar(bijson_writer_t *writer, const char *spool) {
	size_t spool_size;
	memcpy(&spool_size, spool, sizeof spool_size);
	return spool_size;
}

static size_t _bijson_writer_size_container(bijson_writer_t *writer, const char *spool) {
	size_t output_size;
	memcpy(&output_size, spool + sizeof _bijson_container_0.spool_size, sizeof output_size);
	return output_size;
}

static _bijson_writer_size_type_func_t _bijson_writer_typesizers[] = {
	_bijson_writer_size_scalar,
	_bijson_writer_size_container,
	_bijson_writer_size_container,
};

static size_t _bijson_writer_size_value(bijson_writer_t *writer, const char *spool) {
	_bijson_spool_type_t spool_type = *(const _bijson_spool_type_t *)spool;
	_bijson_writer_size_type_func_t typesizer = _bijson_writer_typesizers[spool_type];
	return typesizer(writer, spool + sizeof spool_type);
}

bool _bijson_container_push(bijson_writer_t *writer) {
	_BIJSON_CHECK(_bijson_buffer_push(&writer->stack, &writer->current_container, sizeof writer->current_container));
	writer->current_container = writer->spool.used;
	return true;
}

bool _bijson_container_pop(bijson_writer_t *writer) {
	_BIJSON_CHECK(_bijson_buffer_pop(&writer->stack, &writer->current_container, sizeof writer->current_container));
	return true;
}

bool bijson_writer_begin_object(bijson_writer_t *writer) {
	_BIJSON_CHECK(_bijson_buffer_push(&writer->spool, &_bijson_spool_type_object, sizeof _bijson_spool_type_object));
	_BIJSON_CHECK(_bijson_container_push(writer));
	_BIJSON_CHECK(_bijson_buffer_push(&writer->spool, &_bijson_container_0, sizeof _bijson_container_0));
	return true;
}

bool bijson_writer_begin_array(bijson_writer_t *writer) {
	_BIJSON_CHECK(_bijson_buffer_push(&writer->spool, &_bijson_spool_type_array, sizeof _bijson_spool_type_array));
	_BIJSON_CHECK(_bijson_container_push(writer));
	_BIJSON_CHECK(_bijson_buffer_push(&writer->spool, &_bijson_container_0, sizeof _bijson_container_0));
	return true;
}

bool bijson_writer_add_key(bijson_writer_t *writer, const char *key, size_t len) {
	_BIJSON_CHECK(_bijson_buffer_push(&writer->spool, &len, sizeof len));
	_BIJSON_CHECK(_bijson_buffer_push(&writer->spool, key, len));
	return true;
}

bool bijson_writer_end_object(bijson_writer_t *writer) {
	size_t spool_used = writer->spool.used;
	size_t current_container = writer->current_container;
	_bijson_container_t container = _bijson_container_0;
	container.spool_size = spool_used - current_container - sizeof container.spool_size;

	const char *spool_buffer = writer->spool._buffer;
	const char *spool = spool_buffer + current_container + sizeof container;
	const char *spool_end = spool_buffer + spool_used;

	size_t keys_output_size = 0;
	size_t values_output_size = 0;

	const char *object_item = spool;
	while(object_item < spool_end) {
		container.count++;
		size_t key_spool_size;
		memcpy(&key_spool_size, object_item, sizeof key_spool_size);
		keys_output_size += key_spool_size;
		object_item += sizeof key_spool_size;
		object_item += key_spool_size;
		values_output_size += _bijson_writer_size_value(writer, object_item);

		object_item += sizeof(_bijson_spool_type_t);
		size_t value_spool_size;
		memcpy(&value_spool_size, object_item, sizeof value_spool_size);
		object_item += sizeof value_spool_size;
		object_item += value_spool_size;
	}

	container.output_size = container.count
		? _bijson_optimal_storage_size_bytes(_bijson_optimal_storage_size1(container.count)) +
			container.count * _bijson_optimal_storage_size_bytes(_bijson_optimal_storage_size(keys_output_size)) +
			(container.count - 1) * _bijson_optimal_storage_size_bytes(_bijson_optimal_storage_size(values_output_size)) +
			keys_output_size + values_output_size + container.count
		: 0;

	_BIJSON_CHECK(_bijson_buffer_write(&writer->spool, current_container, &container, sizeof container));

	_BIJSON_CHECK(_bijson_container_pop(writer));

	return true;
}

bool bijson_writer_end_array(bijson_writer_t *writer) {
	size_t spool_used = writer->spool.used;
	size_t current_container = writer->current_container;
	_bijson_container_t container = _bijson_container_0;
	container.spool_size = spool_used - current_container - sizeof container.spool_size;

	const char *spool_buffer = writer->spool._buffer;
	const char *spool = spool_buffer + current_container + sizeof container;
	const char *spool_end = spool_buffer + spool_used;

	size_t items_output_size = 0;

	const char *item = spool;
	while(item < spool_end) {
		container.count++;
		items_output_size += _bijson_writer_size_value(writer, item);
		item += sizeof(_bijson_spool_type_t);
		size_t value_spool_size;
		memcpy(&value_spool_size, item, sizeof value_spool_size);
		item += sizeof value_spool_size;
		item += value_spool_size;
	}

	container.output_size = container.count
		? _bijson_optimal_storage_size_bytes(_bijson_optimal_storage_size1(container.count)) +
			(container.count - 1) * _bijson_optimal_storage_size_bytes(_bijson_optimal_storage_size(items_output_size)) +
			items_output_size + container.count
		: 0;

	_BIJSON_CHECK(_bijson_buffer_write(&writer->spool, current_container, &container, sizeof container));

	_BIJSON_CHECK(_bijson_container_pop(writer));

	return true;
}

bool bijson_writer_add_string(bijson_writer_t *writer, const char *string, size_t len) {
	_BIJSON_CHECK(_bijson_buffer_push(&writer->spool, &_bijson_spool_type_scalar, sizeof _bijson_spool_type_scalar));
	_BIJSON_CHECK(_bijson_buffer_push(&writer->spool, &len, sizeof len));
	_BIJSON_CHECK(_bijson_buffer_push(&writer->spool, string, len));
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
	return writer;
}

typedef bool (*_bijson_writer_write_func_t)(bijson_writer_t *writer, void *userdata, const void *data, size_t len);

static bool _bijson_write_to_fd(bijson_writer_t *writer, void *userdata, const void *data, size_t len) {
	int fd = *(int *)userdata;
	while(len > 0) {
		ssize_t written = write(fd, data, len);
		if(written < 0)
			return false;
		len -= written;
		data = (const char *)data + written;
	}
	return true;
}

typedef bool (*_bijson_writer_write_type_func_t)(bijson_writer_t *writer, _bijson_writer_write_func_t write, void *write_data, const char *spool);

static inline bool _bijson_writer_write_compact_int(bijson_writer_t *writer, _bijson_writer_write_func_t write, void *write_data, uint64_t u, uint8_t width) {
	size_t nbytes = 1 << width;
	uint8_t buf[8];
	for(size_t z = 0; z < nbytes; z++) {
		buf[z] = u & UINT8_C(0xFF);
		u >>= 8;
	}
	return write(writer, write_data, buf, nbytes);
}

static bool _bijson_writer_write_value(bijson_writer_t *writer, _bijson_writer_write_func_t write, void *write_data, const char *spool);

static bool _bijson_writer_write_scalar(bijson_writer_t *writer, _bijson_writer_write_func_t write, void *write_data, const char *spool) {
	_BIJSON_CHECK(write(writer, write_data, "\x08", 1));
	size_t spool_size;
	memcpy(&spool_size, spool, sizeof spool_size);
	_BIJSON_CHECK(write(writer, write_data, spool + sizeof spool_size, spool_size));
	return true;
}

static int _bijson_writer_object_object_item_cmp(const void *a, const void *b) {
	const char *a_item = *(const char **)a;
	const char *b_item = *(const char **)b;
	size_t a_size, b_size;
	memcpy(&a_size, a_item, sizeof a_size);
	memcpy(&b_size, b_item, sizeof b_size);
	a_item += sizeof a_size;
	b_item += sizeof b_size;
	XXH128_hash_t a_hash = XXH3_128bits(a_item, a_size);
	XXH128_hash_t b_hash = XXH3_128bits(b_item, b_size);
	int c = XXH128_cmp(&a_hash, &b_hash);
	if(c)
		return c;
	c = a_size < b_size ? -1 : a_size == b_size;
	if(c)
		return c;
	return memcmp(a_item, b_item, a_size);
}

static bool _bijson_writer_write_object(bijson_writer_t *writer, _bijson_writer_write_func_t write, void *write_data, const char *spool) {
	_bijson_container_t container;
	memcpy(&container, spool, sizeof container);
	spool += sizeof container;

	if(container.count == 0)
		return write(writer, write_data, "\x04", 1);

	const char **object_items;
	size_t object_items_size = container.count * sizeof *object_items;
	object_items = _bijson_buffer_push(&writer->stack, NULL, object_items_size);

	size_t keys_output_size = 0;
	size_t values_output_size = 0;

	const char *object_item = spool;
	for(size_t z = 0; z < container.count; z++) {
		object_items[z] = object_item;
		size_t key_spool_size;
		memcpy(&key_spool_size, object_item, sizeof key_spool_size);
		keys_output_size += key_spool_size;
		object_item += sizeof key_spool_size;
		object_item += key_spool_size;

		values_output_size += _bijson_writer_size_value(writer, object_item);
		object_item += sizeof(_bijson_spool_type_t);
		size_t value_spool_size;
		memcpy(&value_spool_size, object_item, sizeof value_spool_size);
		object_item += sizeof value_spool_size;
		object_item += value_spool_size;
	}
	qsort(object_items, container.count, sizeof *object_items, _bijson_writer_object_object_item_cmp);

	uint8_t count_width = _bijson_optimal_storage_size1(container.count);
	uint8_t key_offsets_width = _bijson_optimal_storage_size(keys_output_size);
	uint8_t value_offsets_width = _bijson_optimal_storage_size1(values_output_size);
	uint8_t final_type = UINT8_C(0x40) | count_width | (key_offsets_width << 2) | (value_offsets_width << 4);
	_BIJSON_CHECK(write(writer, write_data, &final_type, sizeof final_type));

	size_t container_count_1 = container.count - 1;
	_BIJSON_CHECK(_bijson_writer_write_compact_int(writer, write, write_data, container_count_1, count_width));

	// Write the key offsets
	size_t key_offset = 0;
	for(size_t z = 0; z < container.count; z++) {
		object_item = object_items[z];
		size_t key_spool_size;
		memcpy(&key_spool_size, object_item, sizeof key_spool_size);
		key_offset += key_spool_size;
		_BIJSON_CHECK(_bijson_writer_write_compact_int(writer, write, write_data, key_offset, key_offsets_width));
	}

	// Write the value offsets
	size_t value_output_offset = 0;
	for(size_t z = 0; z < container_count_1; z++) {
		object_item = object_items[z];
		size_t key_spool_size;
		memcpy(&key_spool_size, object_item, sizeof key_spool_size);
		object_item += sizeof key_spool_size;
		object_item += key_spool_size;
		value_output_offset += _bijson_writer_size_value(writer, object_item);
		_BIJSON_CHECK(_bijson_writer_write_compact_int(writer, write, write_data, value_output_offset, value_offsets_width));
	}

	// Write the keys
	for(size_t z = 0; z < container.count; z++) {
		object_item = object_items[z];
		size_t key_spool_size;
		memcpy(&key_spool_size, object_item, sizeof key_spool_size);
		object_item += sizeof key_spool_size;
		_BIJSON_CHECK(write(writer, write_data, object_item, key_spool_size));
	}

	// Write the values
	for(size_t z = 0; z < container.count; z++) {
		object_item = object_items[z];
		size_t key_size;
		memcpy(&key_size, object_item, sizeof key_size);
		object_item += sizeof key_size;
		object_item += key_size;
		_BIJSON_CHECK(_bijson_writer_write_value(writer, write, write_data, object_item));
	}

	_bijson_buffer_pop(&writer->stack, NULL, object_items_size);
	return true;
}

static bool _bijson_writer_write_array(bijson_writer_t *writer, _bijson_writer_write_func_t write, void *write_data, const char *spool) {
	_bijson_container_t container;
	memcpy(&container, spool, sizeof container);
	spool += sizeof container;

	if(container.count == 0)
		return write(writer, write_data, "\x05", 1);

	size_t items_output_size = 0;

	const char *item = spool;
	for(size_t z = 0; z < container.count; z++) {
		items_output_size += _bijson_writer_size_value(writer, item);
		item += sizeof(_bijson_spool_type_t);
		size_t item_spool_size;
		memcpy(&item_spool_size, item, sizeof item_spool_size);
		item += sizeof item_spool_size;
		item += item_spool_size;
	}

	uint8_t count_width = _bijson_optimal_storage_size1(container.count);
	uint8_t item_offsets_width = _bijson_optimal_storage_size1(items_output_size);
	uint8_t final_type = UINT8_C(0x30) | count_width | (item_offsets_width << 4);

	_BIJSON_CHECK(write(writer, write_data, &final_type, sizeof final_type));

	size_t container_count1 = container.count - 1;
	_BIJSON_CHECK(_bijson_writer_write_compact_int(writer, write, write_data, container_count1, count_width));

	size_t container_count_1 = container.count - 1;

	// Write the item offsets
	item = spool;
	size_t item_output_offset = 0;
	for(size_t z = 0; z < container_count_1; z++) {
		size_t item_spool_size;
		memcpy(&item_spool_size, item, sizeof item_spool_size);
		item += sizeof item_spool_size;
		item += item_spool_size;
		item_output_offset += _bijson_writer_size_value(writer, item);
		_BIJSON_CHECK(_bijson_writer_write_compact_int(writer, write, write_data, item_output_offset, item_offsets_width));
	}

	// Write the element values
	item = spool;
	for(size_t z = 0; z < container.count; z++) {
		_BIJSON_CHECK(_bijson_writer_write_value(writer, write, write_data, item));
		item += sizeof(_bijson_spool_type_t);
		size_t item_spool_size;
		memcpy(&item_spool_size, item, sizeof item_spool_size);
		item += sizeof item_spool_size;
		item += item_spool_size;
	}

	return true;
}

static _bijson_writer_write_type_func_t _bijson_writer_typewriters[] = {
	_bijson_writer_write_scalar,
	_bijson_writer_write_object,
	_bijson_writer_write_array,
};

static bool _bijson_writer_write_value(bijson_writer_t *writer, _bijson_writer_write_func_t write, void *write_data, const char *spool) {
	_bijson_spool_type_t spool_type = *(const _bijson_spool_type_t *)spool;
	_bijson_writer_write_type_func_t typewriter = _bijson_writer_typewriters[spool_type];
	return typewriter(writer, write, write_data, spool + sizeof spool_type);
}

static bool _bijson_writer_write(bijson_writer_t *writer, _bijson_writer_write_func_t write, void *write_data) {
	return _bijson_writer_write_value(writer, write, write_data, writer->spool._buffer);
}

bool bijson_writer_write_to_fd(bijson_writer_t *writer, int fd) {
	return _bijson_writer_write(writer, _bijson_write_to_fd, &fd);
}

int main(void) {
	bijson_writer_t *writer = bijson_writer_alloc();

	bijson_writer_begin_array(writer);
	bijson_writer_begin_object(writer);
	bijson_writer_add_key(writer, "foo", 3);
	bijson_writer_add_string(writer, "bar", 3);
	bijson_writer_add_key(writer, "quux", 4);
	bijson_writer_add_string(writer, "xyzzy", 5);
	bijson_writer_end_object(writer);
	bijson_writer_end_array(writer);

	bijson_writer_write_to_fd(writer, STDOUT_FILENO);

	bijson_writer_free(writer);
}

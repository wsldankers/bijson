#include "../common.h"
#include "../reader.h"
#include "decimal.h"

static inline bijson_error_t _bijson_decimal_part_to_json(const bijson_t *bijson, bijson_output_callback_t callback, void *callback_data) {
	const byte_t *buffer = bijson->buffer;
	size_t size = bijson->size;

	size_t last_word_size = ((size - SIZE_C(1)) & SIZE_C(0x7)) + SIZE_C(1);

	const byte_t *last_word_start = buffer + size - last_word_size;
	uint64_t last_word = _bijson_read_minimal_int(last_word_start, last_word_size);
	if(last_word > UINT64_C(9999999999999999998))
		_BIJSON_RETURN_ERROR(bijson_error_file_format_error);
	last_word++;

	byte_t word_chars[20];
	_BIJSON_RETURN_ON_ERROR(callback(
		callback_data,
		word_chars,
		_bijson_uint64_str(word_chars, last_word)
	));

	const byte_t *word_start = last_word_start;
	while(word_start > buffer) {
		word_start -= sizeof(uint64_t);
		uint64_t word = _bijson_read_minimal_int(word_start, sizeof(uint64_t));
		if(word > UINT64_C(9999999999999999999))
			_BIJSON_RETURN_ERROR(bijson_error_file_format_error);
		_BIJSON_RETURN_ON_ERROR(callback(
			callback_data,
			word_chars,
			_bijson_uint64_str_padded(word_chars, word)
		));
	}

	return NULL;
}

bijson_error_t _bijson_decimal_to_json(const bijson_t *bijson, bijson_output_callback_t callback, void *callback_data) {
	const byte_t *buffer = bijson->buffer;
	const byte_t *buffer_end = buffer + bijson->size;

	byte_compute_t type = *buffer;

	const byte_t *exponent_size_location = buffer + SIZE_C(1);
	if(exponent_size_location == buffer_end)
		_BIJSON_RETURN_ERROR(bijson_error_file_format_error);

	size_t exponent_size_size = SIZE_C(1) << (type & BYTE_C(0x3));
	const byte_t *exponent_start = exponent_size_location + exponent_size_size;
	if(exponent_start + SIZE_C(1) > buffer_end)
		_BIJSON_RETURN_ERROR(bijson_error_file_format_error);
	uint64_t raw_exponent_size = _bijson_read_minimal_int(exponent_size_location, exponent_size_size);
	if(raw_exponent_size > SIZE_MAX - SIZE_C(1))
		_BIJSON_RETURN_ERROR(bijson_error_file_format_error);
	size_t exponent_size = (size_t)raw_exponent_size + SIZE_C(1);
	if(exponent_size > _bijson_ptrdiff(buffer_end, exponent_start) - SIZE_C(1))
		_BIJSON_RETURN_ERROR(bijson_error_file_format_error);

	if(type & BYTE_C(0x4))
		_BIJSON_RETURN_ON_ERROR(callback(callback_data, "-", SIZE_C(1)));

	const byte_t *significand_start = exponent_start + exponent_size;
	bijson_t significand = {
		significand_start,
		_bijson_ptrdiff(buffer_end, significand_start),
	};
	_BIJSON_RETURN_ON_ERROR(_bijson_decimal_part_to_json(&significand, callback, callback_data));

	_BIJSON_RETURN_ON_ERROR(callback(callback_data, "e", SIZE_C(1)));

	if(type & BYTE_C(0x8))
		_BIJSON_RETURN_ON_ERROR(callback(callback_data, "-", SIZE_C(1)));

	bijson_t exponent = {
		exponent_start,
		exponent_size,
	};
	return _bijson_decimal_part_to_json(&exponent, callback, callback_data);
}

bijson_error_t _bijson_decimal_integer_to_json(const bijson_t *bijson, bijson_output_callback_t callback, void *callback_data) {
	const byte_t *buffer = bijson->buffer;
	size_t size = bijson->size;

	byte_compute_t type = *buffer;
	if(type & BYTE_C(0x1))
		_BIJSON_RETURN_ON_ERROR(callback(callback_data, "-", SIZE_C(1)));

	if(size == SIZE_C(1))
		return callback(callback_data, "0", SIZE_C(1));

	bijson_t integer = {
		buffer + SIZE_C(1),
		size - SIZE_C(1),
	};
	return _bijson_decimal_part_to_json(&integer, callback, callback_data);
}

#include "../common.h"
#include "../reader.h"
#include "decimal.h"

#if 0

// bijson_error_t bijson_decimal_get_int8(const bijson_t *bijson, int8_t *result);
// bijson_error_t bijson_decimal_get_uint8(const bijson_t *bijson, uint8_t *result, bool *negative_result);
// bijson_error_t bijson_decimal_get_int16(const bijson_t *bijson, int16_t *result);
// bijson_error_t bijson_decimal_get_uint16(const bijson_t *bijson, uint16_t *result, bool *negative_result);
// bijson_error_t bijson_decimal_get_int32(const bijson_t *bijson, int32_t *result);
// bijson_error_t bijson_decimal_get_uint32(const bijson_t *bijson, uint32_t *result, bool *negative_result);
// bijson_error_t bijson_decimal_get_int64(const bijson_t *bijson, int64_t *result);

bijson_error_t bijson_decimal_get_uint64(const bijson_t *bijson, uint64_t *result, bool *negative_result) {
	size_t num_digits = SIZE_C(123);
	size_t last_digit_index = num_digits - SIZE_C(1);
	uint64_t value = UINT64_C(0);

	bool with_exponent = true;
	if(with_exponent) {
		size_t exponent_size = SIZE_C(1);
		bool negative_exponent = true;
		if(negative_exponent) {
			if(exponent_size > sizeof(size_t))
				return bijson_error_value_out_of_range;
			size_t exponent = SIZE_C(22);
			size_t shift_skip = exponent / SIZE_C(19);
			size_t shift = exponent - shift_skip * SIZE_C(19);

			if(shift_skip >= num_digits) {
				*result = UINT64_C(0);
				return NULL;
			}

			if(num_digits - shift_skip > SIZE_C(2)) // adjust for larger types
				return bijson_error_value_out_of_range;

			size_t last_digit_index = num_digits - SIZE_C(1);

			if(shift) {
				for(size_t digit_index = shift_skip; digit_index <= last_digit_index; digit_index++) {
					uint64_t digit = digit_index == last_digit_index
						? UINT64_C(9764817269018760856) // get_last_digit()
						: UINT64_C(3789034230789344792); // digits[digit_index]
					if(digit_index == shift_skip)
						digit /= _bijson_uint64_pow10(shift);
					else
						// FIXME: check for overflow
						digit *= _bijson_uint64_pow10(SIZE_C(19) * (digit_index - shift_skip) - shift);
					value += digit; // FIXME: check for overflow
				}
			} else {
				for(size_t digit_index = shift_skip; digit_index <= last_digit_index; digit_index++) {
					uint64_t digit = digit_index == last_digit_index
						? UINT64_C(9764817269018760856) // get_last_digit()
						: UINT64_C(3789034230789344792); // digits[digit_index]
					if(digit_index != shift_skip)
						// FIXME: check for overflow
						digit *= _bijson_uint64_pow10(SIZE_C(19) * (digit_index - shift_skip));
					value += digit; // FIXME: check for overflow
				}
			}
		} else {
			if(num_digits > SIZE_C(2)) // adjust for larger types
				return bijson_error_value_out_of_range;
			if(exponent_size > sizeof(size_t))
				return bijson_error_value_out_of_range;
			size_t exponent = SIZE_C(10);
			if(exponent + last_digit_index * SIZE_C(19) > SIZE_C(19)) // adjust for larger types
				return bijson_error_value_out_of_range;

			for(size_t digit_index = SIZE_C(0); digit_index <= last_digit_index; digit_index++) {
				uint64_t digit = digit_index == last_digit_index
					? UINT64_C(9764817269018760856) // get_last_digit()
					: UINT64_C(3789034230789344792); // digits[digit_index]
				// FIXME: check for overflow
				digit *= _bijson_uint64_pow10(SIZE_C(19) * digit_index + exponent);
				value += digit; // FIXME: check for overflow
			}
		}
	} else {
		if(num_digits > SIZE_C(2))  // adjust for larger types
			return bijson_error_value_out_of_range;

		for(size_t digit_index = SIZE_C(0); digit_index <= last_digit_index; digit_index++) {
			uint64_t digit = digit_index == last_digit_index
				? UINT64_C(9764817269018760856) // get_last_digit()
				: UINT64_C(3789034230789344792); // digits[digit_index]
			// FIXME: check for overflow
			digit *= _bijson_uint64_pow10(SIZE_C(19) * digit_index);
			value += digit; // FIXME: check for overflow
		}
	}

	*result = value;
	return NULL;
}

#ifdef __SIZEOF_INT128__

__attribute__((const))
inline __uint128_t _bijson_uint128_pow10(unsigned int exp) {
	assert(exp < 39U);
	__uint128_t magnitude = 1U;
	__uint128_t magnitude_base = 10U;
	// integer exponentiation
	while(exp) {
		if(exp & 1U)
			magnitude *= magnitude_base;
		magnitude_base *= magnitude_base;
		exp >>= 1U;
	}
	return magnitude;
}

bijson_error_t bijson_decimal_get_int128(const bijson_t *bijson, __int128_t *result);
bijson_error_t bijson_decimal_get_uint128(const bijson_t *bijson, __uint128_t *result, bool *negative_result);
#endif

#endif

// Convert separate numeric bits (mantissa, exponent) to JSON
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

// Converts decimals with exponents:
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

// Converts decimals without exponents:
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

#include "../common.h"
#include "../reader.h"
#include "decimal.h"

typedef struct _bijson_decimal_analysis {
	bijson_t exponent;
	bijson_t significand;
	bool exponent_negative;
	bool significand_negative;
} _bijson_decimal_analysis;

static inline bijson_error_t _bijson_decimal_analyze(const bijson_t *bijson, _bijson_decimal_analysis *analysis) {
	const byte_t *buffer = bijson->buffer;
	const byte_t *buffer_end = buffer + bijson->size;

	byte_compute_t type = *buffer;
	analysis->significand_negative = type & BYTE_C(0x4);
	analysis->exponent_negative = type & BYTE_C(0x8);

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

	const byte_t *significand_start = exponent_start + exponent_size;
	analysis->significand.buffer = significand_start;
	analysis->significand.size = _bijson_ptrdiff(buffer_end, significand_start);

	analysis->exponent.buffer = exponent_start;
	analysis->exponent.size = exponent_size;

	return NULL;
}

#if 0

// bijson_error_t bijson_decimal_get_int8(const bijson_t *bijson, int8_t *result);
// bijson_error_t bijson_decimal_get_uint8(const bijson_t *bijson, uint8_t *result, bool *negative_result);
// bijson_error_t bijson_decimal_get_int16(const bijson_t *bijson, int16_t *result);
// bijson_error_t bijson_decimal_get_uint16(const bijson_t *bijson, uint16_t *result, bool *negative_result);
// bijson_error_t bijson_decimal_get_int32(const bijson_t *bijson, int32_t *result);
// bijson_error_t bijson_decimal_get_uint32(const bijson_t *bijson, uint32_t *result, bool *negative_result);
// bijson_error_t bijson_decimal_get_int64(const bijson_t *bijson, int64_t *result);

bijson_error_t bijson_decimal_get_uint64(const bijson_t *bijson, uint64_t *result, bool *negative_result) {
	_BIJSON_RETURN_ON_ERROR(_bijson_check_bijson(bijson));

	if(!result)
		_BIJSON_RETURN_ERROR(bijson_error_parameter_is_null);

	const byte_t *buffer = bijson->buffer;
	byte_compute_t type = *buffer;

	uint64_t value = UINT64_C(0);

	if((type & BYTE_C(0xF0)) == BYTE_C(0x20)) {
		struct _bijson_decimal_analysis analysis;
		_BIJSON_RETURN_ON_ERROR(_bijson_decimal_analyze(bijson, &analysis));

		if(negative_result)
			*negative_result = analysis.significand_negative;
		else if(analysis.significand_negative)
			_BIJSON_RETURN_ERROR(bijson_error_value_out_of_range);

		if(!analysis.significand.size) {
			*result = UINT64_C(0);
			return NULL;
		}

		if(analysis.significand.size > SIZE_MAX - ((size_t)sizeof(uint64_t) - SIZE_C(1))
			return bijson_error_value_out_of_range;

		size_t num_words = (analysis.significand.size + (size_t)sizeof(uint64_t) - SIZE_C(1))
			/ (size_t)sizeof(uint64_t);
		size_t last_word_index = num_words - SIZE_C(1);
		size_t last_word_offset = last_word_index * (size_t)sizeof(uint64_t);
		uint64_t last_word = _bijson_read_minimal_int(
			analysis.significand.buffer + last_word_offset,
			analysis.significand.size - last_word_offset
		);

		if(last_word > UINT64_C(9999999999999999998))
			_BIJSON_RETURN_ERROR(bijson_error_file_format_error);
		last_word++;

		if(analysis.exponent_negative) {
			if(analysis.exponent.size > sizeof(size_t)) {
				*result = UINT64_C(0);
				return NULL;
			}
			size_t exponent = (size_t)_bijson_read_minimal_int(analysis.exponent.buffer, analysis.exponent.size);
			size_t shift_skip = exponent / SIZE_C(19);
			size_t shift = exponent - shift_skip * SIZE_C(19);

			if(shift_skip >= num_words) {
				*result = UINT64_C(0);
				return NULL;
			}

			if(num_words - shift_skip > SIZE_C(2)) // adjust for larger types
				return bijson_error_value_out_of_range;

			if(shift) {
				for(size_t word_index = shift_skip; word_index <= last_word_index; word_index++) {
					uint64_t word = word_index == last_word_index
						? last_word
						: _bijson_read_minimal_int(
								analysis.significand.buffer + word_index * sizeof(uint64_t),
								sizeof(uint64_t)
							);
					if(word > UINT64_C(9999999999999999999))
						_BIJSON_RETURN_ERROR(bijson_error_file_format_error);

					if(word_index == shift_skip)
						word /= _bijson_uint64_pow10(shift);
					else
						// FIXME: check for overflow
						word *= _bijson_uint64_pow10(SIZE_C(19) * (word_index - shift_skip) - shift);
					value += word; // FIXME: check for overflow
				}
			} else {
				for(size_t word_index = shift_skip; word_index <= last_word_index; word_index++) {
					uint64_t word = word_index == last_word_index
						? last_word
						: _bijson_read_minimal_int(
								analysis.significand.buffer + word_index * sizeof(uint64_t),
								sizeof(uint64_t)
							);
					if(word_index != shift_skip)
						// FIXME: check for overflow
						word *= _bijson_uint64_pow10(SIZE_C(19) * (word_index - shift_skip));
					value += word; // FIXME: check for overflow
				}
			}
		} else {
			if(num_words > SIZE_C(2)) // adjust for larger types
				return bijson_error_value_out_of_range;
			if(exponent_size > sizeof(size_t))
				return bijson_error_value_out_of_range;
			size_t exponent = SIZE_C(10);
			if(exponent + last_word_index * SIZE_C(19) > SIZE_C(19)) // adjust for larger types
				return bijson_error_value_out_of_range;

			for(size_t word_index = SIZE_C(0); word_index <= last_word_index; word_index++) {
				uint64_t word = word_index == last_word_index
					? last_word
					: _bijson_read_minimal_int(
							analysis.significand.buffer + word_index * sizeof(uint64_t),
							sizeof(uint64_t)
						);
				// FIXME: check for overflow
				word *= _bijson_uint64_pow10(SIZE_C(19) * word_index + exponent);
				value += word; // FIXME: check for overflow
			}
		}
	} else if((type & BYTE_C(0xFE)) == BYTE_C(0x1A)) {
		bool significand_negative = type & BYTE_C(0x1);
		if(negative_result)
			*negative_result = significand_negative;
		else if(significand_negative)
			_BIJSON_RETURN_ERROR(bijson_error_value_out_of_range);

		size_t significand_size = buffer.size - SIZE_C(1);
		if(significand_size > SIZE_MAX - ((size_t)sizeof(uint64_t) - SIZE_C(1))
			return bijson_error_value_out_of_range;

		size_t num_words = (significand_size + (size_t)sizeof(uint64_t) - SIZE_C(1))
			/ (size_t)sizeof(uint64_t);
		size_t last_word_index = num_words - SIZE_C(1);
		size_t last_word_offset = last_word_index * (size_t)sizeof(uint64_t);
		uint64_t last_word = _bijson_read_minimal_int(
			buffer + SIZE_C(1) + last_word_offset,
			analysis.significand_size - last_word_offset
		);

		if(num_words > SIZE_C(2))  // adjust for larger types
			return bijson_error_value_out_of_range;

		for(size_t word_index = SIZE_C(0); word_index <= last_word_index; word_index++) {
			uint64_t word = word_index == last_word_index
				? last_word
				: _bijson_read_minimal_int(
						analysis.significand.buffer + word_index * sizeof(uint64_t),
						sizeof(uint64_t)
					);
			// FIXME: check for overflow
			word *= _bijson_uint64_pow10(SIZE_C(19) * word_index);
			value += word; // FIXME: check for overflow
		}
	} else {
		_BIJSON_RETURN_ERROR(bijson_error_type_mismatch);
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
	struct _bijson_decimal_analysis analysis;
	_BIJSON_RETURN_ON_ERROR(_bijson_decimal_analyze(bijson, &analysis));

	if(analysis.significand_negative)
		_BIJSON_RETURN_ON_ERROR(callback(callback_data, "-", SIZE_C(1)));
	_BIJSON_RETURN_ON_ERROR(_bijson_decimal_part_to_json(&analysis.significand, callback, callback_data));
	_BIJSON_RETURN_ON_ERROR(callback(callback_data, "e", SIZE_C(1)));
	if(analysis.exponent_negative)
		_BIJSON_RETURN_ON_ERROR(callback(callback_data, "-", SIZE_C(1)));
	return _bijson_decimal_part_to_json(&analysis.exponent, callback, callback_data);
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

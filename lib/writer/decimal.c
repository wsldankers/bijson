#include <stdbool.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>

#include "../../include/writer.h"

#include "../common.h"
#include "../io.h"
#include "../writer.h"

 __attribute__((const))
static inline bool _bijson_is_ascii_digit(int c) {
	return '0' <= c && c <= '9';
}

 __attribute__((const))
static inline unsigned int _bijson_ascii_digit(int c) {
	assert(_bijson_is_ascii_digit(c));
	return (unsigned int)(c - '0');
}

 __attribute__((pure))
static inline int _bijson_compare_digits(const byte_t *a_start, size_t a_len, const byte_t *b_start, size_t b_len) {
	assert(!a_len || *a_start != '0');
	assert(!b_len || *b_start != '0');
	return a_len == b_len
		? memcmp(a_start, b_start, a_len)
		: a_len < b_len ? -1 : 1;
}

static inline bijson_error_t _bijson_shift_digits(const byte_t *start, size_t len, const byte_t *decimal_point, size_t shift, bijson_output_callback_t write, void *write_data) {
	assert(!len || *start != '0');

	if(!len)
		return NULL;

	size_t big_shift = shift / SIZE_C(19);
	_BIJSON_RETURN_ON_ERROR(_bijson_io_write_nul_bytes(write, write_data, big_shift * sizeof(uint64_t)));
	shift -= big_shift * SIZE_C(19);

	uint64_t magnitude = _bijson_uint64_pow10((unsigned int)shift);

	if(write == _bijson_io_bytecounter_output_callback) {
		// crude hack
		size_t skip = len / SIZE_C(19);
		*(size_t *)write_data += skip * sizeof(uint64_t);
		len -= skip * SIZE_C(19);
	}

	const byte_t *s = start + len;
	uint64_t word = 0;
	while(s-- != start) {
		if(s == decimal_point)
			continue;
		uint64_t value = _bijson_ascii_digit(*s);
		if(magnitude == UINT64_C(10000000000000000000)) {
			_BIJSON_RETURN_ON_ERROR(_bijson_writer_write_minimal_int(write, write_data, word, sizeof word));
			word = value;
			magnitude = UINT64_C(1);
		} else {
			word += value * magnitude;
			magnitude *= UINT64_C(10);
		}
	}

	assert(word);

	word--;
	_BIJSON_RETURN_ON_ERROR(_bijson_writer_write_minimal_int(write, write_data, word, _bijson_fit_uint64(word)));

	return NULL;
}

static bijson_error_t _bijson_add_digits(const byte_t *a_start, size_t a_len, const byte_t *b_start, size_t b_len, bijson_output_callback_t write, void *write_data) {
	assert(!a_len || *a_start != '0');
	assert(!b_len || *b_start != '0');

	if(!a_len)
		return b_len ? _bijson_shift_digits(b_start, b_len, NULL, 0, write, write_data) : NULL;
	else if(!b_len)
		return _bijson_shift_digits(a_start, a_len, NULL, 0, write, write_data);

	size_t magnitude = 1;
	uint64_t carry = UINT64_C(0);
	const byte_t *a = a_start + a_len;
	const byte_t *b = b_start + b_len;
	uint64_t word = UINT64_C(0);
	for(;;) {
		a--;
		b--;
		uint64_t a_value = a < a_start ? UINT64_C(0) : _bijson_ascii_digit(*a);
		uint64_t b_value = b < b_start ? UINT64_C(0) : _bijson_ascii_digit(*b);
		uint64_t a_b_sum = a_value + b_value + carry;
		carry = a_b_sum >= UINT64_C(10);
		if(carry)
			a_b_sum -= UINT64_C(10);
		word += a_b_sum * magnitude;
		if(a <= a_start && b <= b_start && !carry)
			break;
		magnitude *= UINT64_C(10);
		if(magnitude == UINT64_C(10000000000000000000)) {
			_BIJSON_RETURN_ON_ERROR(_bijson_writer_write_minimal_int(write, write_data, word, sizeof word));
			magnitude = UINT64_C(1);
			word = UINT64_C(0);
		}
		if(a <= a_start && b <= b_start) {
			// only reached if carry==1
			word += magnitude;
			break;
		}
	}

	assert(word);

	return _bijson_writer_write_minimal_int(write, write_data, word, _bijson_fit_uint64(word - 1));
}

static bijson_error_t _bijson_subtract_digits(const byte_t *a_start, size_t a_len, const byte_t *b_start, size_t b_len, bijson_output_callback_t write, void *write_data) {
	// Compute a-b; a must not be smaller than b. Neither a nor be can have
	// leading zeroes.
	assert(!a_len || *a_start != '0');
	assert(!b_len || *b_start != '0');
	assert(_bijson_compare_digits(a_start, a_len, b_start, b_len) >= 0);

	if(!a_len)
		return NULL;

	if(!b_len)
		return _bijson_shift_digits(a_start, a_len, NULL, 0, write, write_data);

	size_t magnitude = 1;
	uint64_t carry = UINT64_C(0);
	const byte_t *a = a_start + a_len;
	const byte_t *b = b_start + b_len;
	uint64_t word = UINT64_C(0);
	bool pending_word = false;
	uint64_t pending_zeroes = UINT64_C(0);
	for(;;) {
		a--;
		b--;
		uint64_t a_value = _bijson_ascii_digit(*a);
		uint64_t b_value = b < b_start ? UINT64_C(0) : _bijson_ascii_digit(*b);
		uint64_t a_b_diff = UINT64_C(10) + a_value - b_value - carry;
		carry = a_b_diff < UINT64_C(10);
		if(!carry)
			a_b_diff -= UINT64_C(10);

		if(a_b_diff) {
			// Ok, we have a new contender for the new most significant word.
			// Write out any delayed stuff to make place for it.
			if(pending_word) {
				_BIJSON_RETURN_ON_ERROR(_bijson_writer_write_minimal_int(write, write_data, word, sizeof word));
				pending_word = false;
				word = a_b_diff * magnitude;
			} else {
				word += a_b_diff * magnitude;
			}

			if(pending_zeroes) {
				size_t zeroes_size = pending_zeroes * sizeof word;
				_BIJSON_RETURN_ON_ERROR(_bijson_io_write_nul_bytes(write, write_data, zeroes_size));
				pending_zeroes = UINT64_C(0);
			}
		}

		if(a == a_start)
			break;

		magnitude *= UINT64_C(10);
		if(magnitude == UINT64_C(10000000000000000000)) {
			// We can't write out diff immediately, it might be the most
			// significant word, which requires special handling.
			if(word)
				pending_word = true;
			else
				pending_zeroes++;
			magnitude = UINT64_C(1);
		}
	}

	assert(!carry);

	if(!word)
		return NULL;

	word--;
	return _bijson_writer_write_minimal_int(write, write_data, word, _bijson_fit_uint64(word));
}

typedef struct _bijson_string_analysis {
	const byte_t *significand_start;
	const byte_t *significand_end;
	const byte_t *decimal_point;
	const byte_t *exponent_start;
	const byte_t *exponent_end;
	bool mantissa_negative;
	bool exponent_negative;
} _bijson_string_analysis_t;

static const _bijson_string_analysis_t _bijson_string_analysis_0 = {0};

static inline bijson_error_t _bijson_analyze_string(_bijson_string_analysis_t *result, const byte_t *string, size_t len) {
	*result = _bijson_string_analysis_0;

	const byte_t *string_end = string + len;

	int c = *string;
	result->mantissa_negative = c == '-';
	if(result->mantissa_negative || c == '+')
		string++;

	while(string != string_end) {
		c = *string;
		if(c == '0') {
			string++;
		} else {
			if(!_bijson_is_ascii_digit(c))
				break;
			if(!result->significand_start)
				result->significand_start = string;
			string++;
			result->significand_end = string;
		}
	}

	result->decimal_point = string;

	if(string != string_end && *string == '.') {
		string++;
		while(string != string_end) {
			c = *string;
			if(c == '0') {
				string++;
			} else {
				if(!_bijson_is_ascii_digit(c))
					break;
				if(!result->significand_start)
					result->significand_start = string;
				string++;
				result->significand_end = string;
			}
		}
	}

	if(!result->significand_start)
		result->significand_start =
		result->significand_end = result->decimal_point;

	result->exponent_start = string;
	result->exponent_end = string_end;

	if(string != string_end) {
		c = *string;
		if(c == 'e' || c == 'E') {
			string++;

			if(string != string_end) {
				c = *string;
				result->exponent_negative = c == '-';
				if(result->exponent_negative || c == '+')
					string++;
			}

			while(string != string_end && *string == '0')
				string++;

			result->exponent_start = string;

			while(string != string_end && _bijson_is_ascii_digit(*string))
				string++;
		}
	}

	return string == string_end ? NULL : bijson_error_invalid_decimal_syntax;
}

static bijson_error_t _bijson_decimal_buffer_push_writer(void *spool, const void *data, size_t len) {
	return _bijson_buffer_push(spool, data, len);
}

bijson_error_t bijson_writer_add_decimal_from_string(bijson_writer_t *writer, const void *string, size_t len) {
	if(writer->failed)
		_BIJSON_RETURN_ERROR(bijson_error_writer_failed);
	_BIJSON_RETURN_ON_ERROR(_bijson_writer_check_expect_value(writer));
	if(!len)
		_BIJSON_RETURN_ERROR(bijson_error_parameter_is_zero);

	_bijson_string_analysis_t string_analysis;
	_BIJSON_WRITER_ERROR_RETURN(_bijson_analyze_string(&string_analysis, string, len));

	// fprintf(stderr, "significand_start = %zu\n", string_analysis.significand_start - string);
	// fprintf(stderr, "significand_end = %zu\n", string_analysis.significand_end - string);
	// fprintf(stderr, "decimal_point = %zu\n", string_analysis.decimal_point - string);
	// fprintf(stderr, "exponent_start = %zu\n", string_analysis.exponent_start - string);
	// fprintf(stderr, "exponent_end = %zu\n", string_analysis.exponent_end - string);
	// fprintf(stderr, "mantissa_negative = %d\n", string_analysis.mantissa_negative);
	// fprintf(stderr, "exponent_negative = %d\n", string_analysis.exponent_negative);

	if(string_analysis.significand_start == string_analysis.significand_end) {
		// The number is 0 so don't waste any time optimizing it:
		_BIJSON_WRITER_ERROR_RETURN(_bijson_buffer_push_byte(&writer->spool, _bijson_spool_type_scalar));
		_BIJSON_WRITER_ERROR_RETURN(_bijson_buffer_push_size(&writer->spool, SIZE_C(1)));
		_BIJSON_WRITER_ERROR_RETURN(_bijson_buffer_push_byte(&writer->spool, BYTE_C(0x1A) | string_analysis.mantissa_negative));
		writer->expect = writer->expect_after_value;
		return NULL;
	}

	bool shift_negative = string_analysis.decimal_point < string_analysis.significand_end;
	size_t shift = shift_negative
		? _bijson_ptrdiff(string_analysis.significand_end, string_analysis.decimal_point) - SIZE_C(1)
		: _bijson_ptrdiff(string_analysis.decimal_point, string_analysis.significand_end);

	typedef struct output_parameters {
		size_t total_size;
		size_t shift_adjustment;
		size_t mantissa_size;
		size_t exponent_size;
		size_t adjusted_shift_string_len;
		int exponent_adjusted_shift_cmp;
		byte_t adjusted_shift_string[__SIZEOF_SIZE_T__ * 5 / 2 + 1];
		bool exponent_negative;
		bool adjusted_shift_negative;
	} output_parameters_t;

	output_parameters_t best_output_parameters = {
		.total_size = SIZE_MAX,
		.exponent_negative = string_analysis.exponent_negative,
	};

	// Moving part of the exponent to the mantissa may reduce the size of the
	// exponent while not affecting the size of the mantissa too much due to
	// byte-level packing granularity. Try a few sizes to see what is optimal.
	size_t max_shift_adjustment = shift_negative && string_analysis.exponent_negative ? SIZE_C(0) : SIZE_C(10);
	// (better criterion: in_exp > -shift)

	for(size_t shift_adjustment = 0; shift_adjustment <= max_shift_adjustment; shift_adjustment++) {
		size_t adjusted_shift;
		bool adjusted_shift_negative;
		if(shift_negative) {
			adjusted_shift_negative = true;
			adjusted_shift = shift + shift_adjustment;
		} else {
			adjusted_shift_negative = shift < shift_adjustment;
			adjusted_shift = adjusted_shift_negative
				? shift_adjustment - shift
				: shift - shift_adjustment;
		}

		output_parameters_t output_parameters = {
			.exponent_negative = string_analysis.exponent_negative,
			.shift_adjustment = shift_adjustment,
			.adjusted_shift_negative = adjusted_shift_negative,
			.adjusted_shift_string_len = _bijson_uint64_str_raw(output_parameters.adjusted_shift_string, adjusted_shift),
		};

		if(adjusted_shift_negative == string_analysis.exponent_negative) {
			// add the shift to the exponent
			_BIJSON_WRITER_ERROR_RETURN(_bijson_add_digits(
				string_analysis.exponent_start, _bijson_ptrdiff(string_analysis.exponent_end, string_analysis.exponent_start),
				output_parameters.adjusted_shift_string, output_parameters.adjusted_shift_string_len,
				_bijson_io_bytecounter_output_callback, &output_parameters.exponent_size
			));
		} else {
			// if the exponent is larger than the shift:
			output_parameters.exponent_adjusted_shift_cmp = _bijson_compare_digits(
				string_analysis.exponent_start, _bijson_ptrdiff(string_analysis.exponent_end, string_analysis.exponent_start),
				output_parameters.adjusted_shift_string, output_parameters.adjusted_shift_string_len
			);
			if(output_parameters.exponent_adjusted_shift_cmp < 0) {
				// if the exponent is smaller than the shift, subtract the
				// exponent from the shift and invert the sign
				_BIJSON_WRITER_ERROR_RETURN(_bijson_subtract_digits(
					output_parameters.adjusted_shift_string, output_parameters.adjusted_shift_string_len,
					string_analysis.exponent_start, _bijson_ptrdiff(string_analysis.exponent_end, string_analysis.exponent_start),
					_bijson_io_bytecounter_output_callback, &output_parameters.exponent_size
				));
				output_parameters.exponent_negative = shift_negative;
			} else if(output_parameters.exponent_adjusted_shift_cmp > 0) {
				// if the exponent is larger than the shift, subtract the shift from the exponent
				_BIJSON_WRITER_ERROR_RETURN(_bijson_subtract_digits(
					string_analysis.exponent_start, _bijson_ptrdiff(string_analysis.exponent_end, string_analysis.exponent_start),
					output_parameters.adjusted_shift_string, output_parameters.adjusted_shift_string_len,
					_bijson_io_bytecounter_output_callback, &output_parameters.exponent_size
				));
			}
		}

		// pack the mantissa with shift adjusted by this adjustment
		_BIJSON_WRITER_ERROR_RETURN(_bijson_shift_digits(
			string_analysis.significand_start, _bijson_ptrdiff(string_analysis.significand_end, string_analysis.significand_start),
			string_analysis.decimal_point, shift_adjustment,
			_bijson_io_bytecounter_output_callback, &output_parameters.mantissa_size
		));
		assert(output_parameters.mantissa_size);

		output_parameters.total_size = SIZE_C(1) + output_parameters.mantissa_size;
		if(output_parameters.exponent_size)
			output_parameters.total_size +=
				_bijson_optimal_storage_size_bytes(_bijson_optimal_storage_size1(output_parameters.exponent_size))
				 + output_parameters.exponent_size;

		// Prefer the smallest total output size.
		// If they are equal in length, prefer a fully integer version over one with an exponent.
		if(best_output_parameters.total_size > output_parameters.total_size || (
			best_output_parameters.total_size == output_parameters.total_size
			&& best_output_parameters.exponent_size
			&& !output_parameters.exponent_size
		))
			best_output_parameters = output_parameters;
	}

	_BIJSON_WRITER_ERROR_RETURN(_bijson_buffer_push_byte(&writer->spool, _bijson_spool_type_scalar));
	_BIJSON_WRITER_ERROR_RETURN(_bijson_buffer_push_size(&writer->spool, best_output_parameters.total_size));
#ifndef NDEBUG
	size_t spool_used = writer->spool.used;
#endif
	byte_t type = (byte_t)(best_output_parameters.exponent_size
		? BYTE_C(0x20)
			| _bijson_optimal_storage_size1(best_output_parameters.exponent_size)
			| ((byte_compute_t)string_analysis.mantissa_negative << 2U)
			| ((byte_compute_t)best_output_parameters.exponent_negative << 3U)
		: BYTE_C(0x1A) | (byte_compute_t)string_analysis.mantissa_negative
	);
	_BIJSON_WRITER_ERROR_RETURN(_bijson_buffer_push_byte(&writer->spool, type));

	if(best_output_parameters.exponent_size) {
		size_t exponent_size_1 = best_output_parameters.exponent_size - SIZE_C(1);
		_BIJSON_WRITER_ERROR_RETURN(_bijson_writer_write_compact_int(
			_bijson_decimal_buffer_push_writer, &writer->spool,
			exponent_size_1, _bijson_optimal_storage_size(exponent_size_1)
		));

		if(best_output_parameters.adjusted_shift_negative == string_analysis.exponent_negative) {
			// add the shift to the exponent
			_BIJSON_WRITER_ERROR_RETURN(_bijson_add_digits(
				string_analysis.exponent_start, _bijson_ptrdiff(string_analysis.exponent_end, string_analysis.exponent_start),
				best_output_parameters.adjusted_shift_string, best_output_parameters.adjusted_shift_string_len,
				_bijson_decimal_buffer_push_writer, &writer->spool
			));
		} else {
			if(best_output_parameters.exponent_adjusted_shift_cmp < 0) {
				// if the exponent is smaller than the shift, subtract the exponent from the shift
				_BIJSON_WRITER_ERROR_RETURN(_bijson_subtract_digits(
					best_output_parameters.adjusted_shift_string, best_output_parameters.adjusted_shift_string_len,
					string_analysis.exponent_start, _bijson_ptrdiff(string_analysis.exponent_end, string_analysis.exponent_start),
					_bijson_decimal_buffer_push_writer, &writer->spool
				));
			} else if(best_output_parameters.exponent_adjusted_shift_cmp > 0) {
				// if the exponent is larger than the shift, subtract the shift from the exponent
				_BIJSON_WRITER_ERROR_RETURN(_bijson_subtract_digits(
					string_analysis.exponent_start, _bijson_ptrdiff(string_analysis.exponent_end, string_analysis.exponent_start),
					best_output_parameters.adjusted_shift_string, best_output_parameters.adjusted_shift_string_len,
					_bijson_decimal_buffer_push_writer, &writer->spool
				));
			}
		}
	}

	// packing the mantissa and exponent with the optimal shift adjustment
	_BIJSON_WRITER_ERROR_RETURN(_bijson_shift_digits(
		string_analysis.significand_start, _bijson_ptrdiff(string_analysis.significand_end, string_analysis.significand_start),
		string_analysis.decimal_point, best_output_parameters.shift_adjustment,
		_bijson_decimal_buffer_push_writer, &writer->spool
	));

	assert(writer->spool.used - spool_used == best_output_parameters.total_size);

	writer->expect = writer->expect_after_value;
	return NULL;
}


bijson_error_t bijson_writer_begin_decimal_from_string(bijson_writer_t *writer) {
	if(writer->failed)
		_BIJSON_RETURN_ERROR(bijson_error_writer_failed);
	_BIJSON_RETURN_ON_ERROR(_bijson_writer_check_expect_value(writer));

	// Switch the role of the stack and spool:
	_BIJSON_WRITER_ERROR_RETURN(_bijson_buffer_push_size(&writer->spool, writer->stack.used));

	writer->expect = _bijson_writer_expect_more_decimal_string;
	return NULL;
}

bijson_error_t bijson_writer_append_decimal_from_string(bijson_writer_t *writer, const void *string, size_t len) {
	if(writer->failed)
		_BIJSON_RETURN_ERROR(bijson_error_writer_failed);
	if(writer->expect != _bijson_writer_expect_more_decimal_string)
		_BIJSON_RETURN_ERROR(bijson_error_unmatched_end);

	return _bijson_buffer_push(&writer->stack, string, len);
}

bijson_error_t bijson_writer_end_decimal_from_string(bijson_writer_t *writer) {
	if(writer->failed)
		_BIJSON_RETURN_ERROR(bijson_error_writer_failed);
	if(writer->expect != _bijson_writer_expect_more_decimal_string)
		_BIJSON_RETURN_ERROR(bijson_error_unmatched_end);

	size_t spool_used;
	_bijson_buffer_pop(&writer->spool, &spool_used, sizeof spool_used);

	size_t string_len = writer->stack.used - spool_used;

	_BIJSON_WRITER_ERROR_RETURN(bijson_writer_add_decimal_from_string(
		writer,
		_bijson_buffer_access(&writer->stack, spool_used, string_len),
		string_len
	));

	_bijson_buffer_pop(&writer->stack, NULL, string_len);

	writer->expect = writer->expect_after_value;
	return NULL;
}

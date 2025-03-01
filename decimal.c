#include <stdbool.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <stdint.h>

#include "decimal.h"
#include "format.h"
#include "writer.h"

static inline bool _bijson_is_ascii_digit(char c) {
	return '0' <= c && c <= '9';
}

static inline int _bijson_compare_digits(const char *a_start, size_t a_len, const char *b_start, size_t b_len) {
	assert(!a_len || *a_start != '0');
	assert(!b_len || *b_start != '0');
	return a_len > b_len ? 1 : a_len < b_len ? -1 : memcmp(a_start, b_start, a_len);
}

static bool _bijson_add_digits(const char *a_start, size_t a_len, const char *b_start, size_t b_len, _bijson_writer_write_func_t write, void *write_data) {
	assert(!a_len || *a_start != '0');
	assert(!b_len || *b_start != '0');

	if(!a_len || !b_len)
		return 0;

	size_t magnitude = 1;
	bool carry = 0;
	const char *a = a_start + a_len;
	const char *b = b_start + b_len;
	uint64_t sum = 0;
	for(;;) {
		a--;
		b--;
		uint64_t a_value = a < a_start ? 0 : *a - '0';
		uint64_t b_value = b < b_start ? 0 : *b - '0';
		uint64_t a_b_sum = a_value + b_value + carry;
		carry = a_b_sum >= 10;
		if(carry)
			a_b_sum -= 10;
		sum += a_b_sum * magnitude;
		if(a <= a_start && b <= b_start && !carry)
			break;
		magnitude *= UINT64_C(10);
		if(magnitude == UINT64_C(10000000000000000000)) {
			if(!_bijson_writer_write_minimal_int(write, write_data, sum, sizeof sum))
				return false;
			magnitude = 1;
			sum = 0;
		}
		if(a <= a_start && b <= b_start) {
			// only reached if carry==1
			sum += magnitude;
			break;
		}
	}

	assert(sum);

	if(!_bijson_writer_write_minimal_int(write, write_data, sum, _bijson_fit_uint64(sum - 1)))
		return false;

	return true;
}

static bool _bijson_subtract_digits(const char *a_start, size_t a_len, const char *b_start, size_t b_len, _bijson_writer_write_func_t write, void *write_data) {
	// Compute a-b; a must not be smaller than b. Neither a nor be can have
	// leading zeroes.
	assert(!a_len || *a_start != '0');
	assert(!b_len || *b_start != '0');
	assert(_bijson_compare_digits(a_start, a_len, b_start, b_len) >= 0);

	if(!a_len || !b_len)
		return 0;

	size_t magnitude = 1;
	bool carry = 0;
	const char *a = a_start + a_len;
	const char *b = b_start + b_len;
	uint64_t diff = 0;
	bool pending_diff = false;
	uint64_t pending_zeroes = 0;
	for(;;) {
		a--;
		b--;
		uint64_t a_value = *a - '0';
		uint64_t b_value = b < b_start ? 0 : *b - '0';
		uint64_t a_b_diff = 10 + a_value - b_value - carry;
		carry = a_b_diff < 10;
		if(!carry)
			a_b_diff -= 10;

		if(a_b_diff) {
			// Ok, we have a new contender for the new most significant word.
			// Write out any delayed stuff to make place for it.
			if(pending_diff) {
				if(!_bijson_writer_write_minimal_int(write, write_data, diff, sizeof diff))
					return false;
				pending_diff = false;
				diff = a_b_diff * magnitude;
			} else {
				diff += a_b_diff * magnitude;
			}

			if(pending_zeroes) {
				size_t zeroes_size = pending_zeroes * sizeof diff;
				if(!write(write_data, NULL, zeroes_size))
					return false;
				pending_zeroes = 0;
			}
		}

		if(a == a_start)
			break;

		magnitude *= UINT64_C(10);
		if(magnitude == UINT64_C(10000000000000000000)) {
			// We can't write out diff immediately, it might be the most
			// significant word, which requires special handling.
			if(diff)
				pending_diff = true;
			else
				pending_zeroes++;
			magnitude = 1;
		}
	}

	assert(!carry);

	if(diff && !_bijson_writer_write_minimal_int(write, write_data, diff, _bijson_fit_uint64(diff - 1)))
		return false;

	return true;
}

typedef struct _bijson_digit_analysis {
	size_t digits;
	size_t output_size;
	size_t msb_output_size;
	size_t less_significant_words;
	size_t most_significant_digits;
	uint64_t most_significant_word;
} _bijson_digit_analysis_t;

static const _bijson_digit_analysis_t digit_analysis_0 = {};

static inline void _bijson_analyze_digits(_bijson_digit_analysis_t *result, const char *start, const char *end, const char *decimal_point, size_t shift) {
	// the first and last digit must be non-zero.
	// if decimal_point is not NULL, it must point somewhere in the range of start..end
	result->digits = (end - start) - (bool)decimal_point + shift;
	result->output_size = 0;
	result->msb_output_size = 0;
	result->less_significant_words = 0;
	result->most_significant_digits = 0;
	result->most_significant_word = 0;
	if(result->digits) {
		result->less_significant_words = (result->digits + 18) / 19 - 1;
		result->most_significant_digits = result->digits - result->less_significant_words * 19;
		const char *most_significant_end = start + result->most_significant_digits;
		if(most_significant_end > decimal_point)
			most_significant_end++;
		for(const char *msd = start; msd < most_significant_end; msd++) {
			if(msd != decimal_point) {
				// msd may exceed end in the presence of a non-zero shift
				if(msd < end)
					result->most_significant_word = result->most_significant_word * 10 + *msd - '0';
				else
					result->most_significant_word = result->most_significant_word * 10;
			}
		}
		result->most_significant_word--;

		result->msb_output_size = _bijson_fit_uint64(result->most_significant_word);

		result->output_size = result->less_significant_words * sizeof(uint64_t) + result->msb_output_size;
	}
}

bool bijson_writer_add_decimal_from_string(bijson_writer_t *writer, const char *string, size_t len) {
	if(!len)
		return false;

	const char *string_pointer = string;

	bool mantissa_negative = *string_pointer == '-';
	if(mantissa_negative || *string_pointer == '+')
		string_pointer++;

	const char *string_end = string + len;

	while(string_pointer != string_end && *string_pointer == '0')
		string_pointer++;
	const char *mantissa_start = string_pointer;

	while(string_pointer != string_end && _bijson_is_ascii_digit(*string_pointer))
		string_pointer++;

	const char *mantissa_end = string_pointer;

	const char *decimal_point = NULL;

	if(string_pointer != string_end && *string_pointer == '.') {
		decimal_point = string_pointer++;
		while(string_pointer != string_end) {
			char c = *string_pointer;
			if(_bijson_is_ascii_digit(*string_pointer)) {
				string_pointer++;
				if(*string_pointer != '0')
					mantissa_end = string_pointer;
			} else {
				break;
			}
		}
	}

	const char *exponent_start = NULL;
	if(string_pointer != string_end) {
		int c = *string_pointer;
		if(c == 'e' || c == 'E') {
			string_pointer++;

			while(string_pointer != string_end && *string_pointer == '0')
				string_pointer++;

			exponent_start = string_pointer;

			while(string_pointer != string_end && _bijson_is_ascii_digit(*string_pointer))
				string_pointer++;
		}
	}

	if(string_pointer != string_end)
		return false;

	// compute the number of trailing zeros before any decimal point in the mantissa
	// if the exponent is at most a single digit, and the aforementioned number of
	// trailing zeros added up are at most 5, try to represent the number without
	// an exponent.

	return true;
}

#include <stdbool.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <xxhash.h>

#include "../include/reader.h"

#include "common.h"
#include "io.h"

static inline bijson_error_t _bijson_check_bijson(const bijson_t *bijson) {
	if(!bijson || !bijson->buffer)
		return bijson_error_parameter_is_null;
	if(!bijson->size)
		return bijson_error_parameter_is_zero;
	return NULL;
}

static inline uint64_t _bijson_read_minimal_int(const byte_t *buffer, size_t nbytes) {
	uint64_t r = 0;
	for(size_t u = 0; u < nbytes; u++)
		r |= (uint64_t)buffer[u] << (u * SIZE_C(8));
	return r;
}

static const byte_t _bijson_hex[16] = "0123456789ABCDEF";

static inline bijson_error_t _bijson_raw_string_to_json(const bijson_t *bijson, bijson_output_callback_t callback, void *callback_data) {
	const byte_t *string = bijson->buffer;
	size_t len = bijson->size;
	const byte_t *previously_written = string;
	byte_t escape[2] = "\\";
	byte_t unicode_escape[6] = "\\u00";
	_BIJSON_ERROR_RETURN(callback(callback_data, "\"", 1));
	const byte_t *string_end = string + len;
	while(string < string_end) {
		const byte_t *string_pos = string;
		int c = *string++;
		byte_t plain_escape = 0;
		switch(c) {
			case '"':
				plain_escape = '"';
				break;
			case '\\':
				plain_escape = '\\';
				break;
			case '\b':
				plain_escape = 'b';
				break;
			case '\f':
				plain_escape = 'f';
				break;
			case '\n':
				plain_escape = 'n';
				break;
			case '\r':
				plain_escape = 'r';
				break;
			case '\t':
				plain_escape = 't';
				break;
			default:
				if(c >= 32)
					continue;
		}
		if(string_pos > previously_written)
			_BIJSON_ERROR_RETURN(callback(callback_data, previously_written, _bijson_ptrdiff(string_pos, previously_written)));
		previously_written = string;
		if(plain_escape) {
			escape[1] = plain_escape;
			_BIJSON_ERROR_RETURN(callback(callback_data, escape, sizeof escape));
		} else {
			unicode_escape[4] = _bijson_hex[(size_t)c >> 4U];
			unicode_escape[5] = _bijson_hex[(size_t)c & SIZE_C(0xF)];
			_BIJSON_ERROR_RETURN(callback(callback_data, unicode_escape, sizeof unicode_escape));
		}
	}
	if(string > previously_written)
		_BIJSON_ERROR_RETURN(callback(callback_data, previously_written, (size_t)(string - previously_written)));
	return callback(callback_data, "\"", SIZE_C(1));
}

static inline bijson_error_t _bijson_string_to_json(const bijson_t *bijson, bijson_output_callback_t callback, void *userdata) {
	bijson_t raw_string = { .buffer = (const byte_t *)bijson->buffer + SIZE_C(1), .size = bijson->size - SIZE_C(1) };
	_BIJSON_ERROR_RETURN(_bijson_check_valid_utf8(raw_string.buffer, raw_string.size));
	return _bijson_raw_string_to_json(&raw_string, callback, userdata);
}

static inline bijson_error_t _bijson_decimal_part_to_json(const bijson_t *bijson, bijson_output_callback_t callback, void *callback_data) {
	const byte_t *buffer = bijson->buffer;
	size_t size = bijson->size;

	size_t last_word_size = ((size - SIZE_C(1)) & SIZE_C(0x7)) + SIZE_C(1);

	const byte_t *last_word_start = buffer + size - last_word_size;
	uint64_t last_word = _bijson_read_minimal_int(last_word_start, last_word_size);
	if(last_word > UINT64_C(9999999999999999998))
		return bijson_error_file_format_error;
	last_word++;

	char word_chars[20];
	int sprintf_result = sprintf(word_chars, "%"PRIu64, last_word);
	if(sprintf_result < 0)
		return bijson_error_internal_error;
	_BIJSON_ERROR_RETURN(callback(callback_data, word_chars, (size_t)sprintf_result));

	const byte_t *word_start = last_word_start;
	while(word_start > buffer) {
		word_start -= sizeof(uint64_t);
		uint64_t word = _bijson_read_minimal_int(word_start, sizeof(uint64_t));
		if(word > UINT64_C(9999999999999999999))
			return bijson_error_file_format_error;
		sprintf_result = sprintf(word_chars, "%019"PRIu64, word);
		_BIJSON_ERROR_RETURN(callback(callback_data, word_chars, (size_t)sprintf_result));
	}

	return NULL;
}

static inline bijson_error_t _bijson_decimal_to_json(const bijson_t *bijson, bijson_output_callback_t callback, void *callback_data) {
	const byte_t *buffer = bijson->buffer;
	const byte_t *buffer_end = buffer + bijson->size;

	byte_compute_t type = *buffer;

	const byte_t *exponent_size_location = buffer + SIZE_C(1);
	if(exponent_size_location == buffer_end)
		return bijson_error_file_format_error;

	size_t exponent_size_size = SIZE_C(1) << (type & BYTE_C(0x3));
	const byte_t *exponent_start = exponent_size_location + exponent_size_size;
	if(exponent_start + SIZE_C(1) > buffer_end)
		return bijson_error_file_format_error;
	uint64_t raw_exponent_size = _bijson_read_minimal_int(exponent_size_location, exponent_size_size);
	if(raw_exponent_size > SIZE_MAX - SIZE_C(1))
		return bijson_error_file_format_error;
	size_t exponent_size = (size_t)raw_exponent_size + SIZE_C(1);
	if(exponent_size > _bijson_ptrdiff(buffer_end, exponent_start) - SIZE_C(1))
		return bijson_error_file_format_error;

	if(type & BYTE_C(0x4))
		_BIJSON_ERROR_RETURN(callback(callback_data, "-", SIZE_C(1)));

	const byte_t *significand_start = exponent_start + exponent_size;
	bijson_t significand = {
		significand_start,
		_bijson_ptrdiff(buffer_end, significand_start),
	};
	_BIJSON_ERROR_RETURN(_bijson_decimal_part_to_json(&significand, callback, callback_data));

	_BIJSON_ERROR_RETURN(callback(callback_data, "e", SIZE_C(1)));

	if(type & BYTE_C(0x8))
		_BIJSON_ERROR_RETURN(callback(callback_data, "-", SIZE_C(1)));

	bijson_t exponent = {
		exponent_start,
		exponent_size,
	};
	return _bijson_decimal_part_to_json(&exponent, callback, callback_data);
}

static inline bijson_error_t _bijson_decimal_integer_to_json(const bijson_t *bijson, bijson_output_callback_t callback, void *callback_data) {
	const byte_t *buffer = bijson->buffer;
	size_t size = bijson->size;

	byte_compute_t type = *buffer;
	if(type & BYTE_C(0x1))
		_BIJSON_ERROR_RETURN(callback(callback_data, "-", SIZE_C(1)));

	if(size == SIZE_C(1))
		return callback(callback_data, "0", SIZE_C(1));

	bijson_t integer = {
		buffer + SIZE_C(1),
		size - SIZE_C(1),
	};
	return _bijson_decimal_part_to_json(&integer, callback, callback_data);
}

typedef struct _bijson_object_analysis {
	size_t count;
	size_t count_1;
	const byte_t *key_index;
	size_t key_index_item_size;
	size_t last_key_end_offset;
	const byte_t *key_data_start;
	const byte_t *value_index;
	size_t value_index_item_size;
	const byte_t *value_data_start;
	size_t value_data_size;
} _bijson_object_analysis_t;

static inline bijson_error_t _bijson_object_analyze_count(const bijson_t *bijson, _bijson_object_analysis_t *analysis) {
	_BIJSON_ERROR_RETURN(_bijson_check_bijson(bijson));

	IF_DEBUG(memset(analysis, 'A', sizeof *analysis));

	const byte_t *buffer = bijson->buffer;
	const byte_t *buffer_end = buffer + bijson->size;

	byte_compute_t type = *buffer;
	if((type & BYTE_C(0xC0)) != BYTE_C(0x40))
		return bijson_error_type_mismatch;

	const byte_t *count_location = buffer + SIZE_C(1);
	if(count_location == buffer_end) {
		analysis->count = SIZE_C(0);
		return NULL;
	}

	size_t count_size = SIZE_C(1) << (type & BYTE_C(0x3));
	const byte_t *key_index = count_location + count_size;
	if(key_index > buffer_end)
		return bijson_error_file_format_error;
	uint64_t raw_count = _bijson_read_minimal_int(count_location, count_size);
	if(raw_count > SIZE_MAX - SIZE_C(1))
		return bijson_error_file_format_error;
	size_t count_1 = (size_t)raw_count;

	analysis->count = count_1 + SIZE_C(1);
	analysis->count_1 = count_1;
	analysis->key_index = key_index;

	return NULL;
}

static inline bijson_error_t _bijson_object_count(const bijson_t *bijson, size_t *result) {
	_bijson_object_analysis_t analysis;
	_BIJSON_ERROR_RETURN(_bijson_object_analyze_count(bijson, &analysis));
	*result = analysis.count;
	return NULL;
}

bijson_error_t bijson_object_count(const bijson_t *bijson, size_t *result) {
	return _bijson_object_count(bijson, result);
}

bijson_error_t bijson_analyzed_object_count(const bijson_object_analysis_t *analysis, size_t *result) {
	*result = ((const _bijson_object_analysis_t *)analysis)->count;
	return NULL;
}

static inline bijson_error_t _bijson_object_analyze(const bijson_t *bijson, _bijson_object_analysis_t *analysis) {
	_BIJSON_ERROR_RETURN(_bijson_object_analyze_count(bijson, analysis));
	if(!analysis->count)
		return NULL;

	const byte_t *buffer = bijson->buffer;
	const byte_t *buffer_end = buffer + bijson->size;
	byte_compute_t type = *buffer;

	size_t count = analysis->count;
	size_t count_1 = analysis->count_1;
	size_t index_and_data_size = _bijson_ptrdiff(buffer_end, analysis->key_index);
	size_t key_index_item_size = SIZE_C(1) << ((type >> 2U) & BYTE_C(0x3));
	size_t value_index_item_size = SIZE_C(1) << ((type >> 4U) & BYTE_C(0x3));
	// We need at least one key_index_item, one value_index_item, and one
	// type byte for each item, but the first item does not have a value
	// index entry, so fake that.
	if(count > (index_and_data_size + value_index_item_size)
		/ (key_index_item_size + value_index_item_size + SIZE_C(1))
	)
		return bijson_error_file_format_error;

	const byte_t *value_index = analysis->key_index + count * key_index_item_size;
	const byte_t *key_data_start = value_index + count_1 * value_index_item_size;

	uint64_t raw_last_key_end_offset =
		_bijson_read_minimal_int(analysis->key_index + key_index_item_size * count_1, key_index_item_size);
	if(raw_last_key_end_offset > SIZE_MAX)
		return bijson_error_file_format_error;
	size_t last_key_end_offset = (size_t)raw_last_key_end_offset;

	if(last_key_end_offset > _bijson_ptrdiff(buffer_end, key_data_start) - count)
		return bijson_error_file_format_error;

	const byte_t *value_data_start = key_data_start + last_key_end_offset;

	analysis->key_index_item_size = key_index_item_size;
	analysis->last_key_end_offset = last_key_end_offset;
	analysis->key_data_start = key_data_start;
	analysis->value_index = value_index;
	analysis->value_index_item_size = value_index_item_size;
	analysis->value_data_start = value_data_start;
	analysis->value_data_size = _bijson_ptrdiff(buffer_end, value_data_start);

	return NULL;
}

static inline bijson_error_t _bijson_analyzed_object_get_index(
	const _bijson_object_analysis_t *analysis,
	size_t index,
	const void **key_buffer_result,
	size_t *key_size_result,
	bijson_t *value_result
) {
	size_t count = analysis->count;
	if(index >= count)
		return bijson_error_index_out_of_range;

	const byte_t *key_index = analysis->key_index;
	size_t key_index_item_size = analysis->key_index_item_size;
	size_t last_key_end_offset = analysis->last_key_end_offset;

	uint64_t raw_key_start_offset = index
		? _bijson_read_minimal_int(key_index + key_index_item_size * (index - SIZE_C(1)), key_index_item_size)
		: UINT64_C(0);
	if(raw_key_start_offset > SIZE_MAX)
		return bijson_error_file_format_error;
	size_t key_start_offset = (size_t)raw_key_start_offset;
	if(key_start_offset > last_key_end_offset)
		return bijson_error_file_format_error;

	size_t count_1 = analysis->count_1;
	uint64_t raw_key_end_offset = index == count_1
		? last_key_end_offset
		: _bijson_read_minimal_int(key_index + key_index_item_size * index, key_index_item_size);
	if(raw_key_end_offset > SIZE_MAX)
		return bijson_error_file_format_error;
	size_t key_end_offset = (size_t)raw_key_end_offset;
	if(key_end_offset > last_key_end_offset)
		return bijson_error_file_format_error;

	if(key_start_offset > key_end_offset)
		return bijson_error_file_format_error;

	const byte_t *value_index = analysis->value_index;
	size_t value_index_item_size = analysis->value_index_item_size;
	size_t value_data_size = analysis->value_data_size;

	// This is for comparing the raw offsets to, so it doesn't include
	// the implicit "+ index" term yet.
	size_t highest_valid_value_offset = value_data_size - count;

	uint64_t raw_value_start_offset = index
		? _bijson_read_minimal_int(value_index + value_index_item_size * (index - SIZE_C(1)), value_index_item_size)
		: UINT64_C(0);
	if(raw_value_start_offset > SIZE_MAX)
		return bijson_error_file_format_error;
	size_t value_start_offset = (size_t)raw_value_start_offset;
	if(value_start_offset > highest_valid_value_offset)
		return bijson_error_file_format_error;
	value_start_offset += index;

	uint64_t raw_value_end_offset = index == count_1
		? highest_valid_value_offset
		: _bijson_read_minimal_int(value_index + value_index_item_size * index, value_index_item_size);
	if(raw_value_end_offset > SIZE_MAX)
		return bijson_error_file_format_error;
	size_t value_end_offset = (size_t)raw_value_end_offset;

	if(value_end_offset > highest_valid_value_offset)
		return bijson_error_file_format_error;

	value_end_offset += index + SIZE_C(1);
	if(index == count_1)
		value_end_offset = value_data_size;

	if(value_start_offset >= value_end_offset)
		return bijson_error_file_format_error;

	const byte_t *key_buffer = analysis->key_data_start + key_start_offset;
	size_t key_size = key_end_offset - key_start_offset;

	// We could return an UTF-8 error but it's the file that's at fault here:
	bijson_error_t error = _bijson_check_valid_utf8(key_buffer, key_size);
	if(error == bijson_error_invalid_utf8)
		return bijson_error_file_format_error;
	else if(error)
		return error;

	*key_buffer_result = key_buffer;
	*key_size_result = key_size;
	value_result->buffer = analysis->value_data_start + value_start_offset;
	value_result->size = value_end_offset - value_start_offset;

	return NULL;
}

bijson_error_t bijson_object_get_index(
	const bijson_t *bijson,
	size_t index,
	const void **key_buffer_result,
	size_t *key_size_result,
	bijson_t *value_result
) {
	_bijson_object_analysis_t analysis;
	_BIJSON_ERROR_RETURN(_bijson_object_analyze(bijson, &analysis));
	return _bijson_analyzed_object_get_index(
		&analysis,
		index,
		key_buffer_result,
		key_size_result,
		value_result
	);
}

bijson_error_t bijson_analyzed_object_get_index(
	const bijson_object_analysis_t *analysis,
	size_t index,
	const void **key_buffer_result,
	size_t *key_size_result,
	bijson_t *value_result
) {
	return _bijson_analyzed_object_get_index(
		(const _bijson_object_analysis_t *)analysis,
		index,
		key_buffer_result,
		key_size_result,
		value_result
	);
}

#ifdef __SIZEOF_INT128__
typedef __uint128_t _bijson_hash_t;
#define _BIJSON_HASH_MAX (~(__uint128_t)0)
#define _BIJSON_HASH_C(x) ((__uint128_t)UINT64_C(x))
static inline _bijson_hash_t _bijson_integer_hash(XXH128_hash_t *xxhash) {
	return ((__uint128_t)xxhash->high64 << 64U) | (__uint128_t)xxhash->low64;
}
#else
typedef uint64_t _bijson_hash_t;
#define _BIJSON_HASH_MAX UINT64_MAX
#define _BIJSON_HASH_C(x) UINT64_C(x)
static inline _bijson_hash_t _bijson_integer_hash(XXH128_hash_t *xxhash) {
	return xxhash->high64;
}
#endif

typedef struct _bijson_get_key_entry {
	XXH128_hash_t xxhash;
	_bijson_hash_t hash;
	size_t index;
	const void *key;
	size_t len;
	bijson_t value;
} _bijson_get_key_entry_t;

static inline int _bijson_get_key_entry_cmp(const _bijson_get_key_entry_t *a, const _bijson_get_key_entry_t *b) {
	int c = XXH128_cmp(&a->xxhash, &b->xxhash);
	if(c)
		return c;
	return a->len < b->len
		? -1
		: a->len == b->len
			? memcmp(a->key, b->key, a->len)
			: 1;
}

static inline bijson_error_t _bijson_get_key_entry_get(const _bijson_object_analysis_t *analysis, _bijson_get_key_entry_t *entry) {
	_BIJSON_ERROR_RETURN(_bijson_analyzed_object_get_index(analysis, entry->index, &entry->key, &entry->len, &entry->value));
	entry->xxhash = XXH3_128bits(entry->key, entry->len);
	entry->hash = _bijson_integer_hash(&entry->xxhash);
	return NULL;
}

static inline size_t _bijson_get_key_guess(_bijson_get_key_entry_t *lower, _bijson_get_key_entry_t *upper, _bijson_get_key_entry_t *target) {
	if(lower->hash >= upper->hash)
		return lower->index + ((upper->index - lower->index) >> 1U);
	// Put in a temporary variable to appease -Wbad-function-cast
	long double offset = floorl(
		(long double)(upper->index - lower->index)
		* (long double)(target->hash - lower->hash)
		/ (long double)(upper->hash - lower->hash)
	);
	return lower->index + (size_t)offset;
}

 __attribute__((const))
static inline size_t _bijson_2log64(uint64_t x) {
	assert(x > UINT64_C(1));
	x--;
#ifdef HAVE_BUILTIN_CLZLL
	return SIZE_C(63) - (size_t)__builtin_clzll(x);
#else
	size_t result = SIZE_C(1);
	if(x & UINT64_C(0xFFFFFFFF00000000)) {
		result += SIZE_C(32);
		x >>= 32U;
	}
	if(x & UINT64_C(0xFFFF0000)) {
		result += SIZE_C(16);
		x >>= 16U;
	}
	if(x & UINT64_C(0xFF00)) {
		result += SIZE_C(8);
		x >>= 8U;
	}
	if(x & UINT64_C(0xF0)) {
		result += SIZE_C(4);
		x >>= 4U;
	}
	if(x & UINT64_C(0xC)) {
		result += SIZE_C(2);
		x >>= 2U;
	}
	if(x & UINT64_C(0x2)) {
		result += SIZE_C(1);
	}
	return result;
#endif
}

static inline bijson_error_t _bijson_analyzed_object_get_key(
	const _bijson_object_analysis_t *analysis,
	const char *key,
	size_t len,
	bijson_t *result
) {
	// static size_t record_attempts = SIZE_C(0);

	if(!analysis->count)
		return bijson_error_key_not_found;

	if(analysis->count == SIZE_C(1)) {
		bijson_t value;
		const void *candidate_key;
		size_t candidate_len;
		_BIJSON_ERROR_RETURN(_bijson_analyzed_object_get_index(analysis, SIZE_C(0), &candidate_key, &candidate_len, &value));
		if(candidate_len == len && !memcmp(key, candidate_key, len)) {
			*result = value;
			return NULL;
		}
		return bijson_error_key_not_found;
	}

	_bijson_get_key_entry_t target = {
		.xxhash = XXH3_128bits(key, len),
		.key = key,
		.len = len,
	};
	target.hash = _bijson_integer_hash(&target.xxhash);

	size_t max_attempts = _bijson_2log64(analysis->count);
	_bijson_get_key_entry_t lower = {0};
	_bijson_get_key_entry_t upper = {.index = analysis->count, .hash = _BIJSON_HASH_MAX};

	for(size_t attempt = SIZE_C(0); lower.index != upper.index; attempt++) {
		// if(attempt > record_attempts) {
		// 	record_attempts = attempt;
		// 	fprintf(stderr, "record_attempts: %zu (max_attempts: %zu)\n", record_attempts, max_attempts);
		// }

		_bijson_get_key_entry_t guess = {
			.index = attempt < max_attempts
				? _bijson_get_key_guess(&lower, &upper, &target)
				: lower.index + ((upper.index - lower.index) >> 1U)
		};
		_BIJSON_ERROR_RETURN(_bijson_get_key_entry_get(analysis, &guess));
		int c = _bijson_get_key_entry_cmp(&guess, &target);
		if(c == 0) {
			*result = guess.value;
			return NULL;
		} else if(c < 0) {
			lower.hash = guess.hash;
			lower.index = guess.index + SIZE_C(1);
		} else {
			upper.hash = guess.hash;
			upper.index = guess.index;
		}
	}

	return bijson_error_key_not_found;
}

bijson_error_t bijson_analyzed_object_get_key(
	const bijson_object_analysis_t *analysis,
	const char *key,
	size_t len,
	bijson_t *result
) {
	return _bijson_analyzed_object_get_key((const _bijson_object_analysis_t *)analysis, key, len, result);
}

bijson_error_t bijson_object_get_key(
	const bijson_t *bijson,
	const char *key,
	size_t len,
	bijson_t *result
) {
	_bijson_object_analysis_t analysis;
	_BIJSON_ERROR_RETURN(_bijson_object_analyze(bijson, &analysis));
	return _bijson_analyzed_object_get_key(&analysis, key, len, result);
}

static inline bijson_error_t _bijson_analyzed_object_get_key_range_upper(
	const _bijson_object_analysis_t *analysis,
	_bijson_get_key_entry_t *target,
	_bijson_get_key_entry_t *upper
) {
	// Last valid index
	size_t upper_index_1 = upper->index - SIZE_C(1);

	// Was already the last possible entry
	if(target->index == upper_index_1)
		return NULL;

	size_t jump = SIZE_C(1);
	size_t upper_bound;
	size_t lower_bound = target->index;

	for(;;) {
		if(jump > SIZE_MAX - target->index)
			upper_bound = SIZE_MAX;
		else
			upper_bound = target->index + jump;
		if(upper_bound > upper_index_1)
			upper_bound = upper_index_1;

		bijson_t value;
		const void *candidate_key;
		size_t candidate_len;
		_BIJSON_ERROR_RETURN(_bijson_analyzed_object_get_index(analysis, upper_bound, &candidate_key, &candidate_len, &value));
		if(candidate_len != target->len || memcmp(candidate_key, target->key, candidate_len))
			break;
		if(upper_bound == upper_index_1)
			return NULL;

		lower_bound = upper_bound;
		jump <<= 1U;
	}

	// We now know the boundary is somewhere between lower_bound and upper_bound
	// lower_bound always points at an equal key.
	// upper_bound always points at a higher key.
	// We stop when there are no more keys inbetween the two.

	while(lower_bound != upper_bound - SIZE_C(1)) {
		size_t guess = lower_bound + ((upper_bound - lower_bound) >> 1U);
		bijson_t value;
		const void *candidate_key;
		size_t candidate_len;
		_BIJSON_ERROR_RETURN(_bijson_analyzed_object_get_index(analysis, guess, &candidate_key, &candidate_len, &value));
		if(candidate_len == target->len && !memcmp(candidate_key, target->key, candidate_len))
			lower_bound = guess;
		else
			upper_bound = guess;
	}

	upper->index = upper_bound;
	return NULL;
}

static inline bijson_error_t _bijson_analyzed_object_get_key_range_lower(
	const _bijson_object_analysis_t *analysis,
	_bijson_get_key_entry_t *target,
	_bijson_get_key_entry_t *lower
) {
	// Was already the first possible entry
	if(target->index == lower->index)
		return NULL;

	size_t jump = SIZE_C(1);
	size_t upper_bound = target->index;
	size_t lower_bound;

	for(;;) {
		if(jump > target->index)
			lower_bound = SIZE_C(0);
		else
			lower_bound = target->index - jump;
		if(lower_bound < lower->index)
			lower_bound = lower->index;

		bijson_t value;
		const void *candidate_key;
		size_t candidate_len;
		_BIJSON_ERROR_RETURN(_bijson_analyzed_object_get_index(analysis, lower_bound, &candidate_key, &candidate_len, &value));
		if(candidate_len != target->len || memcmp(candidate_key, target->key, candidate_len))
			break;
		if(lower_bound == lower->index)
			return NULL;

		upper_bound = lower_bound;
		jump <<= 1U;
	}

	// We now know the boundary is somewhere between lower_bound and upper_bound
	// lower_bound always points at a lower key.
	// upper_bound always points at an equal key.
	// We stop when there are no more keys inbetween the two.

	while(lower_bound != upper_bound - SIZE_C(1)) {
		size_t guess = lower_bound + ((upper_bound - lower_bound) >> 1U);
		bijson_t value;
		const void *candidate_key;
		size_t candidate_len;
		_BIJSON_ERROR_RETURN(_bijson_analyzed_object_get_index(analysis, guess, &candidate_key, &candidate_len, &value));
		if(candidate_len == target->len && !memcmp(candidate_key, target->key, candidate_len))
			upper_bound = guess;
		else
			lower_bound = guess;
	}

	lower->index = upper_bound;
	return NULL;
}

static inline bijson_error_t _bijson_analyzed_object_get_key_range(
	const _bijson_object_analysis_t *analysis,
	const char *key,
	size_t len,
	size_t *start_index,
	size_t *end_index
) {
	if(!analysis->count)
		return bijson_error_key_not_found;

	if(analysis->count == SIZE_C(1)) {
		bijson_t value;
		const void *candidate_key;
		size_t candidate_len;
		_BIJSON_ERROR_RETURN(_bijson_analyzed_object_get_index(analysis, SIZE_C(0), &candidate_key, &candidate_len, &value));
		if(candidate_len == len && !memcmp(key, candidate_key, len)) {
			*start_index = SIZE_C(0);
			*end_index = SIZE_C(1);
			return NULL;
		}
		return bijson_error_key_not_found;
	}

	_bijson_get_key_entry_t target = {
		.xxhash = XXH3_128bits(key, len),
		.key = key,
		.len = len,
	};
	target.hash = _bijson_integer_hash(&target.xxhash);

	size_t max_attempts = _bijson_2log64(analysis->count);
	_bijson_get_key_entry_t lower = {0};
	_bijson_get_key_entry_t upper = {.index = analysis->count, .hash = _BIJSON_HASH_MAX};

	for(size_t attempt = SIZE_C(0); lower.index != upper.index; attempt++) {
		_bijson_get_key_entry_t guess = {
			.index = attempt < max_attempts
				? _bijson_get_key_guess(&lower, &upper, &target)
				: lower.index + ((upper.index - lower.index) >> 1U)
		};
		_BIJSON_ERROR_RETURN(_bijson_get_key_entry_get(analysis, &guess));
		int c = _bijson_get_key_entry_cmp(&guess, &target);
		if(c == 0) {
			target.index = guess.index;
			_BIJSON_ERROR_RETURN(_bijson_analyzed_object_get_key_range_lower(analysis, &target, &lower));
			_BIJSON_ERROR_RETURN(_bijson_analyzed_object_get_key_range_upper(analysis, &target, &upper));
			*start_index = lower.index;
			*end_index = upper.index;
			return NULL;
		} else if(c < 0) {
			lower.hash = guess.hash;
			lower.index = guess.index + SIZE_C(1);
		} else {
			upper.hash = guess.hash;
			upper.index = guess.index;
		}
	}

	return bijson_error_key_not_found;
}

bijson_error_t bijson_analyzed_object_get_key_range(
	const bijson_object_analysis_t *analysis,
	const char *key,
	size_t len,
	size_t *start_index,
	size_t *end_index
) {
	return _bijson_analyzed_object_get_key_range((const _bijson_object_analysis_t *)analysis, key, len, start_index, end_index);
}

bijson_error_t bijson_object_get_key_range(
	const bijson_t *bijson,
	const char *key,
	size_t len,
	size_t *start_index,
	size_t *end_index
) {
	_bijson_object_analysis_t analysis;
	_BIJSON_ERROR_RETURN(_bijson_object_analyze(bijson, &analysis));
	return _bijson_analyzed_object_get_key_range(&analysis, key, len, start_index, end_index);
}

bijson_error_t bijson_object_analyze(const bijson_t *bijson, bijson_object_analysis_t *result) {
	assert(sizeof(_bijson_object_analysis_t) <= sizeof(*result));
	return _bijson_object_analyze(bijson, (_bijson_object_analysis_t *)result);
}

typedef struct _bijson_array_analysis {
	size_t count;
	size_t count_1;
	size_t index_item_size;
	size_t highest_valid_offset;
	const byte_t *item_index;
	const byte_t *item_data_start;
} _bijson_array_analysis_t;

static inline bijson_error_t _bijson_array_analyze_count(const bijson_t *bijson, _bijson_array_analysis_t *analysis) {
	_BIJSON_ERROR_RETURN(_bijson_check_bijson(bijson));
	if(!analysis)
		return bijson_error_parameter_is_null;

	IF_DEBUG(memset(analysis, 'A', sizeof *analysis));

	const byte_t *buffer = bijson->buffer;
	const byte_t *buffer_end = buffer + bijson->size;

	byte_compute_t type = *buffer;
	if((type & BYTE_C(0xF0)) != BYTE_C(0x30))
		return bijson_error_type_mismatch;

	const byte_t *count_location = buffer + SIZE_C(1);
	if(count_location == buffer_end) {
		analysis->count = SIZE_C(0);
		return NULL;
	}

	size_t count_size = SIZE_C(1) << (type & BYTE_C(0x3));
	const byte_t *item_index = count_location + count_size;
	if(item_index > buffer_end)
		return bijson_error_file_format_error;
	uint64_t raw_count = _bijson_read_minimal_int(count_location, count_size);
	if(raw_count > SIZE_MAX - SIZE_C(1))
		return bijson_error_file_format_error;
	size_t count_1 = (size_t)raw_count;

	analysis->count = count_1 + SIZE_C(1);
	analysis->count_1 = count_1;
	analysis->item_index = item_index;

	return NULL;
}

static inline bijson_error_t _bijson_array_count(const bijson_t *bijson, size_t *result) {
	_bijson_array_analysis_t analysis;
	_BIJSON_ERROR_RETURN(_bijson_array_analyze_count(bijson, &analysis));
	*result = analysis.count;
	return NULL;
}

bijson_error_t bijson_array_count(const bijson_t *bijson, size_t *result) {
	return _bijson_array_count(bijson, result);
}

bijson_error_t bijson_analyzed_array_count(const bijson_array_analysis_t *analysis, size_t *result) {
	*result = ((const _bijson_array_analysis_t *)analysis)->count;
	return NULL;
}

static inline bijson_error_t _bijson_array_analyze(const bijson_t *bijson, _bijson_array_analysis_t *analysis) {
	_BIJSON_ERROR_RETURN(_bijson_array_analyze_count(bijson, analysis));
	if(!analysis->count)
		return NULL;

	const byte_t *buffer = bijson->buffer;
	const byte_t *buffer_end = buffer + bijson->size;

	byte_compute_t type = *buffer;

	const byte_t *item_index = analysis->item_index;
	size_t index_and_data_size = _bijson_ptrdiff(buffer_end, item_index);

	size_t index_item_size = SIZE_C(1) << ((type >> 2U) & BYTE_C(0x3));
	// We need at least one index_item and one type byte for each item,
	// but the first item does not have an index entry, so fake that.
	size_t count = analysis->count;
	if(count > (index_and_data_size + index_item_size) / (index_item_size + SIZE_C(1)))
		return bijson_error_file_format_error;

	const byte_t *item_data_start = item_index + (count - SIZE_C(1)) * index_item_size;
	size_t item_data_size = _bijson_ptrdiff(buffer_end, item_data_start);

	analysis->index_item_size = index_item_size;
	analysis->item_data_start = item_data_start;
	// This is for comparing the raw offsets to, so it doesn't include
	// the implicit "+ index" term yet.
	analysis->highest_valid_offset = item_data_size - count;

	return NULL;
}

static inline bijson_error_t _bijson_analyzed_array_get_index(const _bijson_array_analysis_t *analysis, size_t index, bijson_t *result) {
	if(!analysis)
		return bijson_error_parameter_is_null;

	if(index >= analysis->count)
		return bijson_error_index_out_of_range;

	uint64_t raw_start_offset = index
		? _bijson_read_minimal_int(analysis->item_index + analysis->index_item_size * (index - SIZE_C(1)), analysis->index_item_size)
		: UINT64_C(0);
	if(raw_start_offset > SIZE_MAX)
		return bijson_error_file_format_error;
	size_t start_offset = (size_t)raw_start_offset;
	if(start_offset > analysis->highest_valid_offset)
		return bijson_error_file_format_error;
	start_offset += index;

	uint64_t raw_end_offset = index == analysis->count_1
		? analysis->highest_valid_offset
		: _bijson_read_minimal_int(analysis->item_index + analysis->index_item_size * index, analysis->index_item_size);
	if(raw_end_offset > SIZE_MAX)
		return bijson_error_file_format_error;
	size_t end_offset = (size_t)raw_end_offset;
	if(end_offset > analysis->highest_valid_offset)
		return bijson_error_file_format_error;
	end_offset += index + SIZE_C(1);

	if(start_offset >= end_offset)
		return bijson_error_file_format_error;

	result->buffer = analysis->item_data_start + start_offset;
	result->size = end_offset - start_offset;

	return NULL;
}

bijson_error_t bijson_array_get_index(const bijson_t *bijson, size_t index, bijson_t *result) {
	_bijson_array_analysis_t analysis;
	_BIJSON_ERROR_RETURN(_bijson_array_analyze(bijson, &analysis));
	return _bijson_analyzed_array_get_index(&analysis, index, result);
}

bijson_error_t bijson_analyzed_array_get_index(const bijson_array_analysis_t *analysis, size_t index, bijson_t *result) {
	return _bijson_analyzed_array_get_index((const _bijson_array_analysis_t *)analysis, index, result);
}

bijson_error_t bijson_array_analyze(const bijson_t *bijson, bijson_array_analysis_t *result) {
	assert(sizeof(_bijson_array_analysis_t) <= sizeof(*result));
	return _bijson_array_analyze(bijson, (_bijson_array_analysis_t *)result);
}

bijson_error_t bijson_get_value_type(const bijson_t *bijson, bijson_value_type_t *result) {
	_BIJSON_ERROR_RETURN(_bijson_check_bijson(bijson));
	const byte_t *buffer = bijson->buffer;
	const byte_compute_t type = *buffer;

	switch(type & BYTE_C(0xF0)) {
		case BYTE_C(0x00):
			switch(type) {
				case BYTE_C(0x00):
					return bijson_error_file_format_error;
				case BYTE_C(0x01):
					return *result = bijson_value_type_null, NULL;
				case BYTE_C(0x05):
					return *result = bijson_value_type_undefined, NULL;
				case BYTE_C(0x02):
					return *result = bijson_value_type_false, NULL;
				case BYTE_C(0x03):
					return *result = bijson_value_type_true, NULL;
				case BYTE_C(0x08):
					return *result = bijson_value_type_string, NULL;
				case BYTE_C(0x09):
					return *result = bijson_value_type_bytes, NULL;
				case BYTE_C(0x0A):
					return *result = bijson_value_type_iee754_2008_float, NULL;
				case BYTE_C(0x0B):
					return *result = bijson_value_type_iee754_2008_decimal_float, NULL;
				case BYTE_C(0x0C):
					return *result = bijson_value_type_iee754_2008_decimal_float_dpd, NULL;
			}
			break;
		case BYTE_C(0x10):
			switch(type & BYTE_C(0xFE)) {
				case BYTE_C(0x10):
					return *result = bijson_value_type_snan, NULL;
				case BYTE_C(0x12):
					return *result = bijson_value_type_qnan, NULL;
				case BYTE_C(0x14):
					return *result = bijson_value_type_inf, NULL;
				case BYTE_C(0x18):
					return *result = bijson_value_type_integer, NULL;
				case BYTE_C(0x1A):
					return *result = bijson_value_type_decimal, NULL;
			}
			break;
		case BYTE_C(0x20):
			return *result = bijson_value_type_decimal, NULL;
		case BYTE_C(0x30):
			return *result = bijson_value_type_array, NULL;
		case BYTE_C(0x40):
		case BYTE_C(0x50):
		case BYTE_C(0x60):
		case BYTE_C(0x70):
			return *result = bijson_value_type_object, NULL;
	}

	return bijson_error_unsupported_data_type;
}

static inline bijson_error_t _bijson_object_to_json(const bijson_t *bijson, bijson_output_callback_t callback, void *callback_data) {
	_bijson_object_analysis_t analysis;
	_BIJSON_ERROR_RETURN(_bijson_object_analyze(bijson, &analysis));

	_BIJSON_ERROR_RETURN(callback(callback_data, "{", 1));

	for(size_t u = 0; u < analysis.count; u++) {
		if(u)
			_BIJSON_ERROR_RETURN(callback(callback_data, ",", 1));

		bijson_t key, value;
		_BIJSON_ERROR_RETURN(_bijson_analyzed_object_get_index(&analysis, u, &key.buffer, &key.size, &value));
		_BIJSON_ERROR_RETURN(_bijson_raw_string_to_json(&key, callback, callback_data));
		_BIJSON_ERROR_RETURN(callback(callback_data, ":", 1));
		_BIJSON_ERROR_RETURN(bijson_to_json(&value, callback, callback_data));
	}

	return callback(callback_data, "}", 1);
}

static inline bijson_error_t _bijson_array_to_json(const bijson_t *bijson, bijson_output_callback_t callback, void *callback_data) {
	_bijson_array_analysis_t analysis;
	_BIJSON_ERROR_RETURN(_bijson_array_analyze(bijson, &analysis));

	_BIJSON_ERROR_RETURN(callback(callback_data, "[", 1));

	for(size_t u = 0; u < analysis.count; u++) {
		if(u)
			_BIJSON_ERROR_RETURN(callback(callback_data, ",", 1));

		bijson_t item;
		_BIJSON_ERROR_RETURN(_bijson_analyzed_array_get_index(&analysis, u, &item));
		_BIJSON_ERROR_RETURN(bijson_to_json(&item, callback, callback_data));
	}

	return callback(callback_data, "]", 1);
}

bijson_error_t bijson_to_json(const bijson_t *bijson, bijson_output_callback_t callback, void *callback_data) {
	_BIJSON_ERROR_RETURN(_bijson_check_bijson(bijson));
	const byte_t *buffer = bijson->buffer;

	const byte_compute_t type = *buffer;

	switch(type & BYTE_C(0xF0)) {
		case BYTE_C(0x00):
			switch(type) {
				case BYTE_C(0x00):
					return bijson_error_file_format_error;
				case BYTE_C(0x01):
				case BYTE_C(0x05): // undefined
					return callback(callback_data, "null", 4);
				case BYTE_C(0x02):
					return callback(callback_data, "false", 5);
				case BYTE_C(0x03):
					return callback(callback_data, "true", 4);
				case BYTE_C(0x08):
					return _bijson_string_to_json(bijson, callback, callback_data);
			}
			break;
		case BYTE_C(0x10):
			switch(type & BYTE_C(0xFE)) {
				case BYTE_C(0x1A):
					return _bijson_decimal_integer_to_json(bijson, callback, callback_data);
			}
			break;
		case BYTE_C(0x20):
			return _bijson_decimal_to_json(bijson, callback, callback_data);
			break;
		case BYTE_C(0x30):
			return _bijson_array_to_json(bijson, callback, callback_data);
			break;
		case BYTE_C(0x40):
		case BYTE_C(0x50):
		case BYTE_C(0x60):
		case BYTE_C(0x70):
			return _bijson_object_to_json(bijson, callback, callback_data);
			break;
	}

	return bijson_error_unsupported_data_type;
}

typedef struct _bijson_to_json_state {
	const bijson_t *bijson;
} _bijson_to_json_state_t;

static bijson_error_t _bijson_to_json_callback(
	void *action_callback_data,
	bijson_output_callback_t output_callback,
	void *output_callback_data
) {
	return bijson_to_json(
		((_bijson_to_json_state_t *)action_callback_data)->bijson,
		output_callback,
		output_callback_data
	);
}

bijson_error_t bijson_to_json_fd(const bijson_t *bijson, int fd) {
	_bijson_to_json_state_t state = {bijson};
	return _bijson_io_write_to_fd(_bijson_to_json_callback, &state, fd, NULL);
}

bijson_error_t bijson_to_json_FILE(const bijson_t *bijson, FILE *file) {
	_bijson_to_json_state_t state = {bijson};
	return _bijson_io_write_to_FILE(_bijson_to_json_callback, &state, file, NULL);
}

bijson_error_t bijson_to_json_malloc(
	const bijson_t *bijson,
	const void **result_buffer,
	size_t *result_size
) {
	_bijson_to_json_state_t state = {bijson};
	return _bijson_io_write_to_malloc(
		_bijson_to_json_callback,
		&state,
		result_buffer,
		result_size
	);
}

bijson_error_t bijson_to_json_tempfile(
	const bijson_t *bijson,
	const void **result_buffer,
	size_t *result_size
) {
	_bijson_to_json_state_t state = {bijson};
	return _bijson_io_write_to_tempfile(
		_bijson_to_json_callback,
		&state,
		result_buffer,
		result_size
	);
}

bijson_error_t bijson_to_json_bytecounter(
	const bijson_t *bijson,
	size_t *result_size
) {
	_bijson_to_json_state_t state = {bijson};
	return _bijson_io_write_bytecounter(
		_bijson_to_json_callback,
		&state,
		result_size
	);
}

bijson_error_t bijson_to_json_filename(const bijson_t *bijson, const char *filename) {
	_bijson_to_json_state_t state = {bijson};
	return _bijson_io_write_to_filename(_bijson_to_json_callback, &state, filename, NULL);
}

bijson_error_t bijson_open_filename(bijson_t *bijson, const char *filename) {
	return _bijson_io_read_from_filename(NULL, bijson, filename);
}

void bijson_close(bijson_t *bijson) {
	_bijson_io_close(bijson);
}

void bijson_free(bijson_t *bijson) {
	if(bijson) {
		free(_bijson_no_const(bijson->buffer));
		*bijson = bijson_0;
	}
}

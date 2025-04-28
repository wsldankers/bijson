
#include <stdbool.h>
#include <inttypes.h>
#include <string.h>
#include <xxhash.h>

#include "../../../include/reader.h"

#include "../../common.h"
#include "../object.h"
#include "key.h"

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
		_BIJSON_RETURN_ON_ERROR(_bijson_analyzed_object_get_index(analysis, upper_bound, &candidate_key, &candidate_len, &value));
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
		_BIJSON_RETURN_ON_ERROR(_bijson_analyzed_object_get_index(analysis, guess, &candidate_key, &candidate_len, &value));
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
		_BIJSON_RETURN_ON_ERROR(_bijson_analyzed_object_get_index(analysis, lower_bound, &candidate_key, &candidate_len, &value));
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
		_BIJSON_RETURN_ON_ERROR(_bijson_analyzed_object_get_index(analysis, guess, &candidate_key, &candidate_len, &value));
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
		_BIJSON_RETURN_ERROR(bijson_error_key_not_found);

	if(analysis->count == SIZE_C(1)) {
		bijson_t value;
		const void *candidate_key;
		size_t candidate_len;
		_BIJSON_RETURN_ON_ERROR(_bijson_analyzed_object_get_index(analysis, SIZE_C(0), &candidate_key, &candidate_len, &value));
		if(candidate_len == len && !memcmp(key, candidate_key, len)) {
			*start_index = SIZE_C(0);
			*end_index = SIZE_C(1);
			return NULL;
		}
		_BIJSON_RETURN_ERROR(bijson_error_key_not_found);
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
		_BIJSON_RETURN_ON_ERROR(_bijson_get_key_entry_get(analysis, &guess));
		int c = _bijson_get_key_entry_cmp(&guess, &target);
		if(c == 0) {
			target.index = guess.index;
			_BIJSON_RETURN_ON_ERROR(_bijson_analyzed_object_get_key_range_lower(analysis, &target, &lower));
			_BIJSON_RETURN_ON_ERROR(_bijson_analyzed_object_get_key_range_upper(analysis, &target, &upper));
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

	_BIJSON_RETURN_ERROR(bijson_error_key_not_found);
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
	_BIJSON_RETURN_ON_ERROR(_bijson_object_analyze(bijson, &analysis));
	return _bijson_analyzed_object_get_key_range(&analysis, key, len, start_index, end_index);
}

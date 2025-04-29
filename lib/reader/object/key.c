#include <stdbool.h>
#include <inttypes.h>
#include <string.h>
#include <math.h>
#include <xxhash.h>

#include "../../../include/reader.h"

#include "../../common.h"
#include "../object.h"
#include "index.h"
#include "key.h"

__attribute__((pure))
int _bijson_get_key_entry_cmp(const _bijson_get_key_entry_t *a, const _bijson_get_key_entry_t *b) {
	int c = XXH128_cmp(&a->xxhash, &b->xxhash);
	if(c)
		return c;
	return a->len == b->len
		? memcmp(a->key, b->key, a->len)
		: a->len < b->len ? -1 : 1;
}

__attribute__((pure))
size_t _bijson_get_key_guess(_bijson_get_key_entry_t *lower, _bijson_get_key_entry_t *upper, _bijson_get_key_entry_t *target) {
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

static inline bijson_error_t _bijson_analyzed_object_get_key(
	const _bijson_object_analysis_t *analysis,
	const char *key,
	size_t len,
	bijson_t *result
) {
	// static size_t record_attempts = SIZE_C(0);

	if(!analysis->count)
		_BIJSON_RETURN_ERROR(bijson_error_key_not_found);

	if(analysis->count == SIZE_C(1)) {
		bijson_t value;
		const void *candidate_key;
		size_t candidate_len;
		_BIJSON_RETURN_ON_ERROR(_bijson_analyzed_object_get_index(analysis, SIZE_C(0), &candidate_key, &candidate_len, &value));
		if(candidate_len == len && !memcmp(key, candidate_key, len)) {
			*result = value;
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
		// if(attempt > record_attempts) {
		// 	record_attempts = attempt;
		// 	fprintf(stderr, "record_attempts: %zu (max_attempts: %zu)\n", record_attempts, max_attempts);
		// }

		_bijson_get_key_entry_t guess = {
			.index = attempt < max_attempts
				? _bijson_get_key_guess(&lower, &upper, &target)
				: lower.index + ((upper.index - lower.index) >> 1U)
		};
		_BIJSON_RETURN_ON_ERROR(_bijson_get_key_entry_get(analysis, &guess));
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

	_BIJSON_RETURN_ERROR(bijson_error_key_not_found);
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
	_BIJSON_RETURN_ON_ERROR(_bijson_object_analyze(bijson, &analysis));
	return _bijson_analyzed_object_get_key(&analysis, key, len, result);
}

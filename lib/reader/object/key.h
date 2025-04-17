#pragma once

#include <xxhash.h>

#include "../../common.h"
#include "../object.h"
#include "index.h"

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

static inline bijson_error_t _bijson_get_key_entry_get(const _bijson_object_analysis_t *analysis, _bijson_get_key_entry_t *entry) {
	_BIJSON_ERROR_RETURN(_bijson_analyzed_object_get_index(analysis, entry->index, &entry->key, &entry->len, &entry->value));
	entry->xxhash = XXH3_128bits(entry->key, entry->len);
	entry->hash = _bijson_integer_hash(&entry->xxhash);
	return NULL;
}

extern int _bijson_get_key_entry_cmp(const _bijson_get_key_entry_t *a, const _bijson_get_key_entry_t *b);
extern size_t _bijson_get_key_guess(_bijson_get_key_entry_t *lower, _bijson_get_key_entry_t *upper, _bijson_get_key_entry_t *target);

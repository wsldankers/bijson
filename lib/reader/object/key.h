#pragma once

#include "../../common.h"
#include "../../rapidhash.h"
#include "../object.h"
#include "index.h"

typedef struct _bijson_get_key_entry {
	uint64_t hash;
	size_t index;
	const void *key;
	size_t len;
	bijson_t value;
} _bijson_get_key_entry_t;

static inline bijson_error_t _bijson_get_key_entry_get(const _bijson_object_analysis_t *analysis, _bijson_get_key_entry_t *entry) {
	_BIJSON_RETURN_ON_ERROR(_bijson_analyzed_object_get_index(analysis, entry->index, &entry->key, &entry->len, &entry->value));
	entry->hash = rapidhash(entry->key, entry->len);
	return NULL;
}

extern int _bijson_get_key_entry_cmp(const _bijson_get_key_entry_t *a, const _bijson_get_key_entry_t *b);
extern size_t _bijson_get_key_guess(_bijson_get_key_entry_t *lower, _bijson_get_key_entry_t *upper, _bijson_get_key_entry_t *target);

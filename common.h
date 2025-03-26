#pragma once

#include <assert.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>

#include <bijson/common.h>

#if __SIZEOF_SIZE_T__ == 8
#define SIZE_C(x) UINT64_C(x)
#elif __SIZEOF_SIZE_T__ == 4
#define SIZE_C(x) UINT32_C(x)
#else
#define SIZE_C(x) ((size_t)(x))
#endif

#ifdef NDEBUG
#define IF_DEBUG(x) do {} while(false)
#else
#define IF_DEBUG(x) do { x; } while(false)
#endif

typedef uint8_t byte;
#define BYTE_C(x) UINT8_C(x)

#define orz(x) (sizeof (x) / sizeof *(x))

#define _BIJSON_ERROR_CLEANUP_AND_RETURN(x, cleanup) do { bijson_error_t _error = (x); if(__builtin_expect((bool)_error, 0)) { cleanup; return _error; } } while(false)
#define _BIJSON_ERROR_RETURN(x) _BIJSON_ERROR_CLEANUP_AND_RETURN((x), do {} while(false))

#define bijson_0 ((bijson_t){0})

extern bijson_error_t _bijson_check_valid_utf8(const byte *string, size_t len);

static inline size_t _bijson_size_min(size_t a, size_t b) {
	return a < b ? a : b;
}

static inline size_t _bijson_size_max(size_t a, size_t b) {
	return a > b ? a : b;
}

static inline size_t _bijson_size_clamp(size_t min, size_t x, size_t max) {
	assert(min <= max);
	return x < min ? min : x > max ? max : x;
}

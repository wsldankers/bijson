#pragma once

#include <assert.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "../include/common.h"

#if __SIZEOF_SIZE_T__ < __SIZEOF_INT__
// Integer promotions would make this too much of a pain
#error "unsupported size_t size"
#endif

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

typedef uint8_t byte_t;
#define BYTE_C(x) x##U

// Types that will prevent integer promotion shenanigans during computations:
// (Integer promotion in some cases changes unsigned values into signed ones).
typedef unsigned int byte_compute_t;
typedef unsigned int uint8_compute_t;
#if __SIZEOF_INT__ == 2
typedef uint16_t uint16_compute_t;
typedef uint32_t uint32_compute_t;
#elif __SIZEOF_INT__ == 4
typedef unsigned int uint16_compute_t;
typedef uint32_t uint32_compute_t;
#else
typedef unsigned int uint16_compute_t;
typedef unsigned int uint32_compute_t;
#endif

#ifndef OFF_MAX
#define OFF_MAX ((((off_t)1 << (sizeof (off_t) * CHAR_BIT - 2)) - (off_t)1) * (off_t)2 + (off_t)1)
#endif

#define _BIJSON_ARRAY_COUNT(x) (sizeof (x) / sizeof *(x))

#ifdef HAVE_BUILTIN_EXPECT
#define _BIJSON_EXPECT_TRUE(x) __builtin_expect((x) != 0, 1)
#define _BIJSON_EXPECT_FALSE(x) __builtin_expect((x) != 0, 0)
#else
#define _BIJSON_EXPECT_TRUE(x) (x)
#define _BIJSON_EXPECT_FALSE(x) (x)
#endif

#define _BIJSON_ERROR_CLEANUP_AND_RETURN(x, cleanup) do { bijson_error_t _error = (x); if(_BIJSON_EXPECT_FALSE(_error)) { IF_DEBUG(fprintf(stderr, "\t%s:%d %s\n", __FILE__, __LINE__, _error)); cleanup; return _error; } } while(false)
#define _BIJSON_ERROR_RETURN(x) _BIJSON_ERROR_CLEANUP_AND_RETURN((x), do {} while(false))
#define _BIJSON_RETURN_ERROR(x) do { bijson_error_t _error = (x); IF_DEBUG(fprintf(stderr, "%s:%d %s\n", __FILE__, __LINE__, _error)); return _error; } while(false)

#define bijson_0 ((bijson_t){0})

 __attribute__((pure))
extern bijson_error_t _bijson_check_valid_utf8(const byte_t *string, size_t len);

__attribute__((const))
extern uint64_t _bijson_uint64_pow10(unsigned int exp);

// Note: these function do not add a trailing \0
extern size_t _bijson_uint64_str(byte_t *dst, uint64_t value);
extern size_t _bijson_uint64_str_padded(byte_t *dst, uint64_t value);
extern size_t _bijson_uint64_str_raw(byte_t *dst, uint64_t value);

__attribute__((const))
static inline size_t _bijson_size_min(size_t a, size_t b) {
	return a < b ? a : b;
}

__attribute__((const))
static inline size_t _bijson_size_max(size_t a, size_t b) {
	return a > b ? a : b;
}

__attribute__((const))
static inline size_t _bijson_size_clamp(size_t min, size_t x, size_t max) {
	assert(min <= max);
	return x < min ? min : x > max ? max : x;
}

// Sometimes we do need to lose the const qualifier. Do it explicitly.
__attribute__((const))
static inline void *_bijson_no_const(const void *p) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-qual"
	return (void *)p;
#pragma GCC diagnostic pop
}

#ifdef NDEBUG
__attribute__((const))
#else
__attribute__((pure))
#endif
static inline size_t _bijson_ptrdiff(const void *end, const void *start) {
	assert(start);
	assert(end);
	const byte_t *end_bytes = (const byte_t *)end;
	const byte_t *start_bytes = (const byte_t *)start;
	assert(end_bytes >= start_bytes);
	return (size_t)(end_bytes - start_bytes);
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

#include <stdbool.h>
#include <inttypes.h>

#include "common.h"

bijson_error_t _bijson_check_valid_utf8(const byte_t *string, size_t len) {
	if(!string)
		return len == 0 ? NULL : bijson_error_parameter_is_null;
	const byte_t *s = string;
	const byte_t *end = s + len;

	while(s != end) {
		uint8_compute_t c = *s++;
		if(!(c & BYTE_C(0x80)))
			continue; // short-circuit common case

		if((c & BYTE_C(0xE0)) == BYTE_C(0xC0)) { // 0b110.....
			// Basic characters can encode 7 bits.
			// Size 2 sequences can encode 5 + 6 = 11 bits.
			if(!(c & BYTE_C(0x1E)))
				return bijson_error_invalid_utf8; // overlong sequence
			if(s == end)
				return bijson_error_invalid_utf8; // premature end
			if((*s++ & BYTE_C(0xC0)) != BYTE_C(0x80))
				return bijson_error_invalid_utf8; // not a continuation byte
		} else if((c & BYTE_C(0xF0)) == BYTE_C(0xE0)) { // 0b1110....
			// Size 3 sequences can encode 4 + 6 + 6 = 16 bits.
			if(s == end)
				return bijson_error_invalid_utf8; // premature end
			uint8_compute_t c2 = *s++;
			if((c2 & BYTE_C(0xC0)) != BYTE_C(0x80))
				return bijson_error_invalid_utf8; // not a continuation byte
			if(!(c & BYTE_C(0x0F)) && !(c2 & BYTE_C(0x30)))
				return bijson_error_invalid_utf8; // overlong sequence
			if(c == BYTE_C(0xE0) && !(c2 & BYTE_C(0x20)))
				return bijson_error_invalid_utf8; // exclude U+0080..U+009F
			if(c == BYTE_C(0xED) && (c2 & BYTE_C(0x20)))
				return bijson_error_invalid_utf8; // UTF-16 surrogate
			if(s == end)
				return bijson_error_invalid_utf8; // premature end
			if((*s++ & BYTE_C(0xC0)) != BYTE_C(0x80))
				return bijson_error_invalid_utf8; // not a continuation byte
		} else if((c & BYTE_C(0xF8)) == BYTE_C(0xF0)) { // 0b11110...
			// Size 4 sequences can encode 3 + 6 + 6 + 6 = 21 bits.
			if(s == end)
				return bijson_error_invalid_utf8; // premature end
			uint8_compute_t c2 = *s++;
			if((c2 & BYTE_C(0xC0)) != BYTE_C(0x80))
				return bijson_error_invalid_utf8; // not a continuation byte
			if(!(c & BYTE_C(0x07)) && !(c2 & BYTE_C(0x30)))
				return bijson_error_invalid_utf8; // overlong sequence
			if(c == BYTE_C(0xF4)) {
				if(c2 & BYTE_C(0x30))
					return bijson_error_invalid_utf8; // outside Unicode code space
			} else if(c & BYTE_C(0x0C)) {
				return bijson_error_invalid_utf8; // outside Unicode code space
			}
			if(s == end)
				return bijson_error_invalid_utf8; // premature end
			if((*s++ & BYTE_C(0xC0)) != BYTE_C(0x80))
				return bijson_error_invalid_utf8; // not a continuation byte
			if(s == end)
				return bijson_error_invalid_utf8; // premature end
			if((*s++ & BYTE_C(0xC0)) != BYTE_C(0x80))
				return bijson_error_invalid_utf8; // not a continuation byte
		} else {
			return bijson_error_invalid_utf8; // invalid byte
		}
	}

	return NULL;
}

__attribute__((const))
inline uint64_t _bijson_uint64_pow10(unsigned int exp) {
	assert(exp < 20U);
#if 0
	uint64_t magnitude = UINT64_C(1);
	uint64_t magnitude_base = UINT64_C(10);
	// integer exponentiation
	while(exp) {
		if(exp & 1)
			magnitude *= magnitude_base;
		magnitude_base *= magnitude_base;
		exp >>= 1U;
	}
	return magnitude;
#else
	static const uint64_t powers[] = {
		UINT64_C(1),
		UINT64_C(10),
		UINT64_C(100),
		UINT64_C(1000),
		UINT64_C(10000),
		UINT64_C(100000),
		UINT64_C(1000000),
		UINT64_C(10000000),
		UINT64_C(100000000),
		UINT64_C(1000000000),
		UINT64_C(10000000000),
		UINT64_C(100000000000),
		UINT64_C(1000000000000),
		UINT64_C(10000000000000),
		UINT64_C(100000000000000),
		UINT64_C(1000000000000000),
		UINT64_C(10000000000000000),
		UINT64_C(100000000000000000),
		UINT64_C(1000000000000000000),
		UINT64_C(10000000000000000000),
	};
	return powers[exp];
#endif
}

__attribute__((hot))
static inline size_t _bijson_uint64_str_impl(byte_t *dst, uint64_t value, bool pad) {
	byte_t *s = dst;
#ifdef __GNUC__
#pragma GCC unroll 20
#endif
	for(unsigned int exp = 19U; exp < 20U; exp--) {
		uint64_t magnitude = _bijson_uint64_pow10(exp);
		uint64_t digit = value / magnitude;
		value -= digit * magnitude;
		if(digit)
			pad = true;
		if(pad)
			*s++ = (byte_t)('0' + digit);
	}
	return _bijson_ptrdiff(s, dst);
}

size_t _bijson_uint64_str(byte_t *dst, uint64_t value) {
	if(value) {
		return _bijson_uint64_str_impl(dst, value, false);
	} else {
		*dst = '0';
		return SIZE_C(1);
	}
}

size_t _bijson_uint64_str_padded(byte_t *dst, uint64_t value) {
	return _bijson_uint64_str_impl(dst, value, true);
}

size_t _bijson_uint64_str_raw(byte_t *dst, uint64_t value) {
	return _bijson_uint64_str_impl(dst, value, false);
}

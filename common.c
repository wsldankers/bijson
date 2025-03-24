#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sysexits.h>
#include <err.h>

#include "common.h"

bijson_error_t _bijson_check_valid_utf8(const byte *string, size_t len) {
	if(!string)
		return len == 0 ? NULL : bijson_error_parameter_is_null;
	const byte *s = string;
	const byte *end = s + len;

	while(s != end) {
		byte c = *s++;
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
			byte c2 = *s++;
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
			byte c2 = *s++;
			if((c2 & BYTE_C(0xC0)) != BYTE_C(0x80))
				return bijson_error_invalid_utf8; // not a continuation byte
			if(!(c & BYTE_C(0x07)) && !(c2 & BYTE_C(0x30)))
				return bijson_error_invalid_utf8; // overlong sequence
			if(c == BYTE_C(0xF4)) {
				if(c2 & BYTE_C(0x30))
					return bijson_error_invalid_utf8; // outside of BMP
			} else if(c & BYTE_C(0x0C)) {
				return bijson_error_invalid_utf8; // outside of BMP
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

/*
void u8test(void) {
	// reference: https://stackoverflow.com/questions/6555015/check-for-invalid-utf8
	union {
		uint32_t u;
		byte b[sizeof(uint32_t)];
	} x;
	x.u = 0;
	do {
		size_t len = 0;
		if(x.b[0] <= BYTE_C(0x7F))
			len = 1;
		else if(x.b[0] >= BYTE_C(0xC2) && x.b[0] <= BYTE_C(0xDF) && x.b[1] >= BYTE_C(0x80) && x.b[1] <= BYTE_C(0xBF))
			len = 2;
		else if(x.b[0] == BYTE_C(0xE0)                            && x.b[1] >= BYTE_C(0xA0) && x.b[1] <= BYTE_C(0xBF) && x.b[2] >= BYTE_C(0x80) && x.b[2] <= BYTE_C(0xBF))
			len = 3;
		else if(x.b[0] >= BYTE_C(0xE1) && x.b[0] <= BYTE_C(0xEC) && x.b[1] >= BYTE_C(0x80) && x.b[1] <= BYTE_C(0xBF) && x.b[2] >= BYTE_C(0x80) && x.b[2] <= BYTE_C(0xBF))
			len = 3;
		else if(x.b[0] == BYTE_C(0xED)                            && x.b[1] >= BYTE_C(0x80) && x.b[1] <= BYTE_C(0x9F) && x.b[2] >= BYTE_C(0x80) && x.b[2] <= BYTE_C(0xBF))
			len = 3;
		else if(x.b[0] >= BYTE_C(0xEE) && x.b[0] <= BYTE_C(0xEF) && x.b[1] >= BYTE_C(0x80) && x.b[1] <= BYTE_C(0xBF) && x.b[2] >= BYTE_C(0x80) && x.b[2] <= BYTE_C(0xBF))
			len = 3;
		else if(x.b[0] == BYTE_C(0xF0)                            && x.b[1] >= BYTE_C(0x90) && x.b[1] <= BYTE_C(0xBF) && x.b[2] >= BYTE_C(0x80) && x.b[2] <= BYTE_C(0xBF) && x.b[3] >= BYTE_C(0x80) && x.b[3] <= BYTE_C(0xBF))
			len = 4;
		else if(x.b[0] >= BYTE_C(0xF1) && x.b[0] <= BYTE_C(0xF3) && x.b[1] >= BYTE_C(0x80) && x.b[1] <= BYTE_C(0xBF) && x.b[2] >= BYTE_C(0x80) && x.b[2] <= BYTE_C(0xBF) && x.b[3] >= BYTE_C(0x80) && x.b[3] <= BYTE_C(0xBF))
			len = 4;
		else if(x.b[0] == BYTE_C(0xF4)                            && x.b[1] >= BYTE_C(0x80) && x.b[1] <= BYTE_C(0x8F) && x.b[2] >= BYTE_C(0x80) && x.b[2] <= BYTE_C(0xBF) && x.b[3] >= BYTE_C(0x80) && x.b[3] <= BYTE_C(0xBF))
			len = 4;

		if(len) {
			if(_bijson_check_valid_utf8(x.b, len))
				fprintf(stderr, "%02X %02X %02X %02X (%zu) should not have failed\n", x.b[0], x.b[1], x.b[2], x.b[3], len);
		} else for(len = 1; len <= 4; len++) {
			if(!_bijson_check_valid_utf8(x.b, len))
				fprintf(stderr, "%02X %02X %02X %02X (%zu) should have failed\n", x.b[0], x.b[1], x.b[2], x.b[3], len);
		}
	} while(x.u++ != UINT32_MAX);
}
*/

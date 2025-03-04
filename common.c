#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sysexits.h>
#include <err.h>

void *_bijson_xalloc(size_t len) {
#ifdef NDEBUG
	char *buffer = malloc(len);
	if (!buffer)
		errx(EX_OSERR, "malloc");
	return buffer;
#else
	len += sizeof(size_t) * 4;
	char *buffer = malloc(len);
	if (!buffer)
		errx(EX_OSERR, "malloc");
	memset(buffer, 'A', len);
	*(size_t *)buffer = len;
	return buffer + sizeof(size_t) * 2;
#endif
}

void _bijson_xfree(void *buffer) {
#ifdef NDEBUG
	free(buffer);
#else
	if(buffer) {
		char *char_buffer = buffer;
		char_buffer -= sizeof(size_t) * 2;
		size_t len = *(size_t *)char_buffer;
		for(size_t z = 0; z < sizeof(size_t); z++)
			if(char_buffer[sizeof(size_t) + z] != 'A')
				errx(EX_DATAERR, "heap underrun detected");
		for(size_t z = 0; z < sizeof(size_t) * 2; z++)
			if(char_buffer[len - z - 1] != 'A')
				errx(EX_DATAERR, "heap overrun detected");
		memset(char_buffer, 'A', len);
		free(char_buffer);
	}
#endif
}

bool _bijson_is_valid_utf8(const char *string, size_t len) {
	if(!string)
		return len == 0;

	const uint8_t *s = (const uint8_t *)string;
	const uint8_t *end = s + len;

	while(s != end) {
		uint8_t c = *s++;
		if(!(c & 0x80))
			continue; // short-circuit common case

		if((c & 0xE0) == 0xC0) { // 0b110.....
			// Basic characters can encode 7 bits.
			// Size 2 sequences can encode 5 + 6 = 11 bits.
			if(!(c & 0x1E))
				return false; // overlong sequence
			if(s == end)
				return false; // premature end
			if((*s++ & 0xC0) != 0x80)
				return false; // not a continuation byte
		} if((c & 0xF0) == 0xE0) { // 0b1110....
			// Size 3 sequences can encode 5 + 6 + 6 = 17 bits.
			if(s == end)
				return false; // premature end
			uint8_t c2 = *s++;
			if((c2 & 0xC0) != 0x80)
				return false; // not a continuation byte
			if(!(c & 0x0F) && !(c2 & 0x30))
				return false; // overlong sequence
			if(s == end)
				return false; // premature end
			if((*s++ & 0xC0) != 0x80)
				return false; // not a continuation byte
		} if((c & 0xF8) == 0xF0) { // 0b11110...
			// Size 4 sequences can encode 5 + 6 + 6 + 6 = 23 bits.
			if(s == end)
				return false; // premature end
			uint8_t c2 = *s++;
			if((c2 & 0xC0) != 0x80)
				return false; // not a continuation byte
			if(!(c & 0x07) && !(c2 & 0x38))
				return false; // overlong sequence
			if(s == end)
				return false; // premature end
			if((*s++ & 0xC0) != 0x80)
				return false; // not a continuation byte
			if(s == end)
				return false; // premature end
			if((*s++ & 0xC0) != 0x80)
				return false; // not a continuation byte
		} else {
			return false; // invalid byte
		}
	}

	return true;
}

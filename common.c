#include <complex.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sysexits.h>
#include <err.h>

void *xalloc(size_t len) {
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

void xfree(void *buffer) {
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <err.h>
#include <sysexits.h>
#include <assert.h>

#include "../include/reader.h"
#include "../include/writer.h"

#ifndef PACKAGE_STRING
#define PACKAGE_STRING "bijson (git)"
#endif

inline static void _c(const char *file, int line, const char *body, bijson_error_t error) {
	if(error == bijson_error_system)
		err(EX_OSERR, "%s:%d: %s", file, line, body);
	else if(error)
		errx(EX_SOFTWARE, "%s:%d: %s: %s", file, line, body, error);
}
#define C(error) do { _c(__FILE__, __LINE__, #error, (error)); } while(0)

static void usage(FILE *fh, const char *progname) {
	fprintf(fh, "Usage:\n");
	fprintf(fh, "\t%s help\n", progname);
	fprintf(fh, "\t%s version\n", progname);
	fprintf(fh, "\t%s load-json <input.json> <output.bijson>\n", progname);
	fprintf(fh, "\t%s dump-json <input.bijson> <output.json>\n", progname);
	// fprintf(fh, "\t%s load-yaml <input.yaml> <output.bijson>\n", progname);
	// fprintf(fh, "\t%s dump-yaml <input.bijson> <output.yaml>\n", progname);
}

int main(int argc, char **argv) {
	const char *progname = argv[0];
	char *basename = strrchr(progname, '/');
	if (basename) {
		progname = basename + 1U;
	}

	if (argc < 2) {
		usage(stderr, progname);
		return EXIT_FAILURE;
	}

	char *command = argv[1];

	if(!strcmp(command, "help")) {
		usage(stdout, progname);
	} else if(!strcmp(command, "version")) {
		printf("%s - https://fruit.je/bijson\n", PACKAGE_STRING);
	} else if(!strcmp(command, "load-json")) {
		if (argc < 4) {
			usage(stderr, progname);
			fprintf(stderr, "%s: missing arguments\n", progname);
			return EXIT_FAILURE;
		}
		bijson_writer_t *writer;
		C(bijson_writer_alloc(&writer));
		C(bijson_parse_json_filename(writer, argv[2], NULL));
		C(bijson_writer_write_to_filename(writer, argv[3]));
		bijson_writer_free(writer);
	} else if(!strcmp(command, "dump-json")) {
		if (argc < 4) {
			usage(stderr, progname);
			fprintf(stderr, "%s: missing arguments\n", progname);
			return EXIT_FAILURE;
		}
		bijson_t bijson;
		C(bijson_open_filename(&bijson, argv[2]));
		C(bijson_to_json_filename(&bijson, argv[3]));
		bijson_close(&bijson);
	} else {
		usage(stderr, progname);
		fprintf(stderr, "%s: unknown command %s\n", progname, command);
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

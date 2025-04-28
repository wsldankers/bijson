#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <assert.h>
#include <stdarg.h>
#include <errno.h>

#include "../include/reader.h"
#include "../include/writer.h"

#ifndef PACKAGE_STRING
#define PACKAGE_STRING "bijson (git)"
#endif

static const char *progname;

__attribute__((format(printf, 2, 3)))
static void _c(bijson_error_t error, const char *fmt, ...) {
	if(!error)
		return;

	va_list ap;
	va_start(ap, fmt);

	fprintf(stderr, "%s: ", progname);
	vfprintf(stderr, fmt, ap);
	if(error == bijson_error_system) {
		fprintf(stderr, ": %s\n", strerror(errno));
		exit(EX_OSERR);
	} else {
		fprintf(stderr, ": %s\n", error);
		exit(EX_SOFTWARE);
	}
}
#define C(error, ...) do { _c((error), __VA_ARGS__); } while(0)

static void usage(FILE *fh) {
	fprintf(fh, "Usage:\n");
	fprintf(fh, "\t%s help\n", progname);
	fprintf(fh, "\t%s version\n", progname);
	fprintf(fh, "\t%s load-json <input.json> <output.bijson>\n", progname);
	fprintf(fh, "\t%s dump-json <input.bijson> <output.json>\n", progname);
	// fprintf(fh, "\t%s load-yaml <input.yaml> <output.bijson>\n", progname);
	// fprintf(fh, "\t%s dump-yaml <input.bijson> <output.yaml>\n", progname);
}

int main(int argc, char **argv) {
	progname = argv[0];
	char *basename = strrchr(progname, '/');
	if (basename) {
		progname = basename + 1U;
	}

	if (argc < 2) {
		usage(stderr);
		return EXIT_FAILURE;
	}

	char *command = argv[1];

	if(!strcmp(command, "help")) {
		usage(stdout);
	} else if(!strcmp(command, "version")) {
		printf("%s - https://fruit.je/bijson\n", PACKAGE_STRING);
	} else if(!strcmp(command, "load-json")) {
		if (argc < 4) {
			usage(stderr);
			fprintf(stderr, "%s: missing arguments\n", progname);
			return EXIT_FAILURE;
		}
		bijson_writer_t *writer;
		C(bijson_writer_alloc(&writer), "bijson_writer_alloc()");
		C(bijson_parse_json_filename(writer, argv[2], NULL), "bijson_parse_json_filename(%s)", argv[2]);
		C(bijson_writer_write_to_filename(writer, argv[3]), "bijson_writer_write_to_filename(%s)", argv[3]);
		bijson_writer_free(writer);
	} else if(!strcmp(command, "dump-json")) {
		if (argc < 4) {
			usage(stderr);
			fprintf(stderr, "%s: missing arguments\n", progname);
			return EXIT_FAILURE;
		}
		bijson_t bijson;
		C(bijson_open_filename(&bijson, argv[2]), "bijson_open_filename(%s)", argv[2]);
		C(bijson_to_json_filename(&bijson, argv[3]), "bijson_to_json_filename(%s)", argv[3]);
		bijson_close(&bijson);
	} else {
		usage(stderr);
		fprintf(stderr, "%s: unknown command %s\n", progname, command);
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

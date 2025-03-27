#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <inttypes.h>
#include <err.h>
#include <sysexits.h>

#include <bijson/writer.h>
#include <bijson/reader.h>

#include "common.h"
#include "writer.h"

#define C(x) do { bijson_error_t error = (x); if(error == bijson_error_system) err(EX_OSERR, "%s:%d: %s", __FILE__, __LINE__, #x); else if(error) errx(EX_SOFTWARE, "%s:%d: %s: %s", __FILE__, __LINE__, #x, error); } while(0)


int main(void) {
	bijson_t bijson __attribute__((unused)) = {};
	bijson_error_t error __attribute__((unused));

	bijson_writer_t *writer = NULL;

	fprintf(stderr, "checking ranges...\n");
	fflush(stderr);

	for(size_t a = SIZE_C(1); a < SIZE_C(10); a++) {
		for(size_t b = SIZE_C(1); b < SIZE_C(10); b++) {
			for(size_t c = SIZE_C(1); c < SIZE_C(10); c++) {
				C(bijson_writer_alloc(&writer));
				C(bijson_writer_begin_object(writer));

				for(size_t u = 0; u < a; u++) {
					C(bijson_writer_add_key(writer, "foo", 3));
					C(bijson_writer_add_null(writer));
				}
				for(size_t u = 0; u < b; u++) {
					C(bijson_writer_add_key(writer, "bar", 3));
					C(bijson_writer_add_null(writer));
				}
				for(size_t u = 0; u < c; u++) {
					C(bijson_writer_add_key(writer, "baz", 3));
					C(bijson_writer_add_null(writer));
				}
				C(bijson_writer_end_object(writer));

				C(bijson_writer_write_to_malloc(writer, &bijson));
				// C(bijson_writer_write_to_filename(writer, "/dev/shm/foobarbaz.bijson"));
				bijson_writer_free(writer);

				bijson_object_analysis_t analysis;
				C(bijson_object_analyze(&bijson, &analysis));

				size_t a_b, a_e;
				C(bijson_analyzed_object_get_key_range(&analysis, "foo", 3, &a_b, &a_e));
				if(a_b != 0)
					errx(EX_SOFTWARE, "foo beginning does not match %zu", a_b);
				if(a_e - a_b != a)
					errx(EX_SOFTWARE, "foo count does not match");

				size_t b_b, b_e;
				C(bijson_analyzed_object_get_key_range(&analysis, "bar", 3, &b_b, &b_e));
				if(b_b != a_e)
					errx(EX_SOFTWARE, "bar beginning does not match %zu != %zu (%zu)", b_b, a_e, b_e);
				if(b_e - b_b != b)
					errx(EX_SOFTWARE, "bar count does not match");

				size_t c_b, c_e;
				C(bijson_analyzed_object_get_key_range(&analysis, "baz", 3, &c_b, &c_e));
				if(c_b != b_e)
					errx(EX_SOFTWARE, "bar beginning does not match");
				if(c_e != a + b + c)
					errx(EX_SOFTWARE, "bar end does not match");
				if(c_e - c_b != c)
					errx(EX_SOFTWARE, "baz count does not match");

				bijson_free(&bijson);
			}
		}
	}

	fprintf(stderr, "ranges OK.\n");
	fflush(stderr);

	C(bijson_writer_alloc(&writer));

	// C(bijson_writer_begin_array(writer));
	// for(size_t u = 0; u < SIZE_C(20); u++)
	// 	C(bijson_writer_add_null(writer));
	// C(bijson_writer_add_bytes(writer, NULL, 0));
	// C(bijson_writer_end_array(writer));

	C(bijson_writer_begin_array(writer));
	C(bijson_writer_begin_object(writer));
	C(bijson_writer_end_object(writer));
	C(bijson_writer_add_bytes(writer, NULL, 0));
	C(bijson_writer_end_array(writer));

	const char output_smol_bijson_filename[] = "/tmp/smol.bijson";
	fprintf(stderr, "writing %s\n", output_smol_bijson_filename);
	fflush(stderr);

	C(bijson_writer_write_to_filename(writer, output_smol_bijson_filename));

	bijson_writer_free(writer);

	// C(bijson_writer_alloc(&writer));
	// C(bijson_writer_begin_array(writer));
	// C(bijson_writer_begin_object(writer));
	// C(bijson_writer_add_key(writer, "foo", 3));
	// C(bijson_writer_add_string(writer, "quux", 4));
	// C(bijson_writer_add_key(writer, "bar", 3));
	// C(bijson_writer_add_string(writer, "xyzzy", 5));
	// C(bijson_writer_end_object(writer));
	// C(bijson_writer_add_true(writer));
	// C(bijson_writer_add_false(writer));
	// C(bijson_writer_add_decimal_from_string(writer, "123456", 6));
	// C(bijson_writer_add_decimal_from_string(writer, "10000000", 8));
	// C(bijson_writer_add_decimal_from_string(writer, "100000000", 9));
	// C(bijson_writer_add_decimal_from_string(writer, "3.1415", 6));
	// C(bijson_writer_add_decimal_from_string(writer, "1e1", 3));
	// C(bijson_writer_add_decimal_from_string(writer, "1e2", 3));
	// C(bijson_writer_add_decimal_from_string(writer, "1e3", 3));
	// C(bijson_writer_add_decimal_from_string(writer, "1e4", 3));
	// C(bijson_writer_add_decimal_from_string(writer, "1e5", 3));
	// C(bijson_writer_add_decimal_from_string(writer, "1e6", 3));
	// C(bijson_writer_add_decimal_from_string(writer, "1e7", 3));
	// C(bijson_writer_add_decimal_from_string(writer, "1e8", 3));
	// C(bijson_writer_add_string(writer, "あ", 3);
	// // for(uint32_t u = 0; u < UINT32_C(10000000); u++)
	// // 	C(bijson_writer_add_string(writer, "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA", 64));
	// C(bijson_writer_end_array(writer));
	// bijson_writer_free(writer);

	// C(bijson_writer_alloc(&writer));
	// const char json[] = " [ 42, -1e-1, 0.1, 0, 0.0, \"hoi\" , \"iedereen\\t\", { \"x\\by\": null }, { \"true\" : true , \"false\" : false , \"null\" : null }, [0, 0.0, 0e0, 0e1, 0.0e0, 0.0e1], [[]], {} ] ";
	// size_t end;
	// error = bijson_parse_json(writer, json, sizeof json - 1, &end);
	// fprintf(stderr, "'%s'\n", json);
	// for(size_t u = 0; u < end; u++)
	// 	fputc(' ', stderr);
	// fputs(" ^\n", stderr);
	// C(error);
	// bijson_writer_free(writer);

	// C(bijson_writer_write_to_malloc(writer, &bijson));

	// C(bijson_writer_alloc(&writer));
	// C(bijson_writer_begin_object(writer));
	// for(size_t u = 0; u < SIZE_C(100000000); u++) {
	// 	char getal[20];
	// 	int len = sprintf(getal, "%zu", u);
	// 	C(bijson_writer_add_key(writer, getal, len));
	// 	C(bijson_writer_add_string(writer, getal, len));
	// }
	// C(bijson_writer_end_object(writer));
	// bijson_writer_free(writer);

	C(bijson_writer_alloc(&writer));

	const char input_json_filename[] = "/tmp/records.json";
	fprintf(stderr, "parsing %s\n", input_json_filename);
	fflush(stderr);

	C(bijson_parse_json_filename(writer, input_json_filename, NULL));

	fprintf(stderr, "writing to tempfile\n");
	fflush(stderr);

	C(bijson_writer_write_to_tempfile(writer, &bijson));

	fprintf(stderr, "freeing writer object\n");
	fprintf(stderr, "spool was %zu bytes, stack was %zu bytes.\n", writer->spool._size, writer->stack._size);
	fflush(stderr);

	bijson_writer_free(writer);

	fprintf(stderr, "checking keys at root level\n");
	fflush(stderr);

	bijson_object_analysis_t analysis;
	C(bijson_object_analyze(&bijson, &analysis));

	bijson_t value;
	error = bijson_analyzed_object_get_key(&analysis, "foo", 3, &value);
	if(error != bijson_error_key_not_found)
		errx(EX_SOFTWARE, "foo: %s", error);
	error = bijson_analyzed_object_get_key(&analysis, "bar", 3, &value);
	if(error != bijson_error_key_not_found)
		errx(EX_SOFTWARE, "bar: %s", error);
	error = bijson_analyzed_object_get_key(&analysis, "baz", 3, &value);
	if(error != bijson_error_key_not_found)
		errx(EX_SOFTWARE, "baz: %s", error);
	error = bijson_analyzed_object_get_key(&analysis, "quux", 4, &value);
	if(error != bijson_error_key_not_found)
		errx(EX_SOFTWARE, "quux: %s", error);
	error = bijson_analyzed_object_get_key(&analysis, "xyzzy", 5, &value);
	if(error != bijson_error_key_not_found)
		errx(EX_SOFTWARE, "xyzzy: %s", error);

	fprintf(stderr, "no non-existent keys found\n");
	fflush(stderr);

	size_t count;
	C(bijson_analyzed_object_count(&analysis, &count));
	for(size_t u = SIZE_C(0); u < count; u++) {
		const void *key;
		size_t len;
		bijson_t value;
		C(bijson_analyzed_object_get_index(&analysis, u, &key, &len, &value));
		bijson_t check;
		C(bijson_analyzed_object_get_key(&analysis, key, len, &check));
		if(value.buffer != check.buffer)
			errx(EX_SOFTWARE, "can't find key %zu :(", u);
	}
	fprintf(stderr, "all %zu accounted for\n", count);
	fflush(stderr);

	const char output_json_filename[] = "/dev/shm/test.json";
	fprintf(stderr, "writing %s\n", output_json_filename);
	fflush(stderr);
	C(bijson_to_json_filename(&bijson, output_json_filename));

	fprintf(stderr, "done\n");
	fflush(stderr);

	bijson_close(&bijson);

	return 0;
}

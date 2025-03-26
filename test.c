#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <inttypes.h>
#include <err.h>

#include <bijson/writer.h>
#include <bijson/reader.h>

#include "common.h"
#include "writer.h"

#define C(x) do { bijson_error_t error = (x); if(error == bijson_error_system) err(2, "%s:%d: %s", __FILE__, __LINE__, #x); else if(error) errx(1, "%s:%d: %s: %s", __FILE__, __LINE__, #x, error); } while(0)


int main(void) {
	bijson_t bijson __attribute__((unused)) = {};
	bijson_error_t error __attribute__((unused));

	bijson_writer_t *writer = NULL;

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

	const char output_bijson_filename[] = "/dev/shm/test.bijson";

	C(bijson_writer_alloc(&writer));

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

	// const char json[] = " [ 42, -1e-1, 0.1, 0, 0.0, \"hoi\" , \"iedereen\\t\", { \"x\\by\": null }, { \"true\" : true , \"false\" : false , \"null\" : null }, [0, 0.0, 0e0, 0e1, 0.0e0, 0.0e1], [[]], {} ] ";
	// size_t end;
	// error = bijson_parse_json(writer, json, sizeof json - 1, &end);
	// fprintf(stderr, "'%s'\n", json);
	// for(size_t u = 0; u < end; u++)
	// 	fputc(' ', stderr);
	// fputs(" ^\n", stderr);
	// C(error);

	// bijson_writer_write_to_malloc(writer, (void **)&bijson.buffer, &bijson.size);

	// C(bijson_writer_begin_object(writer));
	// for(size_t u = 0; u < SIZE_C(100000000); u++) {
	// 	char getal[20];
	// 	int len = sprintf(getal, "%zu", u);
	// 	C(bijson_writer_add_key(writer, getal, len));
	// 	C(bijson_writer_add_string(writer, getal, len));
	// }
	// C(bijson_writer_end_object(writer));

	const char input_json_filename[] = "/tmp/records.json";
	fprintf(stderr, "parsing %s\n", input_json_filename);
	fflush(stderr);

	C(bijson_parse_json_filename(writer, input_json_filename, NULL));

	fprintf(stderr, "writing %s\n", output_bijson_filename);
	fflush(stderr);

	C(bijson_writer_write_to_filename(writer, output_bijson_filename));

	fprintf(stderr, "freeing writer object\n");
	fprintf(stderr, "spool was %zu bytes, stack was %zu bytes.\n", writer->spool._size, writer->stack._size);
	fflush(stderr);

	bijson_writer_free(writer);

	fprintf(stderr, "opening %s\n", output_bijson_filename);
	fflush(stderr);

	C(bijson_open_filename(&bijson, output_bijson_filename));

	fprintf(stderr, "checking keys at root level\n");
	fflush(stderr);

	bijson_object_analysis_t analysis;
	C(bijson_object_analyze(&bijson, &analysis));

	bijson_t value;
	error = bijson_analyzed_object_get_key(&analysis, "foo", 3, &value);
	if(error != bijson_error_key_not_found)
		C(error);
	error = bijson_analyzed_object_get_key(&analysis, "bar", 3, &value);
	if(error != bijson_error_key_not_found)
		C(error);
	error = bijson_analyzed_object_get_key(&analysis, "baz", 3, &value);
	if(error != bijson_error_key_not_found)
		C(error);
	error = bijson_analyzed_object_get_key(&analysis, "quux", 4, &value);
	if(error != bijson_error_key_not_found)
		C(error);
	error = bijson_analyzed_object_get_key(&analysis, "xyzzy", 5, &value);
	if(error != bijson_error_key_not_found)
		C(error);

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
		if(value.buffer != check.buffer) {
			fprintf(stderr, "sad :(\n");
			break;
		}
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

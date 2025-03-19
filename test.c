#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

#include <bijson/writer.h>
#include <bijson/reader.h>

int main(void) {
	bijson_writer_t *writer = NULL;
	bijson_writer_alloc(&writer);
	// bijson_writer_begin_array(writer);
	// bijson_writer_begin_object(writer);
	// bijson_writer_add_key(writer, "foo", 3);
	// bijson_writer_add_string(writer, "quux", 4);
	// bijson_writer_add_key(writer, "bar", 3);
	// bijson_writer_add_string(writer, "xyzzy", 5);
	// bijson_writer_end_object(writer);
	// bijson_writer_add_true(writer);
	// bijson_writer_add_false(writer);
	// bijson_writer_add_decimal_from_string(writer, "123456", 6);
	// bijson_writer_add_decimal_from_string(writer, "10000000", 8);
	// bijson_writer_add_decimal_from_string(writer, "100000000", 9);
	// bijson_writer_add_decimal_from_string(writer, "3.1415", 6);
	// bijson_writer_add_decimal_from_string(writer, "1e1", 3);
	// bijson_writer_add_decimal_from_string(writer, "1e2", 3);
	// bijson_writer_add_decimal_from_string(writer, "1e3", 3);
	// bijson_writer_add_decimal_from_string(writer, "1e4", 3);
	// bijson_writer_add_decimal_from_string(writer, "1e5", 3);
	// bijson_writer_add_decimal_from_string(writer, "1e6", 3);
	// bijson_writer_add_decimal_from_string(writer, "1e7", 3);
	// bijson_writer_add_decimal_from_string(writer, "1e8", 3);
	// bijson_writer_add_string(writer, "あ", 3);
	// // for(uint32_t u = 0; u < UINT32_C(10000000); u++)
	// // 	bijson_writer_add_string(writer, "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA", 64);
	// bijson_writer_end_array(writer);

	const char json[] = " [ \"hoi\" , \"iedereen\" ] ";
	bijson_error_t error = bijson_parse_json(writer, json, sizeof json - 1);
	fprintf(stderr, "%s\n", error);


	int fd = open("/tmp/test.bijson", O_WRONLY|O_CREAT|O_TRUNC|O_NOCTTY|O_CLOEXEC, 0666);
	bijson_writer_write_to_fd(writer, fd);
	close(fd);

	bijson_t bijson = {};
	bijson_writer_write_to_malloc(writer, (void **)&bijson.buffer, &bijson.size);

	bijson_writer_free(writer);

	// bijson_to_json_FILE(&bijson, stdout);
	bijson_to_json_fd(&bijson, STDOUT_FILENO);
	putchar('\n');

	return 0;
}

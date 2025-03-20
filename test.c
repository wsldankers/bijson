#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <inttypes.h>
#include <fcntl.h>
#include <err.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include <bijson/writer.h>
#include <bijson/reader.h>

int main(void) {
	bijson_writer_t *writer = NULL;
	bijson_t bijson = {};
	int fd;
	struct stat st;

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

	// const char json[] = " [ 42, -1e-1, 0.1, 0, 0.0, \"hoi\" , \"iedereen\\t\", { \"true\" : true , \"false\" : false , \"null\" : null }, [0, 0.0, 0e0, 0e1, 0.0e0, 0.0e1], [[]], {} ] ";
	// size_t end;
	// bijson_error_t error = bijson_parse_json(writer, json, sizeof json - 1, &end);
	// fprintf(stderr, "'%s'\n", json);
	// for(size_t u = 0; u < end; u++)
	// 	fputc(' ', stderr);
	// fputs(" ^\n", stderr);
	// fprintf(stderr, "%s\n", error);

	// bijson_writer_write_to_malloc(writer, (void **)&bijson.buffer, &bijson.size);

	fd = open("/tmp/records.json", O_RDONLY|O_NOCTTY|O_CLOEXEC);
	if(fd == -1)
		err(2, "open(/tmp/records.json)");
	if(fstat(fd, &st) == -1)
		err(2, "stat");
	if(st.st_size > SIZE_MAX)
		errx(2, "/tmp/records.json too large");
	const void *json = mmap(NULL, st.st_size, PROT_READ, MAP_SHARED, fd, 0);
	if(json == MAP_FAILED)
		err(2, "mmap");
	close(fd);
	size_t end;
	bijson_error_t error = bijson_parse_json(writer, json, st.st_size, &end);
	fprintf(stderr, "%s\n", error ?: "success");
	fprintf(stderr, "%zu\n", end);

	fd = open("/tmp/test.bijson", O_WRONLY|O_CREAT|O_TRUNC|O_NOCTTY|O_CLOEXEC, 0666);
	if(fd == -1)
		err(2, "open(/tmp/test.bijson)");
	bijson_writer_write_to_fd(writer, fd);
	close(fd);

	bijson_writer_free(writer);

	fd = open("/tmp/test.bijson", O_RDONLY|O_NOCTTY|O_CLOEXEC);
	if(fd == -1)
		err(2, "open(/tmp/test.bijson)");
	if(fstat(fd, &st) == -1)
		err(2, "stat");
	if(st.st_size > SIZE_MAX)
		errx(2, "/tmp/test.bijson too large");
	bijson.buffer = mmap(NULL, st.st_size, PROT_READ, MAP_SHARED, fd, 0);
	if(bijson.buffer == MAP_FAILED)
		err(2, "mmap");
	bijson.size = st.st_size;
	close(fd);

	fd = open("/tmp/test.json", O_WRONLY|O_CREAT|O_TRUNC|O_NOCTTY|O_CLOEXEC, 0666);
	if(fd == -1)
		err(2, "open(/tmp/test.json)");
	bijson_to_json_fd(&bijson, fd);
	close(fd);

	return 0;
}

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

#define C(x) do { bijson_error_t error = (x); if(error == bijson_error_system) err(2, "%s:%d: %s", __FILE__, __LINE__, #x); else if(error) errx(1, "%s:%d: %s: %s", __FILE__, __LINE__, #x, error); } while(0)


int main(void) {
	bijson_t bijson = {};
	int fd;
	struct stat st;
	bijson_error_t error __attribute__((unused));

	bijson_writer_t *writer = NULL;
	bijson_writer_alloc(&writer);

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
	// // 	C(bijson_writer_add_string(writer, "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA", 64);
	// C(bijson_writer_end_array(writer);

	// const char json[] = " [ 42, -1e-1, 0.1, 0, 0.0, \"hoi\" , \"iedereen\\t\", { \"x\\by\": null }, { \"true\" : true , \"false\" : false , \"null\" : null }, [0, 0.0, 0e0, 0e1, 0.0e0, 0.0e1], [[]], {} ] ";
	// size_t end;
	// error = bijson_parse_json(writer, json, sizeof json - 1, &end);
	// fprintf(stderr, "'%s'\n", json);
	// for(size_t u = 0; u < end; u++)
	// 	fputc(' ', stderr);
	// fputs(" ^\n", stderr);
	// C(error);

	// bijson_writer_write_to_malloc(writer, (void **)&bijson.buffer, &bijson.size);

	fprintf(stderr, "parsing /tmp/records.json\n");
	fflush(stderr);

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
	C(bijson_parse_json(writer, json, st.st_size, &end));

	fprintf(stderr, "writing /dev/shm/test.bijson\n");
	fflush(stderr);

	fd = open("/dev/shm/test.bijson", O_WRONLY|O_CREAT|O_TRUNC|O_NOCTTY|O_CLOEXEC, 0666);
	if(fd == -1)
		err(2, "open(/dev/shm/test.bijson)");
	C(bijson_writer_write_to_fd(writer, fd));
	close(fd);

	fprintf(stderr, "freeing writer object\n");
	fflush(stderr);

	bijson_writer_free(writer);

	fprintf(stderr, "opening /dev/shm/test.bijson\n");
	fflush(stderr);

	fd = open("/dev/shm/test.bijson", O_RDONLY|O_NOCTTY|O_CLOEXEC);
	if(fd == -1)
		err(2, "open(/dev/shm/test.bijson)");
	if(fstat(fd, &st) == -1)
		err(2, "stat");
	if(st.st_size > SIZE_MAX)
		errx(2, "/dev/shm/test.bijson too large");
	bijson.buffer = mmap(NULL, st.st_size, PROT_READ, MAP_SHARED, fd, 0);
	if(bijson.buffer == MAP_FAILED)
		err(2, "mmap");
	bijson.size = st.st_size;
	close(fd);
	// fprintf(stderr, "opened test.bijson at address %p\n", bijson.buffer);

	fprintf(stderr, "writing /dev/shm/test.json\n");
	fflush(stderr);

	fd = open("/dev/shm/test.json", O_WRONLY|O_CREAT|O_TRUNC|O_NOCTTY|O_CLOEXEC, 0666);
	if(fd == -1)
		err(2, "open(/dev/shm/test.json)");
	C(bijson_to_json_fd(&bijson, fd));

	close(fd);

	fprintf(stderr, "done\n");
	fflush(stderr);

	return 0;
}

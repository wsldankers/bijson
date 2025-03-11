#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

#include <bijson/writer.h>
#include <bijson/reader.h>

int main(void) {
    bijson_writer_t *writer = bijson_writer_alloc();
    bijson_writer_begin_array(writer);
    bijson_writer_begin_object(writer);
    bijson_writer_add_key(writer, "foo", 3);
    bijson_writer_add_string(writer, "quux", 4);
    bijson_writer_add_key(writer, "bar", 3);
    bijson_writer_add_string(writer, "xyzzy", 5);
    bijson_writer_end_object(writer);
    bijson_writer_add_true(writer);
    bijson_writer_add_false(writer);
    bijson_writer_end_array(writer);

    int fd = open("/tmp/test.bijson", O_WRONLY|O_CREAT|O_TRUNC|O_NOCTTY|O_CLOEXEC, 0666);
    bijson_writer_write_to_fd(writer, fd);
    close(fd);

    bijson_t bijson = {};
    bijson_writer_write_to_buffer(writer, (void **)&bijson.buffer, &bijson.size);

    bijson_writer_free(writer);

    bijson_to_json(&bijson, bijson_stdio_output_callback, stdout);
    putchar('\n');

    return 0;
}

#! /usr/bin/make -f

programs = test

all: $(programs)

include Makeconf

CFLAGS += -Iinclude
CFLAGS += $(shell pkg-config --cflags libxxhash)
LIBS += $(shell pkg-config --libs libxxhash)
test_EXTRA_OBJECTS = writer.o reader.o buffer.o common.o container.o format.o string.o decimal.o bytes.o constants.o io.o error.o

include Makerules

clean:
	rm -f $(sort $(objects) $(programs))

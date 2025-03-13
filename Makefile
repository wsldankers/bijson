#! /usr/bin/make -f

src := $(dir $(firstword $(MAKEFILE_LIST)))

programs = test

all: $(programs)

include $(src)Makeconf

CFLAGS += -I$(src)include
CFLAGS += $(shell pkg-config --cflags libxxhash)
LIBS += $(shell pkg-config --libs libxxhash)
test_EXTRA_OBJECTS = writer.o reader.o buffer.o common.o container.o format.o string.o decimal.o bytes.o constants.o io.o error.o

include $(src)Makerules

clean:
	rm -f $(generated)

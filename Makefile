#! /usr/bin/make -f

programs = writer

all: $(programs)

include ../Makeconf

STANDARD = -std=gnu99
CFLAGS += $(shell pkg-config --cflags libxxhash)
LIBS += $(shell pkg-config --libs libxxhash)
writer_EXTRA_OBJECTS = buffer.o common.o container.o format.o string.o decimal.o bytes.o constants.o

include ../Makerules

clean:
	rm -f $(sort $(objects) $(programs))

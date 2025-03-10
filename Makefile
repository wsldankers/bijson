#! /usr/bin/make -f

programs = writer reader

all: $(programs)

include Makeconf

CFLAGS += $(shell pkg-config --cflags libxxhash)
LIBS += $(shell pkg-config --libs libxxhash)
writer_EXTRA_OBJECTS = buffer.o common.o container.o format.o string.o decimal.o bytes.o constants.o
reader_EXTRA_OBJECTS = common.o

include Makerules

clean:
	rm -f $(sort $(objects) $(programs))

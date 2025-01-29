#! /usr/bin/make -f

programs = writer

all: $(programs)

include ../Makeconf

STANDARD = -std=gnu99
CFLAGS += $(shell pkg-config --cflags libxxhash)
LIBS += $(shell pkg-config --libs libxxhash)

include ../Makerules

clean:
	rm -f $(sort $(objects) $(programs))

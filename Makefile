#! /usr/bin/make -f

programs = writer

all: $(programs)

include ../Makeconf

STANDARD = -std=gnu99

include ../Makerules

clean:
	rm -f $(sort $(objects) $(programs))

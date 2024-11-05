CC = gcc
STRIP = strip

DEBUG_ARGS = -g3 -Wall -Wextra -Wconversion -Wdouble-promotion \
		-Wno-unused-parameter -Wno-unused-function -Wno-sign-conversion \
		-fsanitize=address,undefined -fsanitize-undefined-trap-on-error \
		-std=c99 -pedantic -DDEBUG
RELEASE_ARGS = -O3 -ffast-math
LIBS = -lm -ldl -lmvec

.PHONY: all
all: generator_debug generator_release haversine_debug haversine_release

.PHONY: clean
clean:
	rm -fv generator_debug generator_release haversine_debug haversine_release

haversine_debug: haversine.c prof.h
	$(CC) $(DEBUG_ARGS) $(LIBS) -o $@ $<

haversine_release: haversine.c prof.h
	$(CC) $(RELEASE_ARGS) $(LIBS) -o $@ $<
	$(STRIP) $@

generator_debug: generator.c
	$(CC) $(DEBUG_ARGS) $(LIBS) -o $@ $<

generator_release: generator.c
	$(CC) $(RELEASE_ARGS) $(LIBS) -o $@ $<
	$(STRIP) $@

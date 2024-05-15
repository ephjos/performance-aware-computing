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

%_debug: %.c
	$(CC) $(DEBUG_ARGS) $(LIBS) -o $@ $<

%_release: %.c
	$(CC) $(RELEASE_ARGS) $(LIBS) -o $@ $<
	$(STRIP) $@


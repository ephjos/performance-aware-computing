CC = gcc

DEBUG_ARGS = -g3 -Wall -Wextra -Wconversion -Wdouble-promotion \
		-Wno-unused-parameter -Wno-unused-function -Wno-sign-conversion \
		-fsanitize=address,undefined -fsanitize-undefined-trap-on-error \
		-std=c2x -pedantic 
RELEASE_ARGS = -O3 -ffast-math
LIBS = -lm

.PHONY: all
all: generator_debug generator_release

%_debug: %.c
	$(CC) $(DEBUG_ARGS) $(LIBS) -o $@ $<

%_release: %.c
	$(CC) $(RELEASE_ARGS) $(LIBS) -o $@ $<
	strip --strip-all $@


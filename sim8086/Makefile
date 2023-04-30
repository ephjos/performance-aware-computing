
sim8086: main.c
	gcc -g3 -Wall -Wextra -Wconversion -Wdouble-promotion \
		-Wno-unused-parameter -Wno-unused-function -Wno-sign-conversion \
		-fsanitize=address,undefined -fsanitize-undefined-trap-on-error \
		-std=c99 -pedantic -o sim8086 main.c

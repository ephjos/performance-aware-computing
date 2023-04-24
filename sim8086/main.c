#include <stdio.h>
#include <stdlib.h>

// Table 4-9
const char **REG_TABLE[2] = {
	(const char*[8]){"al", "cl", "dl", "bl", "ah", "ch", "dh", "bh"}, // w = 0
	(const char*[8]){"ax", "cx", "dx", "bx", "sp", "bp", "si", "di"}, // w = 1
};

#define MOV 0b100010

int main(int argc, char *argv[])
{
	if (argc != 2) {
		printf("Must provide machine code filename as sole argument\n");
		return 1;
	}

	FILE *fp = fopen(argv[1], "rb");
	if (fp == NULL) {
		printf("Could not open file {%s}\n", argv[1]);
		return 2;
	}

	printf("; %s:\nbits 16\n\n", argv[1]);

	int b;
	int op;
	while (1) {
		b = fgetc(fp); if (b == EOF) { break; }

		op = b >> 2;

		switch(op) {
			case MOV:
			{
				int nb = fgetc(fp);
				if (nb == EOF) { break; }

				int d = (b >> 1) & 1;
				int w = b & 1;
				int mod = (nb >> 6) & 0b11;
				int reg = (nb >> 3) & 0b111;
				int rm = nb & 0b111;

				if (d) {
					printf("mov %s, %s\n", REG_TABLE[w][reg], REG_TABLE[w][rm]);
				} else {
					printf("mov %s, %s\n", REG_TABLE[w][rm], REG_TABLE[w][reg]);
				}
				break;
			}
			default:
				break;
		}
	}

	fclose(fp);
	return 0;
}

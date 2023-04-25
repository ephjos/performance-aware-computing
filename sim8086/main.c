//
// sim8086 for part 1 of Casey Muratori's Performance Aware Computing Course.
// This file takes a single argument, a filename to an 8086 machine code file,
// and outputs the assembly to STDOUT. It is effectively a disassembler written
// to learn 8086 and "how to think like a CPU".
//
// Joe Hines - April 2023
//

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

// ============================================================================
// Defines
// ============================================================================

// ============================================================================
// Structs/Types
// ============================================================================
typedef struct op_t op_t;
typedef struct sim_state_t sim_state_t;

typedef struct op_t {
	uint8_t opcode_len;
	uint8_t opcode;
	void (*impl)(sim_state_t *state, op_t *op);
} op_t;

typedef struct sim_state_t {
	FILE *fp; // Open file, where bytes are read from
	uint8_t b0; // Relevant bytes for current op, most only use the first few
	uint8_t b1;
	uint8_t b2;
	uint8_t b3;
	uint8_t b4;
	uint8_t b5;
} sim_state_t;

// ============================================================================
// Constants
// ============================================================================
// Table 4-9
const char **REG_TABLE[2] = {
	(const char*[8]){"al", "cl", "dl", "bl", "ah", "ch", "dh", "bh"}, // w = 0
	(const char*[8]){"ax", "cx", "dx", "bx", "sp", "bp", "si", "di"}, // w = 1
};

// ============================================================================
// Utils
// ============================================================================
uint8_t read_byte(FILE *fp) {
	int b = fgetc(fp);
	if (b == EOF) {
		fclose(fp);
		exit(0);
	}
	return b;
}

// ============================================================================
// Instruction handlers
// ============================================================================
void handle_mov_0(sim_state_t *state, op_t *op) {
	state->b1 = read_byte(state->fp);

	uint8_t d = (state->b0 >> 1) & 1;
	uint8_t w = state->b0 & 1;

	uint8_t mod = (state->b1 >> 6) & 0b11;
	uint8_t reg = (state->b1 >> 3) & 0b111;
	uint8_t rm = state->b1 & 0b111;

	if (d) {
		printf("mov %s, %s\n", REG_TABLE[w][reg], REG_TABLE[w][rm]);
	} else {
		printf("mov %s, %s\n", REG_TABLE[w][rm], REG_TABLE[w][reg]);
	}
}


// ============================================================================
// Operation list
// ============================================================================
const op_t OPS[] = {
	(op_t) { .opcode_len = 6, .opcode = 0b100010, .impl = handle_mov_0 },
};

// ============================================================================
// Entrypoint
// ============================================================================
int main(int argc, char *argv[]) {
	if (argc != 2) {
		printf("Must provide machine code filename as sole argument\n");
		return 1;
	}
	sim_state_t state = {0};

	state.fp = fopen(argv[1], "rb");
	if (state.fp == NULL) {
		printf("Could not open file {%s}\n", argv[1]);
		return 2;
	}

	printf("; %s:\nbits 16\n\n", argv[1]);

	while (1) {
		state.b0 = read_byte(state.fp);

		uint8_t found = 0;
		for (int i = 0; i < sizeof(OPS)/sizeof(op_t); i++) {
			op_t op = OPS[i];
			if ((state.b0 >> (8-op.opcode_len)) == op.opcode) {
				op.impl(&state, &op);
				found = 1;
				break;
			}
		}

		if (!found) {
			printf("Unknown op from byte=%d\n", state.b0);
			return 100;
		}
	}

	fclose(state.fp);
	return 0;
}

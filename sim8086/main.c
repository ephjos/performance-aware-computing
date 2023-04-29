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

// Table 4-9 (MOD = 11)
const char **REGISTER_MODE_TABLE[2] = {
	(const char*[8]){"al", "cl", "dl", "bl", "ah", "ch", "dh", "bh"}, // w = 0
	(const char*[8]){"ax", "cx", "dx", "bx", "sp", "bp", "si", "di"}, // w = 1
};

const char *EFFECTIVE_ADDRESS_CALCULATION_TABLE[8] = {
	"bx + si",
	"bx + di",
	"bp + si",
	"bp + di",
	"si",
	"di",
	"bp",
	"bx",
};

// ============================================================================
// Utils
// ============================================================================

// Read a byte from the file, exiting if it is EOF. Otherwise, return the byte
// as an unsigned 8-bit integer
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
void mov_rm2rm(sim_state_t *state, op_t *op) {
	state->b1 = read_byte(state->fp);

	uint8_t d = (state->b0 >> 1) & 1;
	uint8_t w = state->b0 & 1;

	uint8_t mod = (state->b1 >> 6) & 0b11;
	uint8_t reg = (state->b1 >> 3) & 0b111;
	uint8_t rm = state->b1 & 0b111;

	switch (mod) {
		case 0b11:
			// Register mode
			if (d) {
				printf("mov %s, %s\n", REGISTER_MODE_TABLE[w][reg], REGISTER_MODE_TABLE[w][rm]);
			} else {
				printf("mov %s, %s\n", REGISTER_MODE_TABLE[w][rm], REGISTER_MODE_TABLE[w][reg]);
			}
			break;
		case 0b10:
			// Memory mode, 16-bit displacement
			state->b2 = read_byte(state->fp);
			state->b3 = read_byte(state->fp);
			uint16_t v = (state->b3 << 8) | (state->b2);
			if (d) {
				printf("mov %s, [%s + %d]\n", REGISTER_MODE_TABLE[w][reg], EFFECTIVE_ADDRESS_CALCULATION_TABLE[rm], v);
			} else {
				printf("mov [%s + %d], %s\n", EFFECTIVE_ADDRESS_CALCULATION_TABLE[rm], v, REGISTER_MODE_TABLE[w][reg]);
			}
			break;
		case 0b01:
			// Memory mode, 8-bit displacement
			state->b2 = read_byte(state->fp);
			if (d) {
				printf("mov %s, [%s + %d]\n", REGISTER_MODE_TABLE[w][reg], EFFECTIVE_ADDRESS_CALCULATION_TABLE[rm], state->b2);
			} else {
				printf("mov [%s + %d], %s\n", EFFECTIVE_ADDRESS_CALCULATION_TABLE[rm], state->b2, REGISTER_MODE_TABLE[w][reg]);
			}
			break;
		case 0b00:
			// Memory mode, no displacement (except when rm = 0b110, then 16-bit)
			if (rm == 0b110) {
				// Direct address, 16-bit
				state->b2 = read_byte(state->fp);
				state->b3 = read_byte(state->fp);
				uint16_t v = (state->b3 << 8) | (state->b2);
				if (d) {
					printf("mov %s, [%d]\n", REGISTER_MODE_TABLE[w][reg], v);
				} else {
					printf("mov [%d], %s\n", v, REGISTER_MODE_TABLE[w][reg]);
				}
			} else {
				// Normal memory mode, no displacement
				if (d) {
					printf("mov %s, [%s]\n", REGISTER_MODE_TABLE[w][reg], EFFECTIVE_ADDRESS_CALCULATION_TABLE[rm]);
				} else {
					printf("mov [%s], %s\n", EFFECTIVE_ADDRESS_CALCULATION_TABLE[rm], REGISTER_MODE_TABLE[w][reg]);
				}
			}
			break;
		default:
			break;
	}
}

void mov_i2rm(sim_state_t *state, op_t *op) {
	printf("move_i2rm\n");
}

void mov_i2r(sim_state_t *state, op_t *op) {
	uint8_t w = (state->b0 >> 3) & 1;
	uint8_t reg = (state->b0) & 0b111;

	if (w) {
		state->b1 = read_byte(state->fp);
		state->b2 = read_byte(state->fp);
		uint16_t v = (state->b2 << 8) | (state->b1);
		printf("mov %s, %d \n", REGISTER_MODE_TABLE[w][reg], v);
	} else {
		state->b1 = read_byte(state->fp);
		printf("mov %s, %d \n", REGISTER_MODE_TABLE[w][reg], state->b1);
	}
}

void mov_m2a(sim_state_t *state, op_t *op) {
	printf("move_m2a\n");
}
void mov_a2m(sim_state_t *state, op_t *op) {
	printf("move_a2m\n");
}


// ============================================================================
// Operation list
// ============================================================================
const op_t OPS[] = {
	(op_t) { .opcode_len = 6,  .opcode = 0b100010,   .impl = mov_rm2rm          },
	(op_t) { .opcode_len = 7,  .opcode = 0b1100011,  .impl = mov_i2rm           },
	(op_t) { .opcode_len = 4,  .opcode = 0b1011,     .impl = mov_i2r            },
	(op_t) { .opcode_len = 7,  .opcode = 0b1010000,  .impl = mov_m2a            },
	(op_t) { .opcode_len = 7,  .opcode = 0b1010001,  .impl = mov_a2m            },
};
const int NUM_OPS = sizeof(OPS)/sizeof(op_t);

// ============================================================================
// Entrypoint
// ============================================================================
int main(int argc, char *argv[]) {
	if (argc != 2) {
		printf("Must provide machine code filename as sole argument\n");
		return 1;
	}

	// Init state and open input file
	sim_state_t state = {0};
	state.fp = fopen(argv[1], "rb");
	if (state.fp == NULL) {
		printf("Could not open file {%s}\n", argv[1]);
		return 2;
	}

	// Write header
	printf("; %s:\nbits 16\n\n", argv[1]);

	// Read bytes while there are bytes left and we haven't hit an error
	while (1) {
		state.b0 = read_byte(state.fp);

		// Check if current byte starts a known op
		uint8_t found = 0;
		for (int i = 0; i < NUM_OPS; i++) {
			op_t op = OPS[i];
			if ((state.b0 >> (8-op.opcode_len)) == op.opcode) {
				op.impl(&state, &op);
				found = 1;
				break;
			}
		}

		if (!found) {
			printf("Unknown op from byte=%d\n", state.b0);
			fclose(state.fp);
			return 100;
		}
	}

	fclose(state.fp);
	return 0;
}

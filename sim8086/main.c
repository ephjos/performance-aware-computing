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

#define REGISTER_MODE 3
#define MEMORY_MODE_16 2
#define MEMORY_MODE_8 1
#define MEMORY_MODE_0 0

// ============================================================================
// Structs/Types
// ============================================================================

struct sim_state_t {
	FILE *fp; // Open file, where bytes are read from
	uint8_t op_byte;
};
struct op_t {
	uint8_t opcode_len;
	uint8_t opcode;
	void (*impl)(struct sim_state_t *state, struct op_t *op);
};

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
uint8_t read_byte(struct sim_state_t *state) {
	int b = fgetc(state->fp);
	if (b == EOF) {
		fclose(state->fp);
		exit(0);
	}
	return (uint8_t)b;
}

// Read two bytes from the file, returning a signed word constructed from them.
// The second byte contains the most significant bits.
int16_t read_word(struct sim_state_t *state) {
	uint8_t a = read_byte(state);
	uint8_t b = read_byte(state);
	return (uint16_t)(b << 8) | (uint16_t)(a);
}

// ============================================================================
// Instruction handlers
// ============================================================================
void mov_rm2rm(struct sim_state_t *state, struct op_t *op) {
	uint8_t options_byte = read_byte(state);

	uint8_t d = (state->op_byte >> 1) & 1;
	uint8_t w = state->op_byte & 1;

	uint8_t mod = (options_byte >> 6) & 3;
	uint8_t reg = (options_byte >> 3) & 7;
	uint8_t rm = options_byte & 7;

	switch (mod) {
		case REGISTER_MODE:
			if (d) {
				printf("mov %s, %s\n", REGISTER_MODE_TABLE[w][reg], REGISTER_MODE_TABLE[w][rm]);
			} else {
				printf("mov %s, %s\n", REGISTER_MODE_TABLE[w][rm], REGISTER_MODE_TABLE[w][reg]);
			}
			break;
		case MEMORY_MODE_16:
			{
				int16_t displacement = read_word(state);
				if (d) {
					printf("mov %s, [%s + %d]\n", REGISTER_MODE_TABLE[w][reg], EFFECTIVE_ADDRESS_CALCULATION_TABLE[rm], displacement);
				} else {
					printf("mov [%s + %d], %s\n", EFFECTIVE_ADDRESS_CALCULATION_TABLE[rm], displacement, REGISTER_MODE_TABLE[w][reg]);
				}
				break;
			}
		case MEMORY_MODE_8:
			{
				int8_t displacement = read_byte(state);
				if (d) {
					printf("mov %s, [%s + %d]\n", REGISTER_MODE_TABLE[w][reg], EFFECTIVE_ADDRESS_CALCULATION_TABLE[rm], displacement);
				} else {
					printf("mov [%s + %d], %s\n", EFFECTIVE_ADDRESS_CALCULATION_TABLE[rm], displacement, REGISTER_MODE_TABLE[w][reg]);
				}
				break;
			}
		case MEMORY_MODE_0:
			if (rm == 6) {
				// Direct address, 16-bit
				int16_t displacement = read_word(state);
				if (d) {
					printf("mov %s, [%d]\n", REGISTER_MODE_TABLE[w][reg], displacement);
				} else {
					printf("mov [%d], %s\n", displacement, REGISTER_MODE_TABLE[w][reg]);
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

void mov_i2rm(struct sim_state_t *state, struct op_t *op) {
	uint8_t w = state->op_byte & 1;

	uint8_t options_byte = read_byte(state);
	uint8_t mod = (options_byte>> 6) & 3;
	uint8_t rm = options_byte & 7;

	switch (mod) {
		case REGISTER_MODE:
			if (w) {
				int16_t data = read_word(state);
				printf("mov %s, word %d\n", REGISTER_MODE_TABLE[w][rm], data);
			} else {
				int8_t data = read_byte(state);
				printf("mov %s, byte %d\n", REGISTER_MODE_TABLE[w][rm], data);
			}
			break;
		case MEMORY_MODE_16:
			{
				int16_t displacement = read_word(state);
				if (w) {
					int16_t data = read_word(state);
					printf("mov [%s + %d], word %d\n", EFFECTIVE_ADDRESS_CALCULATION_TABLE[rm], displacement, data);
				} else {
					int8_t data = read_byte(state);
					printf("mov [%s + %d], byte %d\n", EFFECTIVE_ADDRESS_CALCULATION_TABLE[rm], displacement, data);
				}
				break;
			}
		case MEMORY_MODE_8:
			{
				int8_t displacement = read_byte(state);
				if (w) {
					int16_t data = read_word(state);
					printf("mov [%s + %d], word %d\n", EFFECTIVE_ADDRESS_CALCULATION_TABLE[rm], displacement, data);
				} else {
					int8_t data = read_byte(state);
					printf("mov [%s + %d], byte %d\n", EFFECTIVE_ADDRESS_CALCULATION_TABLE[rm], displacement, data);
				}
				break;
			}
			break;
		case MEMORY_MODE_0:
			if (rm == 6) {
				int16_t displacement = read_word(state);
				if (w) {
					int16_t data = read_word(state);
					printf("mov [%d], word %d\n", displacement, data);
				} else {
					int8_t data = read_byte(state);
					printf("mov [%d], byte %d\n", displacement, data);
				}
			} else {
				if (w) {
					int16_t data = read_word(state);
					printf("mov [%s], word %d\n", EFFECTIVE_ADDRESS_CALCULATION_TABLE[rm], data);
				} else {
					int8_t data = read_byte(state);
					printf("mov [%s], byte %d\n", EFFECTIVE_ADDRESS_CALCULATION_TABLE[rm], data);
				}
				break;
			}
		default:
			break;
	}
}

void mov_i2r(struct sim_state_t *state, struct op_t *op) {
	uint8_t w = (state->op_byte >> 3) & 1;
	uint8_t reg = (state->op_byte) & 7;

	if (w) {
		int16_t data = read_word(state);
		printf("mov %s, word %d \n", REGISTER_MODE_TABLE[w][reg], data);
	} else {
		int8_t data = read_byte(state);
		printf("mov %s, byte %d \n", REGISTER_MODE_TABLE[w][reg], data);
	}
}

void mov_m2a(struct sim_state_t *state, struct op_t *op) {
	uint8_t w = state->op_byte & 1;
	uint16_t addr = read_word(state);

	if (w) {
		printf("mov ax, [%d]\n", addr);
	} else {
		printf("mov al, [%d]\n", addr);
	}
}

void mov_a2m(struct sim_state_t *state, struct op_t *op) {
	uint8_t w = state->op_byte & 1;
	uint16_t addr = read_word(state);

	if (w) {
		printf("mov [%d], ax\n", addr);
	} else {
		printf("mov [%d], al\n", addr);
	}
}


// ============================================================================
// Entrypoint
// ============================================================================
int main(int argc, char *argv[]) {
	// Main-scoped constants
	const struct op_t OPS[] = {
		(struct op_t) { .opcode_len = 6,  .opcode = 34,  .impl = mov_rm2rm         },
		(struct op_t) { .opcode_len = 7,  .opcode = 99,  .impl = mov_i2rm          },
		(struct op_t) { .opcode_len = 4,  .opcode = 11,  .impl = mov_i2r           },
		(struct op_t) { .opcode_len = 7,  .opcode = 80,  .impl = mov_m2a           },
		(struct op_t) { .opcode_len = 7,  .opcode = 81,  .impl = mov_a2m           },
	};
	const int NUM_OPS = sizeof(OPS)/sizeof(struct op_t);

	// "Parse" args
	if (argc != 2) {
		printf("Must provide machine code filename as sole argument\n");
		return 1;
	}

	// Init state and open input file
	struct sim_state_t state = {0};
	state.fp = fopen(argv[1], "rb");
	if (state.fp == NULL) {
		printf("Could not open file {%s}\n", argv[1]);
		return 2;
	}

	// Write header
	printf("; %s:\nbits 16\n\n", argv[1]);

	// Read bytes while there are bytes left and we haven't hit an error
	while (1) {
		state.op_byte = read_byte(&state);

		// Check if current byte starts a known op
		uint8_t found = 0;
		for (int i = 0; i < NUM_OPS; i++) {
			struct op_t op = OPS[i];
			if ((state.op_byte >> (8-op.opcode_len)) == op.opcode) {
				op.impl(&state, &op);
				found = 1;
				break;
			}
		}

		if (!found) {
			printf("Unknown op from byte=%d\n", state.op_byte);
			fclose(state.fp);
			return 100;
		}
	}

	fclose(state.fp);
	return 0;
}

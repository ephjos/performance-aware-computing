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
	uint32_t labels[1<<15];
	uint32_t labels_len;
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
void core_rm2rm(const char* op_name, struct sim_state_t *state, struct op_t *op) {
	uint8_t options_byte = read_byte(state);

	uint8_t d = (state->op_byte >> 1) & 1;
	uint8_t w = state->op_byte & 1;

	uint8_t mod = (options_byte >> 6) & 3;
	uint8_t reg = (options_byte >> 3) & 7;
	uint8_t rm = options_byte & 7;

	switch (mod) {
		case REGISTER_MODE:
			if (d) {
				printf("%s %s, %s\n", op_name, REGISTER_MODE_TABLE[w][reg], REGISTER_MODE_TABLE[w][rm]);
			} else {
				printf("%s %s, %s\n", op_name, REGISTER_MODE_TABLE[w][rm], REGISTER_MODE_TABLE[w][reg]);
			}
			break;
		case MEMORY_MODE_16:
			{
				int16_t displacement = read_word(state);
				if (d) {
					printf("%s %s, [%s + %d]\n", op_name, REGISTER_MODE_TABLE[w][reg], EFFECTIVE_ADDRESS_CALCULATION_TABLE[rm], displacement);
				} else {
					printf("%s [%s + %d], %s\n", op_name, EFFECTIVE_ADDRESS_CALCULATION_TABLE[rm], displacement, REGISTER_MODE_TABLE[w][reg]);
				}
				break;
			}
		case MEMORY_MODE_8:
			{
				int8_t displacement = read_byte(state);
				if (d) {
					printf("%s %s, [%s + %d]\n", op_name, REGISTER_MODE_TABLE[w][reg], EFFECTIVE_ADDRESS_CALCULATION_TABLE[rm], displacement);
				} else {
					printf("%s [%s + %d], %s\n", op_name, EFFECTIVE_ADDRESS_CALCULATION_TABLE[rm], displacement, REGISTER_MODE_TABLE[w][reg]);
				}
				break;
			}
		case MEMORY_MODE_0:
			if (rm == 6) {
				// Direct address, 16-bit
				int16_t displacement = read_word(state);
				if (d) {
					printf("%s %s, [%d]\n", op_name, REGISTER_MODE_TABLE[w][reg], displacement);
				} else {
					printf("%s [%d], %s\n", op_name, displacement, REGISTER_MODE_TABLE[w][reg]);
				}
			} else {
				// Normal memory mode, no displacement
				if (d) {
					printf("%s %s, [%s]\n", op_name, REGISTER_MODE_TABLE[w][reg], EFFECTIVE_ADDRESS_CALCULATION_TABLE[rm]);
				} else {
					printf("%s [%s], %s\n", op_name, EFFECTIVE_ADDRESS_CALCULATION_TABLE[rm], REGISTER_MODE_TABLE[w][reg]);
				}
			}
			break;
		default:
			break;
	}
}

void core_i2rm(char *op_name, struct sim_state_t *state, struct op_t *op, uint8_t s, uint8_t w, uint8_t mod, uint8_t rm) {
	int16_t data_disp = read_byte(state);
	char *prefix = "byte";

	if (!s && w) {
		data_disp = (uint16_t)(read_byte(state) << 8) | data_disp;
		prefix = "word";
	}

	switch (mod) {
		case REGISTER_MODE:
			printf("%s %s, %s %d\n", op_name, REGISTER_MODE_TABLE[w][rm], prefix, data_disp);
			break;
		case MEMORY_MODE_16:
			{
				int16_t data = read_word(state);
				printf("%s [%s + %d], %s %d\n", op_name, EFFECTIVE_ADDRESS_CALCULATION_TABLE[rm], data_disp, prefix, data);
				break;
			}
		case MEMORY_MODE_8:
			{
				int8_t data = read_byte(state);
				printf("%s [%s + %d], %s %d\n", op_name, EFFECTIVE_ADDRESS_CALCULATION_TABLE[rm], data_disp, prefix, data);
				break;
			}
			break;
		case MEMORY_MODE_0:
			if (rm == 6) {
				int16_t data = read_word(state);
				printf("%s [%d], %s %d\n", op_name, data_disp, prefix, data);
			} else {
				printf("%s [%s], %s %d\n", op_name, EFFECTIVE_ADDRESS_CALCULATION_TABLE[rm], prefix, data_disp);
			}
			break;
		default:
			break;
	}
}

void mov_rm2rm(struct sim_state_t *state, struct op_t *op) {
	core_rm2rm("mov", state, op);
}

void mov_i2rm(struct sim_state_t *state, struct op_t *op) {
	uint8_t w = state->op_byte & 1;

	uint8_t options_byte = read_byte(state);
	uint8_t mod = (options_byte>> 6) & 3;
	uint8_t rm = options_byte & 7;

	core_i2rm("mov", state, op, 0, w, mod, rm);
}

void mov_i2r(struct sim_state_t *state, struct op_t *op) {
	uint8_t w = (state->op_byte >> 3) & 1;
	uint8_t reg = (state->op_byte) & 7;
	int16_t data = read_byte(state);
	char *prefix = "byte";

	if (w) {
		data = (uint16_t)(read_byte(state) << 8) | data;
		prefix = "word";
	}

	printf("mov %s, %s %d \n", REGISTER_MODE_TABLE[w][reg], prefix, data);
}

void mov_m2a(struct sim_state_t *state, struct op_t *op) {
	uint8_t w = state->op_byte & 1;
	uint16_t addr = read_word(state);
	char reg_suffix = 'l';

	if (w) {
		reg_suffix = 'x';
	}

	printf("mov a%c, [%d]\n", reg_suffix, addr);
}

void mov_a2m(struct sim_state_t *state, struct op_t *op) {
	uint8_t w = state->op_byte & 1;
	uint16_t addr = read_word(state);
	char reg_suffix = 'l';

	if (w) {
		reg_suffix = 'x';
	}

	printf("mov [%d], a%c\n", addr, reg_suffix);
}

void shared_i2rm(struct sim_state_t *state, struct op_t *op) {
	uint8_t s = (state->op_byte >> 1) & 1;
	uint8_t w = state->op_byte & 1;

	uint8_t options_byte = read_byte(state);
	uint8_t mod = (options_byte >> 6) & 3;
	uint8_t local_op = (options_byte >> 3) & 7;
	uint8_t rm = options_byte & 7;

	switch (local_op) {
		case 0:
			core_i2rm("add", state, op, s, w, mod, rm);
			break;
		case 5:
			core_i2rm("sub", state, op, s, w, mod, rm);
			break;
		case 7:
			core_i2rm("cmp", state, op, s, w, mod, rm);
			break;
	}
}

void core_i2a(char *op_name, struct sim_state_t *state, struct op_t *op) {
	uint8_t w = state->op_byte & 1;
	uint16_t data = read_byte(state);
	char reg_suffix = 'l';

	if (w) {
		data = (uint16_t)(read_byte(state) << 8) | data;
		reg_suffix = 'x';
	}

	printf("%s a%c, %d \n", op_name, reg_suffix, data);
}

void add_rm2rm(struct sim_state_t *state, struct op_t *op) {
	core_rm2rm("add", state, op);
}

void add_i2a(struct sim_state_t *state, struct op_t *op) {
	core_i2a("add", state, op);
}

void sub_rm2rm(struct sim_state_t *state, struct op_t *op) {
	core_rm2rm("sub", state, op);
}

void sub_i2a(struct sim_state_t *state, struct op_t *op) {
	core_i2a("sub", state, op);
}

void cmp_rm2rm(struct sim_state_t *state, struct op_t *op) {
	core_rm2rm("cmp", state, op);
}

void cmp_i2a(struct sim_state_t *state, struct op_t *op) {
	core_i2a("cmp", state, op);
}

void je(struct sim_state_t *state, struct op_t *op) {
	printf("je %d\n", (int8_t)read_byte(state));
}

void jl(struct sim_state_t *state, struct op_t *op) {
	printf("jl %d\n", (int8_t)read_byte(state));
}

void jle(struct sim_state_t *state, struct op_t *op) {
	printf("jle %d\n", (int8_t)read_byte(state));
}

void jb(struct sim_state_t *state, struct op_t *op) {
	printf("jb %d\n", (int8_t)read_byte(state));
}

void jbe(struct sim_state_t *state, struct op_t *op) {
	printf("jbe %d\n", (int8_t)read_byte(state));
}

void jp(struct sim_state_t *state, struct op_t *op) {
	printf("jp %d\n", (int8_t)read_byte(state));
}

void jo(struct sim_state_t *state, struct op_t *op) {
	printf("jo %d\n", (int8_t)read_byte(state));
}

void js(struct sim_state_t *state, struct op_t *op) {
	printf("js %d\n", (int8_t)read_byte(state));
}

void jnz(struct sim_state_t *state, struct op_t *op) {
	printf("jnz %d\n", (int8_t)read_byte(state));
}

void jnl(struct sim_state_t *state, struct op_t *op) {
	printf("jnl %d\n", (int8_t)read_byte(state));
}

void jnle(struct sim_state_t *state, struct op_t *op) {
	printf("jnle %d\n", (int8_t)read_byte(state));
}

void jnb(struct sim_state_t *state, struct op_t *op) {
	printf("jnb %d\n", (int8_t)read_byte(state));
}

void jnbe(struct sim_state_t *state, struct op_t *op) {
	printf("jnbe %d\n", (int8_t)read_byte(state));
}

void jnp(struct sim_state_t *state, struct op_t *op) {
	printf("jnp %d\n", (int8_t)read_byte(state));
}

void jno(struct sim_state_t *state, struct op_t *op) {
	printf("jno %d\n", (int8_t)read_byte(state));
}

void jns(struct sim_state_t *state, struct op_t *op) {
	printf("jns %d\n", (int8_t)read_byte(state));
}

void loop(struct sim_state_t *state, struct op_t *op) {
	printf("loop %d\n", (int8_t)read_byte(state));
}

void loopz(struct sim_state_t *state, struct op_t *op) {
	printf("loopz %d\n", (int8_t)read_byte(state));
}

void loopnz(struct sim_state_t *state, struct op_t *op) {
	printf("loopnz %d\n", (int8_t)read_byte(state));
}

void jcxz(struct sim_state_t *state, struct op_t *op) {
	printf("jcxz %d\n", (int8_t)read_byte(state));
}


// ============================================================================
// Entrypoint
// ============================================================================
int main(int argc, char *argv[]) {
	// Main-scoped constants
	const struct op_t OPS[] = {
		(struct op_t) { .opcode_len = 6,  .opcode = 0b00100010,   .impl = mov_rm2rm          },
		(struct op_t) { .opcode_len = 7,  .opcode = 0b01100011,   .impl = mov_i2rm           },
		(struct op_t) { .opcode_len = 4,  .opcode = 0b00001011,   .impl = mov_i2r            },
		(struct op_t) { .opcode_len = 7,  .opcode = 0b01010000,   .impl = mov_m2a            },
		(struct op_t) { .opcode_len = 7,  .opcode = 0b01010001,   .impl = mov_a2m            },
		(struct op_t) { .opcode_len = 6,  .opcode = 0b00100000,   .impl = shared_i2rm        },
		(struct op_t) { .opcode_len = 6,  .opcode = 0b00000000,   .impl = add_rm2rm          },
		(struct op_t) { .opcode_len = 7,  .opcode = 0b00000010,   .impl = add_i2a            },
		(struct op_t) { .opcode_len = 6,  .opcode = 0b00001010,   .impl = sub_rm2rm          },
		(struct op_t) { .opcode_len = 7,  .opcode = 0b00010110,   .impl = sub_i2a            },
		(struct op_t) { .opcode_len = 6,  .opcode = 0b00001110,   .impl = cmp_rm2rm          },
		(struct op_t) { .opcode_len = 7,  .opcode = 0b00011110,   .impl = cmp_i2a            },
		(struct op_t) { .opcode_len = 8,  .opcode = 0b01110100,   .impl = je                 },
		(struct op_t) { .opcode_len = 8,  .opcode = 0b01111100,   .impl = jl                 },
		(struct op_t) { .opcode_len = 8,  .opcode = 0b01111110,   .impl = jle                },
		(struct op_t) { .opcode_len = 8,  .opcode = 0b01110010,   .impl = jb                 },
		(struct op_t) { .opcode_len = 8,  .opcode = 0b01110110,   .impl = jbe                },
		(struct op_t) { .opcode_len = 8,  .opcode = 0b01111010,   .impl = jp                 },
		(struct op_t) { .opcode_len = 8,  .opcode = 0b01110000,   .impl = jo                 },
		(struct op_t) { .opcode_len = 8,  .opcode = 0b01111000,   .impl = js                 },
		(struct op_t) { .opcode_len = 8,  .opcode = 0b01110101,   .impl = jnz                },
		(struct op_t) { .opcode_len = 8,  .opcode = 0b01111101,   .impl = jnl                },
		(struct op_t) { .opcode_len = 8,  .opcode = 0b01111111,   .impl = jnle               },
		(struct op_t) { .opcode_len = 8,  .opcode = 0b01110011,   .impl = jnb                },
		(struct op_t) { .opcode_len = 8,  .opcode = 0b01110111,   .impl = jnbe               },
		(struct op_t) { .opcode_len = 8,  .opcode = 0b01111011,   .impl = jnp                },
		(struct op_t) { .opcode_len = 8,  .opcode = 0b01110001,   .impl = jno                },
		(struct op_t) { .opcode_len = 8,  .opcode = 0b01111001,   .impl = jns                },
		(struct op_t) { .opcode_len = 8,  .opcode = 0b11100010,   .impl = loop               },
		(struct op_t) { .opcode_len = 8,  .opcode = 0b11100001,   .impl = loopz              },
		(struct op_t) { .opcode_len = 8,  .opcode = 0b11100000,   .impl = loopnz             },
		(struct op_t) { .opcode_len = 8,  .opcode = 0b11100011,   .impl = jcxz               },
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

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
// Macros
// ============================================================================

#define MAX_OPERANDS 5
#define MAX_BLOCKS 16
#define INITIAL_CAP 512

// ============================================================================
// Encodings macros + table
// ============================================================================

#define LITERAL(b) {bits_literal, sizeof(#b) - 1, 0b##b}
#define END {bits_end, 0, 0}
#define D {bits_d, 1, 0}
#define W {bits_w, 1, 0}
#define MOD {bits_mod, 2, 0}
#define REG {bits_reg, 3, 0}
#define RM {bits_rm, 3, 0}
#define DISP_LO {bits_disp_lo, 8, 0}
#define DISP_HI {bits_disp_hi, 8, 0}
#define DATA {bits_data, 8, 0}
#define DATA_IF_W {bits_data_if_w, 8, 0}
#define ADDR_LO {bits_addr_lo, 8, 0}
#define ADDR_HI {bits_addr_hi, 8, 0}


#define ENCODINGS(FIRST, DUPE) \
	FIRST(mov, { LITERAL(100010), D, W, MOD, REG, RM, DISP_LO, DISP_HI }) \
	DUPE(mov, { LITERAL(1100011), W, MOD, LITERAL(000), RM, DISP_LO, DISP_HI, DATA, DATA_IF_W }) \
	DUPE(mov, { LITERAL(1011), W, REG, DATA, DATA_IF_W }) \
	DUPE(mov, { LITERAL(1010000), W, ADDR_LO, ADDR_HI }) \
	DUPE(mov, { LITERAL(1010001), W, ADDR_LO, ADDR_HI }) \

#define ENCODINGS_NOOP(name, ...)

// ============================================================================
// Bits macros + table
// ============================================================================

#define BITS(F) \
	F(literal) \
	F(d) \
	F(w) \
	F(mod) \
	F(reg) \
	F(rm) \
	F(disp_lo) \
	F(disp_hi) \
	F(data) \
	F(data_if_w) \
	F(addr_lo) \
	F(addr_hi) \


// ============================================================================
// Enums
// ============================================================================

#define BITS_TO_ENUM(name) bits_##name,
enum bits {
	bits_end,

	BITS(BITS_TO_ENUM)

	bits_count,
};

#define ENCODING_TO_PNEUMONIC(name, ...) op_##name,
enum pneumonic {
	ENCODINGS(ENCODING_TO_PNEUMONIC, ENCODINGS_NOOP)
	op_count,
};

enum operand_type {
	operand_end,

	operand_register,
	operand_memory,
	operand_immediate,

	operand_count,
};

// ============================================================================
// Structs
// ============================================================================

struct encoding_block {
	enum bits type;
	uint8_t size;
	uint8_t value;
};

struct encoding {
	enum pneumonic op;
	struct encoding_block blocks[MAX_BLOCKS];
};

struct operand {
	enum operand_type type;
	union {
		struct register_operand {
			uint8_t index;
			uint8_t offset; // in bytes
			uint8_t width; // in bytes
		} reg;
		struct memory_operand {
			struct register_operand effective_address[2];
			int16_t displacement;
		} mem;
		struct immediate_operand {
			int16_t value;
		} imm;
	};
};

struct instruction {
	enum pneumonic op;
	struct operand operands[MAX_OPERANDS];
};

struct decoder_t {
	char *filename;

	uint32_t bytes_curr;
	uint32_t bytes_cap;
	uint32_t bytes_len;
	uint8_t *bytes;

	uint32_t instructions_curr;
	uint32_t instructions_cap;
	uint32_t instructions_len;
	struct instruction *instructions;

	// TODO: labels?
};
struct decoder_t decoder;


// ============================================================================
// Constants
// ============================================================================

#define BITS_TO_STRING(name) #name,
const char *bits_strings[bits_count] = {
	"[END]",
	BITS(BITS_TO_STRING)
};

#define ENCODING_TO_PNEUMONIC_STRING(name, ...) #name,
const char *pneumonic_strings[op_count] = {
	ENCODINGS(ENCODING_TO_PNEUMONIC_STRING, ENCODINGS_NOOP)
};

#define ENCODING_TO_STRUCT(name, blocks, ...) { op_##name, blocks, __VA_ARGS__ },
const struct encoding encodings[] = {
	ENCODINGS(ENCODING_TO_STRUCT, ENCODING_TO_STRUCT)
};
const uint32_t NUM_ENCODINGS = sizeof(encodings) / sizeof(encodings[0]);

// Given a register (index, offset, width) = REG_NAMES[index][offset][width-1]
const char *REG_NAMES[13][2][2] = {
 { {"al", "ax"}, {"ah"} },
 { {"bl", "bx"}, {"bh"} },
 { {"cl", "cx"}, {"ch"} },
 { {"dl", "dx"}, {"dh"} },
 { {0, "sp"}, {0} },
 { {0, "bp"}, {0} },
 { {0, "si"}, {0} },
 { {0, "di"}, {0} },
 { {0, "cs"}, {0} },
 { {0, "ds"}, {0} },
 { {0, "ss"}, {0} },
 { {0, "es"}, {0} },
 { {0, "ip"}, {0} },
};

#define AL {0,  0, 1}
#define AX {0,  0, 2}
#define AH {0,  1, 1}
#define BL {1,  0, 1}
#define BX {1,  0, 2}
#define BH {1,  1, 1}
#define CL {2,  0, 1}
#define CX {2,  0, 2}
#define CH {2,  1, 1}
#define DL {3,  0, 1}
#define DX {3,  0, 2}
#define DH {3,  1, 1}
#define SP {4,  0, 2}
#define BP {5,  0, 2}
#define SI {6,  0, 2}
#define DI {7,  0, 2}
#define CS {8,  0, 2}
#define DS {9,  0, 2}
#define SS {10, 0, 2}
#define ES {11, 0, 2}
#define IP {12, 0, 2}

const struct register_operand W_RM_REG[2][8] = {
	{ AL, CL, DL, BL, AH, CH, DH, BH },
	{ AX, CX, DX, BX, SP, BP, SI, DI },
};

// ============================================================================
// Functions
// ============================================================================

// TODO: trap for this?
void cleanup_and_exit(int exit_code) {
	free(decoder.bytes);
	free(decoder.instructions);
	exit(exit_code);
}

void print_instruction_disasm(struct instruction instr) {
	printf("%s", pneumonic_strings[instr.op]);

	uint8_t done = 0;
	char *sep = " ";
	for (int j = 0; j < MAX_OPERANDS; j++) {
		struct operand o = instr.operands[j];
		switch (o.type) {
			case operand_end:
				done = 1;
				break;
			case operand_count:
				break;
			case operand_register:
				printf("%s%s", sep, REG_NAMES[o.reg.index][o.reg.offset][o.reg.width-1]);
				break;
			case operand_memory:
				break;
			case operand_immediate:
				break;
		}

		if (done) {
			break;
		}

		sep = ", ";
	}
	printf("\n");
}

void print_disasm() {
	printf("; %s\nbits 16\n\n", decoder.filename);

	for (uint32_t i = 0; i < decoder.instructions_len; i++) {
		print_instruction_disasm(decoder.instructions[i]);
	}
}

void init_decoder(char *filename) {
	decoder.filename = filename;

	// Init state and open input file.
	// Do this before struct init to avoid cleanup
	FILE *fp = fopen(filename, "rb");
	if (fp == NULL) {
		printf("Could not open file {%s}\n", filename);
		exit(2);
	}

	// Struct init
	decoder.bytes_curr = 0;
	decoder.bytes_cap = INITIAL_CAP*2;
	decoder.bytes_len = 0;
	decoder.bytes = malloc(decoder.bytes_cap);
	decoder.instructions_curr = 0;
	decoder.instructions_cap = INITIAL_CAP;
	decoder.instructions_len = 0;
	decoder.instructions = malloc(
			sizeof(struct instruction) * decoder.instructions_cap);

	// Read input file into memory
	int c = fgetc(fp);
	while (c != EOF) {
		decoder.bytes[decoder.bytes_len++] = (uint8_t)c;

		// Expand capacity if needed
		if (decoder.bytes_len == decoder.bytes_cap) {
			decoder.bytes_cap <<= 1;
			decoder.bytes = realloc(decoder.bytes, decoder.bytes_cap);
		}
		c = fgetc(fp);
	}

	// Shrink container to fit content
	decoder.bytes = realloc(decoder.bytes, decoder.bytes_len);

	fclose(fp);
}

uint8_t decoder_next() {
	//printf("decoder loc %d\n", decoder.bytes_curr);
	if (decoder.bytes_curr < decoder.bytes_len) {
		return decoder.bytes[decoder.bytes_curr++];
	}

	printf("Reached end of bytes unexpectedly!\n");
	cleanup_and_exit(4);

	__builtin_unreachable();
}

void build_and_store_instruction(struct encoding current_encoding, uint8_t bits_table[bits_count], uint8_t bits_seen[bits_count]) {
	/*
	printf("Matched %s!\n", pneumonic_strings[current_encoding.op]);
	for (int j = 1; j < bits_count; j++) {
		printf("  %s=%d (%d)\n", bits_strings[j], bits_table[j], bits_seen[j]);
	}
	*/

	struct instruction instr = {
		.op = current_encoding.op,
	};

	struct operand op1, op2;

	uint8_t mod = bits_table[bits_mod];
	uint8_t d = bits_table[bits_d];
	uint8_t w = bits_table[bits_w];
	uint8_t reg = bits_table[bits_reg];
	uint8_t rm = bits_table[bits_rm];

	if (bits_seen[bits_mod]) {
		// MOD formatted instruction
		if (mod == 0b11) {
			op1 = (struct operand){
				.type = operand_register,
				.reg = W_RM_REG[w][reg]
			};
			op2 = (struct operand){
				.type = operand_register,
				.reg = W_RM_REG[w][rm]
			};
		} else if ((mod == 0b10) || (mod == 0b00 && rm == 0b110)) {
			// TODO: 16-bit displacement
		} else if (mod == 0b01) {
			// TODO: 8-bit displacement
		}
	} else if (bits_seen[bits_reg]) {
		// Single reg instruction
		// TODO: HANDLE SINGLE REG
	}

	if (bits_seen[bits_d]) {
		if (d) {
			instr.operands[0] = op1;
			instr.operands[1] = op2;
		} else {
			instr.operands[0] = op2;
			instr.operands[1] = op1;
		}
	} else {
		// TODO:
	}

	print_instruction_disasm(instr);

	decoder.instructions[decoder.instructions_len++] = instr;

	// Expand capacity if needed
	if (decoder.instructions_len == decoder.instructions_cap) {
		decoder.instructions_cap <<= 1;
		decoder.instructions = realloc(decoder.instructions, decoder.instructions_cap);
	}
}

void decode() {
	while (decoder.bytes_curr < decoder.bytes_len) {
		uint8_t current_byte = decoder_next();
		uint8_t found = 0;

		// Test all encodings to see if this matches
		for (uint32_t i = 0; i < NUM_ENCODINGS; i++) {
			struct encoding current_encoding = encodings[i];

			// Check first block
			struct encoding_block *current_block = current_encoding.blocks;

			// If the current_byte does not look match the first block, skip it
			uint32_t shift = 8 - current_block->size;
			if ((current_byte >> shift) != current_block->value) {
				continue;
			}

			// First literal matched, this *could* be the encoding
			found = 1;

			// Vars for parsing blocks
			uint8_t bits_table[bits_count] = {0};
			uint8_t bits_seen[bits_count] = {0};
			uint32_t decoder_prev = decoder.bytes_curr;

			// Pull data out of rest of blocks
			while ((++current_block)->type != bits_end) {
				uint8_t w = bits_table[bits_w];
				uint8_t mod = bits_table[bits_mod];
				uint8_t rm = bits_table[bits_rm];
				uint8_t need_disp_lo = bits_seen[mod] && ((mod == 0b01) || (mod == 0b10) || (mod == 0b00 && rm == 0b110));
				uint8_t need_disp_hi= bits_seen[mod] && ((mod == 0b10) || (mod == 0b00 && rm == 0b110));

				if (current_block->type == bits_disp_lo && !need_disp_lo) {
					continue;
				} else if (current_block->type == bits_disp_hi && !need_disp_hi) {
					continue;
				} else if (current_block->type == bits_data_if_w && !w) {
					continue;
				}

				// Take bits
				if (shift == 0) {
					shift = 8;
					current_byte = decoder_next();
				}

				if (current_block->size) {
					// Get bits if this block needs to
					shift -= current_block->size;
					uint8_t mask = (uint8_t)((1<<current_block->size)-1);
					uint8_t value = (uint8_t)(current_byte >> shift) & mask;
					bits_table[current_block->type] = value;
					bits_seen[current_block->type] = 1;
				} else {
					// Set bits if block provides explicit value
					bits_table[current_block->type] = current_block->value;
					bits_seen[current_block->type] = 1;
				}

				if ((current_block->type == bits_literal) &&
						(bits_table[current_block->type] != current_block->value)) {
					// If this is a literal and the value does not line up
					// this is not the encoding
					decoder.bytes_curr = decoder_prev;
					found = 0;
					break;
				}
			}

			// Some other literal did not match, not the encoding
			if (!found) {
				continue;
			}

			build_and_store_instruction(current_encoding, bits_table, bits_seen);
			break;
		}

		if (!found) {
			printf("No encodings found for byte: %d\n", current_byte);
			cleanup_and_exit(5);
		}
	}

	// Shrink container to fit content
	decoder.instructions = realloc(decoder.instructions, sizeof(struct instruction) * decoder.instructions_len);
}

void verify_encodings() {
	for (uint32_t i = 0; i < NUM_ENCODINGS; i++) {
		struct encoding current_encoding = encodings[i];

		// Check first block
		struct encoding_block first_block = current_encoding.blocks[0];

		// Must be literal
		if (first_block.type != bits_literal) {
			printf("First block in encoding [%d] %s is not bits_literal; is %s\n",
					i,
					pneumonic_strings[current_encoding.op],
					bits_strings[first_block.type]);
			cleanup_and_exit(2);
		}

		// Confirm that there are trailing 0 bytes at the end of the blocks
		uint8_t has_end = 0;
		for (int j = 0; j < MAX_BLOCKS; j++) {
			struct encoding_block current_block = current_encoding.blocks[j];
			if (current_block.type == bits_end) {
				has_end = 1;
			}
		}

		if (!has_end) {
			printf("Encoding [%d] %s has too many blocks\n",
					i, pneumonic_strings[current_encoding.op]);
			cleanup_and_exit(3);
		}
	}
}

// ============================================================================
// Entrypoint
// ============================================================================
int main(int argc, char *argv[]) {
	// "Parse" args
	if (argc != 2) {
		printf("USAGE: sim8086 FILENAME\n");
		return 1;
	}

	verify_encodings();

	init_decoder(argv[1]);
	decode();
	print_disasm();
	cleanup_and_exit(0);
}

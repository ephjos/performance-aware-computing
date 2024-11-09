#include <assert.h>
#include <ctype.h>
#include <inttypes.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <x86intrin.h>

#include "shared.h"

#define PROF_ENABLE 1
#include "prof.h"

/*******************************************************************************
 * Debug helpers
 */
#ifndef DEBUG
#define DEBUG 0
#endif

void __log_line(const char *fmt, ...) {
  char buf[1024] = {0};
  strcat(buf, "[DEBUG] ");
  strcat(buf, fmt);
  strcat(buf, "\n");

  va_list args;
  va_start(args, fmt);
  vfprintf(stderr, buf, args);
  va_end(args);
}

#define LOG(x) do { if (DEBUG) __log_line x; } while (0)

enum token_type {
  TOKEN_END,

  TOKEN_LSQUIRLY,
  TOKEN_RSQUIRLY,
  TOKEN_LBRACKET,
  TOKEN_RBRACKET,
  TOKEN_DQUOTE,
  TOKEN_COMMA,
  TOKEN_COLON,
  TOKEN_IDENT,
  TOKEN_NUMBER,

  TOKEN_UNKNOWN,
};

struct token {
  enum token_type type;
  union {
    char *ident;
    f64 number;
    char unknown;
  } value;
}; 

typedef f64 pair[4];

struct json_input {
  pair *pairs;
  u32 pairs_len;
  f64 expected;
};

// Convert [x0, x1, y0, y1] to [0, 1, 2, 3]
#define ident2index(s) (s[1]+(s[0]<<1)-288)

#define EARTH_RADIUS_KM 6372.8
#define square(x) ((x)*(x))
#define deg2rad(d) (0.01745329251994329577*(d))

static inline f64 haversine(f64 x0, f64 y0, f64 x1, f64 y1) {
  f64 dy = deg2rad(y1-y0);
  f64 dx = deg2rad(x1-x0);
  f64 ry0 = deg2rad(y0);
  f64 ry1 = deg2rad(y1);

  f64 a = square(sin(dy/2.0)) + cos(ry0)*cos(ry1)*square(sin(dx/2.0));
  f64 c = 2.0*asin(sqrt(a));

  return EARTH_RADIUS_KM * c;
}

char *read_file(char *filename, u64 *size) {
  FILE *input_file = fopen(filename, "rb");

  if (input_file == NULL) {
    fprintf(stderr, "Could not open %s for reading\n", filename);
    exit(1);
  }

  struct stat stats;

  stat(filename, &stats);
  *size = stats.st_size;
  char *bytes = malloc(*size);

  if (bytes == NULL) {
    fprintf(stderr, "Could not alloc %"PRIu64" bytes for reading %s\n", *size, filename);
    fclose(input_file);
    exit(1);
  }

  {
    PROF_BANDWIDTH("read", *size);
    if(fread(bytes, *size, 1, input_file) != 1) {
      fprintf(stderr, "Unable to read %s\n", filename);
      free(bytes);
      exit(1);
    }
  }

  fclose(input_file);
  return bytes;
}

struct token *lex(char *bytes, u64 size, u64 *num_tokens) {
  PROF_BANDWIDTH(__func__, size);

  u32 tokens_cap = 1024;
  u32 tokens_len = 0;
  struct token *tokens = malloc(sizeof(struct token) * tokens_cap);

  u64 i = 0;
  for (; i < size; i++) {
    char c = bytes[i];
    switch (c) {
      case '{':
        tokens[tokens_len++] = (struct token) {
          .type = TOKEN_LSQUIRLY,
        };
        break;
      case '}':
        tokens[tokens_len++] = (struct token) {
          .type = TOKEN_RSQUIRLY,
        };
        break;
      case '[':
        tokens[tokens_len++] = (struct token) {
          .type = TOKEN_LBRACKET,
        };
        break;
      case ']':
        tokens[tokens_len++] = (struct token) {
          .type = TOKEN_RBRACKET,
        };
        break;
      case '"':
        tokens[tokens_len++] = (struct token) {
          .type = TOKEN_DQUOTE,
        };
        break;
      case ',':
        tokens[tokens_len++] = (struct token) {
          .type = TOKEN_COMMA,
        };
        break;
      case ':':
        tokens[tokens_len++] = (struct token) {
          .type = TOKEN_COLON,
        };
        break;
      default:
        {
          if (isdigit(c) || c == '-') {
            u32 buf_i = 0;
            char buf[256] = {0};
            buf[buf_i++] = c;
            while (isdigit((c = bytes[++i])) || c == '.') {
              buf[buf_i++] = c;
            }
            i--;
            tokens[tokens_len++] = (struct token) {
              .type = TOKEN_NUMBER,
              .value = { .number = atof(buf) },
            };
          } else if (isalnum(c)) {
            u32 buf_i = 0;
            char *buf = calloc(128, sizeof(char));
            buf[buf_i++] = c;
            while (isalnum((c = bytes[++i]))) {
              buf[buf_i++] = c;
            }
            i--;
            buf = realloc(buf, strlen(buf)+1);
            tokens[tokens_len++] = (struct token) {
              .type = TOKEN_IDENT,
              .value = { .ident = buf },
            };
          } else if (!isspace(c)) {
            tokens[tokens_len++] = (struct token) {
              .type = TOKEN_UNKNOWN,
              .value = { .unknown = c },
            };
            break;
          }
        }
        break;
    }

    if (tokens_len >= tokens_cap) {
      tokens_cap <<= 1;
      tokens = realloc(tokens, sizeof(struct token) * tokens_cap);
    }
  }

  tokens[tokens_len++] = (struct token) {
    .type = TOKEN_END,
  };

  *num_tokens = tokens_len;

  tokens = realloc(tokens, sizeof(struct token) * tokens_len);
  return tokens;
}


struct json_input parse(struct token *tokens, u64 num_tokens) {
  PROF_BANDWIDTH(__func__, num_tokens * sizeof(struct token));

  u32 pairs_cap = 1024;
  struct json_input input = {
    .pairs = malloc(sizeof(pair) * pairs_cap),
    .pairs_len = 0,
    .expected = 0,
  };
  
  u32 stack[1024] = {0};
  u32 sp = 0;

  u32 i = 0;
  struct token curr = tokens[i++];
  while (curr.type != TOKEN_END) {
    switch (curr.type) {
      case TOKEN_LSQUIRLY:
      case TOKEN_LBRACKET:
      case TOKEN_DQUOTE:
        if (sp && stack[sp-1] == TOKEN_DQUOTE) {
          assert(stack[--sp] == TOKEN_DQUOTE);
        } else {
          stack[sp++] = curr.type;
        }
        break;
      case TOKEN_RSQUIRLY:
        assert(stack[--sp] == TOKEN_LSQUIRLY);
        break;
      case TOKEN_RBRACKET:
        assert(stack[--sp] == TOKEN_LBRACKET);
        break;
      case TOKEN_IDENT:
        if (sp == 2) {
          if (strcmp(curr.value.ident, "expected") == 0) {
            assert((curr = tokens[i++]).type == TOKEN_DQUOTE);
            assert(stack[--sp] == TOKEN_DQUOTE);
            assert((curr = tokens[i++]).type == TOKEN_COLON);
            assert((curr = tokens[i++]).type == TOKEN_NUMBER);
            input.expected = curr.value.number;
          } else {
            assert(strcmp(curr.value.ident, "pairs") == 0);
          }
        } else if (sp == 4) {
          // Find all 4 components of a pair
          for (int j = 0; j < 4; j++) {
            char *curr_ident = curr.value.ident;

            assert((curr = tokens[i++]).type == TOKEN_DQUOTE);
            assert(stack[--sp] == TOKEN_DQUOTE);
            assert((curr = tokens[i++]).type == TOKEN_COLON);
            assert((curr = tokens[i++]).type == TOKEN_NUMBER);
            input.pairs[input.pairs_len][ident2index(curr_ident)] = curr.value.number;

            if (j != 3) {
              assert((curr = tokens[i++]).type == TOKEN_COMMA);
              assert((curr = tokens[i++]).type == TOKEN_DQUOTE);
              stack[sp++] = curr.type;
              assert((curr = tokens[i++]).type == TOKEN_IDENT);
            }
          }

          input.pairs_len++;

          if (input.pairs_len >= pairs_cap) {
            pairs_cap <<= 1;
            input.pairs = realloc(input.pairs, sizeof(pair) * pairs_cap);
          }
        }
        break;
      default:
        break;
    }

    curr = tokens[i++];
  }

  assert(sp == 0);

  input.pairs = realloc(input.pairs, sizeof(pair) * input.pairs_len);
  return input;
}

f64 sum_pairs(struct json_input *input) {
  PROF_BANDWIDTH("sum", input->pairs_len * sizeof(pair));

  f64 sum = 0;
  for (u32 i = 0; i < input->pairs_len; i++) {
    sum += haversine(input->pairs[i][0], input->pairs[i][2], input->pairs[i][1], input->pairs[i][3]);
  }
  return sum;
}

int main(int argc, char *argv[]) {
  PROF_INIT();

  if (argc != 2) {
    fprintf(stderr, "Usage: haversine filename\n");
    exit(1);
  }

  u64 file_size = 0;
  char *file_bytes = read_file(argv[1], &file_size);

  u64 num_tokens = 0;
  struct token *tokens = lex(file_bytes, file_size, &num_tokens);
  struct json_input input = parse(tokens, num_tokens);

  f64 sum = sum_pairs(&input);
  f64 average = (f64)sum/input.pairs_len;
  printf("expected = %12.6f\nactual   = %12.6f\n", input.expected, average);

  {
    PROF_BANDWIDTH("cleanup", (file_size) + (input.pairs_len * sizeof(pair)) + (num_tokens * sizeof(struct token)));
    if (file_bytes != NULL) free(file_bytes);
    if (input.pairs != NULL) free(input.pairs);
    if (tokens != NULL) {
      u32 i = 0;
      struct token curr = tokens[i++];
      while (curr.type != TOKEN_END) {
        if (curr.type == TOKEN_IDENT) {
          free(curr.value.ident);
        }
        curr = tokens[i++];
      }
      free(tokens);
    }
  }

  return 0;
}


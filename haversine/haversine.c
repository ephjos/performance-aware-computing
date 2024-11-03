#include <assert.h>
#include <ctype.h>
#include <inttypes.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <x86intrin.h>


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

/******************************************************************************
 * Timing
 */

uint64_t get_os_timer_freq() {
  return 1000000;
}

uint64_t read_os_timer() {
  struct timeval value;
  gettimeofday(&value, 0);

  return get_os_timer_freq() * (uint64_t)value.tv_sec + (uint64_t)value.tv_usec;
}

static inline uint64_t read_cpu_timer() {
  return __rdtsc();
}

static inline uint64_t estimate_cpu_freq(uint64_t wait_ms) {
  uint64_t os_freq = get_os_timer_freq();

  uint64_t cpu_start = read_cpu_timer();
  uint64_t os_start = read_os_timer();
  uint64_t os_end = 0;
  uint64_t os_elapsed = 0;
  uint64_t os_wait_time = os_freq * wait_ms / 1000;

  while (os_elapsed < os_wait_time) {
    os_end = read_os_timer();
    os_elapsed = os_end - os_start;
  }

  uint64_t cpu_end = read_cpu_timer();
  uint64_t cpu_elapsed = cpu_end - cpu_start;
  uint64_t cpu_freq = 0;

  if (os_elapsed) {
    cpu_freq = os_freq * cpu_elapsed / os_elapsed;
  }

  return cpu_freq;
}

#define MAX_TIMING_CONTEXTS 4096

struct timing_context {
  uint64_t start;
  uint64_t end;
  const char *name;
};
struct timing_context timing_contexts[MAX_TIMING_CONTEXTS] = {0};
uint32_t timing_contexts_count = 0;

void __save_end_time__(struct timing_context *ctx) {
  ctx->end = read_cpu_timer();
  if (timing_contexts_count < MAX_TIMING_CONTEXTS) {
    timing_contexts[timing_contexts_count++] = *ctx;
  } else {
    fprintf(stderr, "Maximum timing contexts (%d) reached, exiting.", MAX_TIMING_CONTEXTS);
    exit(1);
  }
}

void __print_timing__(uint64_t *overall_start) {
  uint64_t overall_end = read_cpu_timer();
  uint64_t total = overall_end - *overall_start;
  uint64_t cpu_freq = estimate_cpu_freq(100);

  printf("\nTotal time: %.4fms (CPU Freq %"PRIu64")\n", ((double)total / (double)cpu_freq) * 1000, cpu_freq);
  for (uint32_t i = 0; i < timing_contexts_count; i++) {
    struct timing_context ctx = timing_contexts[i];
    uint64_t duration = ctx.end - ctx.start;
    printf("  %16s:  %"PRIu64" (%.2f%%)\n", ctx.name, duration, ((double)duration / (double)total) * 100);
  }
  printf("\n");
}

#define BEGIN_TIMING() uint64_t __begin_timing__ __attribute__ ((__cleanup__(__print_timing__))) = read_cpu_timer();
#define TIME_FUNCTION() \
  struct timing_context __ctx__ __attribute__ ((__cleanup__(__save_end_time__))) = {\
    .start = read_cpu_timer(),\
    .end = 0,\
    .name = __func__,\
  }
#define TIME_BLOCK(x) \
  struct timing_context __ctx__ __attribute__ ((__cleanup__(__save_end_time__))) = {\
    .start = read_cpu_timer(),\
    .end = 0,\
    .name = x,\
  }


//******************************************************************************

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
    double number;
    char unknown;
  } value;
}; 

typedef double pair[4];

struct json_input {
  pair *pairs;
  uint32_t pairs_len;
  double expected;
};

// Convert [x0, x1, y0, y1] to [0, 1, 2, 3]
#define ident2index(s) (s[1]+(s[0]<<1)-288)

#define EARTH_RADIUS_KM 6372.8
#define square(x) ((x)*(x))
#define deg2rad(d) (0.01745329251994329577*(d))

static inline double haversine(double x0, double y0, double x1, double y1) {
  double dy = deg2rad(y1-y0);
  double dx = deg2rad(x1-x0);
  double ry0 = deg2rad(y0);
  double ry1 = deg2rad(y1);

  double a = square(sin(dy/2.0)) + cos(ry0)*cos(ry1)*square(sin(dx/2.0));
  double c = 2.0*asin(sqrt(a));

  return EARTH_RADIUS_KM * c;
}

struct token *lex(FILE *fp) {
  TIME_FUNCTION();

  uint32_t tokens_cap = 1024;
  uint32_t tokens_len = 0;
  struct token *tokens = malloc(sizeof(struct token) * tokens_cap);

  char c;

  while((c = (char)fgetc(fp)) != EOF) {
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
            uint32_t buf_i = 0;
            char buf[256] = {0};
            buf[buf_i++] = c;
            while (isdigit((c = (char)fgetc(fp))) || c == '.') {
              buf[buf_i++] = c;
            }
            fseek(fp, -1, SEEK_CUR);
            tokens[tokens_len++] = (struct token) {
              .type = TOKEN_NUMBER,
              .value = { .number = atof(buf) },
            };
          } else if (isalnum(c)) {
            uint32_t buf_i = 0;
            char *buf = calloc(128, sizeof(char));
            buf[buf_i++] = c;
            while (isalnum((c = (char)fgetc(fp)))) {
              buf[buf_i++] = c;
            }
            fseek(fp, -1, SEEK_CUR);
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

  fclose(fp);
  tokens = realloc(tokens, sizeof(struct token) * tokens_len);
  return tokens;
}


struct json_input parse(struct token *tokens) {
  TIME_FUNCTION();

  uint32_t pairs_cap = 1024;
  struct json_input input = {
    .pairs = malloc(sizeof(pair) * pairs_cap),
    .pairs_len = 0,
    .expected = 0,
  };
  
  uint32_t stack[1024] = {0};
  uint32_t sp = 0;

  uint32_t i = 0;
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

int main(int argc, char *argv[]) {
  BEGIN_TIMING();

  if (argc != 2) {
    fprintf(stderr, "Usage: haversine filename\n");
    exit(1);
  }

  char *filename = argv[1];
  FILE *input_file;
  {
    TIME_BLOCK("read");
    input_file = fopen(filename, "r");
  }

  if (input_file == NULL) {
    fprintf(stderr, "Could not open %s for reading\n", filename);
    exit(1);
  }

  struct token *tokens = lex(input_file);
  struct json_input input = parse(tokens);

  double sum = 0;
  {
    TIME_BLOCK("sum");
    for (uint32_t i = 0; i < input.pairs_len; i++) {
      sum += haversine(input.pairs[i][0], input.pairs[i][2], input.pairs[i][1], input.pairs[i][3]);
    }

    double average = (double)sum/input.pairs_len;
    printf("expected = %12.6f\nactual   = %12.6f\n", input.expected, average);
  }

  {
    TIME_BLOCK("cleanup");
    if (input.pairs != NULL) free(input.pairs);
    if (tokens != NULL) {
      uint32_t i = 0;
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

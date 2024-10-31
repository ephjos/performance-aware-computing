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
struct timing_t {
  uint64_t cpu_freq;

  uint64_t start;

  uint64_t startup;
  uint64_t read;
  uint64_t lex;
  uint64_t parse;
  uint64_t sum;
  uint64_t output;
};
struct timing_t timing = {0};

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
  timing.cpu_freq = estimate_cpu_freq(100);
  timing.start = read_cpu_timer();

  LOG(("BEGIN"));
  if (argc != 2) {
    fprintf(stderr, "Usage: haversine filename\n");
    exit(1);
  }
  char *filename = argv[1];
  timing.startup = read_cpu_timer();


  LOG(("Reading %s", filename));
  FILE *input_file = fopen(filename, "r");
  if (input_file == NULL) {
    fprintf(stderr, "Could not open %s for reading\n", filename);
    exit(1);
  }
  timing.read = read_cpu_timer();

  LOG(("Lexing"));
  struct token *tokens = lex(input_file);
  timing.lex = read_cpu_timer();

  LOG(("Parsing tokens"));
  struct json_input input = parse(tokens);
  timing.parse = read_cpu_timer();

  LOG(("Calculating sum of haversines for %d pairs", input.pairs_len));
  double sum = 0;
  for (uint32_t i = 0; i < input.pairs_len; i++) {
    sum += haversine(input.pairs[i][0], input.pairs[i][2], input.pairs[i][1], input.pairs[i][3]);
  }

  LOG(("Getting average from sum %f", sum));
  double average = (double)sum/input.pairs_len;
  timing.sum = read_cpu_timer();

  printf("expected = %12.6f\nactual   = %12.6f\n", input.expected, average);
  timing.output = read_cpu_timer();

  {
    // Write timing info out
    double total = (double)(timing.output - timing.start);
    double freq = (double)timing.cpu_freq;
    uint64_t startup = timing.startup - timing.start;
    uint64_t read = timing.read - timing.startup;
    uint64_t lex = timing.lex - timing.read;
    uint64_t parse = timing.parse - timing.lex;
    uint64_t sum = timing.sum - timing.parse;
    uint64_t output = timing.output - timing.sum;

    printf("Total time: %.4fms (CPU Freq %"PRIu64")\n", (total / freq) * 1000, timing.cpu_freq);
    printf("  Startup:  %"PRIu64" (%.2f%%)\n", startup, ((double)startup / total) * 100);
    printf("  Read:     %"PRIu64" (%.2f%%)\n", read, ((double)read / total) * 100);
    printf("  Lex:      %"PRIu64" (%.2f%%)\n", lex, ((double)lex / total) * 100);
    printf("  Parse:    %"PRIu64" (%.2f%%)\n", parse, ((double)parse / total) * 100);
    printf("  Sum:      %"PRIu64" (%.2f%%)\n", sum, ((double)sum / total) * 100);
    printf("  Output:   %"PRIu64" (%.2f%%)\n", output, ((double)output / total) * 100);
  }

  LOG(("Freeing input.pairs"));
  if (input.pairs != NULL) free(input.pairs);

  LOG(("Freeing tokens"));
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

  LOG(("DONE"));
  return 0;
}

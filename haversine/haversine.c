# include <assert.h>
# include <ctype.h>
# include <inttypes.h>
# include <math.h>
# include <stdio.h>
# include <stdlib.h>
# include <string.h>
# include <time.h>

typedef char unsigned u8;
typedef short unsigned u16;
typedef int unsigned u32;
typedef long long unsigned u64;

typedef char s8;
typedef short s16;
typedef int s32;
typedef long long s64;

typedef float f32;
typedef double f64;

typedef s32 b32;

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
  };
}; 

struct pair {
  f64 x0;
  f64 x1;
  f64 y0;
  f64 y1;
};

struct json_input {
  struct pair *pairs;
  u32 pairs_len;
  f64 expected;
};

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

struct token *lex(char *filename) {
  FILE *fp = fopen(filename, "r");
  if (fp == NULL) {
    fprintf(stderr, "Could not open %s for reading\n", filename);
    exit(1);
  }

  u32 tokens_cap = 1024;
  u32 tokens_len = 0;
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
            u32 buf_i = 0;
            char *buf = calloc(sizeof(char), 128);
            buf[buf_i++] = c;
            while (isdigit((c = (char)fgetc(fp))) || c == '.') {
              buf[buf_i++] = c;
            }
            fseek(fp, -1, SEEK_CUR);
            tokens[tokens_len++] = (struct token) {
              .type = TOKEN_NUMBER,
              .number = atof(buf),
            };
            free(buf);
          } else if (isalnum(c)) {
            u32 buf_i = 0;
            char *buf = calloc(sizeof(char), 128);
            buf[buf_i++] = c;
            while (isalnum((c = (char)fgetc(fp)))) {
              buf[buf_i++] = c;
            }
            fseek(fp, -1, SEEK_CUR);
            tokens[tokens_len++] = (struct token) {
              .type = TOKEN_IDENT,
              .ident = buf,
            };
          } else if (!isspace(c)) {
            tokens[tokens_len++] = (struct token) {
              .type = TOKEN_UNKNOWN,
              .unknown = c,
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
  return tokens;
}

struct json_input parse(struct token *tokens) {
  u32 pairs_cap = 1024;
  struct json_input input = {
    .pairs = malloc(sizeof(struct pair) * pairs_cap),
    .pairs_len = 0,
    .expected = 0,
  };
  
#define STACK_SIZE 1024
  u32 stack[STACK_SIZE] = {0};
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
          if (strcmp(curr.ident, "expected") == 0) {
            assert((curr = tokens[i++]).type == TOKEN_DQUOTE);
            assert(stack[--sp] == TOKEN_DQUOTE);
            assert((curr = tokens[i++]).type == TOKEN_COLON);
            assert((curr = tokens[i++]).type == TOKEN_NUMBER);
            input.expected = curr.number;
          } else {
            assert(strcmp(curr.ident, "pairs") == 0);
          }
        } else if (sp == 4) {
          // TODO: get pair
          struct pair p = {0};

          char *curr_ident = curr.ident;
          assert((curr = tokens[i++]).type == TOKEN_DQUOTE);
          assert(stack[--sp] == TOKEN_DQUOTE);
          assert((curr = tokens[i++]).type == TOKEN_COLON);
          assert((curr = tokens[i++]).type == TOKEN_NUMBER);
          if (strcmp(curr_ident, "x0") == 0) {
            p.x0 = curr.number;
          } else if (strcmp(curr_ident, "x1") == 0) {
            p.x1 = curr.number;
          } else if (strcmp(curr_ident, "y0") == 0) {
            p.y0 = curr.number;
          } else if (strcmp(curr_ident, "y1") == 0) {
            p.y1 = curr.number;
          }
          assert((curr = tokens[i++]).type == TOKEN_COMMA);
          assert((curr = tokens[i++]).type == TOKEN_DQUOTE);
          stack[sp++] = curr.type;
          assert((curr = tokens[i++]).type == TOKEN_IDENT);

          curr_ident = curr.ident;
          assert((curr = tokens[i++]).type == TOKEN_DQUOTE);
          assert(stack[--sp] == TOKEN_DQUOTE);
          assert((curr = tokens[i++]).type == TOKEN_COLON);
          assert((curr = tokens[i++]).type == TOKEN_NUMBER);
          if (strcmp(curr_ident, "x0") == 0) {
            p.x0 = curr.number;
          } else if (strcmp(curr_ident, "x1") == 0) {
            p.x1 = curr.number;
          } else if (strcmp(curr_ident, "y0") == 0) {
            p.y0 = curr.number;
          } else if (strcmp(curr_ident, "y1") == 0) {
            p.y1 = curr.number;
          }
          assert((curr = tokens[i++]).type == TOKEN_COMMA);
          assert((curr = tokens[i++]).type == TOKEN_DQUOTE);
          stack[sp++] = curr.type;
          assert((curr = tokens[i++]).type == TOKEN_IDENT);

          curr_ident = curr.ident;
          assert((curr = tokens[i++]).type == TOKEN_DQUOTE);
          assert(stack[--sp] == TOKEN_DQUOTE);
          assert((curr = tokens[i++]).type == TOKEN_COLON);
          assert((curr = tokens[i++]).type == TOKEN_NUMBER);
          if (strcmp(curr_ident, "x0") == 0) {
            p.x0 = curr.number;
          } else if (strcmp(curr_ident, "x1") == 0) {
            p.x1 = curr.number;
          } else if (strcmp(curr_ident, "y0") == 0) {
            p.y0 = curr.number;
          } else if (strcmp(curr_ident, "y1") == 0) {
            p.y1 = curr.number;
          }
          assert((curr = tokens[i++]).type == TOKEN_COMMA);
          assert((curr = tokens[i++]).type == TOKEN_DQUOTE);
          stack[sp++] = curr.type;
          assert((curr = tokens[i++]).type == TOKEN_IDENT);

          curr_ident = curr.ident;
          assert((curr = tokens[i++]).type == TOKEN_DQUOTE);
          assert(stack[--sp] == TOKEN_DQUOTE);
          assert((curr = tokens[i++]).type == TOKEN_COLON);
          assert((curr = tokens[i++]).type == TOKEN_NUMBER);
          if (strcmp(curr_ident, "x0") == 0) {
            p.x0 = curr.number;
          } else if (strcmp(curr_ident, "x1") == 0) {
            p.x1 = curr.number;
          } else if (strcmp(curr_ident, "y0") == 0) {
            p.y0 = curr.number;
          } else if (strcmp(curr_ident, "y1") == 0) {
            p.y1 = curr.number;
          }

          input.pairs[input.pairs_len++] = p;

          if (input.pairs_len >= pairs_cap) {
            pairs_cap <<= 1;
            input.pairs = realloc(input.pairs, sizeof(struct pair) * pairs_cap);
          }
        }
        break;
      default:
        break;
    }

    curr = tokens[i++];
  }

  assert(sp == 0);

  return input;
}

int main(int argc, char *argv[]) {
  if (argc != 2) {
    fprintf(stderr, "Usage: haversine filename\n");
    exit(1);
  }

  struct token *tokens = lex(argv[1]);
  struct json_input input = parse(tokens);

  f64 sum = 0;
  for (u32 i = 0; i < input.pairs_len; i++) {
    sum += haversine(input.pairs[i].x0, input.pairs[i].y0, input.pairs[i].x1, input.pairs[i].y1);
  }

  f64 average = (f64)sum/input.pairs_len;

  printf("expected = %12.6f\nactual   = %12.6f\n", input.expected, average);
  
  if (input.pairs != NULL) free(input.pairs);
  if (tokens != NULL) {
    u32 i = 0;
    struct token curr = tokens[i++];
    while (curr.type != TOKEN_END) {
      if (curr.type == TOKEN_IDENT) {
        free(curr.ident);
      }
      curr = tokens[i++];
    }
    free(tokens);
  }

  return 0;
}

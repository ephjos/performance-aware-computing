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
};

struct token {
  enum token_type type;
  union {
    char *ident;
    f64 number;
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
    printf("%c\n", c);

    // TODO: get tokens

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
  // TODO: parse tokens

  return (struct json_input){
    .pairs = NULL,
    .pairs_len = 0,
    .expected = 1,
  };
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

  printf("expected = %12.4f\nactual   = %12.4f\n", input.expected, average);
  
  if (input.pairs != NULL) free(input.pairs);
  if (tokens != NULL) free(tokens);

  return 0;
}

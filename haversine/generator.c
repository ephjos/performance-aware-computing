# include <inttypes.h>
# include <math.h>
# include <stdio.h>
# include <stdlib.h>
# include <string.h>

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

enum Mode {
  ModeUniform,
  ModeCluster,
};

#define rand_uniform() ((f64)rand() / RAND_MAX)

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

f64 writeUniformPairs(FILE *fp, u64 pairs) {
  u64 i;
  char sep = ',';

  f64 sum = 0;

  for (i = 0; i < pairs; i++) {
    if (i == pairs - 1) {
      sep = ' ';
    }

    f64 x0 = (360.0*rand_uniform())-180;
    f64 y0 = (180.0*rand_uniform())-90;
    f64 x1 = (360.0*rand_uniform())-180;
    f64 y1 = (180.0*rand_uniform())-90;
    sum += haversine(x0, y0, x1, y1);

    fprintf(fp, "{\"x0\": %f, \"y0\": %f, \"x1\": %f, \"y1\": %f}%c", 
        x0, y0, x1, y1, sep);
  }

  return (f64)sum/(f64)pairs;
}

f64 writeClusterPairs(FILE *fp, u64 pairs) {
  // Define 2 random squares on the globe and pull 1 point from each
  f64 a_size = 30.0*rand_uniform();
  f64 a_x0 = ((360.0-a_size)*rand_uniform())-180;
  f64 a_y0 = ((180.0-a_size)*rand_uniform())-90;

  f64 b_size = 30.0*rand_uniform();
  f64 b_x0 = ((360.0-b_size)*rand_uniform())-180;
  f64 b_y0 = ((180.0-b_size)*rand_uniform())-90;

  u64 i;
  char sep = ',';

  f64 sum = 0;

  for (i = 0; i < pairs; i++) {
    if (i == pairs - 1) {
      sep = ' ';
    }

    f64 x0 = a_x0 + (a_size*rand_uniform());
    f64 y0 = a_y0 + (a_size*rand_uniform());
    f64 x1 = b_x0 + (b_size*rand_uniform());
    f64 y1 = b_y0 + (b_size*rand_uniform());
    sum += haversine(x0, y0, x1, y1);

    fprintf(fp, "{\"x0\": %f, \"y0\": %f, \"x1\": %f, \"y1\": %f}%c", 
        x0, y0, x1, y1, sep);
  }

  return (f64)sum/(f64)pairs;
}

int main(int argc, char *argv[]) {
  if (argc != 4) {
    fprintf(stderr, "Usage: generator cluster/uniform seed pairs\n");
    exit(1);
  }

  u8 mode;

  if (strcmp(argv[1], "cluster") == 0) {
    mode = ModeCluster;
  } else if (strcmp(argv[1], "uniform") == 0) {
    mode = ModeUniform;
  } else {
    fprintf(stderr, "Unknown mode: %s\n", argv[1]);
    exit(2);
  }

  u32 seed = (u32)atoi(argv[2]);
  u64 pairs = atoll(argv[3]);

  srand(seed);

#define OUTPUT_NAME_BUF_SIZE 256
  char output_name_buf[OUTPUT_NAME_BUF_SIZE] = {0};
  snprintf(output_name_buf, OUTPUT_NAME_BUF_SIZE, "haversine_%d_%u_%llu.json", mode, seed, pairs);

  FILE *fp = fopen(output_name_buf, "w");
  fprintf(fp, "{\"pairs\": [");

  f64 average;
  switch (mode) {
    case ModeUniform:
      average = writeUniformPairs(fp, pairs);
      break;
    case ModeCluster:
      average = writeClusterPairs(fp, pairs);
      break;
  }

  fprintf(fp, "], \"expected\": %f}", average);
  fclose(fp);
  return 0;
}

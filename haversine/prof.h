#ifndef __PROF_H__
#define __PROF_H__

#include <ctype.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>
#include <x86intrin.h>

uint64_t prof_get_os_timer_freq() {
  return 1000000;
}

uint64_t prof_read_os_timer() {
  struct timeval value;
  gettimeofday(&value, 0);

  return prof_get_os_timer_freq() * (uint64_t)value.tv_sec + (uint64_t)value.tv_usec;
}

static inline uint64_t prof_read_cpu_timer() {
  return __rdtsc();
}

static inline uint64_t prof_estimate_cpu_freq(uint64_t wait_ms) {
  uint64_t os_freq = prof_get_os_timer_freq();

  uint64_t cpu_start = prof_read_cpu_timer();
  uint64_t os_start = prof_read_os_timer();
  uint64_t os_end = 0;
  uint64_t os_elapsed = 0;
  uint64_t os_wait_time = os_freq * wait_ms / 1000;

  while (os_elapsed < os_wait_time) {
    os_end = prof_read_os_timer();
    os_elapsed = os_end - os_start;
  }

  uint64_t cpu_end = prof_read_cpu_timer();
  uint64_t cpu_elapsed = cpu_end - cpu_start;
  uint64_t cpu_freq = 0;

  if (os_elapsed) {
    cpu_freq = os_freq * cpu_elapsed / os_elapsed;
  }

  return cpu_freq;
}

#define PROF_MAX_CONTEXTS 4096
#define PROF_MAX_CONTEXT_STACK 4096

struct prof_context {
  uint64_t start;
  uint64_t duration;
  uint64_t child_duration;
  uint64_t count;
  uint32_t index;
  uint32_t stack_count;
  const char *label;
};

struct prof_context prof_contexts[PROF_MAX_CONTEXTS] = {0};

struct prof_context_stack {
  struct prof_context items[PROF_MAX_CONTEXT_STACK];
  int32_t sp;
};
struct prof_context_stack prof_context_stack = {
  .sp = -1,
};

void prof_end_time_block(uint32_t *index) {
  uint64_t end = prof_read_cpu_timer();

  /*
  printf("[");
  for (int i = 0; i <= timing_context_stack.sp; i++) {
    struct timing_context c = timing_context_stack.items[i];
    printf(" %s(%d)", c.label, c.index);
  }
  printf("]\n");
  */

  // Pop
  struct prof_context stack_ctx = prof_context_stack.items[prof_context_stack.sp--];
  struct prof_context *list_ctx = &prof_contexts[stack_ctx.index];
  uint64_t duration = end - stack_ctx.start;

  // Without this, we end up counting sub-calls towards the total duration.
  uint8_t is_recursing = (--list_ctx->stack_count) != 0;

  if (!is_recursing) {
    // Copy
    list_ctx->index = stack_ctx.index;
    list_ctx->start = stack_ctx.start;
    list_ctx->label = stack_ctx.label;

    // Update
    list_ctx->duration += duration;
    list_ctx->count++;

    // Increment child duration on next item
    if (prof_context_stack.sp > -1) {
      struct prof_context top = prof_context_stack.items[prof_context_stack.sp];
      prof_contexts[top.index].child_duration += duration;
    }
  }
}

#define PROF_BLOCK(l) \
  uint32_t __index__ __attribute__((cleanup(prof_end_time_block))) = __COUNTER__ + 1; \
  prof_contexts[__index__].stack_count++; \
  prof_context_stack.items[++prof_context_stack.sp] = (struct prof_context){\
    .index = __index__,\
    .start = prof_read_cpu_timer(),\
    .label = l,\
  }

#define PROF_FUNCTION() PROF_BLOCK(__func__)

void prof_end_timing(uint64_t *start) {
  uint64_t end = prof_read_cpu_timer();
  uint64_t duration = end - *start;
  uint64_t cpu_freq = prof_estimate_cpu_freq(100);

  printf("\nTotal: %0.2fms (%"PRIu64" ticks at %"PRIu64"hz)\n", ((double)duration / (double)cpu_freq) * 1000, duration, cpu_freq);

  for (uint32_t i = 1; i < PROF_MAX_CONTEXTS; i++) {
    struct prof_context ctx = prof_contexts[i];
    if (ctx.start == 0) {
      break;
    }

    printf("  %12s: %6.2f%% (%0.2fms %"PRIu64")",
        ctx.label, 
        ((double)(ctx.duration - ctx.child_duration)/ (double)duration) * 100,
        ((double)(ctx.duration - ctx.child_duration)/ (double)cpu_freq) * 1000,
        ctx.duration
      );

    if (ctx.child_duration > 0) {
    printf(" { %0.2f%% (%0.2fms %"PRIu64") }",
        ((double)(ctx.child_duration)/ (double)duration) * 100,
        ((double)(ctx.child_duration)/ (double)cpu_freq) * 1000,
        ctx.child_duration
      );
    }
    printf(" [%"PRIu64"]\n", ctx.count);
  }

}

#define PROF_INIT() \
  uint64_t __start__ __attribute__((cleanup(prof_end_timing))) = prof_read_cpu_timer();

#endif

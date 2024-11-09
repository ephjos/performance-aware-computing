#ifndef __PROF_H__
#define __PROF_H__

#include <ctype.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>
#include <x86intrin.h>

#include "shared.h"

#ifndef PROF_ENABLE
#define PROF_ENABLE 0
#endif

#define PROF_MAX_CONTEXTS 4096
#define PROF_MAX_CONTEXT_STACK 4096

u64 prof_get_os_timer_freq() {
  return 1000000;
}

u64 prof_read_os_timer() {
  struct timeval value;
  gettimeofday(&value, 0);

  return prof_get_os_timer_freq() * (u64)value.tv_sec + (u64)value.tv_usec;
}

static inline u64 prof_read_cpu_timer() {
  return __rdtsc();
}

static inline u64 prof_estimate_cpu_freq(u64 wait_ms) {
  u64 os_freq = prof_get_os_timer_freq();

  u64 cpu_start = prof_read_cpu_timer();
  u64 os_start = prof_read_os_timer();
  u64 os_end = 0;
  u64 os_elapsed = 0;
  u64 os_wait_time = os_freq * wait_ms / 1000;

  while (os_elapsed < os_wait_time) {
    os_end = prof_read_os_timer();
    os_elapsed = os_end - os_start;
  }

  u64 cpu_end = prof_read_cpu_timer();
  u64 cpu_elapsed = cpu_end - cpu_start;
  u64 cpu_freq = 0;

  if (os_elapsed) {
    cpu_freq = os_freq * cpu_elapsed / os_elapsed;
  }

  return cpu_freq;
}

struct prof_context {
  u64 start;
  u64 duration;
  u64 child_duration;
  u64 count;
  u64 bytes;
  u32 index;
  u32 stack_count;
  const char *label;
};

struct prof_context prof_contexts[PROF_MAX_CONTEXTS] = {0};

struct prof_context_stack {
  struct prof_context items[PROF_MAX_CONTEXT_STACK];
  s32 sp;
};
struct prof_context_stack prof_context_stack = {
  .sp = -1,
};

void prof_end_time_block(u32 *index) {
  u64 end = prof_read_cpu_timer();

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
  u64 duration = end - stack_ctx.start;

  // Without this, we end up counting sub-calls towards the total duration.
  uint8_t is_recursing = (--list_ctx->stack_count) != 0;

  if (!is_recursing) {
    // Copy
    list_ctx->index = stack_ctx.index;
    list_ctx->start = stack_ctx.start;
    list_ctx->label = stack_ctx.label;
    list_ctx->bytes = stack_ctx.bytes;

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

void prof_end_timing(u64 *start) {
  u64 end = prof_read_cpu_timer();
  u64 duration = end - *start;
  u64 cpu_freq = prof_estimate_cpu_freq(100);

  printf("\nTotal: %0.2fms (%"PRIu64" ticks at %"PRIu64"hz)\n", ((f64)duration / (f64)cpu_freq) * 1000, duration, cpu_freq);

  for (u32 i = 1; i < PROF_MAX_CONTEXTS; i++) {
    struct prof_context ctx = prof_contexts[i];
    if (ctx.start == 0) {
      break;
    }

    printf("  %12s: %6.2f%% (%0.2fms %"PRIu64")",
        ctx.label, 
        ((f64)(ctx.duration - ctx.child_duration)/ (f64)duration) * 100,
        ((f64)(ctx.duration - ctx.child_duration)/ (f64)cpu_freq) * 1000,
        ctx.duration
        );

    if (ctx.child_duration > 0) {
      printf(" { %0.2f%% (%0.2fms %"PRIu64") }",
          ((f64)(ctx.child_duration)/ (f64)duration) * 100,
          ((f64)(ctx.child_duration)/ (f64)cpu_freq) * 1000,
          ctx.child_duration
          );
    }

    printf(" [%"PRIu64"]", ctx.count);

    if (ctx.bytes > 0) {
      /*
           f64 Megabyte = 1024.0f*1024.0f;
    f64 Gigabyte = Megabyte*1024.0f;
        
    f64 Seconds = (f64)Anchor->TSCElapsedInclusive / (f64)TimerFreq;
    f64 BytesPerSecond = (f64)Anchor->ProcessedByteCount / Seconds;
    f64 Megabytes = (f64)Anchor->ProcessedByteCount / (f64)Megabyte;
    f64 GigabytesPerSecond = BytesPerSecond / Gigabyte;
       
    printf("  %.3fmb at %.2fgb/s", Megabytes, GigabytesPerSecond);
    */
      f64 MB = 1024.0f*1024.0f;
      f64 GB = MB*(f64)1024.0f;

      f64 seconds = (f64)ctx.duration / (f64)cpu_freq;
      f64 bytes_per_second = (f64)ctx.bytes / seconds;
      f64 mb = (f64)ctx.bytes / MB;
      f64 gbs = bytes_per_second / GB;

      printf(" | %.3fmb at %.2fgb/s", mb, gbs);
    }

    printf("\n");
  }

}

#define PROF_INIT() \
  u64 __start__ __attribute__((cleanup(prof_end_timing))) = prof_read_cpu_timer()

#define PROF_CLEANUP() \
  _Static_assert(__COUNTER__ < PROF_MAX_CONTEXTS, "Number of profile points exceeds PROF_MAX_CONTEXTS")

#if PROF_ENABLE

#define PROF_BANDWIDTH(l,b) \
  u32 __index__ __attribute__((cleanup(prof_end_time_block))) = __COUNTER__ + 1; \
  prof_contexts[__index__].stack_count++; \
  prof_context_stack.items[++prof_context_stack.sp] = (struct prof_context){\
    .index = __index__,\
    .start = prof_read_cpu_timer(),\
    .label = l,\
    .bytes = b,\
  }

#define PROF_BLOCK(l) PROF_BANDWIDTH(l, 0)

#define PROF_FUNCTION() PROF_BLOCK(__func__)

#else

#define PROF_BANDWIDTH(...) 
#define PROF_BLOCK(...) 
#define PROF_FUNCTION(...) 

#endif

#endif

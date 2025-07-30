
#include <ev.h>
#include <stdio.h>
#include <time.h>

static struct timespec start;
static double avg_latency = 0.0;
static int iterations = 0;

static void timeout_cb(EV_P_ ev_timer *w, int revents) {
  struct timespec end;
  iterations++;

  clock_gettime(CLOCK_MONOTONIC, &end);
  double elapsed =
    (end.tv_sec - start.tv_sec) * 1e6 + (end.tv_nsec - start.tv_nsec) / 1e3;
  avg_latency += elapsed;
  clock_gettime(CLOCK_MONOTONIC, &start);

  // printf("[Elapsed: %.9f microseconds]\n", elapsed);
  if (iterations % 500 == 0) {
    printf("Iteration %d: Average latency: %.9f microseconds\n", iterations,
           avg_latency / iterations);
    if (iterations == 10000)
      ev_break(EV_A_ EVBREAK_ONE);
  }
}

int main() {
  struct ev_loop *loop = EV_DEFAULT;
  ev_timer timeout_watcher;

  double interval = 0.0003; // 500 microseconds
  ev_timer_init(&timeout_watcher, timeout_cb, interval, interval);
  ev_timer_start(loop, &timeout_watcher);

  clock_gettime(CLOCK_MONOTONIC, &start);
  ev_run(loop, 0);
  printf("Average latency: %.9f microseconds over %d iterations\n",
         avg_latency / iterations, iterations);
  return 0;
}

#if 0

#  include <ev.h>
#  include <stdio.h>
#  include <time.h>

static struct timespec start;

static void timeout_cb(EV_P_ ev_timer *w, int revents) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    double elapsed_clock = (now.tv_sec - start.tv_sec) + (now.tv_nsec - start.tv_nsec) / 1e9;
    double ev_now_time = ev_now(EV_A);

    printf("ev_now: %.9f seconds, clock_gettime: %.9f seconds, difference: %.9f seconds\n",
           ev_now_time, elapsed_clock, ev_now_time - elapsed_clock);

    clock_gettime(CLOCK_MONOTONIC, &start);
}

int main() {
    struct ev_loop *loop = EV_DEFAULT;
    ev_timer timeout_watcher;

    double interval = 1.0;  // 1 second interval

    ev_timer_init(&timeout_watcher, timeout_cb, interval, interval);
    ev_timer_start(loop, &timeout_watcher);

    clock_gettime(CLOCK_MONOTONIC, &start);
    ev_run(loop, 0);

    return 0;
}
#endif
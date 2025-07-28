
#include <ev.h>
#include <stdio.h>
#include <time.h>

static struct timespec start;

static void timeout_cb(EV_P_ ev_timer *w, int revents) {
  struct timespec end;
  clock_gettime(CLOCK_MONOTONIC, &end);
  double elapsed =
    (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
  printf("[Elapsed: %.9f seconds]\n", elapsed);
  // ev_break(EV_A_ EVBREAK_ONE);
  clock_gettime(CLOCK_MONOTONIC, &start);
}

int main() {
  struct ev_loop *loop = EV_DEFAULT;
  ev_timer timeout_watcher;

  double interval = 0.001; // 1 millisecond

  ev_timer_init(&timeout_watcher, timeout_cb, interval, interval);
  ev_timer_start(loop, &timeout_watcher);

  clock_gettime(CLOCK_MONOTONIC, &start);
  ev_run(loop, 0);

  return 0;
}

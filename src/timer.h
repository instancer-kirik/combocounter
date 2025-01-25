#ifndef TIMER_H
#define TIMER_H

#include <stdbool.h>

typedef struct {
    double current_time;
    double best_time;
    bool running;
    bool has_best;
} LapTimer;

void lap_timer_init(LapTimer* timer);
void lap_timer_update(LapTimer* timer, float dt);
void lap_timer_start(LapTimer* timer);
void lap_timer_stop(LapTimer* timer);
void lap_timer_reset(LapTimer* timer);
const char* format_time(int seconds);

#endif // TIMER_H 
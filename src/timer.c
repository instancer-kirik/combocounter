#include "timer.h"
#include <stdio.h>

void lap_timer_init(LapTimer* timer) {
    timer->current_time = 0.0;
    timer->best_time = 0.0;
    timer->running = false;
    timer->has_best = false;
}

void lap_timer_update(LapTimer* timer, float dt) {
    if (timer->running) {
        timer->current_time += dt;
    }
}

void lap_timer_start(LapTimer* timer) {
    timer->running = true;
}

void lap_timer_stop(LapTimer* timer) {
    timer->running = false;
    if (!timer->has_best || timer->current_time < timer->best_time) {
        timer->best_time = timer->current_time;
        timer->has_best = true;
    }
}

void lap_timer_reset(LapTimer* timer) {
    timer->current_time = 0.0;
    timer->running = false;
}

const char* format_time(int seconds) {
    static char buffer[32];
    int minutes = seconds / 60;
    int remaining_seconds = seconds % 60;
    snprintf(buffer, sizeof(buffer), "%02d:%02d", minutes, remaining_seconds);
    return buffer;
} 
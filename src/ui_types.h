#ifndef UI_TYPES_H
#define UI_TYPES_H

#include "core.h"
#include "clay.h"

#define MAX_TRACKERS 8
#define MAX_LABEL_LENGTH 64

typedef struct {
    char label_buffer[MAX_LABEL_LENGTH];
    char reps_buffer[8];
    char duration_buffer[8];
    bool form_visible;
    bool error_visible;
    const char* error_message;
    int selected_tracker;
} IntervalForm;

typedef struct {
    char label_buffer[MAX_LABEL_LENGTH];
    bool form_visible;
    bool error_visible;
    const char* error_message;
} TrackerForm;

typedef struct {
    ComboState trackers[MAX_TRACKERS];
    int tracker_count;
    ComboState* selected_tracker;
    Clay_Color perfect_color;
    Clay_Color good_color;
    Clay_Color miss_color;
    Clay_Color paused_color;
    Clay_Color active_color;
    Clay_Color objective_color;
    Clay_Color completed_color;
    Clay_Color break_color;
    Clay_Color quick_break_color;
    Clay_Color error_color;
    float animation_time;
    BreakMenu break_menu;
    TrackerForm tracker_form;
    IntervalForm interval_form;
    float current_time;
    float best_time;
} ComboUI;

#endif // UI_TYPES_H 
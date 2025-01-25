#ifndef COMBO_COMMON_H
#define COMBO_COMMON_H

#include <string.h>
#include "core.h"
#include "clay.h"
#include "ui_types.h"

#define MAX_TRACKERS 8
#define MAX_LABEL_LENGTH 64
#define MAX_INTERVALS 10

// Helper function for creating Clay strings
static inline Clay_String make_clay_string(const char* str) {
    Clay_String result = {0};
    result.chars = str;
    result.length = strlen(str);
    return result;
}

// Form structures
typedef struct {
    char label_buffer[MAX_LABEL_LENGTH];
    char duration_buffer[16];
    char reps_buffer[8];
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
    uint32_t tracker_count;
    TrackerForm tracker_form;
    IntervalForm interval_form;
    BreakMenu* break_menu;
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
} ComboUI;

// Forward declarations for UI functions
void handle_click(Clay_ElementId elementId, Clay_PointerData pointerData, intptr_t userData);
void handle_tracker_form_input(TrackerForm* form);
void handle_interval_form_input(IntervalForm* form);

#endif // COMBO_COMMON_H 
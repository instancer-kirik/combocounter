#ifndef CORE_H
#define CORE_H

#include <stdbool.h>
#include <stdint.h>

#define MAX_LABEL_LENGTH 64
#define MAX_TRACKERS 8
#define MAX_BREAK_ACTIVITIES 16

typedef struct {
    char label[MAX_LABEL_LENGTH];
    int duration;
    int reps;
} Interval;

typedef struct {
    bool has_interval;
    bool is_running;
    bool interval_active;
    int current_time;
    int current_rep;
    Interval current_interval;
    Interval* intervals;
    int interval_count;
    int current_interval_index;
} IntervalTracker;

typedef struct {
    char name[MAX_LABEL_LENGTH];
    char description[MAX_LABEL_LENGTH];
    int target_score;
    int current_score;
    bool completed;
} Objective;

typedef struct {
    char name[MAX_LABEL_LENGTH];
    char description[MAX_LABEL_LENGTH];
    int duration;
    bool is_quick_break;
} BreakActivity;

typedef struct {
    bool visible;
    bool menu_visible;
    bool break_active;
    int selected_activity;
    float remaining_time;
    BreakActivity activities[MAX_BREAK_ACTIVITIES];
    int activity_count;
} BreakMenu;

typedef struct {
    char label[MAX_LABEL_LENGTH];
    int score;
    int combo;
    int max_combo;
    bool paused;
    bool has_objective;
    int objective;
    int completed_intervals;
    float multiplier;
    float decay_pause;
    uint32_t total_hits;
    uint32_t perfect_hits;
    uint32_t miss_hits;
    Objective* objectives;
    uint32_t objective_count;
    uint32_t active_objective_index;
    IntervalTracker interval_tracker;
} ComboState;

// Core combo functions
void combo_init(ComboState* state, const char* label);
void combo_pause(ComboState* state);
void combo_resume(ComboState* state);
void combo_update(ComboState* state, float dt);
void combo_increment(ComboState* state, uint32_t amount);
void combo_decrement(ComboState* state, uint32_t amount);
void combo_update_objective_progress(ComboState* state, uint32_t score_increment);

// Interval tracker functions
void interval_tracker_init(IntervalTracker* tracker);
void interval_tracker_add(IntervalTracker* tracker, const char* label, int duration, int reps);
void interval_tracker_start(IntervalTracker* tracker);
void interval_tracker_stop(IntervalTracker* tracker);
void interval_tracker_reset(IntervalTracker* tracker);
void interval_tracker_update(IntervalTracker* tracker, float dt);
void interval_tracker_clear(IntervalTracker* tracker);

// Objective functions
void objective_init(Objective* objective, const char* name, const char* description, int target_score);
void objective_update(Objective* objective, int score);
void combo_set_objectives(ComboState* state, Objective* objectives, uint32_t count);
void combo_switch_objective(ComboState* state, uint32_t index);

// Break activity functions
void break_activity_init(BreakActivity* activity, const char* name, const char* description, int duration, bool is_quick_break);
void break_menu_init(BreakMenu* menu);
void break_menu_add_activity(BreakMenu* menu, const char* name, const char* description, int duration, bool is_quick_break);

#endif // CORE_H

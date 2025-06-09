#include "fitness_core.h"
#include <string.h>
#include <stddef.h>

// Magic number for persistent data validation
#define PERSISTENT_DATA_MAGIC 0xF17E5555  // FITNESS

// Helper macros
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

// Static function declarations
static void reset_workout_session(WorkoutSession* workout);
static void reset_exercise_session(ExerciseSession* exercise);
static uint16_t calculate_checksum(const uint8_t* data, size_t length);
static void update_display_timeout(FitnessTracker* tracker);

void fitness_init(FitnessTracker* tracker) {
    if (!tracker) return;
    
    // Clear all memory
    memset(tracker, 0, sizeof(FitnessTracker));
    
    // Initialize power state
    tracker->power.display_timeout = 30;  // 30 seconds
    tracker->power.idle_timeout = 300;    // 5 minutes
    tracker->power.battery_level = 100;
    tracker->power.charging = false;
    
    // Initialize settings
    tracker->auto_start_rest = true;
    tracker->vibrate_enabled = true;
    tracker->rep_detection_sensitivity = 5;
    
    // Initialize rest timer
    tracker->rest_timer.warning_seconds = 10;
    tracker->rest_timer.vibrate_on_complete = true;
    
    // Mark display as dirty for initial render
    tracker->display_dirty = true;
    tracker->current_screen = SCREEN_WORKOUT_SELECT;
    tracker->last_interaction = 0;  // Will be set by system time
    
    // Try to load persistent data
    persistent_data_load(tracker);
}

void fitness_update(FitnessTracker* tracker, float dt) {
    if (!tracker) return;
    
    // Update rest timer
    if (tracker->rest_timer.active) {
        rest_timer_update(&tracker->rest_timer, dt);
    }
    
    // Update workout timing
    if (tracker->current_workout.active && !tracker->current_workout.paused) {
        tracker->current_workout.total_time += (uint32_t)(dt * 1000);  // Convert to ms
    }
    
    // Update power management
    update_display_timeout(tracker);
}

void workout_start(FitnessTracker* tracker, const char* name) {
    if (!tracker || !name) return;
    
    // End current workout if active
    if (tracker->current_workout.active) {
        workout_end(tracker);
    }
    
    reset_workout_session(&tracker->current_workout);
    
    // Copy name with bounds checking
    strncpy(tracker->current_workout.name, name, MAX_WORKOUT_NAME - 1);
    tracker->current_workout.name[MAX_WORKOUT_NAME - 1] = '\0';
    
    tracker->current_workout.active = true;
    tracker->current_workout.start_time = 0;  // Will be set by system
    tracker->current_screen = SCREEN_EXERCISE_LIST;
    tracker->display_dirty = true;
}

void workout_end(FitnessTracker* tracker) {
    if (!tracker || !tracker->current_workout.active) return;
    
    tracker->current_workout.active = false;
    tracker->current_workout.paused = false;
    
    // Update statistics
    tracker->total_workouts++;
    
    // Calculate totals for this workout
    for (uint8_t i = 0; i < tracker->current_workout.total_exercises; i++) {
        ExerciseSession* exercise = &tracker->current_workout.exercises[i];
        for (uint8_t j = 0; j < exercise->total_sets; j++) {
            if (exercise->sets[j].completed) {
                tracker->total_sets++;
                tracker->total_reps += exercise->sets[j].reps_completed;
                tracker->total_volume += (uint32_t)exercise->sets[j].weight_used * exercise->sets[j].reps_completed;
            }
        }
    }
    
    // Stop rest timer
    rest_timer_stop(&tracker->rest_timer);
    
    // Save persistent data
    persistent_data_save(tracker);
    
    tracker->current_screen = SCREEN_STATISTICS;
    tracker->display_dirty = true;
}

void workout_pause(FitnessTracker* tracker) {
    if (!tracker || !tracker->current_workout.active) return;
    
    tracker->current_workout.paused = true;
    rest_timer_stop(&tracker->rest_timer);
    tracker->display_dirty = true;
}

void workout_resume(FitnessTracker* tracker) {
    if (!tracker || !tracker->current_workout.active) return;
    
    tracker->current_workout.paused = false;
    tracker->display_dirty = true;
}

void exercise_add(WorkoutSession* workout, const char* name, ExerciseType type, 
                 uint16_t target_reps, uint16_t target_weight) {
    if (!workout || !name || workout->total_exercises >= MAX_EXERCISES) return;
    
    ExerciseSession* exercise = &workout->exercises[workout->total_exercises];
    reset_exercise_session(exercise);
    
    // Set exercise parameters
    strncpy(exercise->exercise.name, name, MAX_LABEL_LENGTH - 1);
    exercise->exercise.name[MAX_LABEL_LENGTH - 1] = '\0';
    exercise->exercise.type = type;
    exercise->exercise.target_reps = target_reps;
    exercise->exercise.target_weight = target_weight;
    exercise->exercise.rest_time = 90;  // Default 90 seconds rest
    
    workout->total_exercises++;
}

void exercise_start(FitnessTracker* tracker, uint8_t exercise_index) {
    if (!tracker || !tracker->current_workout.active || 
        exercise_index >= tracker->current_workout.total_exercises) return;
    
    tracker->current_workout.current_exercise = exercise_index;
    tracker->current_workout.exercises[exercise_index].active = true;
    tracker->current_screen = SCREEN_ACTIVE_SET;
    tracker->display_dirty = true;
}

void exercise_complete(FitnessTracker* tracker) {
    if (!tracker || !tracker->current_workout.active) return;
    
    uint8_t current_idx = tracker->current_workout.current_exercise;
    ExerciseSession* exercise = &tracker->current_workout.exercises[current_idx];
    
    exercise->active = false;
    tracker->current_screen = SCREEN_EXERCISE_LIST;
    tracker->display_dirty = true;
}

void set_start(FitnessTracker* tracker) {
    if (!tracker || !tracker->current_workout.active) return;
    
    uint8_t exercise_idx = tracker->current_workout.current_exercise;
    ExerciseSession* exercise = &tracker->current_workout.exercises[exercise_idx];
    
    if (exercise->current_set >= MAX_SETS) return;
    
    // Initialize new set
    Set* set = &exercise->sets[exercise->current_set];
    memset(set, 0, sizeof(Set));
    set->weight_used = exercise->exercise.target_weight;
    set->timestamp = 0;  // Will be set by system
    
    tracker->display_dirty = true;
}

void set_add_rep(FitnessTracker* tracker, RepQuality quality) {
    if (!tracker || !tracker->current_workout.active) return;
    
    uint8_t exercise_idx = tracker->current_workout.current_exercise;
    ExerciseSession* exercise = &tracker->current_workout.exercises[exercise_idx];
    Set* current_set = &exercise->sets[exercise->current_set];
    
    // Add rep based on quality
    switch (quality) {
        case REP_QUALITY_PERFECT:
            current_set->perfect_reps++;
            current_set->reps_completed++;
            break;
        case REP_QUALITY_GOOD:
            current_set->good_reps++;
            current_set->reps_completed++;
            break;
        case REP_QUALITY_PARTIAL:
            current_set->partial_reps++;
            current_set->reps_completed++;
            break;
        case REP_QUALITY_FAILED:
            current_set->failed_reps++;
            break;
    }
    
    tracker->display_dirty = true;
}

void set_complete(FitnessTracker* tracker, uint16_t weight) {
    if (!tracker || !tracker->current_workout.active) return;
    
    uint8_t exercise_idx = tracker->current_workout.current_exercise;
    ExerciseSession* exercise = &tracker->current_workout.exercises[exercise_idx];
    Set* current_set = &exercise->sets[exercise->current_set];
    
    current_set->weight_used = weight;
    current_set->completed = true;
    
    // Update exercise totals
    exercise->total_reps += current_set->reps_completed;
    exercise->total_volume += (uint32_t)weight * current_set->reps_completed;
    
    // Move to next set
    exercise->current_set++;
    exercise->total_sets = MAX(exercise->total_sets, exercise->current_set);
    
    // Auto-start rest timer if enabled
    if (tracker->auto_start_rest) {
        rest_timer_start(&tracker->rest_timer, exercise->exercise.rest_time);
        tracker->current_screen = SCREEN_REST_TIMER;
    }
    
    tracker->display_dirty = true;
}

void rest_timer_start(RestTimer* timer, uint16_t duration) {
    if (!timer) return;
    
    timer->active = true;
    timer->duration = duration;
    timer->remaining = duration;
}

void rest_timer_stop(RestTimer* timer) {
    if (!timer) return;
    
    timer->active = false;
    timer->remaining = 0;
}

void rest_timer_update(RestTimer* timer, float dt) {
    if (!timer || !timer->active) return;
    
    if (timer->remaining > 0) {
        uint16_t decrement = (uint16_t)dt;
        timer->remaining = (timer->remaining > decrement) ? 
                          (timer->remaining - decrement) : 0;
        
        // Timer finished
        if (timer->remaining == 0) {
            timer->active = false;
            // TODO: Trigger vibration/notification
        }
    }
}

uint16_t get_session_volume(const WorkoutSession* workout) {
    if (!workout) return 0;
    
    uint32_t total = 0;
    for (uint8_t i = 0; i < workout->total_exercises; i++) {
        total += get_exercise_volume(&workout->exercises[i]);
    }
    
    return (uint16_t)MIN(total, UINT16_MAX);
}

uint16_t get_exercise_volume(const ExerciseSession* exercise) {
    if (!exercise) return 0;
    
    return (uint16_t)MIN(exercise->total_volume, UINT16_MAX);
}

float get_average_rest_time(const ExerciseSession* exercise) {
    if (!exercise || exercise->total_sets == 0) return 0.0f;
    
    uint32_t total_rest = 0;
    uint8_t valid_sets = 0;
    
    for (uint8_t i = 0; i < exercise->total_sets; i++) {
        if (exercise->sets[i].completed && exercise->sets[i].rest_taken > 0) {
            total_rest += exercise->sets[i].rest_taken;
            valid_sets++;
        }
    }
    
    return valid_sets > 0 ? (float)total_rest / valid_sets : 0.0f;
}

void power_enter_low_power(FitnessTracker* tracker) {
    if (!tracker) return;
    
    tracker->power.low_power_mode = true;
    tracker->current_screen = SCREEN_SLEEP;
    tracker->display_dirty = true;
}

void power_wake_up(FitnessTracker* tracker) {
    if (!tracker) return;
    
    tracker->power.low_power_mode = false;
    tracker->current_screen = tracker->current_workout.active ? 
                             SCREEN_ACTIVE_SET : SCREEN_WORKOUT_SELECT;
    tracker->display_dirty = true;
}

void power_update_battery(FitnessTracker* tracker, uint8_t level, bool charging) {
    if (!tracker) return;
    
    tracker->power.battery_level = MIN(level, 100);
    tracker->power.charging = charging;
    tracker->display_dirty = true;
}

bool display_needs_refresh(const FitnessTracker* tracker) {
    return tracker ? tracker->display_dirty : false;
}

void display_mark_clean(FitnessTracker* tracker) {
    if (tracker) {
        tracker->display_dirty = false;
    }
}

void display_mark_dirty(FitnessTracker* tracker) {
    if (tracker) {
        tracker->display_dirty = true;
    }
}

bool persistent_data_save(const FitnessTracker* tracker) {
    if (!tracker) return false;
    
    PersistentData data = {
        .magic = PERSISTENT_DATA_MAGIC,
        .version = 1,
        .total_workouts = tracker->total_workouts,
        .total_sets = tracker->total_sets,
        .total_reps = tracker->total_reps,
        .total_volume = tracker->total_volume,
        .settings_flags = (tracker->auto_start_rest ? 0x01 : 0) |
                         (tracker->vibrate_enabled ? 0x02 : 0),
        .sensitivity = tracker->rep_detection_sensitivity,
        .checksum = 0
    };
    
    // Calculate checksum
    data.checksum = calculate_checksum((const uint8_t*)&data, 
                                      sizeof(data) - sizeof(data.checksum));
    
    // TODO: Write to Nordic flash storage
    // For now, return true (would implement with Nordic SDK)
    return true;
}

bool persistent_data_load(FitnessTracker* tracker) {
    if (!tracker) return false;
    
    // TODO: Read from Nordic flash storage
    // For now, return false (no data to load)
    return false;
}

// Static helper functions
static void reset_workout_session(WorkoutSession* workout) {
    if (!workout) return;
    
    memset(workout, 0, sizeof(WorkoutSession));
    workout->current_exercise = 0;
    workout->total_exercises = 0;
}

static void reset_exercise_session(ExerciseSession* exercise) {
    if (!exercise) return;
    
    memset(exercise, 0, sizeof(ExerciseSession));
    exercise->current_set = 0;
    exercise->total_sets = 0;
}

static uint16_t calculate_checksum(const uint8_t* data, size_t length) {
    if (!data) return 0;
    
    uint16_t checksum = 0;
    for (size_t i = 0; i < length; i++) {
        checksum ^= data[i];
        checksum = (checksum << 1) | (checksum >> 15);  // Rotate left
    }
    return checksum;
}

static void update_display_timeout(FitnessTracker* tracker) {
    if (!tracker) return;
    
    // TODO: Implement timeout logic with system time
    // This would check if display should enter sleep mode
}
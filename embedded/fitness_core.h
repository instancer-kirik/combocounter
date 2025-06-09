#ifndef FITNESS_CORE_H
#define FITNESS_CORE_H

#include <stdint.h>
#include <stdbool.h>

// Memory constraints for nRF52840
#define MAX_LABEL_LENGTH 32
#define MAX_EXERCISES 8
#define MAX_SETS 16
#define MAX_WORKOUT_NAME 32

// E-Paper display constraints (296x160, 4-color)
#define DISPLAY_WIDTH 296
#define DISPLAY_HEIGHT 160
#define DISPLAY_COLORS 4  // Black, White, Red, Yellow

// Fitness-specific enums
typedef enum {
    REP_QUALITY_PERFECT = 3,    // Full ROM, good form
    REP_QUALITY_GOOD = 2,       // Minor form issues
    REP_QUALITY_PARTIAL = 1,    // Partial ROM
    REP_QUALITY_FAILED = 0      // Failed rep
} RepQuality;

typedef enum {
    EXERCISE_COMPOUND,          // Squats, deadlifts, bench
    EXERCISE_ISOLATION,         // Curls, extensions
    EXERCISE_CARDIO,           // For tracking intervals
    EXERCISE_ISOMETRIC         // Planks, holds
} ExerciseType;

typedef struct {
    char name[MAX_LABEL_LENGTH];
    ExerciseType type;
    uint16_t target_reps;
    uint16_t target_weight;     // In 0.1kg units (2550 = 255.0kg)
    uint16_t rest_time;         // In seconds
} Exercise;

typedef struct {
    uint16_t reps_completed;
    uint16_t weight_used;       // In 0.1kg units
    uint16_t rest_taken;        // Actual rest in seconds
    uint8_t perfect_reps;
    uint8_t good_reps;
    uint8_t partial_reps;
    uint8_t failed_reps;
    bool completed;
    uint32_t timestamp;         // Unix timestamp
} Set;

typedef struct {
    Exercise exercise;
    Set sets[MAX_SETS];
    uint8_t current_set;
    uint8_t total_sets;
    uint16_t total_reps;
    uint16_t total_volume;      // Weight × reps
    bool active;
} ExerciseSession;

typedef struct {
    char name[MAX_WORKOUT_NAME];
    ExerciseSession exercises[MAX_EXERCISES];
    uint8_t current_exercise;
    uint8_t total_exercises;
    uint32_t start_time;
    uint32_t total_time;
    bool active;
    bool paused;
} WorkoutSession;

// Rest timer state
typedef struct {
    bool active;
    uint16_t duration;          // Target rest time
    uint16_t remaining;         // Time remaining
    bool vibrate_on_complete;
    uint8_t warning_seconds;    // When to show warning (default 10s)
} RestTimer;

// Power management
typedef struct {
    bool low_power_mode;
    uint16_t display_timeout;   // Seconds until display sleeps
    uint16_t idle_timeout;      // Seconds until deep sleep
    uint8_t battery_level;      // 0-100%
    bool charging;
} PowerState;

// Main fitness tracker state
typedef struct {
    WorkoutSession current_workout;
    RestTimer rest_timer;
    PowerState power;
    
    // Display state
    bool display_dirty;         // Needs refresh
    uint8_t current_screen;     // Which screen to show
    uint32_t last_interaction;  // For auto-sleep
    
    // Statistics
    uint32_t total_workouts;
    uint32_t total_sets;
    uint32_t total_reps;
    uint32_t total_volume;      // Lifetime volume in 0.1kg
    
    // Settings
    bool auto_start_rest;       // Start rest timer after set
    bool vibrate_enabled;
    uint8_t rep_detection_sensitivity;  // 1-10
    
} FitnessTracker;

// Core functions
void fitness_init(FitnessTracker* tracker);
void fitness_update(FitnessTracker* tracker, float dt);

// Workout management
void workout_start(FitnessTracker* tracker, const char* name);
void workout_end(FitnessTracker* tracker);
void workout_pause(FitnessTracker* tracker);
void workout_resume(FitnessTracker* tracker);

// Exercise management
void exercise_add(WorkoutSession* workout, const char* name, ExerciseType type, 
                 uint16_t target_reps, uint16_t target_weight);
void exercise_start(FitnessTracker* tracker, uint8_t exercise_index);
void exercise_complete(FitnessTracker* tracker);

// Set management
void set_start(FitnessTracker* tracker);
void set_add_rep(FitnessTracker* tracker, RepQuality quality);
void set_complete(FitnessTracker* tracker, uint16_t weight);

// Rest timer
void rest_timer_start(RestTimer* timer, uint16_t duration);
void rest_timer_stop(RestTimer* timer);
void rest_timer_update(RestTimer* timer, float dt);

// Statistics
uint16_t get_session_volume(const WorkoutSession* workout);
uint16_t get_exercise_volume(const ExerciseSession* exercise);
float get_average_rest_time(const ExerciseSession* exercise);

// Power management
void power_enter_low_power(FitnessTracker* tracker);
void power_wake_up(FitnessTracker* tracker);
void power_update_battery(FitnessTracker* tracker, uint8_t level, bool charging);

// Display helpers
bool display_needs_refresh(const FitnessTracker* tracker);
void display_mark_clean(FitnessTracker* tracker);
void display_mark_dirty(FitnessTracker* tracker);

// Data persistence (for Nordic's flash storage)
typedef struct __attribute__((packed)) {
    uint32_t magic;             // Validation magic number
    uint32_t version;           // Data structure version
    uint32_t total_workouts;
    uint32_t total_sets;
    uint32_t total_reps;
    uint32_t total_volume;
    uint8_t settings_flags;     // Packed settings
    uint8_t sensitivity;
    uint16_t checksum;
} PersistentData;

bool persistent_data_save(const FitnessTracker* tracker);
bool persistent_data_load(FitnessTracker* tracker);

// Bluetooth data structures (for companion app sync)
typedef struct __attribute__((packed)) {
    uint8_t message_type;       // 0=workout, 1=set, 2=stats
    uint32_t timestamp;
    uint8_t data_length;
    uint8_t data[];            // Variable length payload
} BleMessage;

// Screen definitions for navigation
typedef enum {
    SCREEN_WORKOUT_SELECT,
    SCREEN_EXERCISE_LIST,
    SCREEN_ACTIVE_SET,
    SCREEN_REST_TIMER,
    SCREEN_STATISTICS,
    SCREEN_SETTINGS,
    SCREEN_SLEEP
} ScreenType;

#endif // FITNESS_CORE_H
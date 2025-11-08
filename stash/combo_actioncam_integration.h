#ifndef COMBO_ACTIONCAM_INTEGRATION_H
#define COMBO_ACTIONCAM_INTEGRATION_H

#include <stdint.h>
#include <stdbool.h>
#include "simple_combo_core.h"
#include "action_cam.h"
#include "app_timer.h"

// Integration modes
typedef enum {
    COMBO_CAM_MODE_DISABLED = 0,        // No camera integration
    COMBO_CAM_MODE_MANUAL,              // User controls camera manually
    COMBO_CAM_MODE_AUTO_WORKOUT,        // Auto record during workouts
    COMBO_CAM_MODE_COMBO_TRIGGER,       // Record on combo milestones
    COMBO_CAM_MODE_FORM_CHECK,          // Take photos for form analysis
    COMBO_CAM_MODE_TIMELAPSE            // Workout timelapse
} combo_cam_mode_t;

// Trigger conditions
typedef struct {
    bool trigger_on_workout_start;      // Start recording when first counter used
    bool trigger_on_workout_end;        // Stop recording on inactivity
    uint16_t workout_timeout_sec;       // Inactivity timeout (default 60s)
    
    bool trigger_on_combo_milestone;    // Record on combo achievements
    uint32_t combo_milestone_values[5]; // e.g., {10, 25, 50, 100, 250}
    uint8_t milestone_count;
    
    bool trigger_on_pr;                 // Record on personal records
    bool trigger_on_perfect_streak;     // Record on perfect quality streaks
    uint16_t perfect_streak_threshold;
    
    bool auto_photo_on_set_complete;    // Take photo after each set
    bool form_check_every_n_reps;       // Photo every N reps
    uint16_t form_check_interval;
} combo_cam_triggers_t;

// Recording session data
typedef struct {
    uint32_t session_start_time;
    uint32_t session_end_time;
    char session_filename[32];
    uint16_t total_reps_recorded;
    uint16_t total_sets_recorded;
    uint32_t max_combo_achieved;
    float average_quality;
    bool session_active;
} combo_cam_session_t;

// Workout metadata to embed in video files
typedef struct {
    char workout_name[32];
    char exercise_name[32];
    uint32_t timestamp;
    uint16_t total_reps;
    uint16_t total_sets;
    uint32_t max_combo;
    uint32_t total_volume;              // For weight tracking
    float accuracy_percentage;
    uint16_t workout_duration_sec;
    counter_type_t primary_counter_type;
} workout_metadata_t;

// Main integration structure
typedef struct {
    action_cam_t* camera;
    ComboDevice* combo_device;
    
    combo_cam_mode_t mode;
    combo_cam_triggers_t triggers;
    combo_cam_session_t current_session;
    
    // State tracking
    bool workout_in_progress;
    uint32_t last_activity_time;
    uint32_t session_rep_count;
    uint32_t session_set_count;
    uint32_t last_combo_milestone;
    
    // Timers
    app_timer_id_t workout_timeout_timer;
    app_timer_id_t form_check_timer;
    
    // Statistics
    uint32_t total_workout_sessions;
    uint32_t total_videos_recorded;
    uint32_t total_photos_taken;
    uint32_t total_workout_time_sec;
} combo_actioncam_t;

// Core integration functions
ret_code_t combo_actioncam_init(combo_actioncam_t* integration, 
                               action_cam_t* camera, 
                               ComboDevice* combo_device);
ret_code_t combo_actioncam_deinit(combo_actioncam_t* integration);
ret_code_t combo_actioncam_set_mode(combo_actioncam_t* integration, combo_cam_mode_t mode);

// Trigger configuration
ret_code_t combo_actioncam_configure_triggers(combo_actioncam_t* integration, 
                                             const combo_cam_triggers_t* triggers);
ret_code_t combo_actioncam_add_combo_milestone(combo_actioncam_t* integration, uint32_t milestone);
ret_code_t combo_actioncam_enable_auto_workout(combo_actioncam_t* integration, bool enabled);
ret_code_t combo_actioncam_set_workout_timeout(combo_actioncam_t* integration, uint16_t timeout_sec);

// Session management
ret_code_t combo_actioncam_start_workout_session(combo_actioncam_t* integration, const char* workout_name);
ret_code_t combo_actioncam_end_workout_session(combo_actioncam_t* integration);
ret_code_t combo_actioncam_pause_session(combo_actioncam_t* integration);
ret_code_t combo_actioncam_resume_session(combo_actioncam_t* integration);

// Manual controls
ret_code_t combo_actioncam_manual_start_recording(combo_actioncam_t* integration);
ret_code_t combo_actioncam_manual_stop_recording(combo_actioncam_t* integration);
ret_code_t combo_actioncam_manual_take_photo(combo_actioncam_t* integration);

// Event handlers (called by ComboCounter core)
void combo_actioncam_on_counter_increment(combo_actioncam_t* integration, 
                                         uint8_t counter_id, 
                                         uint32_t new_count, 
                                         uint32_t combo_count,
                                         ActionQuality quality);
void combo_actioncam_on_counter_decrement(combo_actioncam_t* integration, uint8_t counter_id);
void combo_actioncam_on_combo_achieved(combo_actioncam_t* integration, 
                                      uint8_t counter_id, 
                                      uint32_t combo_count);
void combo_actioncam_on_personal_record(combo_actioncam_t* integration, 
                                       uint8_t counter_id, 
                                       uint32_t new_record);
void combo_actioncam_on_perfect_streak(combo_actioncam_t* integration, 
                                      uint8_t counter_id, 
                                      uint32_t streak_count);

// Automatic workout detection
ret_code_t combo_actioncam_detect_workout_start(combo_actioncam_t* integration);
ret_code_t combo_actioncam_detect_workout_end(combo_actioncam_t* integration);
bool combo_actioncam_is_workout_active(combo_actioncam_t* integration);

// Form checking and analysis
ret_code_t combo_actioncam_schedule_form_check(combo_actioncam_t* integration, uint16_t delay_sec);
ret_code_t combo_actioncam_take_form_photo(combo_actioncam_t* integration, const char* exercise_name);
ret_code_t combo_actioncam_create_form_comparison(combo_actioncam_t* integration, 
                                                 const char* reference_photo,
                                                 const char* current_photo);

// Metadata and file management
ret_code_t combo_actioncam_embed_workout_metadata(combo_actioncam_t* integration, 
                                                 const char* filename,
                                                 const workout_metadata_t* metadata);
ret_code_t combo_actioncam_generate_workout_summary(combo_actioncam_t* integration,
                                                   workout_metadata_t* summary);
ret_code_t combo_actioncam_export_session_data(combo_actioncam_t* integration,
                                              const char* export_filename);

// Timelapse functionality
ret_code_t combo_actioncam_start_timelapse(combo_actioncam_t* integration, uint16_t interval_sec);
ret_code_t combo_actioncam_stop_timelapse(combo_actioncam_t* integration);
ret_code_t combo_actioncam_set_timelapse_interval(combo_actioncam_t* integration, uint16_t interval_sec);

// Status and statistics
ret_code_t combo_actioncam_get_session_stats(combo_actioncam_t* integration, 
                                            combo_cam_session_t* session);
uint32_t combo_actioncam_get_total_sessions(combo_actioncam_t* integration);
uint32_t combo_actioncam_get_total_recording_time(combo_actioncam_t* integration);
bool combo_actioncam_is_recording(combo_actioncam_t* integration);

// Configuration presets
ret_code_t combo_actioncam_load_powerlifting_preset(combo_actioncam_t* integration);
ret_code_t combo_actioncam_load_calisthenics_preset(combo_actioncam_t* integration);
ret_code_t combo_actioncam_load_cardio_preset(combo_actioncam_t* integration);
ret_code_t combo_actioncam_load_yoga_preset(combo_actioncam_t* integration);

// Utility functions
const char* combo_actioncam_get_mode_string(combo_cam_mode_t mode);
ret_code_t combo_actioncam_generate_filename(combo_actioncam_t* integration, 
                                           const char* prefix,
                                           char* filename_buffer, 
                                           size_t buffer_size);
uint32_t combo_actioncam_calculate_estimated_file_size(combo_actioncam_t* integration,
                                                      uint16_t duration_sec,
                                                      cam_quality_t quality);

// Timer callbacks (internal)
void combo_actioncam_workout_timeout_handler(void* p_context);
void combo_actioncam_form_check_timer_handler(void* p_context);

// Default trigger configurations
#define COMBO_CAM_POWERLIFTING_TRIGGERS {               \
    .trigger_on_workout_start = true,                   \
    .trigger_on_workout_end = true,                     \
    .workout_timeout_sec = 180,                         \
    .trigger_on_combo_milestone = true,                 \
    .combo_milestone_values = {5, 10, 15, 20, 25},     \
    .milestone_count = 5,                               \
    .trigger_on_pr = true,                              \
    .trigger_on_perfect_streak = false,                 \
    .perfect_streak_threshold = 0,                      \
    .auto_photo_on_set_complete = true,                 \
    .form_check_every_n_reps = true,                    \
    .form_check_interval = 5                            \
}

#define COMBO_CAM_CALISTHENICS_TRIGGERS {               \
    .trigger_on_workout_start = true,                   \
    .trigger_on_workout_end = true,                     \
    .workout_timeout_sec = 120,                         \
    .trigger_on_combo_milestone = true,                 \
    .combo_milestone_values = {10, 25, 50, 75, 100},   \
    .milestone_count = 5,                               \
    .trigger_on_pr = true,                              \
    .trigger_on_perfect_streak = true,                  \
    .perfect_streak_threshold = 10,                     \
    .auto_photo_on_set_complete = false,                \
    .form_check_every_n_reps = true,                    \
    .form_check_interval = 10                           \
}

#define COMBO_CAM_CARDIO_TRIGGERS {                     \
    .trigger_on_workout_start = true,                   \
    .trigger_on_workout_end = true,                     \
    .workout_timeout_sec = 60,                          \
    .trigger_on_combo_milestone = false,                \
    .milestone_count = 0,                               \
    .trigger_on_pr = false,                             \
    .trigger_on_perfect_streak = false,                 \
    .perfect_streak_threshold = 0,                      \
    .auto_photo_on_set_complete = false,                \
    .form_check_every_n_reps = false,                   \
    .form_check_interval = 0                            \
}

// Error codes
#define COMBO_CAM_ERROR_BASE           0x8100
#define COMBO_CAM_ERROR_NOT_INIT       (COMBO_CAM_ERROR_BASE + 1)
#define COMBO_CAM_ERROR_INVALID_MODE   (COMBO_CAM_ERROR_BASE + 2)
#define COMBO_CAM_ERROR_NO_SESSION     (COMBO_CAM_ERROR_BASE + 3)
#define COMBO_CAM_ERROR_CAMERA_BUSY    (COMBO_CAM_ERROR_BASE + 4)
#define COMBO_CAM_ERROR_NO_STORAGE     (COMBO_CAM_ERROR_BASE + 5)

#endif // COMBO_ACTIONCAM_INTEGRATION_H
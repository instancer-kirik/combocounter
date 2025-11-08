#ifndef AUDIO_ACTION_RECORDER_H
#define AUDIO_ACTION_RECORDER_H

#include <stdint.h>
#include <stdbool.h>
#include "nrf_gpio.h"
#include "app_timer.h"
#include "nrf_drv_pdm.h"
#include "nrf_drv_spi.h"

// Audio system configuration
#define AUDIO_SAMPLE_RATE           16000   // 16kHz for voice/movement
#define AUDIO_BUFFER_SIZE           1024    // Sample buffer
#define AUDIO_ANALYSIS_WINDOW       256     // FFT analysis window
#define MAX_MEMO_DURATION_SEC       30      // 30 second memo limit
#define MAX_MEMOS_STORED            50      // Total memo capacity
#define AUDIO_THRESHOLD_SILENCE     100     // Silence detection threshold
#define AUDIO_THRESHOLD_MOVEMENT    500     // Movement detection threshold

// VS1053 MusicMaker pins (existing)
#define VS1053_CS_PIN               6
#define VS1053_DCS_PIN              10
#define VS1053_DREQ_PIN             9
#define VS1053_RESET_PIN            8

// PDM microphone configuration
#define PDM_CLK_PIN                 18
#define PDM_DATA_PIN                19
#define PDM_POWER_PIN               20

// Audio recording modes
typedef enum {
    AUDIO_MODE_OFF = 0,             // System off for ultra-low power
    AUDIO_MODE_LISTEN,              // Background listening (movement detection)
    AUDIO_MODE_MEMO_RECORDING,      // Recording voice memo
    AUDIO_MODE_WORKOUT_ANALYSIS,    // Real-time movement analysis
    AUDIO_MODE_PLAYBACK,            // Playing back memos
    AUDIO_MODE_PROCESSING          // Post-processing recordings
} audio_mode_t;

// Movement analysis results
typedef struct {
    uint32_t timestamp;
    uint16_t movement_intensity;    // 0-1000 scale
    uint16_t movement_frequency;    // Dominant frequency in Hz
    uint16_t movement_duration_ms;  // How long the movement lasted
    uint8_t movement_quality;       // 0-10 quality score based on consistency
    uint8_t tempo_regularity;       // 0-10 rhythm consistency score
    bool is_rep_detected;           // True if this looks like a exercise rep
    float audio_signature[8];       // Compressed frequency signature
} movement_analysis_t;

// Voice memo structure
typedef struct {
    uint32_t memo_id;
    uint32_t timestamp;
    uint16_t duration_sec;
    uint32_t file_size_bytes;
    char filename[32];
    char transcript_preview[64];    // First few words if speech-to-text enabled
    bool is_protected;              // Prevent auto-deletion
    bool is_workout_related;        // Tagged as workout memo
    uint8_t audio_quality;          // Recording quality 0-10
} voice_memo_t;

// Workout audio session
typedef struct {
    uint32_t session_id;
    uint32_t start_timestamp;
    uint32_t end_timestamp;
    uint16_t total_reps_detected;
    uint16_t total_sets_detected;
    float average_tempo;
    float movement_consistency_score;
    uint16_t peak_intensity;
    uint8_t workout_type_detected;  // 0=unknown, 1=cardio, 2=strength, etc
    char session_notes[128];        // Optional voice notes about the session
    movement_analysis_t movements[200]; // Up to 200 movements per session
} workout_audio_session_t;

// Audio configuration
typedef struct {
    // Recording settings
    uint16_t sample_rate;
    uint8_t bit_depth;              // 8 or 16 bit
    uint8_t compression_level;      // 0-9 (0=none, 9=max)
    bool auto_gain_control;         // Automatic volume adjustment
    uint8_t microphone_gain;        // Manual gain 0-100%
    
    // Movement detection settings  
    bool movement_detection_enabled;
    uint16_t movement_threshold;    // Sensitivity 0-1000
    uint16_t movement_timeout_ms;   // How long to track after movement stops
    bool auto_rep_counting;         // Automatic rep detection
    
    // Memo recording settings
    bool voice_activation;          // Start recording on voice detection
    uint16_t silence_timeout_sec;   // Stop recording after silence
    bool memo_timestamps;           // Add timestamps to memos
    bool quick_memo_mode;           // One-button memo recording
    
    // Analysis settings
    bool real_time_analysis;        // Process audio in real-time
    bool form_feedback_audio;       // Audio cues for form consistency
    uint8_t feedback_volume;        // 0-100% volume for audio feedback
    
    // Power management
    bool ultra_low_power_mode;      // Minimize power consumption
    uint16_t sleep_timeout_sec;     // Auto-sleep timeout
    bool wake_on_sound;            // Wake from sleep on audio activity
} audio_config_t;

// Main audio system structure
typedef struct {
    // Hardware interfaces
    nrf_drv_pdm_t pdm_instance;
    nrf_drv_spi_t spi_instance;
    
    // State
    audio_mode_t current_mode;
    audio_config_t config;
    bool is_initialized;
    bool is_recording;
    bool is_analyzing;
    
    // Audio buffers
    int16_t audio_buffer[AUDIO_BUFFER_SIZE];
    int16_t analysis_buffer[AUDIO_ANALYSIS_WINDOW];
    uint16_t buffer_index;
    
    // Movement analysis
    movement_analysis_t current_movement;
    uint32_t last_movement_time;
    uint16_t consecutive_reps;
    float baseline_noise_level;
    
    // Voice memo management
    voice_memo_t memos[MAX_MEMOS_STORED];
    uint16_t memo_count;
    uint16_t current_memo_id;
    bool memo_recording_active;
    uint32_t memo_start_time;
    
    // Workout session tracking
    workout_audio_session_t current_session;
    bool session_active;
    
    // Timers
    app_timer_id_t analysis_timer;
    app_timer_id_t memo_timeout_timer;
    app_timer_id_t sleep_timer;
    
    // Statistics
    uint32_t total_recording_time_sec;
    uint32_t total_memos_recorded;
    uint32_t total_movements_detected;
    uint32_t total_workouts_analyzed;
} audio_action_recorder_t;

// Core system functions
ret_code_t audio_recorder_init(audio_action_recorder_t* recorder);
ret_code_t audio_recorder_deinit(audio_action_recorder_t* recorder);
ret_code_t audio_recorder_set_mode(audio_action_recorder_t* recorder, audio_mode_t mode);
ret_code_t audio_recorder_configure(audio_action_recorder_t* recorder, const audio_config_t* config);

// Movement analysis functions
ret_code_t audio_start_movement_analysis(audio_action_recorder_t* recorder);
ret_code_t audio_stop_movement_analysis(audio_action_recorder_t* recorder);
movement_analysis_t* audio_get_last_movement(audio_action_recorder_t* recorder);
uint16_t audio_get_rep_count(audio_action_recorder_t* recorder);
float audio_get_movement_consistency(audio_action_recorder_t* recorder);

// Voice memo functions
ret_code_t audio_start_memo_recording(audio_action_recorder_t* recorder);
ret_code_t audio_stop_memo_recording(audio_action_recorder_t* recorder);
ret_code_t audio_play_memo(audio_action_recorder_t* recorder, uint16_t memo_id);
ret_code_t audio_delete_memo(audio_action_recorder_t* recorder, uint16_t memo_id);
ret_code_t audio_protect_memo(audio_action_recorder_t* recorder, uint16_t memo_id, bool protect);
voice_memo_t* audio_get_memo_info(audio_action_recorder_t* recorder, uint16_t memo_id);
uint16_t audio_get_memo_count(audio_action_recorder_t* recorder);

// Quick memo functions (one-button operation)
ret_code_t audio_quick_memo_start(audio_action_recorder_t* recorder);
ret_code_t audio_quick_memo_stop(audio_action_recorder_t* recorder);
ret_code_t audio_quick_memo_playback_last(audio_action_recorder_t* recorder);

// Workout session functions
ret_code_t audio_start_workout_session(audio_action_recorder_t* recorder, const char* workout_name);
ret_code_t audio_end_workout_session(audio_action_recorder_t* recorder);
ret_code_t audio_add_workout_note(audio_action_recorder_t* recorder, const char* note);
workout_audio_session_t* audio_get_current_session(audio_action_recorder_t* recorder);
ret_code_t audio_export_session_data(audio_action_recorder_t* recorder, const char* filename);

// Real-time analysis functions
bool audio_is_movement_detected(audio_action_recorder_t* recorder);
uint16_t audio_get_current_intensity(audio_action_recorder_t* recorder);
float audio_get_tempo_estimate(audio_action_recorder_t* recorder);
uint8_t audio_get_form_quality_score(audio_action_recorder_t* recorder);

// Audio feedback functions
ret_code_t audio_play_rep_count_feedback(audio_action_recorder_t* recorder, uint16_t rep_count);
ret_code_t audio_play_form_feedback(audio_action_recorder_t* recorder, uint8_t quality_score);
ret_code_t audio_play_combo_milestone_sound(audio_action_recorder_t* recorder, uint32_t combo);
ret_code_t audio_play_custom_sound(audio_action_recorder_t* recorder, const char* sound_name);

// Integration with ComboCounter
ret_code_t audio_sync_with_combo_counter(audio_action_recorder_t* recorder, 
                                        uint8_t counter_id, 
                                        uint32_t count, 
                                        uint32_t combo);
ret_code_t audio_validate_rep_with_counter(audio_action_recorder_t* recorder, bool rep_confirmed);
ret_code_t audio_tag_memo_with_workout(audio_action_recorder_t* recorder, 
                                      uint16_t memo_id, 
                                      const char* exercise_name);

// Power management
ret_code_t audio_enter_low_power_mode(audio_action_recorder_t* recorder);
ret_code_t audio_exit_low_power_mode(audio_action_recorder_t* recorder);
ret_code_t audio_set_wake_on_sound(audio_action_recorder_t* recorder, bool enabled);
uint32_t audio_get_estimated_battery_life_hours(audio_action_recorder_t* recorder);

// File management
ret_code_t audio_cleanup_old_memos(audio_action_recorder_t* recorder, uint32_t days_old);
ret_code_t audio_export_all_memos(audio_action_recorder_t* recorder, const char* export_path);
uint32_t audio_get_storage_used_kb(audio_action_recorder_t* recorder);
uint32_t audio_get_storage_free_kb(audio_action_recorder_t* recorder);

// Configuration presets
ret_code_t audio_load_strength_training_preset(audio_action_recorder_t* recorder);
ret_code_t audio_load_cardio_preset(audio_action_recorder_t* recorder);
ret_code_t audio_load_memo_only_preset(audio_action_recorder_t* recorder);
ret_code_t audio_load_ultra_low_power_preset(audio_action_recorder_t* recorder);

// Utility functions
const char* audio_get_mode_string(audio_mode_t mode);
const char* audio_format_duration(uint32_t seconds);
float audio_calculate_movement_similarity(const movement_analysis_t* move1, const movement_analysis_t* move2);
ret_code_t audio_generate_movement_signature(const int16_t* audio_data, uint16_t length, float* signature);

// Event callbacks (implemented by user)
void audio_on_movement_detected(audio_action_recorder_t* recorder, const movement_analysis_t* movement);
void audio_on_rep_detected(audio_action_recorder_t* recorder, uint16_t rep_count);
void audio_on_memo_recorded(audio_action_recorder_t* recorder, voice_memo_t* memo);
void audio_on_silence_detected(audio_action_recorder_t* recorder);
void audio_on_storage_low(audio_action_recorder_t* recorder, uint32_t free_space_kb);
void audio_on_battery_low(audio_action_recorder_t* recorder, uint8_t battery_percent);

// Timer callbacks (internal)
void audio_analysis_timer_handler(void* p_context);
void audio_memo_timeout_handler(void* p_context);
void audio_sleep_timer_handler(void* p_context);

// PDM callback (internal)
void audio_pdm_event_handler(nrf_drv_pdm_evt_t const * p_evt);

// Default configurations
#define AUDIO_STRENGTH_TRAINING_CONFIG {                \
    .sample_rate = 16000,                               \
    .bit_depth = 16,                                    \
    .compression_level = 3,                             \
    .auto_gain_control = true,                          \
    .microphone_gain = 80,                              \
    .movement_detection_enabled = true,                 \
    .movement_threshold = 300,                          \
    .movement_timeout_ms = 2000,                        \
    .auto_rep_counting = true,                          \
    .voice_activation = true,                           \
    .silence_timeout_sec = 3,                           \
    .memo_timestamps = true,                            \
    .quick_memo_mode = true,                            \
    .real_time_analysis = true,                         \
    .form_feedback_audio = true,                        \
    .feedback_volume = 60,                              \
    .ultra_low_power_mode = true,                       \
    .sleep_timeout_sec = 300,                           \
    .wake_on_sound = true                               \
}

#define AUDIO_MEMO_ONLY_CONFIG {                        \
    .sample_rate = 8000,                                \
    .bit_depth = 8,                                     \
    .compression_level = 6,                             \
    .auto_gain_control = true,                          \
    .microphone_gain = 90,                              \
    .movement_detection_enabled = false,                \
    .auto_rep_counting = false,                         \
    .voice_activation = true,                           \
    .silence_timeout_sec = 2,                           \
    .memo_timestamps = true,                            \
    .quick_memo_mode = true,                            \
    .real_time_analysis = false,                        \
    .form_feedback_audio = false,                       \
    .ultra_low_power_mode = true,                       \
    .sleep_timeout_sec = 60,                            \
    .wake_on_sound = true                               \
}

// Error codes
#define AUDIO_ERROR_BASE                0x8200
#define AUDIO_ERROR_NOT_INIT           (AUDIO_ERROR_BASE + 1)
#define AUDIO_ERROR_INVALID_MODE       (AUDIO_ERROR_BASE + 2)
#define AUDIO_ERROR_RECORDING_ACTIVE   (AUDIO_ERROR_BASE + 3)
#define AUDIO_ERROR_NO_STORAGE         (AUDIO_ERROR_BASE + 4)
#define AUDIO_ERROR_HARDWARE_FAULT     (AUDIO_ERROR_BASE + 5)
#define AUDIO_ERROR_MEMO_NOT_FOUND     (AUDIO_ERROR_BASE + 6)
#define AUDIO_ERROR_BUFFER_OVERFLOW    (AUDIO_ERROR_BASE + 7)

// Power consumption estimates (µA)
#define POWER_AUDIO_OFF                 1       // Deep sleep
#define POWER_AUDIO_LISTENING           200     // Background monitoring
#define POWER_AUDIO_RECORDING           2000    // Active recording
#define POWER_AUDIO_ANALYSIS            500     // Real-time processing
#define POWER_AUDIO_PLAYBACK            1500    // Playing audio

#endif // AUDIO_ACTION_RECORDER_H
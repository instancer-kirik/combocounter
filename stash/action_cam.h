#ifndef ACTION_CAM_H
#define ACTION_CAM_H

#include <stdint.h>
#include <stdbool.h>
#include "nrf_gpio.h"
#include "nrf_spi_mngr.h"
#include "app_timer.h"

// Camera module configuration
#define CAM_SPI_INSTANCE        2
#define CAM_CS_PIN              11
#define CAM_POWER_PIN           12
#define CAM_TRIGGER_PIN         14
#define CAM_RESET_PIN           8

// Video recording settings
#define CAM_MAX_RECORD_TIME_SEC 300     // 5 minutes max per clip
#define CAM_BUFFER_SIZE         4096    // SPI transfer buffer
#define CAM_FRAME_RATE          30      // FPS for video
#define CAM_RESOLUTION_WIDTH    1280
#define CAM_RESOLUTION_HEIGHT   720

// File system settings
#define CAM_MAX_FILES           100
#define CAM_FILE_PREFIX         "ACT"
#define CAM_FILE_EXTENSION      ".mp4"
#define CAM_THUMBNAIL_SIZE      160     // 160x120 thumbnail

// Action cam modes
typedef enum {
    ACTION_CAM_MODE_OFF = 0,
    ACTION_CAM_MODE_PHOTO,
    ACTION_CAM_MODE_VIDEO,
    ACTION_CAM_MODE_TIMELAPSE,
    ACTION_CAM_MODE_MOTION_DETECT
} action_cam_mode_t;

// Recording states
typedef enum {
    CAM_STATE_IDLE = 0,
    CAM_STATE_RECORDING,
    CAM_STATE_PAUSED,
    CAM_STATE_PROCESSING,
    CAM_STATE_ERROR
} cam_state_t;

// Video quality settings
typedef enum {
    CAM_QUALITY_LOW = 0,    // 480p, high compression
    CAM_QUALITY_MED,        // 720p, medium compression  
    CAM_QUALITY_HIGH        // 1080p, low compression
} cam_quality_t;

// Camera statistics
typedef struct {
    uint32_t total_photos;
    uint32_t total_videos;
    uint32_t total_record_time_sec;
    uint32_t storage_used_mb;
    uint32_t storage_free_mb;
    uint8_t battery_level;
    float last_record_duration;
} cam_stats_t;

// Camera configuration
typedef struct {
    action_cam_mode_t mode;
    cam_quality_t quality;
    uint16_t record_duration_sec;
    bool auto_off_enabled;
    uint16_t auto_off_timeout_sec;
    bool motion_detect_enabled;
    uint8_t motion_sensitivity;     // 1-10 scale
    bool audio_recording_enabled;
    uint8_t microphone_gain;        // 0-100%
    bool led_indicator_enabled;
    bool vibration_feedback_enabled;
} cam_config_t;

// File information structure
typedef struct {
    char filename[32];
    uint32_t file_size_kb;
    uint32_t timestamp;
    uint16_t duration_sec;          // For videos
    cam_quality_t quality;
    bool has_audio;
    bool is_protected;              // Prevent auto-deletion
} cam_file_info_t;

// Camera module interface
typedef struct {
    cam_state_t state;
    cam_config_t config;
    cam_stats_t stats;
    uint32_t current_file_size;
    uint32_t record_start_time;
    bool is_initialized;
    bool storage_available;
    
    // Hardware interfaces
    nrf_spi_mngr_t const * p_spi_mngr;
    uint32_t spi_transaction_id;
    uint8_t spi_buffer[CAM_BUFFER_SIZE];
    
    // Timer for recording duration
    app_timer_id_t record_timer_id;
    app_timer_id_t auto_off_timer_id;
} action_cam_t;

// Core camera functions
ret_code_t action_cam_init(action_cam_t* cam);
ret_code_t action_cam_deinit(action_cam_t* cam);
ret_code_t action_cam_power_on(action_cam_t* cam);
ret_code_t action_cam_power_off(action_cam_t* cam);

// Recording functions
ret_code_t action_cam_start_recording(action_cam_t* cam);
ret_code_t action_cam_stop_recording(action_cam_t* cam);
ret_code_t action_cam_pause_recording(action_cam_t* cam);
ret_code_t action_cam_resume_recording(action_cam_t* cam);
ret_code_t action_cam_take_photo(action_cam_t* cam);

// Configuration functions
ret_code_t action_cam_set_mode(action_cam_t* cam, action_cam_mode_t mode);
ret_code_t action_cam_set_quality(action_cam_t* cam, cam_quality_t quality);
ret_code_t action_cam_set_record_duration(action_cam_t* cam, uint16_t duration_sec);
ret_code_t action_cam_enable_motion_detect(action_cam_t* cam, bool enabled);
ret_code_t action_cam_set_motion_sensitivity(action_cam_t* cam, uint8_t sensitivity);

// File management functions  
ret_code_t action_cam_get_file_count(action_cam_t* cam, uint16_t* count);
ret_code_t action_cam_get_file_info(action_cam_t* cam, uint16_t index, cam_file_info_t* info);
ret_code_t action_cam_delete_file(action_cam_t* cam, const char* filename);
ret_code_t action_cam_protect_file(action_cam_t* cam, const char* filename, bool protect);
ret_code_t action_cam_format_storage(action_cam_t* cam);

// Status and statistics
ret_code_t action_cam_get_stats(action_cam_t* cam, cam_stats_t* stats);
cam_state_t action_cam_get_state(action_cam_t* cam);
uint8_t action_cam_get_battery_level(action_cam_t* cam);
uint32_t action_cam_get_free_space_mb(action_cam_t* cam);
bool action_cam_is_recording(action_cam_t* cam);

// Integration with ComboCounter
ret_code_t action_cam_trigger_on_combo(action_cam_t* cam, uint32_t combo_count);
ret_code_t action_cam_auto_photo_on_rep(action_cam_t* cam, bool enabled);
ret_code_t action_cam_start_workout_recording(action_cam_t* cam);
ret_code_t action_cam_end_workout_recording(action_cam_t* cam);

// Utility functions
ret_code_t action_cam_preview_mode(action_cam_t* cam, bool enabled);
ret_code_t action_cam_get_thumbnail(action_cam_t* cam, const char* filename, uint8_t* thumbnail_data);
const char* action_cam_get_mode_string(action_cam_mode_t mode);
const char* action_cam_get_quality_string(cam_quality_t quality);
const char* action_cam_get_state_string(cam_state_t state);

// Event callbacks (implemented by user)
void action_cam_on_recording_started(action_cam_t* cam);
void action_cam_on_recording_stopped(action_cam_t* cam, const char* filename);
void action_cam_on_photo_taken(action_cam_t* cam, const char* filename);
void action_cam_on_storage_full(action_cam_t* cam);
void action_cam_on_battery_low(action_cam_t* cam);
void action_cam_on_error(action_cam_t* cam, ret_code_t error_code);

// Timer callback functions (internal)
void action_cam_record_timer_handler(void* p_context);
void action_cam_auto_off_timer_handler(void* p_context);

// SPI callback functions (internal)
void action_cam_spi_callback(nrf_spi_mngr_evt_t const * p_evt);

// Hardware abstraction macros
#define ACTION_CAM_CS_ASSERT()      nrf_gpio_pin_clear(CAM_CS_PIN)
#define ACTION_CAM_CS_DEASSERT()    nrf_gpio_pin_set(CAM_CS_PIN)
#define ACTION_CAM_POWER_ON()       nrf_gpio_pin_set(CAM_POWER_PIN)
#define ACTION_CAM_POWER_OFF()      nrf_gpio_pin_clear(CAM_POWER_PIN)
#define ACTION_CAM_RESET_ASSERT()   nrf_gpio_pin_clear(CAM_RESET_PIN)
#define ACTION_CAM_RESET_DEASSERT() nrf_gpio_pin_set(CAM_RESET_PIN)

// Error codes (extends nrf_error.h)
#define ACTION_CAM_ERROR_BASE       0x8000
#define ACTION_CAM_ERROR_NOT_INIT   (ACTION_CAM_ERROR_BASE + 1)
#define ACTION_CAM_ERROR_NO_STORAGE (ACTION_CAM_ERROR_BASE + 2)
#define ACTION_CAM_ERROR_RECORDING  (ACTION_CAM_ERROR_BASE + 3)
#define ACTION_CAM_ERROR_FILE_IO    (ACTION_CAM_ERROR_BASE + 4)
#define ACTION_CAM_ERROR_HW_FAULT   (ACTION_CAM_ERROR_BASE + 5)

// Default configuration
#define ACTION_CAM_DEFAULT_CONFIG {                     \
    .mode = ACTION_CAM_MODE_VIDEO,                      \
    .quality = CAM_QUALITY_MED,                         \
    .record_duration_sec = 30,                          \
    .auto_off_enabled = true,                           \
    .auto_off_timeout_sec = 300,                        \
    .motion_detect_enabled = false,                     \
    .motion_sensitivity = 5,                            \
    .audio_recording_enabled = true,                    \
    .microphone_gain = 75,                              \
    .led_indicator_enabled = true,                      \
    .vibration_feedback_enabled = true                  \
}

#endif // ACTION_CAM_H
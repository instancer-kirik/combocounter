#ifndef SIMPLE_COMBO_CORE_H
#define SIMPLE_COMBO_CORE_H

#include <stdint.h>
#include <stdbool.h>

// Memory constraints for nRF52840
#define MAX_LABEL_LENGTH 16
#define MAX_COUNTERS 8
#define MAX_BLUETOOTH_MSG_SIZE 64

// Counter types for different use cases
typedef enum {
    COUNTER_TYPE_SIMPLE,        // Basic increment/decrement
    COUNTER_TYPE_COMBO,         // Combo with multiplier decay
    COUNTER_TYPE_TIMED,         // Time-based with auto-decay
    COUNTER_TYPE_ACCUMULATOR    // Always incrementing total
} CounterType;

// Quality levels for tracking rep/action quality
typedef enum {
    QUALITY_PERFECT = 3,        // 100% - full points
    QUALITY_GOOD = 2,           // 75% - most points  
    QUALITY_PARTIAL = 1,        // 50% - some points
    QUALITY_MISS = 0            // 0% - no points, may break combo
} ActionQuality;

// Individual counter state
typedef struct {
    char label[MAX_LABEL_LENGTH];   // User-defined label
    CounterType type;               // How this counter behaves
    
    // Current values
    int32_t count;                  // Current count/combo
    int32_t total;                  // Lifetime total for this counter
    int32_t max_combo;              // Highest combo reached
    float multiplier;               // Current multiplier (for combo types)
    
    // Configuration
    int32_t increment_amount;       // Points per action
    float decay_rate;               // How fast combo/multiplier decays
    float max_multiplier;           // Cap on multiplier
    bool breaks_on_miss;            // Whether QUALITY_MISS resets combo
    
    // Statistics
    uint32_t perfect_count;
    uint32_t good_count;
    uint32_t partial_count;
    uint32_t miss_count;
    
    // Timing
    uint32_t last_update_ms;        // For timed decay
    bool active;                    // Whether this counter is enabled
} Counter;

// Main device state
typedef struct {
    Counter counters[MAX_COUNTERS];
    uint8_t counter_count;
    uint8_t current_counter;        // Which counter is selected
    
    // Device settings
    bool auto_cycle_counters;       // Cycle through counters automatically
    uint16_t cycle_delay_ms;        // How long to show each counter
    bool bluetooth_enabled;         // Send data via BLE
    uint8_t button_increment_amount; // How much each button press adds
    
    // Power management
    bool low_power_mode;
    uint32_t last_interaction_ms;
    uint16_t sleep_timeout_sec;
    
    // Statistics
    uint32_t device_uptime_sec;
    uint32_t total_button_presses;
    
} ComboDevice;

// Bluetooth message structure for external app sync
typedef struct __attribute__((packed)) {
    uint8_t message_type;           // 1=counter_update, 2=stats, 3=config
    uint8_t counter_id;
    uint32_t timestamp;             // Unix timestamp or device uptime
    int32_t count;
    int32_t total;
    uint8_t quality;                // ActionQuality value
    char label[MAX_LABEL_LENGTH];
    uint8_t checksum;
} BluetoothMessage;

// Core functions
void combo_device_init(ComboDevice* device);
void combo_device_update(ComboDevice* device, float dt_sec);

// Counter management
bool counter_add(ComboDevice* device, const char* label, CounterType type);
bool counter_remove(ComboDevice* device, uint8_t index);
bool counter_set_active(ComboDevice* device, uint8_t index);
void counter_reset(Counter* counter);
void counter_clear_stats(Counter* counter);

// User actions
void counter_increment(Counter* counter, ActionQuality quality);
void counter_decrement(Counter* counter, uint32_t amount);
void counter_add_raw(Counter* counter, int32_t amount);

// Navigation
void device_next_counter(ComboDevice* device);
void device_prev_counter(ComboDevice* device);
Counter* device_get_current_counter(ComboDevice* device);

// Configuration helpers
void counter_configure_simple(Counter* counter, const char* label, int32_t increment);
void counter_configure_combo(Counter* counter, const char* label, 
                           int32_t base_points, float max_mult, float decay_rate);
void counter_configure_timed(Counter* counter, const char* label,
                           int32_t base_points, float decay_per_sec);
void counter_configure_accumulator(Counter* counter, const char* label, int32_t increment);

// Preset configurations for common use cases
void preset_workout_reps(ComboDevice* device);         // Rep counting with quality
void preset_meditation_breath(ComboDevice* device);    // Breath counting
void preset_habit_tracker(ComboDevice* device);        // Daily habit streaks  
void preset_game_score(ComboDevice* device);           // Gaming combo system
void preset_productivity(ComboDevice* device);         // Pomodoro/task counting

// Statistics and data export
float counter_get_accuracy(const Counter* counter);
uint32_t counter_get_total_actions(const Counter* counter);
float counter_get_average_quality(const Counter* counter);

// Bluetooth/external communication
bool bluetooth_send_counter_update(const Counter* counter, uint8_t counter_id, ActionQuality quality);
bool bluetooth_send_device_stats(const ComboDevice* device);
void bluetooth_message_pack(BluetoothMessage* msg, const Counter* counter, 
                          uint8_t counter_id, ActionQuality quality);

// Power management
void device_enter_sleep(ComboDevice* device);
void device_wake_up(ComboDevice* device);
bool device_should_sleep(const ComboDevice* device);

// Persistence (for flash storage)
typedef struct __attribute__((packed)) {
    uint32_t magic;                 // Validation
    uint32_t version;               // Data version
    uint8_t counter_count;
    Counter counters[MAX_COUNTERS]; // Full counter states
    uint32_t device_uptime_sec;
    uint32_t total_button_presses;
    uint16_t checksum;
} PersistentData;

bool device_save_to_flash(const ComboDevice* device);
bool device_load_from_flash(ComboDevice* device);

// Error handling
typedef enum {
    COMBO_OK = 0,
    COMBO_ERROR_COUNTER_FULL,
    COMBO_ERROR_INVALID_INDEX,
    COMBO_ERROR_BLUETOOTH_FAILED,
    COMBO_ERROR_STORAGE_FAILED,
    COMBO_ERROR_INVALID_CONFIG
} ComboError;

ComboError combo_get_last_error(void);
const char* combo_error_string(ComboError error);

// Utility macros
#define COUNTER_IS_VALID(device, index) \
    ((index) < (device)->counter_count && (device)->counters[index].active)

#define COUNTER_GET_CURRENT_RATE(counter) \
    ((counter)->multiplier * (counter)->increment_amount)

#define DEVICE_GET_CURRENT_LABEL(device) \
    ((device)->counter_count > 0 ? (device)->counters[(device)->current_counter].label : "None")

// Default configuration values
#define DEFAULT_INCREMENT_AMOUNT 1
#define DEFAULT_MAX_MULTIPLIER 5.0f
#define DEFAULT_DECAY_RATE 0.1f
#define DEFAULT_SLEEP_TIMEOUT_SEC 300
#define DEFAULT_CYCLE_DELAY_MS 3000

#endif // SIMPLE_COMBO_CORE_H
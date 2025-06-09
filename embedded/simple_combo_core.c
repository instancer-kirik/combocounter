#include "simple_combo_core.h"
#include <string.h>
#include <math.h>

// Static error state
static ComboError g_last_error = COMBO_OK;

// Helper functions
static void set_error(ComboError error) {
    g_last_error = error;
}

static uint16_t calculate_checksum(const uint8_t* data, size_t length) {
    uint16_t checksum = 0;
    for (size_t i = 0; i < length; i++) {
        checksum ^= data[i];
        checksum = (checksum << 1) | (checksum >> 15);
    }
    return checksum;
}

static void counter_init_defaults(Counter* counter) {
    memset(counter, 0, sizeof(Counter));
    counter->increment_amount = DEFAULT_INCREMENT_AMOUNT;
    counter->max_multiplier = DEFAULT_MAX_MULTIPLIER;
    counter->decay_rate = DEFAULT_DECAY_RATE;
    counter->multiplier = 1.0f;
    counter->breaks_on_miss = true;
    counter->active = false;
}

static void counter_apply_decay(Counter* counter, float dt_sec) {
    if (counter->type == COUNTER_TYPE_TIMED && counter->multiplier > 1.0f) {
        counter->multiplier -= counter->decay_rate * dt_sec;
        if (counter->multiplier < 1.0f) {
            counter->multiplier = 1.0f;
        }
    }
}

// Core device functions
void combo_device_init(ComboDevice* device) {
    if (!device) return;
    
    memset(device, 0, sizeof(ComboDevice));
    
    // Initialize all counters as inactive
    for (uint8_t i = 0; i < MAX_COUNTERS; i++) {
        counter_init_defaults(&device->counters[i]);
    }
    
    // Set device defaults
    device->counter_count = 0;
    device->current_counter = 0;
    device->auto_cycle_counters = false;
    device->cycle_delay_ms = DEFAULT_CYCLE_DELAY_MS;
    device->bluetooth_enabled = true;
    device->button_increment_amount = 1;
    device->sleep_timeout_sec = DEFAULT_SLEEP_TIMEOUT_SEC;
    device->low_power_mode = false;
    device->last_interaction_ms = 0;
}

void combo_device_update(ComboDevice* device, float dt_sec) {
    if (!device) return;
    
    device->device_uptime_sec += (uint32_t)dt_sec;
    
    // Update all active counters
    for (uint8_t i = 0; i < device->counter_count; i++) {
        Counter* counter = &device->counters[i];
        if (counter->active) {
            counter->last_update_ms += (uint32_t)(dt_sec * 1000);
            counter_apply_decay(counter, dt_sec);
        }
    }
    
    // Check for sleep timeout
    if (device->last_interaction_ms > 0 && 
        (device->device_uptime_sec * 1000 - device->last_interaction_ms) > 
        (device->sleep_timeout_sec * 1000)) {
        device->low_power_mode = true;
    }
}

// Counter management
bool counter_add(ComboDevice* device, const char* label, CounterType type) {
    if (!device || !label || device->counter_count >= MAX_COUNTERS) {
        set_error(COMBO_ERROR_COUNTER_FULL);
        return false;
    }
    
    Counter* counter = &device->counters[device->counter_count];
    counter_init_defaults(counter);
    
    // Set label with bounds checking
    strncpy(counter->label, label, MAX_LABEL_LENGTH - 1);
    counter->label[MAX_LABEL_LENGTH - 1] = '\0';
    
    counter->type = type;
    counter->active = true;
    device->counter_count++;
    
    return true;
}

bool counter_remove(ComboDevice* device, uint8_t index) {
    if (!device || index >= device->counter_count) {
        set_error(COMBO_ERROR_INVALID_INDEX);
        return false;
    }
    
    // Shift remaining counters down
    for (uint8_t i = index; i < device->counter_count - 1; i++) {
        device->counters[i] = device->counters[i + 1];
    }
    
    device->counter_count--;
    
    // Adjust current counter index if needed
    if (device->current_counter >= device->counter_count && device->counter_count > 0) {
        device->current_counter = device->counter_count - 1;
    }
    
    return true;
}

bool counter_set_active(ComboDevice* device, uint8_t index) {
    if (!device || index >= device->counter_count) {
        set_error(COMBO_ERROR_INVALID_INDEX);
        return false;
    }
    
    device->current_counter = index;
    device->last_interaction_ms = device->device_uptime_sec * 1000;
    return true;
}

void counter_reset(Counter* counter) {
    if (!counter) return;
    
    counter->count = 0;
    counter->multiplier = 1.0f;
    counter->last_update_ms = 0;
}

void counter_clear_stats(Counter* counter) {
    if (!counter) return;
    
    counter->total = 0;
    counter->max_combo = 0;
    counter->perfect_count = 0;
    counter->good_count = 0;
    counter->partial_count = 0;
    counter->miss_count = 0;
}

// User actions
void counter_increment(Counter* counter, ActionQuality quality) {
    if (!counter || !counter->active) return;
    
    // Update quality statistics
    switch (quality) {
        case QUALITY_PERFECT:
            counter->perfect_count++;
            break;
        case QUALITY_GOOD:
            counter->good_count++;
            break;
        case QUALITY_PARTIAL:
            counter->partial_count++;
            break;
        case QUALITY_MISS:
            counter->miss_count++;
            break;
    }
    
    // Handle based on counter type
    switch (counter->type) {
        case COUNTER_TYPE_SIMPLE:
            if (quality != QUALITY_MISS) {
                counter->count += counter->increment_amount;
                counter->total += counter->increment_amount;
            }
            break;
            
        case COUNTER_TYPE_COMBO:
            if (quality == QUALITY_MISS && counter->breaks_on_miss) {
                counter->count = 0;
                counter->multiplier = 1.0f;
            } else if (quality != QUALITY_MISS) {
                int32_t points = (int32_t)(counter->increment_amount * counter->multiplier * quality * 0.33f);
                counter->count += points;
                counter->total += points;
                
                // Increase multiplier
                counter->multiplier += 0.1f * quality * 0.33f;
                if (counter->multiplier > counter->max_multiplier) {
                    counter->multiplier = counter->max_multiplier;
                }
                
                // Track max combo
                if (counter->count > counter->max_combo) {
                    counter->max_combo = counter->count;
                }
            }
            break;
            
        case COUNTER_TYPE_TIMED:
            if (quality != QUALITY_MISS) {
                int32_t points = (int32_t)(counter->increment_amount * counter->multiplier * quality * 0.33f);
                counter->count += points;
                counter->total += points;
                
                // Boost multiplier for good actions
                counter->multiplier += 0.05f * quality * 0.33f;
                if (counter->multiplier > counter->max_multiplier) {
                    counter->multiplier = counter->max_multiplier;
                }
            }
            break;
            
        case COUNTER_TYPE_ACCUMULATOR:
            if (quality != QUALITY_MISS) {
                int32_t points = counter->increment_amount * quality;
                counter->total += points;
                counter->count = counter->total; // Always show total
            }
            break;
    }
}

void counter_decrement(Counter* counter, uint32_t amount) {
    if (!counter || !counter->active) return;
    
    if (counter->count >= (int32_t)amount) {
        counter->count -= amount;
    } else {
        counter->count = 0;
    }
    
    // Reset multiplier for combo types
    if (counter->type == COUNTER_TYPE_COMBO) {
        counter->multiplier = 1.0f;
    }
}

void counter_add_raw(Counter* counter, int32_t amount) {
    if (!counter || !counter->active) return;
    
    counter->count += amount;
    if (amount > 0) {
        counter->total += amount;
    }
    
    if (counter->count < 0) {
        counter->count = 0;
    }
}

// Navigation
void device_next_counter(ComboDevice* device) {
    if (!device || device->counter_count == 0) return;
    
    device->current_counter = (device->current_counter + 1) % device->counter_count;
    device->last_interaction_ms = device->device_uptime_sec * 1000;
}

void device_prev_counter(ComboDevice* device) {
    if (!device || device->counter_count == 0) return;
    
    if (device->current_counter == 0) {
        device->current_counter = device->counter_count - 1;
    } else {
        device->current_counter--;
    }
    device->last_interaction_ms = device->device_uptime_sec * 1000;
}

Counter* device_get_current_counter(ComboDevice* device) {
    if (!device || device->counter_count == 0) return NULL;
    return &device->counters[device->current_counter];
}

// Configuration helpers
void counter_configure_simple(Counter* counter, const char* label, int32_t increment) {
    if (!counter || !label) return;
    
    counter_init_defaults(counter);
    strncpy(counter->label, label, MAX_LABEL_LENGTH - 1);
    counter->label[MAX_LABEL_LENGTH - 1] = '\0';
    counter->type = COUNTER_TYPE_SIMPLE;
    counter->increment_amount = increment;
    counter->active = true;
}

void counter_configure_combo(Counter* counter, const char* label, 
                           int32_t base_points, float max_mult, float decay_rate) {
    if (!counter || !label) return;
    
    counter_init_defaults(counter);
    strncpy(counter->label, label, MAX_LABEL_LENGTH - 1);
    counter->label[MAX_LABEL_LENGTH - 1] = '\0';
    counter->type = COUNTER_TYPE_COMBO;
    counter->increment_amount = base_points;
    counter->max_multiplier = max_mult;
    counter->decay_rate = decay_rate;
    counter->active = true;
}

void counter_configure_timed(Counter* counter, const char* label,
                           int32_t base_points, float decay_per_sec) {
    if (!counter || !label) return;
    
    counter_init_defaults(counter);
    strncpy(counter->label, label, MAX_LABEL_LENGTH - 1);
    counter->label[MAX_LABEL_LENGTH - 1] = '\0';
    counter->type = COUNTER_TYPE_TIMED;
    counter->increment_amount = base_points;
    counter->decay_rate = decay_per_sec;
    counter->active = true;
}

void counter_configure_accumulator(Counter* counter, const char* label, int32_t increment) {
    if (!counter || !label) return;
    
    counter_init_defaults(counter);
    strncpy(counter->label, label, MAX_LABEL_LENGTH - 1);
    counter->label[MAX_LABEL_LENGTH - 1] = '\0';
    counter->type = COUNTER_TYPE_ACCUMULATOR;
    counter->increment_amount = increment;
    counter->active = true;
}

// Preset configurations
void preset_workout_reps(ComboDevice* device) {
    if (!device) return;
    
    combo_device_init(device);
    
    counter_add(device, "Reps", COUNTER_TYPE_SIMPLE);
    counter_configure_simple(&device->counters[0], "Reps", 1);
    
    counter_add(device, "Sets", COUNTER_TYPE_SIMPLE);
    counter_configure_simple(&device->counters[1], "Sets", 1);
    
    counter_add(device, "Combo", COUNTER_TYPE_COMBO);
    counter_configure_combo(&device->counters[2], "Combo", 10, 3.0f, 0.1f);
}

void preset_meditation_breath(ComboDevice* device) {
    if (!device) return;
    
    combo_device_init(device);
    
    counter_add(device, "Breaths", COUNTER_TYPE_SIMPLE);
    counter_configure_simple(&device->counters[0], "Breaths", 1);
    
    counter_add(device, "Focus", COUNTER_TYPE_TIMED);
    counter_configure_timed(&device->counters[1], "Focus", 5, 0.2f);
}

void preset_habit_tracker(ComboDevice* device) {
    if (!device) return;
    
    combo_device_init(device);
    
    counter_add(device, "Streak", COUNTER_TYPE_COMBO);
    counter_configure_combo(&device->counters[0], "Streak", 1, 2.0f, 0.0f);
    device->counters[0].breaks_on_miss = true;
    
    counter_add(device, "Total", COUNTER_TYPE_ACCUMULATOR);
    counter_configure_accumulator(&device->counters[1], "Total", 1);
}

void preset_game_score(ComboDevice* device) {
    if (!device) return;
    
    combo_device_init(device);
    
    counter_add(device, "Score", COUNTER_TYPE_COMBO);
    counter_configure_combo(&device->counters[0], "Score", 100, 5.0f, 0.1f);
    
    counter_add(device, "Lives", COUNTER_TYPE_SIMPLE);
    counter_configure_simple(&device->counters[1], "Lives", 1);
    device->counters[1].count = 3; // Start with 3 lives
}

void preset_productivity(ComboDevice* device) {
    if (!device) return;
    
    combo_device_init(device);
    
    counter_add(device, "Tasks", COUNTER_TYPE_SIMPLE);
    counter_configure_simple(&device->counters[0], "Tasks", 1);
    
    counter_add(device, "Pomodoros", COUNTER_TYPE_SIMPLE);
    counter_configure_simple(&device->counters[1], "Pomodoros", 1);
    
    counter_add(device, "Flow", COUNTER_TYPE_TIMED);
    counter_configure_timed(&device->counters[2], "Flow", 25, 0.1f);
}

// Statistics
float counter_get_accuracy(const Counter* counter) {
    if (!counter) return 0.0f;
    
    uint32_t total_actions = counter_get_total_actions(counter);
    if (total_actions == 0) return 0.0f;
    
    uint32_t good_actions = counter->perfect_count + counter->good_count;
    return (float)good_actions / total_actions;
}

uint32_t counter_get_total_actions(const Counter* counter) {
    if (!counter) return 0;
    
    return counter->perfect_count + counter->good_count + 
           counter->partial_count + counter->miss_count;
}

float counter_get_average_quality(const Counter* counter) {
    if (!counter) return 0.0f;
    
    uint32_t total_actions = counter_get_total_actions(counter);
    if (total_actions == 0) return 0.0f;
    
    float weighted_sum = counter->perfect_count * 3.0f + 
                        counter->good_count * 2.0f + 
                        counter->partial_count * 1.0f;
    
    return weighted_sum / total_actions;
}

// Bluetooth/external communication
void bluetooth_message_pack(BluetoothMessage* msg, const Counter* counter, 
                          uint8_t counter_id, ActionQuality quality) {
    if (!msg || !counter) return;
    
    memset(msg, 0, sizeof(BluetoothMessage));
    
    msg->message_type = 1; // counter_update
    msg->counter_id = counter_id;
    msg->timestamp = 0; // Would be set by caller with real timestamp
    msg->count = counter->count;
    msg->total = counter->total;
    msg->quality = (uint8_t)quality;
    
    strncpy(msg->label, counter->label, MAX_LABEL_LENGTH - 1);
    msg->label[MAX_LABEL_LENGTH - 1] = '\0';
    
    // Calculate checksum
    msg->checksum = calculate_checksum((const uint8_t*)msg, sizeof(BluetoothMessage) - 1);
}

bool bluetooth_send_counter_update(const Counter* counter, uint8_t counter_id, ActionQuality quality) {
    if (!counter) return false;
    
    BluetoothMessage msg;
    bluetooth_message_pack(&msg, counter, counter_id, quality);
    
    // TODO: Implement actual Bluetooth transmission
    // This would use Nordic's BLE stack to send the message
    
    return true; // Placeholder
}

bool bluetooth_send_device_stats(const ComboDevice* device) {
    if (!device) return false;
    
    // TODO: Send device-level statistics
    // Could send battery level, uptime, total button presses, etc.
    
    return true; // Placeholder
}

// Power management
void device_enter_sleep(ComboDevice* device) {
    if (!device) return;
    
    device->low_power_mode = true;
    // TODO: Actual hardware sleep implementation
}

void device_wake_up(ComboDevice* device) {
    if (!device) return;
    
    device->low_power_mode = false;
    device->last_interaction_ms = device->device_uptime_sec * 1000;
}

bool device_should_sleep(const ComboDevice* device) {
    if (!device) return false;
    
    if (device->last_interaction_ms == 0) return false;
    
    uint32_t time_since_interaction = device->device_uptime_sec * 1000 - device->last_interaction_ms;
    return time_since_interaction > (device->sleep_timeout_sec * 1000);
}

// Persistence
bool device_save_to_flash(const ComboDevice* device) {
    if (!device) return false;
    
    PersistentData data;
    data.magic = 0xC0C0C0C0; // Combo magic
    data.version = 1;
    data.counter_count = device->counter_count;
    
    memcpy(data.counters, device->counters, sizeof(device->counters));
    data.device_uptime_sec = device->device_uptime_sec;
    data.total_button_presses = device->total_button_presses;
    
    data.checksum = calculate_checksum((const uint8_t*)&data, 
                                      sizeof(PersistentData) - sizeof(data.checksum));
    
    // TODO: Write to Nordic's FDS flash storage
    return true; // Placeholder
}

bool device_load_from_flash(ComboDevice* device) {
    if (!device) return false;
    
    // TODO: Read from Nordic's FDS flash storage
    // Validate magic number and checksum
    // Restore counter states
    
    return false; // Placeholder - no data to load
}

// Error handling
ComboError combo_get_last_error(void) {
    return g_last_error;
}

const char* combo_error_string(ComboError error) {
    switch (error) {
        case COMBO_OK: return "No error";
        case COMBO_ERROR_COUNTER_FULL: return "Maximum counters reached";
        case COMBO_ERROR_INVALID_INDEX: return "Invalid counter index";
        case COMBO_ERROR_BLUETOOTH_FAILED: return "Bluetooth transmission failed";
        case COMBO_ERROR_STORAGE_FAILED: return "Flash storage operation failed";
        case COMBO_ERROR_INVALID_CONFIG: return "Invalid configuration";
        default: return "Unknown error";
    }
}
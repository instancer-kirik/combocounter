#define CLAY_IMPLEMENTATION
#include "../src/clay.h"
#include "clay_epaper_renderer.h"
#include "simple_combo_core.h"
#include "musicmaker_integration.h"
#include "dual_spi_config.h"
#include <stdio.h>
#include <string.h>

// ================================
// AUDIO MODES AND CONFIGURATION
// ================================

typedef enum {
    AUDIO_MODE_SILENT = 0,          // No audio
    AUDIO_MODE_BEEPS = 1,           // Simple beep sounds
    AUDIO_MODE_VOICE_CLIPS = 2,     // Voice counting and feedback
    AUDIO_MODE_MUSIC = 3,           // Background music + effects
    AUDIO_MODE_MAX
} AudioMode;

typedef enum {
    COMBO_STATUS_BUILDING = 0,      // Combo is increasing
    COMBO_STATUS_HOLDING = 1,       // Combo maintained
    COMBO_STATUS_DECAYING = 2,      // Combo slowly decaying
    COMBO_STATUS_BROKEN = 3,        // Combo just broken
    COMBO_STATUS_NEW_RECORD = 4     // New personal record!
} ComboStatus;

typedef struct {
    AudioMode mode;
    bool count_aloud;              // Voice counting: "1, 2, 3..."
    bool form_feedback;            // "Perfect", "Good", "Miss"
    bool combo_announcements;     // "10 combo!", "Combo broken!"
    bool milestone_sounds;         // Special sounds at 10, 25, 50, 100
    uint8_t volume;               // 0-10 volume level
    bool bluetooth_audio;         // Send audio over bluetooth
} AudioConfig;

typedef struct {
    bool connected;               // Bluetooth device connected
    char device_name[32];         // Connected device name
    bool auto_sync;              // Auto sync data
    bool stream_audio;           // Stream audio to phone
    uint32_t last_sync_time;     // Last data sync timestamp
} BluetoothConfig;

// ================================
// GLOBAL STATE
// ================================

static ClayEpaperContext* g_clay_ctx = NULL;
static ComboDevice* g_device = NULL;
static AudioConfig g_audio_config = {
    .mode = AUDIO_MODE_BEEPS,
    .count_aloud = true,
    .form_feedback = true,
    .combo_announcements = true,
    .milestone_sounds = true,
    .volume = 7,
    .bluetooth_audio = false
};

static BluetoothConfig g_bluetooth_config = {
    .connected = false,
    .device_name = "",
    .auto_sync = true,
    .stream_audio = false,
    .last_sync_time = 0
};

static ComboStatus g_combo_status = COMBO_STATUS_BUILDING;
static uint32_t g_last_combo_count = 0;
static float g_combo_decay_timer = 0.0f;
static bool g_show_combo_celebration = false;
static uint32_t g_celebration_timer = 0;

// Audio clip names for voice mode
static const char* VOICE_CLIPS[] = {
    "one.wav", "two.wav", "three.wav", "four.wav", "five.wav",
    "six.wav", "seven.wav", "eight.wav", "nine.wav", "ten.wav",
    "perfect.wav", "good.wav", "partial.wav", "miss.wav",
    "combo_10.wav", "combo_25.wav", "combo_50.wav", "combo_100.wav",
    "combo_broken.wav", "new_record.wav", "workout_complete.wav"
};

// ================================
// AUDIO SYSTEM FUNCTIONS
// ================================

void play_count_audio(uint32_t count) {
    if (!g_audio_config.count_aloud) return;
    
    switch (g_audio_config.mode) {
        case AUDIO_MODE_BEEPS:
            // Simple beep for each count
            musicmaker_play_test_tone(800 + (count % 8) * 50, 100);
            break;
            
        case AUDIO_MODE_VOICE_CLIPS:
            if (count <= 10) {
                musicmaker_play_mp3_file(VOICE_CLIPS[count - 1]);
            } else {
                // For counts > 10, just beep
                musicmaker_play_test_tone(1000, 150);
            }
            break;
            
        case AUDIO_MODE_MUSIC:
            // Play musical note based on count
            uint16_t frequencies[] = {261, 294, 330, 349, 392, 440, 494, 523}; // C major scale
            musicmaker_play_test_tone(frequencies[count % 8], 200);
            break;
            
        case AUDIO_MODE_SILENT:
        default:
            break;
    }
}

void play_quality_feedback(ActionQuality quality) {
    if (!g_audio_config.form_feedback) return;
    
    switch (g_audio_config.mode) {
        case AUDIO_MODE_BEEPS:
            switch (quality) {
                case QUALITY_PERFECT: 
                    musicmaker_play_test_tone(1200, 200);
                    break;
                case QUALITY_GOOD:
                    musicmaker_play_test_tone(900, 150);
                    break;
                case QUALITY_PARTIAL:
                    musicmaker_play_test_tone(600, 200);
                    break;
                case QUALITY_MISS:
                    musicmaker_play_test_tone(300, 400);
                    break;
            }
            break;
            
        case AUDIO_MODE_VOICE_CLIPS:
            musicmaker_play_mp3_file(VOICE_CLIPS[10 + quality]); // perfect.wav, good.wav, etc.
            break;
            
        case AUDIO_MODE_MUSIC:
            // Play chord based on quality
            switch (quality) {
                case QUALITY_PERFECT: 
                    // Major chord
                    musicmaker_play_test_tone(523, 300); // C
                    break;
                case QUALITY_GOOD:
                    musicmaker_play_test_tone(440, 250); // A
                    break;
                case QUALITY_PARTIAL:
                    musicmaker_play_test_tone(349, 300); // F
                    break;
                case QUALITY_MISS:
                    musicmaker_play_test_tone(277, 500); // C# (dissonant)
                    break;
            }
            break;
            
        case AUDIO_MODE_SILENT:
        default:
            break;
    }
}

void play_combo_announcement(uint32_t combo_count, ComboStatus status) {
    if (!g_audio_config.combo_announcements) return;
    
    switch (status) {
        case COMBO_STATUS_NEW_RECORD:
            if (g_audio_config.mode == AUDIO_MODE_VOICE_CLIPS) {
                musicmaker_play_mp3_file("new_record.wav");
            } else {
                // Victory fanfare
                musicmaker_play_test_tone(523, 200);
                musicmaker_play_test_tone(659, 200); 
                musicmaker_play_test_tone(784, 300);
            }
            break;
            
        case COMBO_STATUS_BROKEN:
            if (g_audio_config.mode == AUDIO_MODE_VOICE_CLIPS) {
                musicmaker_play_mp3_file("combo_broken.wav");
            } else {
                // Sad descending tone
                musicmaker_play_test_tone(400, 300);
                musicmaker_play_test_tone(300, 400);
            }
            break;
            
        case COMBO_STATUS_BUILDING:
            // Milestone announcements
            if (g_audio_config.milestone_sounds) {
                if (combo_count == 10 && g_audio_config.mode == AUDIO_MODE_VOICE_CLIPS) {
                    musicmaker_play_mp3_file("combo_10.wav");
                } else if (combo_count == 25 && g_audio_config.mode == AUDIO_MODE_VOICE_CLIPS) {
                    musicmaker_play_mp3_file("combo_25.wav");
                } else if (combo_count == 50 && g_audio_config.mode == AUDIO_MODE_VOICE_CLIPS) {
                    musicmaker_play_mp3_file("combo_50.wav");
                } else if (combo_count == 100 && g_audio_config.mode == AUDIO_MODE_VOICE_CLIPS) {
                    musicmaker_play_mp3_file("combo_100.wav");
                } else if (combo_count % 10 == 0 && combo_count > 0) {
                    // Celebration beep for every 10th combo
                    musicmaker_play_test_tone(1000 + combo_count, 150);
                }
            }
            break;
            
        default:
            break;
    }
}

// ================================
// COMBO STATUS TRACKING
// ================================

ComboStatus update_combo_status(Counter* counter) {
    if (!counter || counter->type != COUNTER_TYPE_COMBO) {
        return COMBO_STATUS_BUILDING;
    }
    
    ComboStatus new_status = COMBO_STATUS_BUILDING;
    uint32_t current_combo = counter->count;
    
    // Check for new record
    if (current_combo > counter->max_combo && current_combo > 0) {
        new_status = COMBO_STATUS_NEW_RECORD;
        g_show_combo_celebration = true;
        g_celebration_timer = 3000; // Show celebration for 3 seconds
    }
    // Check if combo was broken
    else if (current_combo == 0 && g_last_combo_count > 0) {
        new_status = COMBO_STATUS_BROKEN;
        g_show_combo_celebration = true;
        g_celebration_timer = 2000; // Show broken message for 2 seconds
    }
    // Check if combo is decaying (multiplier decreasing)
    else if (counter->multiplier < 1.5f && current_combo > 0) {
        new_status = COMBO_STATUS_DECAYING;
    }
    // Check if combo is being held steady
    else if (current_combo == g_last_combo_count && current_combo > 0) {
        new_status = COMBO_STATUS_HOLDING;
    }
    // Default: combo building
    else {
        new_status = COMBO_STATUS_BUILDING;
    }
    
    // Play audio if status changed
    if (new_status != g_combo_status) {
        play_combo_announcement(current_combo, new_status);
    }
    
    g_last_combo_count = current_combo;
    g_combo_status = new_status;
    
    return new_status;
}

const char* get_combo_status_text(ComboStatus status) {
    switch (status) {
        case COMBO_STATUS_BUILDING: return "Building";
        case COMBO_STATUS_HOLDING: return "Steady";
        case COMBO_STATUS_DECAYING: return "Decaying";
        case COMBO_STATUS_BROKEN: return "Broken";
        case COMBO_STATUS_NEW_RECORD: return "NEW RECORD!";
        default: return "Unknown";
    }
}

// ================================
// BLUETOOTH FUNCTIONS
// ================================

void bluetooth_send_counter_update(Counter* counter) {
    if (!g_bluetooth_config.connected || !g_bluetooth_config.auto_sync) {
        return;
    }
    
    // Create bluetooth message
    BluetoothMessage msg = {
        .message_type = 1, // counter_update
        .counter_id = g_device->current_counter,
        .timestamp = 0, // Would use system time
        .count = counter->count,
        .total = counter->total,
        .quality = 0, // Would set based on last action
    };
    
    strncpy(msg.label, counter->label, MAX_LABEL_LENGTH - 1);
    msg.label[MAX_LABEL_LENGTH - 1] = '\0';
    
    // Calculate simple checksum
    msg.checksum = msg.message_type + msg.counter_id + (msg.count & 0xFF);
    
    // Send via BLE (implementation would depend on Nordic SDK)
    // ble_send_data(&msg, sizeof(msg));
}

void bluetooth_stream_audio_data(const uint8_t* audio_data, uint16_t length) {
    if (!g_bluetooth_config.connected || !g_bluetooth_config.stream_audio) {
        return;
    }
    
    // Stream audio data to connected device
    // Implementation would use Nordic's audio streaming APIs
}

// ================================
// CLAY UI RENDERING
// ================================

void render_audio_status_indicator(void) {
    // Audio mode indicator in top-right corner
    const char* audio_text = "";
    Clay_Color audio_color = {0.5f, 0.5f, 0.5f, 1.0f};
    
    switch (g_audio_config.mode) {
        case AUDIO_MODE_SILENT:
            audio_text = "🔇";
            audio_color = (Clay_Color){0.7f, 0.7f, 0.7f, 1.0f};
            break;
        case AUDIO_MODE_BEEPS:
            audio_text = "🔔";
            audio_color = (Clay_Color){0.2f, 0.8f, 0.2f, 1.0f};
            break;
        case AUDIO_MODE_VOICE_CLIPS:
            audio_text = "🎤";
            audio_color = (Clay_Color){0.2f, 0.2f, 0.8f, 1.0f};
            break;
        case AUDIO_MODE_MUSIC:
            audio_text = "🎵";
            audio_color = (Clay_Color){0.8f, 0.2f, 0.8f, 1.0f};
            break;
    }
    
    CLAY(CLAY_ID("AudioIndicator"),
         CLAY_LAYOUT(.sizing = {CLAY_SIZING_FIXED(20), CLAY_SIZING_FIXED(20)})) {
        CLAY_TEXT(CLAY_ID("AudioIcon"), CLAY_STRING(audio_text),
                 CLAY_TEXT_CONFIG(.textColor = audio_color, .fontSize = 12));
    }
}

void render_bluetooth_status_indicator(void) {
    // Bluetooth status in top-right corner
    const char* bt_text = g_bluetooth_config.connected ? "📱" : "📱❌";
    Clay_Color bt_color = g_bluetooth_config.connected ? 
        (Clay_Color){0.2f, 0.2f, 0.8f, 1.0f} : (Clay_Color){0.7f, 0.7f, 0.7f, 1.0f};
    
    CLAY(CLAY_ID("BluetoothIndicator"),
         CLAY_LAYOUT(.sizing = {CLAY_SIZING_FIXED(20), CLAY_SIZING_FIXED(20)})) {
        CLAY_TEXT(CLAY_ID("BluetoothIcon"), CLAY_STRING(bt_text),
                 CLAY_TEXT_CONFIG(.textColor = bt_color, .fontSize = 12));
    }
}

void render_combo_status_display(Counter* counter) {
    if (counter->type != COUNTER_TYPE_COMBO) {
        return;
    }
    
    // Update combo status
    ComboStatus status = update_combo_status(counter);
    
    CLAY(CLAY_ID("ComboStatusSection"),
         CLAY_LAYOUT(.sizing = {CLAY_SIZING_GROW(), CLAY_SIZING_FIXED(80)},
                     .layoutDirection = CLAY_TOP_TO_BOTTOM,
                     .padding = {8, 8, 8, 8},
                     .childGap = 4)) {
        
        // Combo status header
        CLAY(CLAY_ID("ComboStatusHeader"),
             CLAY_LAYOUT(.sizing = {CLAY_SIZING_GROW(), CLAY_SIZING_FIXED(20)},
                         .layoutDirection = CLAY_LEFT_TO_RIGHT,
                         .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER})) {
            
            CLAY_TEXT(CLAY_ID("ComboLabel"), CLAY_STRING("COMBO STATUS:"),
                     CLAY_TEXT_CONFIG(.textColor = {0, 0, 0, 1}, .fontSize = 10));
            
            // Status text with color coding
            Clay_Color status_color;
            switch (status) {
                case COMBO_STATUS_BUILDING:
                    status_color = (Clay_Color){0.2f, 0.8f, 0.2f, 1.0f}; // Green
                    break;
                case COMBO_STATUS_HOLDING:
                    status_color = (Clay_Color){0.8f, 0.8f, 0.2f, 1.0f}; // Yellow
                    break;
                case COMBO_STATUS_DECAYING:
                    status_color = (Clay_Color){0.8f, 0.5f, 0.2f, 1.0f}; // Orange
                    break;
                case COMBO_STATUS_BROKEN:
                    status_color = (Clay_Color){0.8f, 0.2f, 0.2f, 1.0f}; // Red
                    break;
                case COMBO_STATUS_NEW_RECORD:
                    status_color = (Clay_Color){0.8f, 0.2f, 0.8f, 1.0f}; // Purple
                    break;
                default:
                    status_color = (Clay_Color){0.5f, 0.5f, 0.5f, 1.0f}; // Gray
                    break;
            }
            
            CLAY_TEXT(CLAY_ID("ComboStatusText"), CLAY_STRING(get_combo_status_text(status)),
                     CLAY_TEXT_CONFIG(.textColor = status_color, .fontSize = 12));
        }
        
        // Combo details
        CLAY(CLAY_ID("ComboDetails"),
             CLAY_LAYOUT(.sizing = {CLAY_SIZING_GROW(), CLAY_SIZING_GROW()},
                         .layoutDirection = CLAY_LEFT_TO_RIGHT,
                         .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER},
                         .childGap = 16)) {
            
            // Current combo count
            char combo_text[32];
            snprintf(combo_text, sizeof(combo_text), "Current: %d", counter->count);
            CLAY_TEXT(CLAY_ID("CurrentCombo"), CLAY_STRING(combo_text),
                     CLAY_TEXT_CONFIG(.textColor = {0, 0, 0, 1}, .fontSize = 14));
            
            // Best combo
            char best_text[32];
            snprintf(best_text, sizeof(best_text), "Best: %d", counter->max_combo);
            CLAY_TEXT(CLAY_ID("BestCombo"), CLAY_STRING(best_text),
                     CLAY_TEXT_CONFIG(.textColor = {0, 0, 0, 1}, .fontSize = 14));
            
            // Multiplier
            char mult_text[32];
            snprintf(mult_text, sizeof(mult_text), "×%.2f", counter->multiplier);
            CLAY_TEXT(CLAY_ID("Multiplier"), CLAY_STRING(mult_text),
                     CLAY_TEXT_CONFIG(.textColor = {0, 0, 0, 1}, .fontSize = 16));
        }
        
        // Combo celebration overlay
        if (g_show_combo_celebration && g_celebration_timer > 0) {
            CLAY(CLAY_ID("CelebrationOverlay"),
                 CLAY_LAYOUT(.sizing = {CLAY_SIZING_GROW(), CLAY_SIZING_FIXED(30)},
                             .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}),
                 CLAY_RECTANGLE(.color = {1.0f, 1.0f, 0.8f, 1.0f}),
                 CLAY_BORDER(.width = 2, .color = {0.8f, 0.8f, 0.2f, 1.0f})) {
                
                const char* celebration_text = (status == COMBO_STATUS_NEW_RECORD) ? 
                    "🎉 NEW RECORD! 🎉" : "💔 COMBO BROKEN 💔";
                
                CLAY_TEXT(CLAY_ID("CelebrationText"), CLAY_STRING(celebration_text),
                         CLAY_TEXT_CONFIG(.textColor = {0, 0, 0, 1}, .fontSize = 12));
            }
        }
    }
}

void render_main_counter_display(Counter* counter) {
    CLAY(CLAY_ID("MainCounterDisplay"),
         CLAY_LAYOUT(.sizing = {CLAY_SIZING_GROW(), CLAY_SIZING_GROW()},
                     .layoutDirection = CLAY_TOP_TO_BOTTOM,
                     .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER},
                     .childGap = 8)) {
        
        // Counter name
        CLAY_TEXT(CLAY_ID("CounterName"), CLAY_STRING(counter->label),
                 CLAY_TEXT_CONFIG(.textColor = {0, 0, 0, 1}, .fontSize = 16));
        
        // Main count (large)
        char count_text[32];
        snprintf(count_text, sizeof(count_text), "%d", counter->count);
        CLAY_TEXT(CLAY_ID("MainCount"), CLAY_STRING(count_text),
                 CLAY_TEXT_CONFIG(.textColor = {0, 0, 0, 1}, .fontSize = 48));
        
        // Quality accuracy
        uint32_t total_attempts = counter->perfect_count + counter->good_count + 
                                  counter->partial_count + counter->miss_count;
        
        if (total_attempts > 0) {
            uint32_t good_attempts = counter->perfect_count + counter->good_count;
            float accuracy = (100.0f * good_attempts) / total_attempts;
            
            char accuracy_text[64];
            snprintf(accuracy_text, sizeof(accuracy_text), "Accuracy: %.1f%% (P:%d G:%d B:%d M:%d)",
                    accuracy, counter->perfect_count, counter->good_count,
                    counter->partial_count, counter->miss_count);
            
            CLAY_TEXT(CLAY_ID("AccuracyText"), CLAY_STRING(accuracy_text),
                     CLAY_TEXT_CONFIG(.textColor = {0.5f, 0.5f, 0.5f, 1}, .fontSize = 10));
        }
    }
}

void render_controls_and_status(void) {
    CLAY(CLAY_ID("TopControlsBar"),
         CLAY_LAYOUT(.sizing = {CLAY_SIZING_GROW(), CLAY_SIZING_FIXED(25)},
                     .layoutDirection = CLAY_LEFT_TO_RIGHT,
                     .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER},
                     .padding = {4, 4, 4, 4}),
         CLAY_RECTANGLE(.color = {0.95f, 0.95f, 0.95f, 1.0f}),
         CLAY_BORDER(.width = 1, .color = {0.7f, 0.7f, 0.7f, 1.0f})) {
        
        // Left: Counter navigation
        CLAY_TEXT(CLAY_ID("NavControls"), CLAY_STRING("◀W S▶"),
                 CLAY_TEXT_CONFIG(.textColor = {0, 0, 0, 1}, .fontSize = 10));
        
        // Center: Audio and quality controls
        CLAY(CLAY_ID("CenterControls"),
             CLAY_LAYOUT(.sizing = {CLAY_SIZING_GROW(), CLAY_SIZING_GROW()},
                         .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER})) {
            CLAY_TEXT(CLAY_ID("QualityControls"), CLAY_STRING("P=Perfect G=Good B=Partial M=Miss A=Audio"),
                     CLAY_TEXT_CONFIG(.textColor = {0, 0, 0, 1}, .fontSize = 8));
        }
        
        // Right: Status indicators
        CLAY(CLAY_ID("StatusIndicators"),
             CLAY_LAYOUT(.sizing = {CLAY_SIZING_FIXED(50), CLAY_SIZING_GROW()},
                         .layoutDirection = CLAY_LEFT_TO_RIGHT,
                         .childGap = 4)) {
            render_audio_status_indicator();
            render_bluetooth_status_indicator();
        }
    }
}

void render_complete_combo_ui(void) {
    if (!g_clay_ctx || !g_device) {
        return;
    }
    
    Counter* current_counter = device_get_current_counter(g_device);
    if (!current_counter) {
        return;
    }
    
    // Start Clay frame
    clay_epaper_begin_frame(g_clay_ctx);
    
    // Main container
    CLAY(CLAY_ID("MainContainer"),
         CLAY_LAYOUT(.sizing = {CLAY_SIZING_FIXED(EPAPER_WIDTH), CLAY_SIZING_FIXED(EPAPER_HEIGHT)},
                     .layoutDirection = CLAY_TOP_TO_BOTTOM),
         CLAY_RECTANGLE(.color = {1.0f, 1.0f, 1.0f, 1.0f})) {
        
        // Top controls and status bar
        render_controls_and_status();
        
        // Main content area
        CLAY(CLAY_ID("MainContent"),
             CLAY_LAYOUT(.sizing = {CLAY_SIZING_GROW(), CLAY_SIZING_GROW()},
                         .layoutDirection = CLAY_TOP_TO_BOTTOM,
                         .padding = {8, 8, 8, 8})) {
            
            // Main counter display
            render_main_counter_display(current_counter);
            
            // Combo status (only for combo counters)
            if (current_counter->type == COUNTER_TYPE_COMBO) {
                render_combo_status_display(current_counter);
            }
        }
    }
    
    // End Clay frame
    clay_epaper_end_frame(g_clay_ctx);
}

// ================================
// INPUT HANDLING
// ================================

void handle_complete_combo_input(char key) {
    if (!g_device) return;
    
    Counter* current = device_get_current_counter(g_device);
    if (!current) return;
    
    switch (key) {
        case 'w':
        case 'W':
            device_next_counter(g_device);
            break;
            
        case 's':
        case 'S':
            device_prev_counter(g_device);
            break;
            
        case ' ':
            counter_increment(current, QUALITY_GOOD);
            play_count_audio(current->count);
            play_quality_feedback(QUALITY_GOOD);
            bluetooth_send_counter_update(current);
            break;
            
        case 'p':
        case 'P':
            counter_increment(current, QUALITY_PERFECT);
            play_count_audio(current->count);
            play_quality_feedback(QUALITY_PERFECT);
            bluetooth_send_counter_update(current);
            break;
            
        case 'g':
        case 'G':
            counter_increment(current, QUALITY_GOOD);
            play_count_audio(current->count);
            play_quality_feedback(QUALITY_GOOD);
            bluetooth_send_counter_update(current);
            break;
            
        case 'b':
        case 'B':
            counter_increment(current, QUALITY_PARTIAL);
            play_count_audio(current->count);
            play_quality_feedback(QUALITY_PARTIAL);
            bluetooth_send_counter_update(current);
            break;
            
        case 'm':
        case 'M':
            counter_increment(current, QUALITY_MISS);
            play_quality_feedback(QUALITY_MISS);
            bluetooth_send_counter_update(current);
            break;
            
        case 'a':
        case 'A':
            // Cycle audio modes
            g_audio_config.mode = (g_audio_config.mode + 1) % AUDIO_MODE_MAX;
            break;
            
        case 'v':
        case 'V':
            // Toggle voice counting
            g_audio_config.count_aloud = !g_audio_config.count_aloud;
            break;
            
        case 'c':
        case 'C':
            // Toggle combo announcements
            g_audio_config.combo_announcements = !g_audio_config.combo_announcements;
            break;
            
        case 'x':
        case 'X':
            if (current->count > 0) {
                counter_decrement(current, 1);
                bluetooth_send_counter_update(current);
            }
            break;
    }
}

// ================================
// SYSTEM UPDATE
// ================================

void update_complete_combo_system(float delta_time) {
    // Update celebration timer
    if (g_celebration_timer > 0) {
        g_celebration_timer -= (uint32_t)(delta_time * 1000);
        if (g_celebration_timer <= 0) {
            g_show_combo_celebration = false;
        }
    }
    
    // Update device logic
    combo_device_update(g_device, delta_time);
    
    // Process any ongoing audio
    musicmaker_process();
}

// ================================
// INITIALIZATION
// ================================

bool initialize_complete_combo_system(ClayEpaperContext* clay_ctx, ComboDevice* device) {
    if (!clay_ctx || !device) {
        return false;
    }
    
    g_clay_ctx = clay_ctx;
    g_device = device;
    
    // Initialize audio system
    if (!dual_spi_musicmaker_ready()) {
        // Audio not available, set to silent mode
        g_audio_config.mode = AUDIO_MODE_SILENT;
    }
    
    // Reset state
    g_combo_status = COMBO_STATUS_BUILDING;
    g_last_combo_count = 0;
    g_combo_decay_timer = 0.0f;
    g_show_combo_celebration = false;
    g_celebration_timer = 0;
    
    return true;
}

void print_complete_combo_status(void) {
    printf("=== Complete Combo System Status ===\n");
    printf("Audio Mode: %d (%s)\n", g_audio_config.mode, 
           (g_audio_config.mode == AUDIO_MODE_SILENT) ? "Silent" :
           (g_audio_config.mode == AUDIO_MODE_BEEPS) ? "Beeps" :
           (g_audio_config.mode == AUDIO_MODE_VOICE_CLIPS) ? "Voice" : "Music");
    printf("Count Aloud: %s\n", g_audio_config.count_aloud ? "ON" : "OFF");
    printf("Form Feedback: %s\n", g_audio_config.form_feedback ? "ON" : "OFF");
    printf("Combo Announcements: %s\n", g_audio_config.combo_announ
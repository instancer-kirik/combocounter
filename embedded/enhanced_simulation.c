#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include <sys/time.h>
#include <math.h>
#include <stddef.h>

// Simulate the Nordic SDK types and functions we need
#define NRF_SUCCESS 0
#define APP_ERROR_CHECK(err_code) do { if ((err_code) != NRF_SUCCESS) { printf("Error: %d\n", err_code); exit(1); } } while(0)
#define NRF_LOG_INFO(format, ...) printf("[INFO] " format "\n", ##__VA_ARGS__)
#define NRF_LOG_ERROR(format, ...) printf("[ERROR] " format "\n", ##__VA_ARGS__)
#define NRF_LOG_PROCESS() // No-op in simulation

// Application includes
#include "simple_combo_core.h"
#include "turso_local.h"

// ================================
// ENHANCED FEATURES
// ================================

typedef enum {
    AUDIO_MODE_SILENT = 0,          // No audio
    AUDIO_MODE_BEEPS = 1,           // Simple beep sounds
    AUDIO_MODE_VOICE_CLIPS = 2,     // Voice counting and feedback
    AUDIO_MODE_MUSIC = 3,           // Background music + effects
    AUDIO_MODE_CUSTOM_LABELS = 4,   // User-defined labels
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
    char custom_labels[10][32];   // User-defined counting labels (0-9)
    char quality_labels[4][32];   // Custom quality feedback labels
    bool use_custom_counting;     // Use custom labels for counting
} AudioConfig;

typedef struct {
    // Visual combo meter
    float decay_progress;          // 0.0 to 1.0 (1.0 = about to decay)
    ComboStatus status;
    uint32_t last_combo_time;
    uint32_t time_since_last_rep;
    bool show_multiplier_effect;
    
    // Audio state
    AudioConfig audio;
    bool audio_playing;
    char last_audio_clip[64];
    
    // Enhanced statistics
    uint32_t perfect_reps;
    uint32_t good_reps;
    uint32_t partial_reps;
    uint32_t missed_reps;
    float accuracy_percentage;
    uint32_t longest_combo;
    uint32_t total_workouts;
    
} EnhancedState;

// Timing constants
#define UPDATE_INTERVAL_MS      50     // Faster updates for smooth combo decay
#define DISPLAY_REFRESH_MS      100    // Faster refresh for visual effects
#define SLEEP_TIMEOUT_MS        30000  // 30 seconds to sleep in simulation
#define COMBO_DECAY_INTERVAL_MS 1000   // Combo decays every second

// Screen definitions
typedef enum {
    SCREEN_COUNTER,
    SCREEN_SETTINGS,
    SCREEN_STATS,
    SCREEN_AUDIO_SETTINGS,
    SCREEN_CUSTOM_LABELS,
    SCREEN_CUSTOM_COUNTERS,
    SCREEN_SLEEP
} ScreenType;

// Global state
static ComboDevice g_device;
static EnhancedState g_enhanced;
static ScreenType g_current_screen = SCREEN_COUNTER;
static bool g_running = true;
static struct timeval g_last_interaction;
static struct timeval g_last_display_update;

// Custom labels screen variables
static int g_custom_labels_selection = 0;
static int g_custom_labels_mode = 0; // 0 = counting, 1 = quality
static bool g_editing_label = false;
static char g_temp_label[32] = {0};
static int g_temp_label_pos = 0;

// Custom counters screen variables
static int g_custom_counters_selection = 0;
static bool g_editing_counter_name = false;
static char g_temp_counter_name[MAX_LABEL_LENGTH] = {0};
static int g_temp_counter_name_pos = 0;
static struct timeval g_last_combo_update;
static bool g_display_dirty = true;
static uint8_t g_settings_selection = 0;
static uint8_t g_audio_settings_selection = 0;

// Terminal handling
static struct termios g_old_terminal;

// ================================
// UTILITY FUNCTIONS
// ================================

static uint32_t get_time_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

static uint32_t time_diff_ms(struct timeval *start) {
    struct timeval now;
    gettimeofday(&now, NULL);
    return (now.tv_sec - start->tv_sec) * 1000 + (now.tv_usec - start->tv_usec) / 1000;
}

static void play_system_beep(int frequency, int duration_ms) {
    char cmd[128];
    snprintf(cmd, sizeof(cmd), "aplay -q /usr/share/sounds/alsa/Front_Left.wav 2>/dev/null &");
    system(cmd);
}

static void speak_text(const char* text, bool urgent) {
    char cmd[256];
    // Rally co-driver voice: American English, natural pitch, good resonance
    if (urgent) {
        snprintf(cmd, sizeof(cmd), 
            "espeak-ng -v en-us -s 180 -p 35 -a 75 -g 8 \"%s\" 2>/dev/null &", text);
    } else {
        snprintf(cmd, sizeof(cmd), 
            "espeak-ng -v en-us -s 170 -p 30 -a 70 -g 10 \"%s\" 2>/dev/null &", text);
    }
    system(cmd);
}

static void play_audio_clip(const char* clip_name) {
    if (g_enhanced.audio.mode == AUDIO_MODE_SILENT) {
        return;
    }
    
    strncpy(g_enhanced.last_audio_clip, clip_name, sizeof(g_enhanced.last_audio_clip) - 1);
    g_enhanced.audio_playing = true;
    
    // Parse clip name for rally-style audio
    bool is_number = (clip_name[0] >= '1' && clip_name[0] <= '9' && strlen(clip_name) <= 3);
    bool is_quality = (strstr(clip_name, "Perfect") || strstr(clip_name, "Good") || 
                      strstr(clip_name, "Partial") || strstr(clip_name, "Miss"));
    bool is_combo = strstr(clip_name, "combo") || strstr(clip_name, "Combo");
    bool is_milestone = (strstr(clip_name, "10") || strstr(clip_name, "25") || 
                        strstr(clip_name, "50") || strstr(clip_name, "100"));
    
    // Rally co-driver style audio simulation with actual beeps
    switch (g_enhanced.audio.mode) {
        case AUDIO_MODE_BEEPS:
            if (is_number) {
                int num = atoi(clip_name);
                play_system_beep(400 + (num * 50), 150);
                printf("🔊 *BEEP* (%d)\n", num);
            } else if (is_quality) {
                play_system_beep(600, 100);
                printf("🔊 *CONFIRMATION BEEP*\n");
            } else {
                play_system_beep(300, 200);
                printf("🔊 *SYSTEM BEEP*\n");
            }
            break;
            
        case AUDIO_MODE_VOICE_CLIPS:
            if (is_number) {
                int num = atoi(clip_name);
                char speech[32];
                snprintf(speech, sizeof(speech), "%d!", num);
                speak_text(speech, false);
                printf("🎤 🏁 TTS: \"%d!\" (Rally Co-Driver - American)\n", num);
            } else if (strstr(clip_name, "Perfect")) {
                speak_text("Perfect form!", true);
                printf("🎤 🏁 TTS: \"Perfect form!\" (Encouraging)\n");
            } else if (strstr(clip_name, "Good")) {
                speak_text("Good rep!", false);
                printf("🎤 🏁 TTS: \"Good rep!\" (Steady)\n");
            } else if (strstr(clip_name, "Partial")) {
                speak_text("Keep pushing!", true);
                printf("🎤 🏁 TTS: \"Keep pushing!\" (Motivational)\n");
            } else if (strstr(clip_name, "Miss")) {
                speak_text("Reset and go!", false);
                printf("🎤 🏁 TTS: \"Reset and go!\" (Firm)\n");
            } else if (is_combo && is_milestone) {
                if (strstr(clip_name, "10")) {
                    speak_text("Ten combo! Building momentum!", true);
                    printf("🎤 🏁 TTS: \"Ten combo! Building momentum!\" (Excited)\n");
                } else if (strstr(clip_name, "25")) {
                    speak_text("Twenty five combo! On fire!", true);
                    printf("🎤 🏁 TTS: \"Twenty five combo! On fire!\" (Very excited)\n");
                } else if (strstr(clip_name, "50")) {
                    speak_text("Fifty combo! Unstoppable!", true);
                    printf("🎤 🏁 TTS: \"Fifty combo! Unstoppable!\" (Intense)\n");
                } else if (strstr(clip_name, "100")) {
                    speak_text("Hundred combo! Legendary!", true);
                    printf("🎤 🏁 TTS: \"Hundred combo! Legendary!\" (Peak excitement)\n");
                }
            } else if (strstr(clip_name, "Broken")) {
                speak_text("Combo broken! Back to one!", false);
                printf("🎤 🏁 TTS: \"Combo broken! Back to one!\" (Matter-of-fact)\n");
            } else if (strstr(clip_name, "record")) {
                speak_text("New personal record! Outstanding!", true);
                printf("🎤 🏁 TTS: \"New personal record! Outstanding!\" (Triumphant)\n");
            } else {
                speak_text(clip_name, false);
                printf("🎤 🏁 TTS: \"%s\" (Rally co-driver)\n", clip_name);
            }
            break;
            
        case AUDIO_MODE_CUSTOM_LABELS:
            if (is_number) {
                int num = atoi(clip_name);
                if (num > 0 && num <= 10 && g_enhanced.audio.use_custom_counting) {
                    speak_text(g_enhanced.audio.custom_labels[num-1], false);
                    printf("✏️ 🏁 CUSTOM: \"%s\" (User-defined #%d)\n", g_enhanced.audio.custom_labels[num-1], num);
                } else {
                    char speech[32];
                    snprintf(speech, sizeof(speech), "%d!", num);
                    speak_text(speech, false);
                    printf("✏️ 🏁 DEFAULT: \"%d!\" (No custom label set)\n", num);
                }
            } else if (strstr(clip_name, "Perfect") && strlen(g_enhanced.audio.quality_labels[0]) > 0) {
                speak_text(g_enhanced.audio.quality_labels[0], true);
                printf("✏️ 🏁 CUSTOM: \"%s\" (Custom Perfect)\n", g_enhanced.audio.quality_labels[0]);
            } else if (strstr(clip_name, "Good") && strlen(g_enhanced.audio.quality_labels[1]) > 0) {
                speak_text(g_enhanced.audio.quality_labels[1], false);
                printf("✏️ 🏁 CUSTOM: \"%s\" (Custom Good)\n", g_enhanced.audio.quality_labels[1]);
            } else if (strstr(clip_name, "Partial") && strlen(g_enhanced.audio.quality_labels[2]) > 0) {
                speak_text(g_enhanced.audio.quality_labels[2], true);
                printf("✏️ 🏁 CUSTOM: \"%s\" (Custom Partial)\n", g_enhanced.audio.quality_labels[2]);
            } else if (strstr(clip_name, "Miss") && strlen(g_enhanced.audio.quality_labels[3]) > 0) {
                speak_text(g_enhanced.audio.quality_labels[3], false);
                printf("✏️ 🏁 CUSTOM: \"%s\" (Custom Miss)\n", g_enhanced.audio.quality_labels[3]);
            } else {
                // Fall back to default voice clips
                speak_text(clip_name, false);
                printf("✏️ 🏁 DEFAULT: \"%s\" (No custom label)\n", clip_name);
            }
            break;
            
        case AUDIO_MODE_MUSIC:
            if (is_number) {
                int num = atoi(clip_name);
                char speech[32];
                snprintf(speech, sizeof(speech), "%d!", num);
                speak_text(speech, true);
                play_system_beep(600, 100);
                printf("🎵 🏁 [PHONK BEAT] + TTS: \"%d!\" (Over heavy bass)\n", num);
            } else if (is_quality) {
                speak_text(clip_name, true);
                play_system_beep(800, 150);
                printf("🎵 🏁 [BEAT DROP] + TTS: \"%s\" (Synced with 808s)\n", clip_name);
            } else {
                speak_text(clip_name, false);
                printf("🎵 🏁 [MUSIC] TTS: \"%s\" (Rally voice over phonk)\n", clip_name);
            }
            break;
            
        default:
            break;
    }
    
    // Brief delay to prevent audio overlap
    usleep(50000); // 50ms delay
    g_enhanced.audio_playing = false;
}

static void update_combo_status(Counter* counter) {
    uint32_t current_time = get_time_ms();
    
    if (counter->type != COUNTER_TYPE_COMBO) {
        return;
    }
    
    // Calculate time since last rep
    g_enhanced.time_since_last_rep = current_time - g_enhanced.last_combo_time;
    
    // Calculate decay progress (assuming 10 second decay time)
    const uint32_t decay_time_ms = 10000;
    g_enhanced.decay_progress = (float)g_enhanced.time_since_last_rep / decay_time_ms;
    
    if (g_enhanced.decay_progress > 1.0f) {
        g_enhanced.decay_progress = 1.0f;
    }
    
    // Update combo status
    if (counter->count == 0) {
        g_enhanced.status = COMBO_STATUS_BUILDING;
    } else if (g_enhanced.decay_progress < 0.3f) {
        g_enhanced.status = COMBO_STATUS_BUILDING;
    } else if (g_enhanced.decay_progress < 0.7f) {
        g_enhanced.status = COMBO_STATUS_HOLDING;
    } else if (g_enhanced.decay_progress < 1.0f) {
        g_enhanced.status = COMBO_STATUS_DECAYING;
    } else {
        if (counter->count > 0) {
            g_enhanced.status = COMBO_STATUS_BROKEN;
            if (g_enhanced.audio.combo_announcements) {
                play_audio_clip("Combo Broken!");
            }
        }
    }
    
    // Check for new record
    if ((uint32_t)counter->count > g_enhanced.longest_combo) {
        g_enhanced.longest_combo = counter->count;
        g_enhanced.status = COMBO_STATUS_NEW_RECORD;
        if (g_enhanced.audio.combo_announcements) {
            play_audio_clip("New Record!");
        }
    }
}

static void update_statistics(Counter* counter, ActionQuality quality) {
    switch (quality) {
        case QUALITY_PERFECT:
            g_enhanced.perfect_reps++;
            break;
        case QUALITY_GOOD:
            g_enhanced.good_reps++;
            break;
        case QUALITY_PARTIAL:
            g_enhanced.partial_reps++;
            break;
        case QUALITY_MISS:
            g_enhanced.missed_reps++;
            break;
    }
    
    uint32_t total_reps = g_enhanced.perfect_reps + g_enhanced.good_reps + 
                         g_enhanced.partial_reps + g_enhanced.missed_reps;
    if (total_reps > 0) {
        float good_reps = g_enhanced.perfect_reps + g_enhanced.good_reps;
        g_enhanced.accuracy_percentage = (good_reps / total_reps) * 100.0f;
    }
}

// ================================
// TERMINAL HANDLING
// ================================

static void setup_terminal(void) {
    tcgetattr(STDIN_FILENO, &g_old_terminal);
    struct termios new_terminal = g_old_terminal;
    new_terminal.c_lflag &= ~(ICANON | ECHO);
    new_terminal.c_cc[VMIN] = 1;  // Changed from 0 to 1 for better key handling
    new_terminal.c_cc[VTIME] = 1; // Changed from 0 to 1 for timeout
    tcsetattr(STDIN_FILENO, TCSANOW, &new_terminal);
    // Don't set O_NONBLOCK for better arrow key handling
    // fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK);
}

static void restore_terminal(void) {
    tcsetattr(STDIN_FILENO, TCSANOW, &g_old_terminal);
}

static char get_key(void) {
    char c = 0;
    ssize_t bytes_read = read(STDIN_FILENO, &c, 1);
    
    if (bytes_read == 1) {
        gettimeofday(&g_last_interaction, NULL);
        
        // Handle arrow key sequences
        if (c == 27) { // ESC sequence
            char seq[2];
            // Set a small timeout for reading the sequence
            struct termios old_term;
            tcgetattr(STDIN_FILENO, &old_term);
            struct termios new_term = old_term;
            new_term.c_cc[VTIME] = 1; // 0.1 second timeout
            new_term.c_cc[VMIN] = 0;
            tcsetattr(STDIN_FILENO, TCSANOW, &new_term);
            
            if (read(STDIN_FILENO, seq, 2) == 2) {
                if (seq[0] == '[') {
                    // Restore terminal settings
                    tcsetattr(STDIN_FILENO, TCSANOW, &old_term);
                    switch (seq[1]) {
                        case 'A': return 'w'; // Up arrow -> w
                        case 'B': return 's'; // Down arrow -> s
                        case 'C': return 'd'; // Right arrow -> d
                        case 'D': return 'a'; // Left arrow -> a
                    }
                }
            }
            
            // Restore terminal settings
            tcsetattr(STDIN_FILENO, TCSANOW, &old_term);
            return 27; // Return ESC if not arrow keys
        }
        return c;
    }
    return 0;
}

// ================================
// VISUAL ELEMENTS
// ================================

static void draw_combo_meter(Counter* counter) {
    if (counter->type != COUNTER_TYPE_COMBO) {
        return;
    }
    
    printf("\n");
    
    // Combo status indicator
    const char* status_text[] = {
        "🔥 BUILDING", "💪 HOLDING", "⚠️  DECAYING", "💥 BROKEN", "🏆 RECORD!"
    };
    const char* status_colors[] = {
        "\033[32m", "\033[33m", "\033[31m", "\033[91m", "\033[35m"
    };
    
    printf("%s%s\033[0m  ", status_colors[g_enhanced.status], status_text[g_enhanced.status]);
    
    // Draw decay bar
    printf("Combo Decay: [");
    int bar_width = 20;
    int filled = (int)(g_enhanced.decay_progress * bar_width);
    
    for (int i = 0; i < bar_width; i++) {
        if (i < filled) {
            if (g_enhanced.decay_progress < 0.3f) {
                printf("\033[32m█\033[0m"); // Green
            } else if (g_enhanced.decay_progress < 0.7f) {
                printf("\033[33m█\033[0m"); // Yellow
            } else {
                printf("\033[31m█\033[0m"); // Red
            }
        } else {
            printf("░");
        }
    }
    printf("] %.1f%%\n", g_enhanced.decay_progress * 100);
    
    // Multiplier effect visualization
    if (counter->multiplier > 1.0f) {
        printf("\n🚀 MULTIPLIER: ");
        int mult_bars = (int)((counter->multiplier - 1.0f) / (counter->max_multiplier - 1.0f) * 10);
        printf("[");
        for (int i = 0; i < 10; i++) {
            if (i < mult_bars) {
                printf("\033[35m▰\033[0m");
            } else {
                printf("▱");
            }
        }
        printf("] ×%.2f\n", counter->multiplier);
    }
}

static void draw_audio_indicator(void) {
    const char* mode_icons[] = {"🔇", "🔊", "🎤", "🎵"};
    const char* mode_names[] = {"Silent", "Beeps", "Voice", "Music"};
    
    printf("Audio: %s %s", mode_icons[g_enhanced.audio.mode], mode_names[g_enhanced.audio.mode]);
    
    if (g_enhanced.audio.mode != AUDIO_MODE_SILENT) {
        printf(" (Vol: %d)", g_enhanced.audio.volume);
    }
    
    if (g_enhanced.audio_playing) {
        printf(" 🎶 Playing: %s", g_enhanced.last_audio_clip);
        g_enhanced.audio_playing = false; // Reset for next frame
    }
    
    printf("\n");
}

// ================================
// SCREEN RENDERING
// ================================

static void render_counter_screen(void) {
    if (g_device.counter_count == 0) {
        printf("No counters configured\n");
        return;
    }
    
    printf("📊 MULTI-TRACKER DASHBOARD\n\n");
    
    // Show all trackers in a grid layout
    printf("┌─────────────────────────────────────────────────────────────┐\n");
    printf("│                      ALL TRACKERS                          │\n");
    printf("├─────────────────────────────────────────────────────────────┤\n");
    
    for (int i = 0; i < g_device.counter_count; i++) {
        Counter* counter = &g_device.counters[i];
        bool is_selected = (i == g_device.current_counter);
        
        // Highlight selected tracker
        if (is_selected) {
            printf("│ ► \033[1;33m[%d]\033[0m ", i + 1);
        } else {
            printf("│   \033[2m[%d]\033[0m ", i + 1);
        }
        
        // Tracker name and count with colors
        if (is_selected) {
            printf("\033[1;36m%-12s\033[0m: \033[1;32m%3d\033[0m", counter->label, counter->count);
        } else {
            printf("%-12s: %3d", counter->label, counter->count);
        }
        
        // Total and type info
        printf(" (Total: %d)", counter->total);
        
        // Show type abbreviated
        switch (counter->type) {
            case COUNTER_TYPE_SIMPLE: printf(" [Simple]"); break;
            case COUNTER_TYPE_COMBO: printf(" [Combo×%.1f]", counter->multiplier); break;
            case COUNTER_TYPE_TIMED: printf(" [Timed]"); break;
            case COUNTER_TYPE_ACCUMULATOR: printf(" [Accum]"); break;
        }
        
        // Fill remaining space
        printf("%*s│\n", (int)(20 - strlen(counter->label)), "");
    }
    
    printf("├─────────────────────────────────────────────────────────────┤\n");
    printf("│ QUICK ACTIONS: Press 1-9 to +1 to that tracker            │\n");
    printf("│ Selected: \033[1;33m%s\033[0m (Use SPACE to +1, ↑↓ to switch)    │\n", 
           g_device.counters[g_device.current_counter].label);
    
    // Quality breakdown for selected counter
    Counter* current = &g_device.counters[g_device.current_counter];
    if (g_enhanced.perfect_reps + g_enhanced.good_reps + g_enhanced.partial_reps + g_enhanced.missed_reps > 0) {
        printf("│ Quality: \033[32mP:%d\033[0m \033[33mG:%d\033[0m \033[31mPa:%d\033[0m \033[91mM:%d\033[0m Acc:\033[1;36m%.1f%%\033[0m        │\n",
               g_enhanced.perfect_reps, g_enhanced.good_reps, 
               g_enhanced.partial_reps, g_enhanced.missed_reps, g_enhanced.accuracy_percentage);
    }
    printf("└─────────────────────────────────────────────────────────────┘\n");
    
    printf("\nType of selected: ");
    switch (current->type) {
        case COUNTER_TYPE_SIMPLE:
            printf("Simple (+1 per action)");
            break;
        case COUNTER_TYPE_TIMED:
            printf("Timed (bonus for speed)");
            break;
        case COUNTER_TYPE_COMBO:
            printf("Combo (max ×%.1f, decay %.1f)", current->max_multiplier, current->decay_rate);
            break;
        case COUNTER_TYPE_ACCUMULATOR:
            printf("Accumulator (additive counting)");
            break;
    }
    printf("\n");
    
    // Enhanced combo meter
    draw_combo_meter(current);
    
    // Audio status
    printf("\n");
    draw_audio_indicator();
    
    printf("\n");
}

static void render_settings_screen(void) {
    printf("⚙️  SETTINGS\n\n");
    
    const char* settings[] = {
        "Audio Settings",
        "Custom Counters",
        "Reset Statistics", 
        "Reset All Counters",
        "Back to Counter"
    };
    
    for (int i = 0; i < 5; i++) {
        if (i == g_settings_selection) {
            printf("▶ \033[1;33m%s\033[0m\n", settings[i]);
        } else {
            printf("  %s\n", settings[i]);
        }
    }
}

static void render_audio_settings_screen(void) {
    printf("🎵 AUDIO SETTINGS\n\n");
    
    const char* audio_settings[] = {
        "Audio Mode",
        "Volume",
        "Count Aloud",
        "Form Feedback", 
        "Combo Announcements",
        "Milestone Sounds",
        "Custom Labels",
        "Back"
    };
    
    const char* mode_names[] = {"Silent", "Beeps", "Voice", "Music", "Custom"};
    
    for (int i = 0; i < 8; i++) {
        if (i == g_audio_settings_selection) {
            printf("► ");
        } else {
            printf("  ");
        }
        
        printf("%s", audio_settings[i]);
        
        switch (i) {
            case 0:
                printf(": %s", mode_names[g_enhanced.audio.mode]);
                break;
            case 1:
                printf(": %d", g_enhanced.audio.volume);
                break;
            case 2:
                printf(": %s", g_enhanced.audio.count_aloud ? "ON" : "OFF");
                break;
            case 3:
                printf(": %s", g_enhanced.audio.form_feedback ? "ON" : "OFF");
                break;
            case 4:
                printf(": %s", g_enhanced.audio.combo_announcements ? "ON" : "OFF");
                break;
            case 5:
                printf(": %s", g_enhanced.audio.milestone_sounds ? "ON" : "OFF");
                break;
            case 6:
                if (g_enhanced.audio.mode == AUDIO_MODE_CUSTOM_LABELS) {
                    printf(": Configure");
                } else {
                    printf(": (Available in Custom mode)");
                }
                break;
        }
        printf("\n");
    }
    
    printf("\nControls: ↑↓=Navigate  SPACE=Toggle  Q=Back\n");
}

static void render_custom_labels_screen(void) {
    printf("✏️ CUSTOM LABELS\n\n");
    
    if (g_custom_labels_mode == 0) {
        printf("📢 COUNTING LABELS (1-10)\n");
        printf("Current mode: %s\n\n", g_enhanced.audio.use_custom_counting ? "Custom" : "Default numbers");
        
        for (int i = 0; i < 10; i++) {
            if (i == g_custom_labels_selection && !g_editing_label) {
                printf("► ");
            } else if (i == g_custom_labels_selection && g_editing_label) {
                printf("✏ ");
            } else {
                printf("  ");
            }
            
            printf("%d: ", i + 1);
            
            if (g_editing_label && i == g_custom_labels_selection) {
                printf("\"%s_\"", g_temp_label);
            } else if (strlen(g_enhanced.audio.custom_labels[i]) > 0) {
                printf("\"%s\"", g_enhanced.audio.custom_labels[i]);
            } else {
                printf("(default: \"%d!\")", i + 1);
            }
            printf("\n");
        }
        
        printf("\n► Toggle Mode: %s\n", g_enhanced.audio.use_custom_counting ? "Using Custom" : "Using Default");
        printf("► Switch to Quality Labels\n");
        printf("► Back\n");
        
    } else {
        printf("🎯 QUALITY LABELS\n\n");
        const char* quality_names[] = {"Perfect", "Good", "Partial", "Miss"};
        const char* quality_defaults[] = {"Perfect form!", "Good rep!", "Keep pushing!", "Reset and go!"};
        
        for (int i = 0; i < 4; i++) {
            if (i == g_custom_labels_selection && !g_editing_label) {
                printf("► ");
            } else if (i == g_custom_labels_selection && g_editing_label) {
                printf("✏ ");
            } else {
                printf("  ");
            }
            
            printf("%s: ", quality_names[i]);
            
            if (g_editing_label && i == g_custom_labels_selection) {
                printf("\"%s_\"", g_temp_label);
            } else if (strlen(g_enhanced.audio.quality_labels[i]) > 0) {
                printf("\"%s\"", g_enhanced.audio.quality_labels[i]);
            } else {
                printf("(default: \"%s\")", quality_defaults[i]);
            }
            printf("\n");
        }
        
        printf("\n► Switch to Counting Labels\n");
        printf("► Back\n");
    }
    
    if (!g_editing_label) {
        printf("\nControls: ↑↓=Navigate  ENTER=Edit  SPACE=Toggle  Q=Back  TAB=Switch Mode\n");
    } else {
        printf("\nEditing: Type text, ENTER=Save, ESC=Cancel, BACKSPACE=Delete\n");
    }
}

static void render_custom_counters_screen(void) {
    printf("📝 CUSTOM COUNTERS\n\n");
    
    printf("Current Counters:\n");
    for (int i = 0; i < g_device.counter_count; i++) {
        Counter* counter = &g_device.counters[i];
        if (i == g_custom_counters_selection && !g_editing_counter_name) {
            printf("► ");
        } else if (i == g_custom_counters_selection && g_editing_counter_name) {
            printf("✏ ");
        } else {
            printf("  ");
        }
        
        if (g_editing_counter_name && i == g_custom_counters_selection) {
            printf("\"%s_\" [%s] - Count: %d\n", g_temp_counter_name, 
                   counter->type == COUNTER_TYPE_SIMPLE ? "Simple" :
                   counter->type == COUNTER_TYPE_COMBO ? "Combo" :
                   counter->type == COUNTER_TYPE_TIMED ? "Timed" : "Accumulator",
                   counter->count);
        } else {
            printf("\"%s\" [%s] - Count: %d\n", counter->label,
                   counter->type == COUNTER_TYPE_SIMPLE ? "Simple" :
                   counter->type == COUNTER_TYPE_COMBO ? "Combo" :
                   counter->type == COUNTER_TYPE_TIMED ? "Timed" : "Accumulator",
                   counter->count);
        }
    }
    
    if (g_device.counter_count < MAX_COUNTERS) {
        if (g_device.counter_count == g_custom_counters_selection) {
            printf("► ");
        } else {
            printf("  ");
        }
        printf("+ Add New Counter\n");
        
        if (g_device.counter_count + 1 == g_custom_counters_selection) {
            printf("► ");
        } else {
            printf("  ");
        }
        printf("Delete Selected Counter\n");
    }
    
    printf("\n► Back\n");
    
    if (!g_editing_counter_name) {
        printf("\nControls: ↑↓=Navigate  ENTER=Edit/Add  D=Delete  Q=Back\n");
    } else {
        printf("\nEditing: Type name, ENTER=Save, ESC=Cancel, BACKSPACE=Delete\n");
    }
}

static void render_stats_screen(void) {
    printf("📈 ENHANCED STATISTICS\n\n");
    
    printf("Session Stats:\n");
    printf("  Total Reps: %d\n", g_enhanced.perfect_reps + g_enhanced.good_reps + 
                                 g_enhanced.partial_reps + g_enhanced.missed_reps);
    printf("  Accuracy: %.1f%%\n", g_enhanced.accuracy_percentage);
    printf("  Longest Combo: %d\n", g_enhanced.longest_combo);
    
    printf("\nQuality Breakdown:\n");
    printf("  🟢 Perfect: %d\n", g_enhanced.perfect_reps);
    printf("  🟡 Good: %d\n", g_enhanced.good_reps);
    printf("  🟠 Partial: %d\n", g_enhanced.partial_reps);
    printf("  🔴 Missed: %d\n", g_enhanced.missed_reps);
    
    printf("\nAll-Time Records:\n");
    for (int i = 0; i < g_device.counter_count; i++) {
        Counter* counter = &g_device.counters[i];
        printf("  %s: %d (total)\n", counter->label, counter->total);
    }
    
    printf("\nWorkouts Completed: %d\n", g_enhanced.total_workouts);
}

static void render_sleep_screen(void) {
    printf("💤 SLEEP MODE 💤\n");
    printf("Press any key to wake up...\n");
}

static void render_screen(void) {
    // Clear screen with beautiful header
    printf("\033[2J\033[H");
    printf("╭─────────────────────────────────────────────────────────────╮\n");
    printf("│              \033[1;35mEnhanced Combo Chracker Pro\033[0m               │\n");
    printf("╰─────────────────────────────────────────────────────────────╯\n\n");
    
    switch (g_current_screen) {
        case SCREEN_COUNTER:
            render_counter_screen();
            break;
        case SCREEN_SETTINGS:
            render_settings_screen();
            break;
        case SCREEN_AUDIO_SETTINGS:
            render_audio_settings_screen();
            break;
        case SCREEN_CUSTOM_LABELS:
            render_custom_labels_screen();
            break;
        case SCREEN_CUSTOM_COUNTERS:
            render_custom_counters_screen();
            break;
        case SCREEN_STATS:
            render_stats_screen();
            break;
        case SCREEN_SLEEP:
            render_sleep_screen();
            break;
    }
    
    // Controls
    printf("\n╭─────────────────────────────────────────────────────────────╮\n");
    switch (g_current_screen) {
        case SCREEN_COUNTER:
            printf("│ ↑↓=Switch  SPACE=+1 Selected  1-9=+1 Direct  [A]udio [H]elp│\n");
            printf("│ Quality: [P]erfect [G]ood [B]ad [M]iss  [Q]uit             │\n");
            break;
        case SCREEN_SETTINGS:
        case SCREEN_AUDIO_SETTINGS:
        case SCREEN_CUSTOM_LABELS:
        case SCREEN_CUSTOM_COUNTERS:
            printf("│ Controls: ↑↓=Navigate  SPACE=Select  ESC=Back        Q=Quit│\n");
            break;
        case SCREEN_STATS:
            printf("│ Controls: ESC=Back  R=Reset Stats                    Q=Quit│\n");
            break;
        case SCREEN_SLEEP:
            printf("│ Controls: Any key=Wake                               Q=Quit│\n");
            break;
    }
    printf("╰─────────────────────────────────────────────────────────────╯\n");
    
    fflush(stdout);
}

// ================================
// INPUT HANDLING
// ================================

static void handle_counter_input(char key) {
    Counter* current = device_get_current_counter(&g_device);
    if (!current) return;
    
    ActionQuality quality = QUALITY_GOOD;  // Default quality
    
    switch (key) {
        case 'w':
        case 'W':
        case 'a':  // Up arrow mapped to 'w', left arrow to 'a'
            device_prev_counter(&g_device);
            g_display_dirty = true;
            break;
            
        case 's':
        case 'S':
        case 'd':  // Down arrow mapped to 's', right arrow to 'd'
            device_next_counter(&g_device);
            g_display_dirty = true;
            break;
            
        case ' ':  // Space for increment
            counter_increment(current, QUALITY_GOOD);
            update_statistics(current, QUALITY_GOOD);
    
            // Save counter to database (batched writes for energy efficiency)
            turso_save_counter(current, false);
    
            if (g_enhanced.audio.count_aloud) {
                char count_audio[32];
                snprintf(count_audio, sizeof(count_audio), "%d", current->count);
                play_audio_clip(count_audio);
            }
            gettimeofday(&g_last_interaction, NULL);
            g_display_dirty = true;
            break;
            
        // Quick increment by tracker number (1-9)
        case '1': case '2': case '3': case '4': case '5':
        case '6': case '7': case '8': case '9': {
            int tracker_num = key - '1';
            if (tracker_num < g_device.counter_count) {
                Counter* target = &g_device.counters[tracker_num];
                counter_increment(target, QUALITY_GOOD);
                turso_save_counter(target, false);
                
                // Quick visual feedback without clearing screen
                printf("\r✅ +1 to %s (now: %d)                    ", target->label, target->count);
                fflush(stdout);
                
                // Audio feedback for the specific tracker
                if (g_enhanced.audio.count_aloud) {
                    char count_audio[32];
                    snprintf(count_audio, sizeof(count_audio), "%d", target->count);
                    play_audio_clip(count_audio);
                }
                
                gettimeofday(&g_last_interaction, NULL);
                g_display_dirty = true;
                
                // Brief pause to show the feedback, then refresh
                usleep(300000); // 300ms
            }
            break;
        }
            
        case 'p':
        case 'P':  // Perfect quality
            quality = QUALITY_PERFECT;
            counter_increment(current, quality);
            update_statistics(current, quality);
            if (g_enhanced.audio.form_feedback) {
                play_audio_clip("Perfect!");
            }
            g_enhanced.last_combo_time = get_time_ms();
            g_display_dirty = true;
            break;
            
        case 'g':
        case 'G':  // Good quality
            quality = QUALITY_GOOD;
            counter_increment(current, quality);
            update_statistics(current, quality);
            if (g_enhanced.audio.form_feedback) {
                play_audio_clip("Good!");
            }
            g_enhanced.last_combo_time = get_time_ms();
            g_display_dirty = true;
            break;
            
        case 'b':
        case 'B':  // Partial quality
            quality = QUALITY_PARTIAL;
            counter_increment(current, quality);
            update_statistics(current, quality);
            if (g_enhanced.audio.form_feedback) {
                play_audio_clip("Partial");
            }
            g_enhanced.last_combo_time = get_time_ms();
            g_display_dirty = true;
            break;
            
        case 'm':
        case 'M':  // Miss (breaks combo)
            quality = QUALITY_MISS;
            counter_increment(current, quality);
            update_statistics(current, quality);
            if (g_enhanced.audio.form_feedback) {
                play_audio_clip("Miss!");
            }
            // Don't update combo time on miss - let it decay naturally
            g_display_dirty = true;
            break;
            
        case 'x':
        case 'X':
            counter_decrement(current, 1);
            g_display_dirty = true;
            break;
            
        case 'A':
            g_current_screen = SCREEN_AUDIO_SETTINGS;
            g_display_dirty = true;
            break;
            
        case 'h':
        case 'H':
            g_current_screen = SCREEN_SETTINGS;
            g_settings_selection = 0;
            g_display_dirty = true;
            break;
            
        case 'i':
        case 'I':
            g_current_screen = SCREEN_STATS;
            g_display_dirty = true;
            break;
    }
}

static void handle_settings_input(char key) {
    switch (key) {
        case 'w':
        case 'W':
            if (g_settings_selection > 0) {
                g_settings_selection--;
                g_display_dirty = true;
            }
            break;
            
        case 's':
        case 'S':
            if (g_settings_selection < 4) {
                g_settings_selection++;
                g_display_dirty = true;
            }
            break;
            
        case ' ':  // Space for select
            switch (g_settings_selection) {
                case 0:  // Audio Settings
                    g_current_screen = SCREEN_AUDIO_SETTINGS;
                    break;
                case 1:  // Custom Counters
                    g_current_screen = SCREEN_CUSTOM_COUNTERS;
                    break;
                case 2:  // Reset Statistics
                    memset(&g_enhanced.perfect_reps, 0, sizeof(g_enhanced) - 
                           offsetof(EnhancedState, perfect_reps));
                    break;
                case 3:  // Reset All Counters
                    for (int i = 0; i < g_device.counter_count; i++) {
                        counter_reset(&g_device.counters[i]);
                    }
                    break;
                case 4:  // Back
                    g_current_screen = SCREEN_COUNTER;
                    break;
            }
            g_display_dirty = true;
            break;
            
        case '\033':  // ESC
        case 'b':
        case 'B':
            g_current_screen = SCREEN_COUNTER;
            g_display_dirty = true;
            break;
    }
}

static void handle_custom_labels_input(char key) {
    if (g_editing_label) {
        switch (key) {
            case '\n':
            case '\r':  // Save
                if (g_custom_labels_mode == 0) {
                    strncpy(g_enhanced.audio.custom_labels[g_custom_labels_selection], g_temp_label, sizeof(g_enhanced.audio.custom_labels[0]) - 1);
                    g_enhanced.audio.custom_labels[g_custom_labels_selection][sizeof(g_enhanced.audio.custom_labels[0]) - 1] = '\0';
                } else {
                    strncpy(g_enhanced.audio.quality_labels[g_custom_labels_selection], g_temp_label, sizeof(g_enhanced.audio.quality_labels[0]) - 1);
                    g_enhanced.audio.quality_labels[g_custom_labels_selection][sizeof(g_enhanced.audio.quality_labels[0]) - 1] = '\0';
                }
                g_editing_label = false;
                g_temp_label[0] = '\0';
                g_temp_label_pos = 0;
                g_display_dirty = true;
                break;
                
            case 27:  // ESC - Cancel
                g_editing_label = false;
                g_temp_label[0] = '\0';
                g_temp_label_pos = 0;
                g_display_dirty = true;
                break;
                
            case 127:  // Backspace
                if (g_temp_label_pos > 0) {
                    g_temp_label_pos--;
                    g_temp_label[g_temp_label_pos] = '\0';
                    g_display_dirty = true;
                }
                break;
                
            default:
                if (key >= 32 && key <= 126 && g_temp_label_pos < sizeof(g_temp_label) - 2) {
                    g_temp_label[g_temp_label_pos] = key;
                    g_temp_label_pos++;
                    g_temp_label[g_temp_label_pos] = '\0';
                    g_display_dirty = true;
                }
                break;
        }
        return;
    }
    
    switch (key) {
        case 'w':
        case 'W':
            if (g_custom_labels_selection > 0) {
                g_custom_labels_selection--;
                g_display_dirty = true;
            }
            break;
            
        case 's':
        case 'S':
            if (g_custom_labels_mode == 0) {
                if (g_custom_labels_selection < 12) {  // 10 labels + toggle + switch + back
                    g_custom_labels_selection++;
                    g_display_dirty = true;
                }
            } else {
                if (g_custom_labels_selection < 5) {  // 4 labels + switch + back
                    g_custom_labels_selection++;
                    g_display_dirty = true;
                }
            }
            break;
            
        case '\n':
        case '\r':  // Start editing
            if (g_custom_labels_mode == 0) {
                if (g_custom_labels_selection < 10) {
                    g_editing_label = true;
                    strncpy(g_temp_label, g_enhanced.audio.custom_labels[g_custom_labels_selection], sizeof(g_temp_label) - 1);
                    g_temp_label_pos = strlen(g_temp_label);
                    g_display_dirty = true;
                } else if (g_custom_labels_selection == 10) {
                    g_enhanced.audio.use_custom_counting = !g_enhanced.audio.use_custom_counting;
                    g_display_dirty = true;
                } else if (g_custom_labels_selection == 11) {
                    g_custom_labels_mode = 1;
                    g_custom_labels_selection = 0;
                    g_display_dirty = true;
                } else if (g_custom_labels_selection == 12) {
                    g_current_screen = SCREEN_AUDIO_SETTINGS;
                    g_display_dirty = true;
                }
            } else {
                if (g_custom_labels_selection < 4) {
                    g_editing_label = true;
                    strncpy(g_temp_label, g_enhanced.audio.quality_labels[g_custom_labels_selection], sizeof(g_temp_label) - 1);
                    g_temp_label_pos = strlen(g_temp_label);
                    g_display_dirty = true;
                } else if (g_custom_labels_selection == 4) {
                    g_custom_labels_mode = 0;
                    g_custom_labels_selection = 0;
                    g_display_dirty = true;
                } else if (g_custom_labels_selection == 5) {
                    g_current_screen = SCREEN_AUDIO_SETTINGS;
                    g_display_dirty = true;
                }
            }
            break;
            
        case '\t':  // Tab to switch modes
            g_custom_labels_mode = !g_custom_labels_mode;
            g_custom_labels_selection = 0;
            g_display_dirty = true;
            break;
            
        case 'q':
        case 'Q':
        case 27:  // ESC
            g_current_screen = SCREEN_AUDIO_SETTINGS;
            g_display_dirty = true;
            break;
    }
}

static void handle_audio_settings_input(char key) {
    switch (key) {
        case 'w':
        case 'W':
            if (g_audio_settings_selection > 0) {
                g_audio_settings_selection--;
                g_display_dirty = true;
            }
            break;
            
        case 's':
        case 'S':
            if (g_audio_settings_selection < 7) {
                g_audio_settings_selection++;
                g_display_dirty = true;
            }
            break;
            
        case ' ':  // Space for select/toggle
            switch (g_audio_settings_selection) {
                case 0:  // Audio Mode
                    g_enhanced.audio.mode = (g_enhanced.audio.mode + 1) % AUDIO_MODE_MAX;
                    break;
                case 1:  // Volume
                    g_enhanced.audio.volume = (g_enhanced.audio.volume + 1) % 11;
                    break;
                case 2:  // Count Aloud
                    g_enhanced.audio.count_aloud = !g_enhanced.audio.count_aloud;
                    break;
                case 3:  // Form Feedback
                    g_enhanced.audio.form_feedback = !g_enhanced.audio.form_feedback;
                    break;
                case 4:  // Combo Announcements
                    g_enhanced.audio.combo_announcements = !g_enhanced.audio.combo_announcements;
                    break;
                case 5:  // Milestone Sounds
                    g_enhanced.audio.milestone_sounds = !g_enhanced.audio.milestone_sounds;
                    break;
                case 6:  // Custom Labels
                    if (g_enhanced.audio.mode == AUDIO_MODE_CUSTOM_LABELS) {
                        g_current_screen = SCREEN_CUSTOM_LABELS;
                    }
                    g_display_dirty = true;
                    break;
                case 7:  // Back
                    g_current_screen = SCREEN_COUNTER;
                    break;
            }
            g_display_dirty = true;
            break;
            
        case '\033':  // ESC
        case 'b':
        case 'B':
            g_current_screen = SCREEN_SETTINGS;
            g_display_dirty = true;
            break;
    }
}

static void handle_stats_input(char key) {
    switch (key) {
        case 'r':
        case 'R':
            // Reset statistics
            memset(&g_enhanced.perfect_reps, 0, sizeof(g_enhanced) - 
                   offsetof(EnhancedState, perfect_reps));
            g_display_dirty = true;
            break;
            
        case '\033':  // ESC
        case 'b':
        case 'B':
            g_current_screen = SCREEN_COUNTER;
            g_display_dirty = true;
            break;
    }
}

static void handle_custom_counters_input(char key) {
    if (g_editing_counter_name) {
        switch (key) {
            case '\n':
            case '\r':  // Save
                if (g_custom_counters_selection < g_device.counter_count) {
                    // Edit existing counter name
                    strncpy(g_device.counters[g_custom_counters_selection].label, g_temp_counter_name, MAX_LABEL_LENGTH - 1);
                    g_device.counters[g_custom_counters_selection].label[MAX_LABEL_LENGTH - 1] = '\0';
                    
                    // Save updated counter to database
                    turso_save_counter(&g_device.counters[g_custom_counters_selection], true);
                } else if (g_custom_counters_selection == g_device.counter_count && g_device.counter_count < MAX_COUNTERS) {
                    // Add new counter
                    if (counter_add(&g_device, g_temp_counter_name, COUNTER_TYPE_SIMPLE)) {
                        // Save new counter to database immediately
                        Counter* new_counter = &g_device.counters[g_device.counter_count - 1];
                        turso_save_counter(new_counter, true);
                        printf("✅ Added tracker: %s\n", g_temp_counter_name);
                    }
                }
                g_editing_counter_name = false;
                g_temp_counter_name[0] = '\0';
                g_temp_counter_name_pos = 0;
                g_display_dirty = true;
                break;
                
            case 27:  // ESC - Cancel
                g_editing_counter_name = false;
                g_temp_counter_name[0] = '\0';
                g_temp_counter_name_pos = 0;
                g_display_dirty = true;
                break;
                
            case 127:  // Backspace
                if (g_temp_counter_name_pos > 0) {
                    g_temp_counter_name_pos--;
                    g_temp_counter_name[g_temp_counter_name_pos] = '\0';
                    g_display_dirty = true;
                }
                break;
                
            default:
                if (key >= 32 && key <= 126 && g_temp_counter_name_pos < MAX_LABEL_LENGTH - 2) {
                    g_temp_counter_name[g_temp_counter_name_pos] = key;
                    g_temp_counter_name_pos++;
                    g_temp_counter_name[g_temp_counter_name_pos] = '\0';
                    g_display_dirty = true;
                }
                break;
        }
        return;
    }
    
    switch (key) {
        case 'w':
        case 'W':
            if (g_custom_counters_selection > 0) {
                g_custom_counters_selection--;
                g_display_dirty = true;
            }
            break;
            
        case 's':
        case 'S': {
            int max_selection = g_device.counter_count + 1; // +1 for "Back"
            if (g_device.counter_count < MAX_COUNTERS) {
                max_selection += 2; // +2 for "Add New" and "Delete"
            }
            if (g_custom_counters_selection < max_selection) {
                g_custom_counters_selection++;
                g_display_dirty = true;
            }
            break;
        }
            
        case '\n':
        case '\r':  // Enter - Edit/Add
            if (g_custom_counters_selection < g_device.counter_count) {
                // Edit existing counter
                g_editing_counter_name = true;
                strncpy(g_temp_counter_name, g_device.counters[g_custom_counters_selection].label, MAX_LABEL_LENGTH - 1);
                g_temp_counter_name_pos = strlen(g_temp_counter_name);
                g_display_dirty = true;
            } else if (g_custom_counters_selection == g_device.counter_count && g_device.counter_count < MAX_COUNTERS) {
                // Add new counter
                g_editing_counter_name = true;
                strcpy(g_temp_counter_name, "New Counter");
                g_temp_counter_name_pos = strlen(g_temp_counter_name);
                g_display_dirty = true;
            } else {
                // Back
                g_current_screen = SCREEN_SETTINGS;
                g_display_dirty = true;
            }
            break;
            
        case 'd':
        case 'D':  // Delete counter
            if (g_custom_counters_selection < g_device.counter_count && g_device.counter_count > 1) {
                // Shift all counters down to remove the selected one
                for (int i = g_custom_counters_selection; i < g_device.counter_count - 1; i++) {
                    g_device.counters[i] = g_device.counters[i + 1];
                }
                g_device.counter_count--;
                if (g_custom_counters_selection >= g_device.counter_count) {
                    g_custom_counters_selection = g_device.counter_count - 1;
                }
                if (g_device.current_counter >= g_device.counter_count) {
                    g_device.current_counter = 0;
                }
                g_display_dirty = true;
            }
            break;
            
        case 'q':
        case 'Q':
        case 27:  // ESC
            g_current_screen = SCREEN_SETTINGS;
            g_display_dirty = true;
            break;
    }
}

static void handle_input(char key) {
    if (key == 'q' || key == 'Q') {
        g_running = false;
        return;
    }
    
    // Wake from sleep on any key
    if (g_current_screen == SCREEN_SLEEP) {
        g_current_screen = SCREEN_COUNTER;
        g_display_dirty = true;
        return;
    }
    
    switch (g_current_screen) {
        case SCREEN_COUNTER:
            handle_counter_input(key);
            break;
        case SCREEN_SETTINGS:
            handle_settings_input(key);
            break;
        case SCREEN_AUDIO_SETTINGS:
            handle_audio_settings_input(key);
            break;
        case SCREEN_CUSTOM_LABELS:
            handle_custom_labels_input(key);
            break;
        case SCREEN_CUSTOM_COUNTERS:
            handle_custom_counters_input(key);
            break;
        case SCREEN_STATS:
            handle_stats_input(key);
            break;
        default:
            break;
    }
}

// ================================
// INITIALIZATION
// ================================

static void initialize_enhanced_features(void) {
    // Initialize Turso local database first
    char device_id[32];
    snprintf(device_id, sizeof(device_id), "combochracker_%d", (int)(time(NULL) % 10000));
    
    if (!turso_local_init(device_id)) {
        NRF_LOG_ERROR("Failed to initialize Turso local database");
    } else {
        NRF_LOG_INFO("Turso local database initialized: %s", device_id);
    }
    
    // Initialize enhanced state
    memset(&g_enhanced, 0, sizeof(g_enhanced));
    
    // Set default audio configuration
    g_enhanced.audio.mode = AUDIO_MODE_BEEPS;
    g_enhanced.audio.count_aloud = true;
    g_enhanced.audio.form_feedback = true;
    g_enhanced.audio.combo_announcements = true;
    g_enhanced.audio.milestone_sounds = true;
    g_enhanced.audio.volume = 7;
    g_enhanced.audio.bluetooth_audio = false;
    g_enhanced.audio.use_custom_counting = false;
    
    // Initialize default custom labels (empty = use defaults)
    memset(g_enhanced.audio.custom_labels, 0, sizeof(g_enhanced.audio.custom_labels));
    memset(g_enhanced.audio.quality_labels, 0, sizeof(g_enhanced.audio.quality_labels));
    
    // Initialize timing
    gettimeofday(&g_last_interaction, NULL);
    gettimeofday(&g_last_display_update, NULL);
    gettimeofday(&g_last_combo_update, NULL);
    
    g_enhanced.last_combo_time = get_time_ms();
    
    // Load audio configuration from database
    TursoAudioRecord audio_config;
    if (turso_load_audio_config(&audio_config)) {
        g_enhanced.audio.mode = (AudioMode)audio_config.audio_mode;
        g_enhanced.audio.volume = audio_config.volume;
        g_enhanced.audio.count_aloud = audio_config.count_aloud;
        g_enhanced.audio.form_feedback = audio_config.form_feedback;
        g_enhanced.audio.combo_announcements = audio_config.combo_announcements;
        g_enhanced.audio.milestone_sounds = audio_config.milestone_sounds;
        
        // Load custom labels
        memcpy(g_enhanced.audio.custom_labels, audio_config.custom_labels, 
               sizeof(g_enhanced.audio.custom_labels));
        memcpy(g_enhanced.audio.quality_labels, audio_config.quality_labels,
               sizeof(g_enhanced.audio.quality_labels));
        
        NRF_LOG_INFO("Loaded audio config from database");
    }
}

// ================================
// MAIN FUNCTION
// ================================

int main(void) {
    printf("Enhanced Combo Chracker Simulation\n");
    printf("=================================\n\n");
    
    // Initialize terminal
    setup_terminal();
    
    // Initialize device with default counters
    NRF_LOG_INFO("Setting up enhanced counters...");
    combo_device_init(&g_device);
    counter_add(&g_device, "Reps", COUNTER_TYPE_SIMPLE);
    counter_add(&g_device, "Perfect Form", COUNTER_TYPE_COMBO);
    counter_add(&g_device, "Speed Sets", COUNTER_TYPE_TIMED);
    counter_add(&g_device, "Gym Sim", COUNTER_TYPE_ACCUMULATOR);
    NRF_LOG_INFO("Created %d counters", g_device.counter_count);
    
    // Initialize enhanced features
    initialize_enhanced_features();
    
    // Load saved data
    if (device_load_from_flash(&g_device)) {
        NRF_LOG_INFO("Data loaded from flash storage");
    }
    
    NRF_LOG_INFO("Enhanced Combo Chracker simulation started");
    NRF_LOG_INFO("Press Q to quit, A for audio, H for settings, I for stats");
    
    g_display_dirty = true;
    
    // Main loop
    while (g_running) {
        // Handle input
        char key = get_key();
        if (key != 0) {
            handle_input(key);
        }
        
        // Update combo status
        Counter* current = device_get_current_counter(&g_device);
        if (current && current->type == COUNTER_TYPE_COMBO) {
            uint32_t time_since_update = time_diff_ms(&g_last_combo_update);
            if (time_since_update >= COMBO_DECAY_INTERVAL_MS) {
                update_combo_status(current);
                gettimeofday(&g_last_combo_update, NULL);
                g_display_dirty = true;
            }
        }
        
        // Update display (only when dirty to prevent infinite loop)
        if (g_display_dirty) {
            render_screen();
            gettimeofday(&g_last_display_update, NULL);
            g_display_dirty = false;
        }
        
        // Check for sleep timeout
        uint32_t idle_time = time_diff_ms(&g_last_interaction);
        if (idle_time >= SLEEP_TIMEOUT_MS && g_current_screen != SCREEN_SLEEP) {
            NRF_LOG_INFO("Entering sleep mode (idle timeout)");
            g_current_screen = SCREEN_SLEEP;
            g_display_dirty = true;
        }
        
        // Small delay to prevent excessive CPU usage
        usleep(UPDATE_INTERVAL_MS * 1000);
    }
    
    // Cleanup
    printf("\n💾 Saving all data...\n");
    NRF_LOG_INFO("Saving data...");
    device_save_to_flash(&g_device);
    
    // Save audio configuration to database
    TursoAudioRecord audio_config = {0};
    audio_config.record_id = 1;
    audio_config.updated_at = get_time_ms();
    audio_config.audio_mode = g_enhanced.audio.mode;
    audio_config.volume = g_enhanced.audio.volume;
    audio_config.count_aloud = g_enhanced.audio.count_aloud;
    audio_config.form_feedback = g_enhanced.audio.form_feedback;
    audio_config.combo_announcements = g_enhanced.audio.combo_announcements;
    audio_config.milestone_sounds = g_enhanced.audio.milestone_sounds;
    
    memcpy(audio_config.custom_labels, g_enhanced.audio.custom_labels,
           sizeof(audio_config.custom_labels));
    memcpy(audio_config.quality_labels, g_enhanced.audio.quality_labels,
           sizeof(audio_config.quality_labels));
    
    turso_save_audio_config(&audio_config);
    
    // Force flush all pending database writes
    turso_force_flush_pending_writes();
    
    // Show database statistics
    TursoDatabaseStats db_stats;
    if (turso_get_database_stats(&db_stats)) {
        NRF_LOG_INFO("Database stats - Records: %d, Pending sync: %d, Flash writes: %d", 
                     db_stats.total_records, db_stats.pending_sync_records, db_stats.total_flash_writes);
    }
    
    // Shutdown database
    turso_local_shutdown();
    
    printf("\n\nThank you for using Enhanced Combo Chracker!\n");
    printf("Your progress has been saved to local database.\n");
    if (turso_get_pending_sync_count() > 0) {
        printf("Note: %d records pending BTLE sync.\n", turso_get_pending_sync_count());
    }
    
    restore_terminal();
    return 0;
}
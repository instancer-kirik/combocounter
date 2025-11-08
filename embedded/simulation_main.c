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

// Simulate the Nordic SDK types and functions we need
#define NRF_SUCCESS 0
#define APP_ERROR_CHECK(err_code) do { if ((err_code) != NRF_SUCCESS) { printf("Error: %d\n", err_code); exit(1); } } while(0)
#define NRF_LOG_INFO(format, ...) printf("[INFO] " format "\n", ##__VA_ARGS__)
#define NRF_LOG_ERROR(format, ...) printf("[ERROR] " format "\n", ##__VA_ARGS__)
#define NRF_LOG_PROCESS() // No-op in simulation

// Application includes
#include "simple_combo_core.h"

// Simulated hardware pins (just for documentation)
#define BUTTON_UP_PIN       13
#define BUTTON_DOWN_PIN     14  
#define BUTTON_SELECT_PIN   15
#define BUTTON_BACK_PIN     16

// Timing constants
#define UPDATE_INTERVAL_MS      100
#define DISPLAY_REFRESH_MS      1000   // 1 second refresh for terminal
#define SLEEP_TIMEOUT_MS        30000  // 30 seconds to sleep in simulation

// Screen definitions
typedef enum {
    SCREEN_COUNTER,
    SCREEN_SETTINGS,
    SCREEN_STATS,
    SCREEN_SLEEP
} ScreenType;

// Global state
static ComboDevice g_device;
static ScreenType g_current_screen = SCREEN_COUNTER;
static bool g_running = true;
static time_t g_last_interaction = 0;
static time_t g_last_display_update = 0;
static bool g_display_dirty = true;

// Terminal handling
static struct termios g_old_terminal;
static bool g_terminal_setup = false;

// Function prototypes
static void setup_terminal(void);
static void restore_terminal(void);
static void handle_input(char c);
static void update_device(void);
static void render_display(void);
static void render_counter_screen(void);
static void render_settings_screen(void);
static void render_stats_screen(void);
static void setup_default_counters(void);
static void save_data(void);
static void load_data(void);
static void clear_screen(void);
static char get_char_non_blocking(void);

// Terminal setup for real-time input
static void setup_terminal(void) {
    struct termios new_terminal;
    
    // Get current terminal settings
    tcgetattr(STDIN_FILENO, &g_old_terminal);
    
    // Set up new terminal settings
    new_terminal = g_old_terminal;
    new_terminal.c_lflag &= ~(ICANON | ECHO); // Disable canonical mode and echo
    new_terminal.c_cc[VMIN] = 0;              // Non-blocking read
    new_terminal.c_cc[VTIME] = 0;
    
    tcsetattr(STDIN_FILENO, TCSANOW, &new_terminal);
    
    // Set stdin to non-blocking
    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
    
    g_terminal_setup = true;
}

static void restore_terminal(void) {
    if (g_terminal_setup) {
        tcsetattr(STDIN_FILENO, TCSANOW, &g_old_terminal);
        printf("\n[INFO] Terminal restored\n");
    }
}

static char get_char_non_blocking(void) {
    char c;
    if (read(STDIN_FILENO, &c, 1) == 1) {
        return c;
    }
    return 0;
}

static void clear_screen(void) {
    printf("\033[2J\033[H"); // ANSI escape codes to clear screen and move cursor home
}

// Input handling
static void handle_input(char c) {
    g_last_interaction = time(NULL);
    
    if (g_current_screen == SCREEN_SLEEP) {
        g_current_screen = SCREEN_COUNTER;
        g_display_dirty = true;
        printf("[INFO] Waking from sleep\n");
        return;
    }
    
    switch (g_current_screen) {
        case SCREEN_COUNTER:
            switch (c) {
                case 'w': // UP - next counter
                case 'W':
                    device_next_counter(&g_device);
                    printf("[ACTION] Next counter\n");
                    break;
                    
                case 's': // DOWN - previous counter  
                case 'S':
                    device_prev_counter(&g_device);
                    printf("[ACTION] Previous counter\n");
                    break;
                    
                case ' ': // SPACE - increment (SELECT)
                case '\n':
                case '\r':
                    {
                        Counter* counter = device_get_current_counter(&g_device);
                        if (counter) {
                            counter_increment(counter, QUALITY_GOOD);
                            printf("[ACTION] Increment: %s = %d\n", counter->label, counter->count);
                        }
                    }
                    break;
                    
                case 'x': // X - decrement (BACK)
                case 'X':
                    {
                        Counter* counter = device_get_current_counter(&g_device);
                        if (counter && counter->count > 0) {
                            counter_decrement(counter, 1);
                            printf("[ACTION] Decrement: %s = %d\n", counter->label, counter->count);
                        }
                    }
                    break;
                    
                case 'h': // H - hold BACK (settings)
                case 'H':
                    g_current_screen = SCREEN_SETTINGS;
                    printf("[ACTION] Enter settings\n");
                    break;
                    
                case 'i': // I - hold SELECT (stats)
                case 'I':
                    g_current_screen = SCREEN_STATS;
                    printf("[ACTION] Enter stats\n");
                    break;
            }
            break;
            
        case SCREEN_SETTINGS:
            switch (c) {
                case 'r': // Reset counters
                case 'R':
                    for (uint8_t i = 0; i < g_device.counter_count; i++) {
                        counter_reset(&g_device.counters[i]);
                    }
                    printf("[ACTION] Reset all counters\n");
                    break;
                    
                case 'x': // Back to counter screen
                case 'X':
                case 'h':
                case 'H':
                    g_current_screen = SCREEN_COUNTER;
                    printf("[ACTION] Exit settings\n");
                    break;
            }
            break;
            
        case SCREEN_STATS:
            if (c == 'x' || c == 'X' || c == 'i' || c == 'I') {
                g_current_screen = SCREEN_COUNTER;
                printf("[ACTION] Exit stats\n");
            }
            break;
            
        default:
            break;
    }
    
    // Special commands that work from any screen
    switch (c) {
        case 'q': // Quit
        case 'Q':
        case 27:  // ESC
            g_running = false;
            printf("[ACTION] Quitting...\n");
            break;
            
        case 'p': // Perfect quality increment
        case 'P':
            if (g_current_screen == SCREEN_COUNTER) {
                Counter* counter = device_get_current_counter(&g_device);
                if (counter) {
                    counter_increment(counter, QUALITY_PERFECT);
                    printf("[ACTION] Perfect increment: %s = %d (x%.1f)\n", 
                           counter->label, counter->count, counter->multiplier);
                }
            }
            break;
            
        case 'g': // Good quality increment (default)
        case 'G':
            if (g_current_screen == SCREEN_COUNTER) {
                Counter* counter = device_get_current_counter(&g_device);
                if (counter) {
                    counter_increment(counter, QUALITY_GOOD);
                    printf("[ACTION] Good increment: %s = %d\n", counter->label, counter->count);
                }
            }
            break;
            
        case 'b': // Partial quality increment
        case 'B':
            if (g_current_screen == SCREEN_COUNTER) {
                Counter* counter = device_get_current_counter(&g_device);
                if (counter) {
                    counter_increment(counter, QUALITY_PARTIAL);
                    printf("[ACTION] Partial increment: %s = %d\n", counter->label, counter->count);
                }
            }
            break;
            
        case 'm': // Miss (no increment, may break combo)
        case 'M':
            if (g_current_screen == SCREEN_COUNTER) {
                Counter* counter = device_get_current_counter(&g_device);
                if (counter) {
                    counter_increment(counter, QUALITY_MISS);
                    printf("[ACTION] Miss: %s = %d (combo broken)\n", counter->label, counter->count);
                }
            }
            break;
    }
    
    g_display_dirty = true;
}

// Device update
static void update_device(void) {
    combo_device_update(&g_device, UPDATE_INTERVAL_MS / 1000.0f);
    
    // Check sleep conditions
    time_t current_time = time(NULL);
    if ((current_time - g_last_interaction) > (SLEEP_TIMEOUT_MS / 1000) && 
        g_current_screen != SCREEN_SLEEP) {
        g_current_screen = SCREEN_SLEEP;
        g_display_dirty = true;
        printf("[INFO] Entering sleep mode (idle timeout)\n");
    }
}

// Display rendering
static void render_display(void) {
    if (!g_display_dirty) return;
    
    clear_screen();
    
    printf("╭─────────────────────────────────────────────────────────────╮\n");
    printf("│                    Combo Chracker Simulation               │\n");
    printf("╰─────────────────────────────────────────────────────────────╯\n\n");
    
    switch (g_current_screen) {
        case SCREEN_COUNTER:
            render_counter_screen();
            break;
        case SCREEN_SETTINGS:
            render_settings_screen();
            break;
        case SCREEN_STATS:
            render_stats_screen();
            break;
        case SCREEN_SLEEP:
            printf("💤 SLEEP MODE 💤\n");
            printf("Press any key to wake up...\n");
            break;
    }
    
    printf("\n");
    printf("╭─────────────────────────────────────────────────────────────╮\n");
    printf("│ Controls: W/S=Switch  SPACE=Inc  X=Dec  H=Settings  Q=Quit  │\n");
    printf("│ Quality:  P=Perfect   G=Good    B=Partial   M=Miss          │\n");
    printf("╰─────────────────────────────────────────────────────────────╯\n");
    
    fflush(stdout);
    g_display_dirty = false;
}

static void render_counter_screen(void) {
    Counter* current = device_get_current_counter(&g_device);
    
    if (!current) {
        printf("❌ No counters configured\n");
        printf("   Please set up counters in code\n");
        return;
    }
    
    printf("📊 COUNTER SCREEN\n\n");
    
    // Current counter info
    printf("Current: %s", current->label);
    if (g_device.counter_count > 1) {
        printf(" (%d/%d)", g_device.current_counter + 1, g_device.counter_count);
    }
    printf("\n\n");
    
    // Main count display
    printf("Count: %d", current->count);
    
    // Show multiplier for combo types
    if (current->type == COUNTER_TYPE_COMBO && current->multiplier > 1.0f) {
        printf("  (×%.1f multiplier)", current->multiplier);
    }
    printf("\n");
    
    // Total and best
    printf("Total: %d", current->total);
    if (current->max_combo > 0) {
        printf("  |  Best: %d", current->max_combo);
    }
    printf("\n\n");
    
    // Quality stats
    if (counter_get_total_actions(current) > 0) {
        printf("Quality breakdown:\n");
        printf("  Perfect: %u  Good: %u  Partial: %u  Miss: %u\n",
               current->perfect_count, current->good_count, 
               current->partial_count, current->miss_count);
        printf("  Accuracy: %.1f%%\n", counter_get_accuracy(current) * 100.0f);
    }
    
    // Counter type info
    printf("\nType: ");
    switch (current->type) {
        case COUNTER_TYPE_SIMPLE:
            printf("Simple (+%d per action)", current->increment_amount);
            break;
        case COUNTER_TYPE_COMBO:
            printf("Combo (max ×%.1f, decay %.1f)", current->max_multiplier, current->decay_rate);
            break;
        case COUNTER_TYPE_TIMED:
            printf("Timed (decay %.1f/sec)", current->decay_rate);
            break;
        case COUNTER_TYPE_ACCUMULATOR:
            printf("Accumulator (lifetime total)");
            break;
    }
    printf("\n");
}

static void render_settings_screen(void) {
    printf("⚙️  SETTINGS\n\n");
    printf("Available actions:\n");
    printf("  R - Reset all counters\n");
    printf("  X - Return to counter screen\n\n");
    
    printf("Current configuration:\n");
    printf("  Auto cycle: %s\n", g_device.auto_cycle_counters ? "ON" : "OFF");
    printf("  Sleep timeout: %d seconds\n", g_device.sleep_timeout_sec);
    printf("  Low power mode: %s\n", g_device.low_power_mode ? "ON" : "OFF");
}

static void render_stats_screen(void) {
    printf("📈 STATISTICS\n\n");
    
    printf("Device stats:\n");
    printf("  Total button presses: %u\n", g_device.total_button_presses);
    printf("  Device uptime: %u seconds\n", g_device.device_uptime_sec);
    printf("  Active counters: %d\n", g_device.counter_count);
    printf("\n");
    
    printf("Counter summary:\n");
    for (uint8_t i = 0; i < g_device.counter_count; i++) {
        Counter* c = &g_device.counters[i];
        if (c->active) {
            printf("  %s: %d (total: %d, best: %d)\n", 
                   c->label, c->count, c->total, c->max_combo);
        }
    }
    
    printf("\nPress X to return to counter screen\n");
}

// Setup functions
static void setup_default_counters(void) {
    printf("[INFO] Setting up default counters...\n");
    
    // Simple rep counter
    counter_add(&g_device, "Reps", COUNTER_TYPE_SIMPLE);
    counter_configure_simple(&g_device.counters[0], "Reps", 1);
    
    // Combo counter for perfect form tracking
    counter_add(&g_device, "Perfect Form", COUNTER_TYPE_COMBO);
    counter_configure_combo(&g_device.counters[1], "Perfect Form", 1, 3.0f, 0.1f);
    
    // Simple set counter
    counter_add(&g_device, "Sets", COUNTER_TYPE_SIMPLE);
    counter_configure_simple(&g_device.counters[2], "Sets", 1);
    
    // Accumulator for total volume
    counter_add(&g_device, "Total Volume", COUNTER_TYPE_ACCUMULATOR);
    counter_configure_accumulator(&g_device.counters[3], "Total Volume", 10);
    
    counter_set_active(&g_device, 0);
    
    printf("[INFO] Created %d counters\n", g_device.counter_count);
}

static void save_data(void) {
    FILE* file = fopen("combocounter_save.dat", "wb");
    if (file) {
        fwrite(&g_device, sizeof(ComboDevice), 1, file);
        fclose(file);
        printf("[INFO] Data saved to combocounter_save.dat\n");
    }
}

static void load_data(void) {
    FILE* file = fopen("combocounter_save.dat", "rb");
    if (file) {
        if (fread(&g_device, sizeof(ComboDevice), 1, file) == 1) {
            fclose(file);
            printf("[INFO] Data loaded from combocounter_save.dat\n");
            return;
        }
        fclose(file);
    }
    
    // If load failed, set up defaults
    combo_device_init(&g_device);
    setup_default_counters();
}

// Main function
int main(void) {
    printf("Combo Chracker Embedded Simulation\n");
    printf("=================================\n\n");
    
    // Setup signal handler for clean exit
    atexit(restore_terminal);
    
    // Initialize device
    load_data();
    
    // Setup terminal for real-time input
    setup_terminal();
    
    g_last_interaction = time(NULL);
    g_last_display_update = time(NULL);
    
    printf("[INFO] Combo Chracker simulation started\n");
    printf("[INFO] Press Q to quit, H for settings, I for stats\n\n");
    
    // Main loop
    while (g_running) {
        // Handle input
        char c = get_char_non_blocking();
        if (c != 0) {
            handle_input(c);
        }
        
        // Update device logic
        time_t current_time = time(NULL);
        if (current_time != g_last_display_update) {
            update_device();
            g_last_display_update = current_time;
        }
        
        // Render display
        render_display();
        
        // Small delay to prevent excessive CPU usage
        usleep(UPDATE_INTERVAL_MS * 1000);
    }
    
    // Save data before exit
    save_data();
    
    printf("\n[INFO] Combo Chracker simulation ended\n");
    return 0;
}
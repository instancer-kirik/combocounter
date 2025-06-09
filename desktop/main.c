#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>

// SDL2 for desktop windowing
#include <SDL2/SDL.h>

// Clay UI
#define CLAY_IMPLEMENTATION
#include "../src/clay.h"

// Combo counter core (using embedded version)
#include "../embedded/simple_combo_core.h"

// Desktop-specific constants
#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600
#define EPAPER_SIM_WIDTH 296
#define EPAPER_SIM_HEIGHT 160
#define EPAPER_SCALE 2

// Global state
static ComboDevice g_combo_device;
static SDL_Window* g_window = NULL;
static SDL_Renderer* g_renderer = NULL;
static bool g_running = true;
static bool g_display_dirty = true;

// UI state
static ActionQuality g_current_quality = QUALITY_PERFECT;
static bool g_settings_mode = false;
static uint8_t g_settings_selection = 0;

// Colors for desktop rendering
typedef struct {
    uint8_t r, g, b, a;
} Color;

static const Color COLOR_BLACK = {0, 0, 0, 255};
static const Color COLOR_WHITE = {255, 255, 255, 255};
static const Color COLOR_RED = {255, 0, 0, 255};
static const Color COLOR_YELLOW = {255, 255, 0, 255};
static const Color COLOR_GRAY = {128, 128, 128, 255};
static const Color COLOR_LIGHT_GRAY = {200, 200, 200, 255};

// Forward declarations
static bool init_sdl(void);
static void cleanup_sdl(void);
static void handle_events(void);
static void render_frame(void);
static void render_epaper_simulation(void);
static void handle_keypress(SDL_Keycode key);
static void setup_default_counters(void);
static uint32_t get_ticks_ms(void);

// Clay text measurement function
Clay_Dimensions measure_text_function(Clay_String* text, Clay_TextElementConfig* config) {
    if (!text || text->length == 0) {
        return (Clay_Dimensions){0, 0};
    }
    
    // Simple text measurement based on font size
    float char_width = config->fontSize * 0.6f;  // Approximate monospace width
    float char_height = config->fontSize * 1.2f; // Height with some padding
    
    int lines = 1;
    int max_line_length = 0;
    int current_line_length = 0;
    
    for (uint32_t i = 0; i < text->length; i++) {
        if (text->chars[i] == '\n') {
            if (current_line_length > max_line_length) {
                max_line_length = current_line_length;
            }
            current_line_length = 0;
            lines++;
        } else {
            current_line_length++;
        }
    }
    
    if (current_line_length > max_line_length) {
        max_line_length = current_line_length;
    }
    
    return (Clay_Dimensions){
        .width = max_line_length * char_width,
        .height = lines * char_height
    };
}

// Desktop-specific combo counter functions
static void setup_default_counters(void) {
    // Start with a simple rep counter
    counter_add(&g_combo_device, "Reps", COUNTER_TYPE_SIMPLE);
    counter_configure_simple(&g_combo_device.counters[0], "Reps", 1);
    
    // Add a combo counter for streaks
    counter_add(&g_combo_device, "Streak", COUNTER_TYPE_COMBO);
    counter_configure_combo(&g_combo_device.counters[1], "Streak", 10, 3.0f, 0.1f);
    
    // Add a timed counter for focus
    counter_add(&g_combo_device, "Focus", COUNTER_TYPE_TIMED);
    counter_configure_timed(&g_combo_device.counters[2], "Focus", 5, 0.2f);
}

static uint32_t get_ticks_ms(void) {
    return SDL_GetTicks();
}

// Clay UI rendering
static void render_counter_screen(void) {
    Counter* current = device_get_current_counter(&g_combo_device);
    
    if (!current) {
        CLAY_TEXT(CLAY_ID("NoCounters"), CLAY_STRING("No counters configured"),
                 CLAY_TEXT_CONFIG(.textColor = {0, 0, 0, 1}, .fontSize = 16));
        return;
    }
    
    CLAY(CLAY_ID("CounterScreen"), 
         CLAY_LAYOUT(.sizing = {CLAY_SIZING_GROW(), CLAY_SIZING_GROW()},
                     .layoutDirection = CLAY_TOP_TO_BOTTOM, .padding = {16, 16})) {
        
        // Counter navigation indicator
        if (g_combo_device.counter_count > 1) {
            char nav_text[32];
            snprintf(nav_text, sizeof(nav_text), "Counter %d/%d", 
                    g_combo_device.current_counter + 1, g_combo_device.counter_count);
            
            CLAY(CLAY_ID("NavContainer"),
                 CLAY_LAYOUT(.sizing = {CLAY_SIZING_GROW(), CLAY_SIZING_FIXED(20)},
                             .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER})) {
                CLAY_TEXT(CLAY_ID("NavText"), CLAY_STRING(nav_text),
                         CLAY_TEXT_CONFIG(.textColor = {0.5f, 0.5f, 0.5f, 1}, .fontSize = 12));
            }
        }
        
        // Counter display
        CLAY(CLAY_ID("CounterDisplay"),
             CLAY_LAYOUT(.sizing = {CLAY_SIZING_GROW(), CLAY_SIZING_GROW()},
                         .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER},
                         .layoutDirection = CLAY_TOP_TO_BOTTOM, .childGap = 8)) {
            
            // Counter label
            CLAY_TEXT(CLAY_ID("CounterLabel"), CLAY_STRING(current->label),
                     CLAY_TEXT_CONFIG(.textColor = {0, 0, 0, 1}, .fontSize = 18));
            
            // Main count display
            char count_text[32];
            snprintf(count_text, sizeof(count_text), "%ld", current->count);
            
            Clay_Color count_color = {0, 0, 0, 1};
            if (current->type == COUNTER_TYPE_COMBO && current->multiplier > 1.0f) {
                count_color = (Clay_Color){0.8f, 0.2f, 0.0f, 1}; // Red for combo
            }
            
            CLAY_TEXT(CLAY_ID("CountDisplay"), CLAY_STRING(count_text),
                     CLAY_TEXT_CONFIG(.textColor = count_color, .fontSize = 36));
            
            // Multiplier display for combo types
            if (current->type == COUNTER_TYPE_COMBO || current->type == COUNTER_TYPE_TIMED) {
                char mult_text[16];
                snprintf(mult_text, sizeof(mult_text), "Multiplier: %.1fx", current->multiplier);
                
                CLAY_TEXT(CLAY_ID("MultDisplay"), CLAY_STRING(mult_text),
                         CLAY_TEXT_CONFIG(.textColor = {0.6f, 0.3f, 0.0f, 1}, .fontSize = 14));
            }
            
            // Total/max display
            char total_text[64];
            if (current->type == COUNTER_TYPE_COMBO) {
                snprintf(total_text, sizeof(total_text), "Max Combo: %ld | Total: %ld", 
                        current->max_combo, current->total);
            } else {
                snprintf(total_text, sizeof(total_text), "Total: %ld", current->total);
            }
            
            CLAY_TEXT(CLAY_ID("TotalDisplay"), CLAY_STRING(total_text),
                     CLAY_TEXT_CONFIG(.textColor = {0.5f, 0.5f, 0.5f, 1}, .fontSize = 12));
        }
        
        // Quality indicator
        CLAY(CLAY_ID("QualityIndicator"),
             CLAY_LAYOUT(.sizing = {CLAY_SIZING_GROW(), CLAY_SIZING_FIXED(30)},
                         .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER})) {
            
            const char* quality_names[] = {"Miss", "Partial", "Good", "Perfect"};
            const char* quality_name = quality_names[g_current_quality];
            
            Clay_Color quality_colors[] = {
                {0.5f, 0.5f, 0.5f, 1},  // Miss - gray
                {1.0f, 0.5f, 0.0f, 1},  // Partial - orange
                {0.0f, 0.5f, 1.0f, 1},  // Good - blue
                {0.0f, 0.8f, 0.2f, 1}   // Perfect - green
            };
            
            char quality_text[32];
            snprintf(quality_text, sizeof(quality_text), "Quality: %s", quality_name);
            
            CLAY_TEXT(CLAY_ID("QualityText"), CLAY_STRING(quality_text),
                     CLAY_TEXT_CONFIG(.textColor = quality_colors[g_current_quality], .fontSize = 14));
        }
        
        // Instructions
        CLAY(CLAY_ID("Instructions"),
             CLAY_LAYOUT(.sizing = {CLAY_SIZING_GROW(), CLAY_SIZING_FIXED(60)},
                         .padding = {8, 8})) {
            CLAY_TEXT(CLAY_ID("InstrText"), 
                     CLAY_STRING("SPACE: Increment | X: Decrement\nA/D: Navigate | Q/E: Quality\nS: Settings | ESC: Quit"),
                     CLAY_TEXT_CONFIG(.textColor = {0.3f, 0.3f, 0.3f, 1}, .fontSize = 10));
        }
    }
}

static void render_settings_screen(void) {
    CLAY(CLAY_ID("SettingsScreen"),
         CLAY_LAYOUT(.sizing = {CLAY_SIZING_GROW(), CLAY_SIZING_GROW()},
                     .layoutDirection = CLAY_TOP_TO_BOTTOM, .padding = {16, 16})) {
        
        CLAY_TEXT(CLAY_ID("SettingsTitle"), CLAY_STRING("Settings"),
                 CLAY_TEXT_CONFIG(.textColor = {0, 0, 0, 1}, .fontSize = 20));
        
        // Settings options
        const char* settings[] = {
            "Add Simple Counter",
            "Add Combo Counter", 
            "Add Timed Counter",
            "Remove Current Counter",
            "Load Workout Preset",
            "Load Meditation Preset",
            "Reset All Counters",
            "Exit Settings"
        };
        
        for (int i = 0; i < 8; i++) {
            Clay_Color bg_color = (i == g_settings_selection) ?
                (Clay_Color){0.8f, 0.8f, 0.8f, 1} : (Clay_Color){1.0f, 1.0f, 1.0f, 1};
            
            CLAY(CLAY_IDI("SettingItem", i),
                 CLAY_RECTANGLE(.color = bg_color),
                 CLAY_LAYOUT(.sizing = {CLAY_SIZING_GROW(), CLAY_SIZING_FIXED(24)},
                             .padding = {8, 4})) {
                CLAY_TEXT(CLAY_IDI("SettingText", i), CLAY_STRING(settings[i]),
                         CLAY_TEXT_CONFIG(.textColor = {0, 0, 0, 1}, .fontSize = 12));
            }
        }
        
        CLAY(CLAY_ID("SettingsInstr"),
             CLAY_LAYOUT(.sizing = {CLAY_SIZING_GROW(), CLAY_SIZING_FIXED(40)},
                         .padding = {8, 8})) {
            CLAY_TEXT(CLAY_ID("SettingsInstrText"), 
                     CLAY_STRING("W/S: Navigate | SPACE: Select | ESC: Back"),
                     CLAY_TEXT_CONFIG(.textColor = {0.3f, 0.3f, 0.3f, 1}, .fontSize = 10));
        }
    }
}

static void handle_settings_selection(void) {
    char label[16];
    
    switch (g_settings_selection) {
        case 0: // Add Simple Counter
            if (g_combo_device.counter_count < MAX_COUNTERS) {
                snprintf(label, sizeof(label), "Simple%d", g_combo_device.counter_count + 1);
                counter_add(&g_combo_device, label, COUNTER_TYPE_SIMPLE);
            }
            break;
        case 1: // Add Combo Counter
            if (g_combo_device.counter_count < MAX_COUNTERS) {
                snprintf(label, sizeof(label), "Combo%d", g_combo_device.counter_count + 1);
                counter_add(&g_combo_device, label, COUNTER_TYPE_COMBO);
                counter_configure_combo(&g_combo_device.counters[g_combo_device.counter_count-1], 
                                      label, 10, 3.0f, 0.1f);
            }
            break;
        case 2: // Add Timed Counter
            if (g_combo_device.counter_count < MAX_COUNTERS) {
                snprintf(label, sizeof(label), "Timed%d", g_combo_device.counter_count + 1);
                counter_add(&g_combo_device, label, COUNTER_TYPE_TIMED);
                counter_configure_timed(&g_combo_device.counters[g_combo_device.counter_count-1], 
                                      label, 5, 0.2f);
            }
            break;
        case 3: // Remove Current Counter
            if (g_combo_device.counter_count > 0) {
                counter_remove(&g_combo_device, g_combo_device.current_counter);
            }
            break;
        case 4: // Load Workout Preset
            preset_workout_reps(&g_combo_device);
            break;
        case 5: // Load Meditation Preset
            preset_meditation_breath(&g_combo_device);
            break;
        case 6: // Reset All
            combo_device_init(&g_combo_device);
            setup_default_counters();
            break;
        case 7: // Exit Settings
            g_settings_mode = false;
            break;
    }
    g_display_dirty = true;
}

int main(int argc, char* argv[]) {
    printf("ComboCounter Desktop - Development Version\n");
    printf("=========================================\n");
    
    if (!init_sdl()) {
        fprintf(stderr, "Failed to initialize SDL\n");
        return 1;
    }
    
    // Initialize Clay
    Clay_SetMeasureTextFunction(measure_text_function);
    
    // Create arena for Clay (1MB should be plenty for desktop)
    static char clay_memory[1024 * 1024];
    Clay_Initialize(Clay_CreateArenaWithCapacityAndMemory(sizeof(clay_memory), clay_memory));
    Clay_SetLayoutDimensions((Clay_Dimensions){EPAPER_SIM_WIDTH, EPAPER_SIM_HEIGHT});
    
    // Initialize combo device
    combo_device_init(&g_combo_device);
    setup_default_counters();
    
    printf("Initialized with %d counters\n", g_combo_device.counter_count);
    printf("Controls:\n");
    printf("  SPACE - Increment counter\n");
    printf("  X - Decrement counter\n");
    printf("  A/D - Navigate counters\n");
    printf("  Q/E - Adjust quality\n");
    printf("  S - Settings menu\n");
    printf("  ESC - Quit\n\n");
    
    // Main loop
    uint32_t last_update = get_ticks_ms();
    
    while (g_running) {
        uint32_t current_time = get_ticks_ms();
        float dt = (current_time - last_update) / 1000.0f;
        last_update = current_time;
        
        // Update combo device
        combo_device_update(&g_combo_device, dt);
        
        handle_events();
        render_frame();
        
        SDL_Delay(16); // ~60 FPS
    }
    
    cleanup_sdl();
    return 0;
}

static bool init_sdl(void) {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return false;
    }
    
    g_window = SDL_CreateWindow("ComboCounter Desktop",
                               SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                               WINDOW_WIDTH, WINDOW_HEIGHT,
                               SDL_WINDOW_SHOWN);
    if (!g_window) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        return false;
    }
    
    g_renderer = SDL_CreateRenderer(g_window, -1, SDL_RENDERER_ACCELERATED);
    if (!g_renderer) {
        fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
        return false;
    }
    
    return true;
}

static void cleanup_sdl(void) {
    if (g_renderer) {
        SDL_DestroyRenderer(g_renderer);
    }
    if (g_window) {
        SDL_DestroyWindow(g_window);
    }
    SDL_Quit();
}

static void handle_events(void) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_QUIT:
                g_running = false;
                break;
            case SDL_KEYDOWN:
                handle_keypress(event.key.keysym.sym);
                break;
        }
    }
}

static void handle_keypress(SDL_Keycode key) {
    device_wake_up(&g_combo_device);
    
    if (g_settings_mode) {
        switch (key) {
            case SDLK_w:
                g_settings_selection = (g_settings_selection > 0) ? g_settings_selection - 1 : 7;
                break;
            case SDLK_s:
                g_settings_selection = (g_settings_selection < 7) ? g_settings_selection + 1 : 0;
                break;
            case SDLK_SPACE:
                handle_settings_selection();
                break;
            case SDLK_ESCAPE:
                g_settings_mode = false;
                break;
        }
    } else {
        switch (key) {
            case SDLK_ESCAPE:
                g_running = false;
                break;
            case SDLK_SPACE: {
                Counter* current = device_get_current_counter(&g_combo_device);
                if (current) {
                    counter_increment(current, g_current_quality);
                    printf("Incremented %s: %ld (quality: %d)\n", 
                          current->label, current->count, g_current_quality);
                }
                break;
            }
            case SDLK_x: {
                Counter* current = device_get_current_counter(&g_combo_device);
                if (current) {
                    counter_decrement(current, 1);
                    printf("Decremented %s: %ld\n", current->label, current->count);
                }
                break;
            }
            case SDLK_a:
                device_prev_counter(&g_combo_device);
                printf("Previous counter: %s\n", DEVICE_GET_CURRENT_LABEL(&g_combo_device));
                break;
            case SDLK_d:
                device_next_counter(&g_combo_device);
                printf("Next counter: %s\n", DEVICE_GET_CURRENT_LABEL(&g_combo_device));
                break;
            case SDLK_q:
                g_current_quality = (g_current_quality > QUALITY_MISS) ? 
                                  g_current_quality - 1 : QUALITY_MISS;
                printf("Quality: %d\n", g_current_quality);
                break;
            case SDLK_e:
                g_current_quality = (g_current_quality < QUALITY_PERFECT) ? 
                                  g_current_quality + 1 : QUALITY_PERFECT;
                printf("Quality: %d\n", g_current_quality);
                break;
            case SDLK_s:
                g_settings_mode = true;
                g_settings_selection = 0;
                break;
        }
    }
    g_display_dirty = true;
}

static void render_frame(void) {
    if (!g_display_dirty) return;
    
    // Clear screen
    SDL_SetRenderDrawColor(g_renderer, 240, 240, 240, 255); // Light gray background
    SDL_RenderClear(g_renderer);
    
    // Render e-paper simulation area
    render_epaper_simulation();
    
    SDL_RenderPresent(g_renderer);
    g_display_dirty = false;
}

static void render_epaper_simulation(void) {
    // Calculate centered position for e-paper simulation
    int sim_x = (WINDOW_WIDTH - EPAPER_SIM_WIDTH * EPAPER_SCALE) / 2;
    int sim_y = (WINDOW_HEIGHT - EPAPER_SIM_HEIGHT * EPAPER_SCALE) / 2;
    
    // Draw e-paper border
    SDL_Rect border = {sim_x - 2, sim_y - 2, 
                      EPAPER_SIM_WIDTH * EPAPER_SCALE + 4, 
                      EPAPER_SIM_HEIGHT * EPAPER_SCALE + 4};
    SDL_SetRenderDrawColor(g_renderer, 100, 100, 100, 255);
    SDL_RenderFillRect(g_renderer, &border);
    
    // Draw e-paper background (white)
    SDL_Rect screen = {sim_x, sim_y, 
                      EPAPER_SIM_WIDTH * EPAPER_SCALE, 
                      EPAPER_SIM_HEIGHT * EPAPER_SCALE};
    SDL_SetRenderDrawColor(g_renderer, 255, 255, 255, 255);
    SDL_RenderFillRect(g_renderer, &screen);
    
    // Render Clay UI
    Clay_BeginLayout();
    
    if (g_settings_mode) {
        render_settings_screen();
    } else {
        render_counter_screen();
    }
    
    Clay_RenderCommandArray commands = Clay_EndLayout();
    
    // Simple Clay command rendering (just text for now)
    for (uint32_t i = 0; i < commands.length; i++) {
        Clay_RenderCommand* cmd = Clay_RenderCommandArray_Get(&commands, i);
        if (cmd->commandType == CLAY_RENDER_COMMAND_TYPE_TEXT) {
            // This is a simplified renderer - in a real implementation you'd want proper font rendering
            printf("Render text at (%.0f,%.0f): %.*s\n", 
                  cmd->boundingBox.x, cmd->boundingBox.y,
                  (int)cmd->text.length, cmd->text.chars);
        }
    }
}
#define CLAY_IMPLEMENTATION
#include "../src/clay.h"
#include "clay_epaper_renderer.h"
#include "simple_combo_core.h"
#include <stdio.h>
#include <string.h>

// Configuration for combo meter timing
typedef enum {
    TIMING_FAST = 0,        // 2 seconds
    TIMING_MEDIUM = 1,      // 5 seconds  
    TIMING_SLOW = 2,        // 10 seconds
    TIMING_NO_DECAY = 3     // Infinite
} ComboTiming;

// Global state
static ComboTiming g_combo_timing = TIMING_MEDIUM;
static bool g_controls_at_top = true;
static ClayEpaperContext* g_clay_ctx = NULL;
static ComboDevice* g_device = NULL;

// Timing presets in seconds
static const float TIMING_VALUES[] = {2.0f, 5.0f, 10.0f, 0.0f};

// Get decay time for current timing setting
static float get_decay_time(void) {
    return TIMING_VALUES[g_combo_timing];
}

// Get timing name for display
static const char* get_timing_name(void) {
    switch (g_combo_timing) {
        case TIMING_FAST: return "Fast (2s)";
        case TIMING_MEDIUM: return "Medium (5s)";
        case TIMING_SLOW: return "Slow (10s)";
        case TIMING_NO_DECAY: return "No Decay";
        default: return "Unknown";
    }
}

// Render controls at top of screen
void render_top_controls(void) {
    CLAY(CLAY_ID("TopControls"),
         CLAY_LAYOUT(.sizing = {CLAY_SIZING_GROW(), CLAY_SIZING_FIXED(30)},
                     .layoutDirection = CLAY_LEFT_TO_RIGHT,
                     .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER},
                     .padding = {4, 4, 4, 4}),
         CLAY_RECTANGLE(.color = {0.9f, 0.9f, 0.9f, 1.0f}),
         CLAY_BORDER(.width = 1, .color = {0.5f, 0.5f, 0.5f, 1.0f})) {
        
        // Left: Previous counter button
        CLAY_TEXT(CLAY_ID("PrevBtn"), CLAY_STRING("◀ W"), 
                 CLAY_TEXT_CONFIG(.textColor = {0, 0, 0, 1}, .fontSize = 12));
        
        // Center: Current counter info  
        CLAY(CLAY_ID("CounterInfo"),
             CLAY_LAYOUT(.sizing = {CLAY_SIZING_GROW(), CLAY_SIZING_GROW()},
                         .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER})) {
            
            Counter* current = device_get_current_counter(g_device);
            if (current) {
                char counter_text[64];
                snprintf(counter_text, sizeof(counter_text), "%s (%d/%d) | %s", 
                        current->label, 
                        g_device->current_counter + 1, 
                        g_device->counter_count,
                        get_timing_name());
                
                CLAY_TEXT(CLAY_ID("CounterName"), CLAY_STRING(counter_text),
                         CLAY_TEXT_CONFIG(.textColor = {0, 0, 0, 1}, .fontSize = 10));
            }
        }
        
        // Right: Next counter button
        CLAY_TEXT(CLAY_ID("NextBtn"), CLAY_STRING("S ▶"), 
                 CLAY_TEXT_CONFIG(.textColor = {0, 0, 0, 1}, .fontSize = 12));
    }
}

// Render controls at bottom of screen
void render_bottom_controls(void) {
    CLAY(CLAY_ID("BottomControls"),
         CLAY_LAYOUT(.sizing = {CLAY_SIZING_GROW(), CLAY_SIZING_FIXED(20)},
                     .layoutDirection = CLAY_LEFT_TO_RIGHT,
                     .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER},
                     .padding = {4, 4, 4, 4}),
         CLAY_RECTANGLE(.color = {0.9f, 0.9f, 0.9f, 1.0f}),
         CLAY_BORDER(.width = 1, .color = {0.5f, 0.5f, 0.5f, 1.0f})) {
        
        CLAY_TEXT(CLAY_ID("Controls"), CLAY_STRING("P=Perfect G=Good B=Partial M=Miss SPACE=Inc T=Timing"),
                 CLAY_TEXT_CONFIG(.textColor = {0, 0, 0, 1}, .fontSize = 8));
    }
}

// Render visual combo meter with progress bar
void render_combo_meter(Counter* counter) {
    if (counter->type != COUNTER_TYPE_COMBO) {
        return;
    }
    
    CLAY(CLAY_ID("ComboMeterSection"),
         CLAY_LAYOUT(.sizing = {CLAY_SIZING_GROW(), CLAY_SIZING_FIXED(50)},
                     .layoutDirection = CLAY_TOP_TO_BOTTOM,
                     .padding = {8, 8, 8, 8},
                     .childGap = 6)) {
        
        // Combo meter title
        char title[64];
        float decay_time = get_decay_time();
        if (decay_time > 0) {
            snprintf(title, sizeof(title), "Combo Decay Timer (%.1fs)", decay_time);
        } else {
            snprintf(title, sizeof(title), "Combo Meter (No Decay)");
        }
        
        CLAY_TEXT(CLAY_ID("ComboTitle"), CLAY_STRING(title),
                 CLAY_TEXT_CONFIG(.textColor = {0, 0, 0, 1}, .fontSize = 11));
        
        // Progress bar container
        CLAY(CLAY_ID("ProgressBarContainer"),
             CLAY_LAYOUT(.sizing = {CLAY_SIZING_GROW(), CLAY_SIZING_FIXED(12)}),
             CLAY_RECTANGLE(.color = {0.8f, 0.8f, 0.8f, 1.0f}),
             CLAY_BORDER(.width = 1, .color = {0.5f, 0.5f, 0.5f, 1.0f})) {
            
            // Calculate progress (0.0 to 1.0)
            float progress = 0.0f;
            if (counter->max_multiplier > 1.0f) {
                progress = (counter->multiplier - 1.0f) / (counter->max_multiplier - 1.0f);
                if (progress > 1.0f) progress = 1.0f;
                if (progress < 0.0f) progress = 0.0f;
            }
            
            // Progress bar fill (only if progress > 0)
            if (progress > 0.0f) {
                // Color based on combo level: green -> yellow -> red
                Clay_Color fill_color;
                if (progress < 0.5f) {
                    // Green to yellow
                    fill_color = (Clay_Color){progress * 2.0f, 1.0f, 0.0f, 1.0f};
                } else {
                    // Yellow to red
                    fill_color = (Clay_Color){1.0f, 2.0f - (progress * 2.0f), 0.0f, 1.0f};
                }
                
                CLAY(CLAY_ID("ProgressBarFill"),
                     CLAY_LAYOUT(.sizing = {CLAY_SIZING_PERCENT(progress), CLAY_SIZING_GROW()}),
                     CLAY_RECTANGLE(.color = fill_color));
            }
        }
        
        // Multiplier display
        char multiplier_text[32];
        snprintf(multiplier_text, sizeof(multiplier_text), "×%.2f", counter->multiplier);
        
        CLAY(CLAY_ID("MultiplierContainer"),
             CLAY_LAYOUT(.childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER})) {
            CLAY_TEXT(CLAY_ID("MultiplierValue"), CLAY_STRING(multiplier_text),
                     CLAY_TEXT_CONFIG(.textColor = {0, 0, 0, 1}, .fontSize = 16));
        }
    }
}

// Render main counter display
void render_counter_display(Counter* counter) {
    CLAY(CLAY_ID("CounterDisplay"),
         CLAY_LAYOUT(.sizing = {CLAY_SIZING_GROW(), CLAY_SIZING_GROW()},
                     .layoutDirection = CLAY_TOP_TO_BOTTOM,
                     .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER},
                     .childGap = 8)) {
        
        // Main count (large text)
        char count_text[32];
        snprintf(count_text, sizeof(count_text), "%d", counter->count);
        
        CLAY_TEXT(CLAY_ID("MainCount"), CLAY_STRING(count_text),
                 CLAY_TEXT_CONFIG(.textColor = {0, 0, 0, 1}, .fontSize = 42));
        
        // Counter type
        const char* type_name = "";
        switch (counter->type) {
            case COUNTER_TYPE_SIMPLE: type_name = "Simple"; break;
            case COUNTER_TYPE_COMBO: type_name = "Combo"; break;
            case COUNTER_TYPE_TIMED: type_name = "Timed"; break;
            case COUNTER_TYPE_ACCUMULATOR: type_name = "Accumulator"; break;
        }
        
        CLAY_TEXT(CLAY_ID("CounterType"), CLAY_STRING(type_name),
                 CLAY_TEXT_CONFIG(.textColor = {0.5f, 0.5f, 0.5f, 1}, .fontSize = 12));
        
        // Additional info for combo counters
        if (counter->type == COUNTER_TYPE_COMBO) {
            char combo_info[64];
            snprintf(combo_info, sizeof(combo_info), "Current: %d | Best: %d", 
                    counter->count, counter->max_combo);
            
            CLAY_TEXT(CLAY_ID("ComboInfo"), CLAY_STRING(combo_info),
                     CLAY_TEXT_CONFIG(.textColor = {0, 0, 0, 1}, .fontSize = 10));
        }
        
        // Total count
        char total_text[32];
        snprintf(total_text, sizeof(total_text), "Total: %d", counter->total);
        
        CLAY_TEXT(CLAY_ID("TotalCount"), CLAY_STRING(total_text),
                 CLAY_TEXT_CONFIG(.textColor = {0.5f, 0.5f, 0.5f, 1}, .fontSize = 10));
        
        // Accuracy display
        uint32_t total_attempts = counter->perfect_count + counter->good_count + 
                                  counter->partial_count + counter->miss_count;
        
        if (total_attempts > 0) {
            uint32_t good_attempts = counter->perfect_count + counter->good_count;
            float accuracy = (100.0f * good_attempts) / total_attempts;
            
            char accuracy_text[64];
            snprintf(accuracy_text, sizeof(accuracy_text), "Accuracy: %.1f%% | P:%d G:%d B:%d M:%d", 
                    accuracy, counter->perfect_count, counter->good_count, 
                    counter->partial_count, counter->miss_count);
            
            CLAY_TEXT(CLAY_ID("AccuracyStats"), CLAY_STRING(accuracy_text),
                     CLAY_TEXT_CONFIG(.textColor = {0.5f, 0.5f, 0.5f, 1}, .fontSize = 9));
        }
    }
}

// Main render function
void render_combo_meter_ui(void) {
    if (!g_clay_ctx || !g_device) {
        return;
    }
    
    Counter* current_counter = device_get_current_counter(g_device);
    if (!current_counter) {
        return;
    }
    
    // Start Clay frame
    clay_epaper_begin_frame(g_clay_ctx);
    
    // Main container - full screen
    CLAY(CLAY_ID("MainContainer"), 
         CLAY_LAYOUT(.sizing = {CLAY_SIZING_FIXED(EPAPER_WIDTH), CLAY_SIZING_FIXED(EPAPER_HEIGHT)},
                     .layoutDirection = CLAY_TOP_TO_BOTTOM),
         CLAY_RECTANGLE(.color = {1.0f, 1.0f, 1.0f, 1.0f})) {
        
        // Top controls (if enabled)
        if (g_controls_at_top) {
            render_top_controls();
        }
        
        // Main content area
        CLAY(CLAY_ID("ContentArea"),
             CLAY_LAYOUT(.sizing = {CLAY_SIZING_GROW(), CLAY_SIZING_GROW()},
                         .layoutDirection = CLAY_TOP_TO_BOTTOM,
                         .padding = {8, 8, 8, 8})) {
            
            // Counter display
            render_counter_display(current_counter);
            
            // Combo meter (only for combo counters)
            if (current_counter->type == COUNTER_TYPE_COMBO) {
                render_combo_meter(current_counter);
            }
        }
        
        // Bottom controls (if not at top)
        if (!g_controls_at_top) {
            render_bottom_controls();
        }
    }
    
    // End Clay frame
    clay_epaper_end_frame(g_clay_ctx);
}

// Configuration functions
void combo_meter_set_timing(ComboTiming timing) {
    if (timing < 4) {  // Valid range check
        g_combo_timing = timing;
        
        // Apply to current counter if it's a combo type
        Counter* current = device_get_current_counter(g_device);
        if (current && current->type == COUNTER_TYPE_COMBO) {
            float decay_time = get_decay_time();
            if (decay_time > 0) {
                current->decay_rate = 1.0f / decay_time;
            } else {
                current->decay_rate = 0.0f;  // No decay
            }
        }
    }
}

void combo_meter_cycle_timing(void) {
    g_combo_timing = (g_combo_timing + 1) % 4;
    combo_meter_set_timing(g_combo_timing);
}

void combo_meter_set_controls_position(bool at_top) {
    g_controls_at_top = at_top;
}

ComboTiming combo_meter_get_timing(void) {
    return g_combo_timing;
}

bool combo_meter_get_controls_at_top(void) {
    return g_controls_at_top;
}

// Initialize the combo meter system
bool combo_meter_init(ClayEpaperContext* clay_ctx, ComboDevice* device) {
    if (!clay_ctx || !device) {
        return false;
    }
    
    g_clay_ctx = clay_ctx;
    g_device = device;
    
    // Set default configuration
    g_combo_timing = TIMING_MEDIUM;
    g_controls_at_top = true;
    
    return true;
}

// Handle button input
void combo_meter_handle_input(char key) {
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
            break;
            
        case 'p':
        case 'P':
            counter_increment(current, QUALITY_PERFECT);
            break;
            
        case 'g':
        case 'G':
            counter_increment(current, QUALITY_GOOD);
            break;
            
        case 'b':
        case 'B':
            counter_increment(current, QUALITY_PARTIAL);
            break;
            
        case 'm':
        case 'M':
            counter_increment(current, QUALITY_MISS);
            break;
            
        case 't':
        case 'T':
            combo_meter_cycle_timing();
            break;
            
        case 'c':
        case 'C':
            g_controls_at_top = !g_controls_at_top;
            break;
    }
}

// Print current configuration (for debugging)
void combo_meter_print_config(void) {
    printf("Combo Meter Configuration:\n");
    printf("  Timing: %s (%.1fs)\n", get_timing_name(), get_decay_time());
    printf("  Controls: %s\n", g_controls_at_top ? "Top" : "Bottom");
    
    Counter* current = device_get_current_counter(g_device);
    if (current) {
        printf("  Current Counter: %s (Type: %d)\n", current->label, current->type);
        printf("  Count: %d | Total: %d | Multiplier: %.2f\n", 
               current->count, current->total, current->multiplier);
    }
}
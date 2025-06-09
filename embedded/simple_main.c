#include <stdint.h>
#include <stdbool.h>
#include <string.h>

// Nordic SDK includes
#include "nrf.h"
#include "nrf_drv_spi.h"
#include "nrf_drv_gpiote.h"
#include "nrf_delay.h"
#include "nrf_pwr_mgmt.h"
#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"
#include "app_timer.h"
#include "app_button.h"
#include "app_scheduler.h"
#include "fds.h"

// Clay UI
#define CLAY_IMPLEMENTATION
#include "../src/clay.h"

// Application includes
#include "simple_combo_core.h"
#include "clay_epaper_renderer.h"

// Hardware configuration
#define EPAPER_CS_PIN       8
#define EPAPER_DC_PIN       9
#define EPAPER_RST_PIN      10
#define EPAPER_BUSY_PIN     11
#define EPAPER_SCK_PIN      3
#define EPAPER_MOSI_PIN     4

#define BUTTON_UP_PIN       13      // Cycle counter / Quality up
#define BUTTON_DOWN_PIN     14      // Previous counter / Quality down
#define BUTTON_SELECT_PIN   15      // Increment counter
#define BUTTON_BACK_PIN     16      // Decrement / Settings

#define VIBRATION_PIN       17
#define BATTERY_ADC_PIN     2

// Memory allocation
#define CLAY_MEMORY_SIZE    (32 * 1024)  // 32KB for Clay UI
#define APP_MEMORY_SIZE     (16 * 1024)  // 16KB for app
#define TOTAL_MEMORY_SIZE   (CLAY_MEMORY_SIZE + APP_MEMORY_SIZE)

// Timing constants
#define UPDATE_INTERVAL     APP_TIMER_TICKS(100)  // 100ms
#define DISPLAY_REFRESH_MS  500   // Max 2 FPS for e-Paper
#define BUTTON_HOLD_MS      1000  // Long press threshold

// Global variables
static ComboDevice g_combo_device;
static ClayEpaperContext g_clay_context;
static nrf_drv_spi_t g_spi = NRF_DRV_SPI_INSTANCE(0);
static bool g_spi_initialized = false;

// Memory allocation
static uint8_t g_memory_pool[TOTAL_MEMORY_SIZE];
static uint8_t* g_clay_memory = g_memory_pool;
static uint8_t* g_app_memory = g_memory_pool + CLAY_MEMORY_SIZE;

// Timer handles
APP_TIMER_DEF(m_update_timer_id);
APP_TIMER_DEF(m_display_timer_id);

// UI state
static uint32_t g_last_display_update = 0;
static bool g_display_dirty = true;
static ActionQuality g_current_quality = QUALITY_PERFECT;
static bool g_settings_mode = false;
static uint8_t g_settings_selection = 0;

// Button state tracking
static uint32_t g_button_press_time[4] = {0};
static bool g_button_held[4] = {false};

// Button configuration
static app_button_cfg_t m_buttons[] = {
    {BUTTON_UP_PIN, false, BUTTON_PULL, button_up_handler},
    {BUTTON_DOWN_PIN, false, BUTTON_PULL, button_down_handler},
    {BUTTON_SELECT_PIN, false, BUTTON_PULL, button_select_handler},
    {BUTTON_BACK_PIN, false, BUTTON_PULL, button_back_handler}
};

// Forward declarations
static void combo_update_handler(void* p_context);
static void display_update_handler(void* p_context);
static void button_up_handler(uint8_t pin_no, uint8_t button_action);
static void button_down_handler(uint8_t pin_no, uint8_t button_action);
static void button_select_handler(uint8_t pin_no, uint8_t button_action);
static void button_back_handler(uint8_t pin_no, uint8_t button_action);
static void hardware_init(void);
static void timers_init(void);
static void buttons_init(void);
static void power_management_init(void);
static void log_init(void);
static void render_current_screen(void);
static void handle_button_press(uint8_t button, bool is_press);
static void handle_button_hold(uint8_t button);
static void vibrate_feedback(uint16_t duration_ms);
static uint8_t read_battery_level(void);
static void setup_default_counters(void);

// Clay UI rendering functions
static void render_counter_screen(void);
static void render_settings_screen(void);
static void render_header(void);
static void render_counter_display(const Counter* counter);
static void render_quality_indicator(void);
static void render_counter_stats(const Counter* counter);

// Clay text measurement callback
Clay_Dimensions measure_text_function(Clay_String* text, Clay_TextElementConfig* config) {
    return clay_epaper_measure_text(text, config);
}

// Application entry point
int main(void) {
    // Initialize logging
    log_init();
    NRF_LOG_INFO("Simple Combo Counter starting...");
    
    // Initialize hardware
    hardware_init();
    
    // Initialize Clay UI
    if (!clay_epaper_init(&g_clay_context, g_clay_memory, CLAY_MEMORY_SIZE)) {
        NRF_LOG_ERROR("Failed to initialize Clay e-Paper context");
        APP_ERROR_HANDLER(NRF_ERROR_INTERNAL);
    }
    
    // Initialize Clay
    Clay_SetMeasureTextFunction(measure_text_function);
    Clay_Initialize(Clay_CreateArenaWithCapacityAndMemory(APP_MEMORY_SIZE, g_app_memory));
    Clay_SetLayoutDimensions((Clay_Dimensions){EPAPER_WIDTH, EPAPER_HEIGHT});
    
    // Initialize timers and buttons
    timers_init();
    buttons_init();
    power_management_init();
    
    // Initialize combo device
    combo_device_init(&g_combo_device);
    setup_default_counters();
    
    // Try to load saved state
    if (!device_load_from_flash(&g_combo_device)) {
        NRF_LOG_INFO("No saved data found, using defaults");
    }
    
    // Start timers
    ret_code_t err_code = app_timer_start(m_update_timer_id, UPDATE_INTERVAL, NULL);
    APP_ERROR_CHECK(err_code);
    
    NRF_LOG_INFO("Simple Combo Counter initialized successfully");
    
    // Initial render
    render_current_screen();
    
    // Enter main loop
    for (;;) {
        app_sched_execute();
        
        if (NRF_LOG_PROCESS() == false) {
            nrf_pwr_mgmt_run();
        }
    }
}

static void combo_update_handler(void* p_context) {
    UNUSED_PARAMETER(p_context);
    
    // Update combo device
    combo_device_update(&g_combo_device, 0.1f);
    
    // Check for display update
    uint32_t current_time = app_timer_cnt_get();
    if (g_display_dirty && 
        (current_time - g_last_display_update) > APP_TIMER_TICKS(DISPLAY_REFRESH_MS)) {
        render_current_screen();
    }
    
    // Check for sleep
    if (device_should_sleep(&g_combo_device)) {
        device_enter_sleep(&g_combo_device);
        clay_epaper_hardware_sleep();
    }
}

static void display_update_handler(void* p_context) {
    UNUSED_PARAMETER(p_context);
    render_current_screen();
}

static void render_current_screen(void) {
    clay_epaper_begin_frame(&g_clay_context);
    Clay_BeginLayout();
    
    // Main container
    CLAY(CLAY_ID("MainContainer"), 
         CLAY_LAYOUT(.sizing = {CLAY_SIZING_GROW(), CLAY_SIZING_GROW()},
                     .layoutDirection = CLAY_TOP_TO_BOTTOM)) {
        
        render_header();
        
        if (g_settings_mode) {
            render_settings_screen();
        } else {
            render_counter_screen();
        }
    }
    
    Clay_RenderCommandArray render_commands = Clay_EndLayout();
    clay_epaper_render_commands(&g_clay_context, render_commands);
    clay_epaper_end_frame(&g_clay_context);
    
    g_display_dirty = false;
    g_last_display_update = app_timer_cnt_get();
}

static void render_header(void) {
    CLAY(CLAY_ID("Header"), 
         CLAY_LAYOUT(.sizing = {CLAY_SIZING_GROW(), CLAY_SIZING_FIXED(20)},
                     .padding = {2, 2})) {
        
        // Device name and battery
        char header_text[32];
        snprintf(header_text, sizeof(header_text), "ComboCounter  %d%%", read_battery_level());
        
        CLAY_TEXT(CLAY_ID("HeaderText"), CLAY_STRING(header_text),
                 CLAY_TEXT_CONFIG(.textColor = {0, 0, 0, 1}, .fontSize = 10));
    }
}

static void render_counter_screen(void) {
    Counter* current = device_get_current_counter(&g_combo_device);
    
    if (!current) {
        CLAY_TEXT(CLAY_ID("NoCounters"), CLAY_STRING("No counters configured"),
                 CLAY_TEXT_CONFIG(.textColor = {0, 0, 0, 1}, .fontSize = 12));
        return;
    }
    
    CLAY(CLAY_ID("CounterScreen"), 
         CLAY_LAYOUT(.sizing = {CLAY_SIZING_GROW(), CLAY_SIZING_GROW()},
                     .layoutDirection = CLAY_TOP_TO_BOTTOM, .padding = {8, 8})) {
        
        // Counter navigation indicator
        if (g_combo_device.counter_count > 1) {
            char nav_text[32];
            snprintf(nav_text, sizeof(nav_text), "%d/%d", 
                    g_combo_device.current_counter + 1, g_combo_device.counter_count);
            
            CLAY(CLAY_ID("NavContainer"),
                 CLAY_LAYOUT(.sizing = {CLAY_SIZING_GROW(), CLAY_SIZING_FIXED(16)},
                             .childAlignment = {CLAY_ALIGN_X_RIGHT, CLAY_ALIGN_Y_CENTER})) {
                CLAY_TEXT(CLAY_ID("NavText"), CLAY_STRING(nav_text),
                         CLAY_TEXT_CONFIG(.textColor = {0.5f, 0.5f, 0.5f, 1}, .fontSize = 8));
            }
        }
        
        render_counter_display(current);
        render_quality_indicator();
        render_counter_stats(current);
        
        // Instructions
        CLAY(CLAY_ID("Instructions"),
             CLAY_LAYOUT(.sizing = {CLAY_SIZING_GROW(), CLAY_SIZING_FIXED(24)},
                         .padding = {0, 4})) {
            CLAY_TEXT(CLAY_ID("InstrText"), CLAY_STRING("SELECT: +1  BACK: -1\nUP/DOWN: Navigate"),
                     CLAY_TEXT_CONFIG(.textColor = {0.3f, 0.3f, 0.3f, 1}, .fontSize = 8));
        }
    }
}

static void render_counter_display(const Counter* counter) {
    if (!counter) return;
    
    CLAY(CLAY_ID("CounterDisplay"),
         CLAY_LAYOUT(.sizing = {CLAY_SIZING_GROW(), CLAY_SIZING_GROW()},
                     .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER},
                     .layoutDirection = CLAY_TOP_TO_BOTTOM, .childGap = 4)) {
        
        // Counter label
        CLAY_TEXT(CLAY_ID("CounterLabel"), CLAY_STRING(counter->label),
                 CLAY_TEXT_CONFIG(.textColor = {0, 0, 0, 1}, .fontSize = 14));
        
        // Main count display
        char count_text[32];
        snprintf(count_text, sizeof(count_text), "%ld", counter->count);
        
        Clay_Color count_color = {0, 0, 0, 1};
        if (counter->type == COUNTER_TYPE_COMBO && counter->multiplier > 1.0f) {
            count_color = (Clay_Color){0.8f, 0.2f, 0.0f, 1}; // Red for combo
        }
        
        CLAY_TEXT(CLAY_ID("CountDisplay"), CLAY_STRING(count_text),
                 CLAY_TEXT_CONFIG(.textColor = count_color, .fontSize = 24));
        
        // Multiplier display for combo types
        if (counter->type == COUNTER_TYPE_COMBO || counter->type == COUNTER_TYPE_TIMED) {
            char mult_text[16];
            snprintf(mult_text, sizeof(mult_text), "x%.1f", counter->multiplier);
            
            CLAY_TEXT(CLAY_ID("MultDisplay"), CLAY_STRING(mult_text),
                     CLAY_TEXT_CONFIG(.textColor = {0.6f, 0.3f, 0.0f, 1}, .fontSize = 12));
        }
        
        // Total/max display
        char total_text[32];
        if (counter->type == COUNTER_TYPE_COMBO) {
            snprintf(total_text, sizeof(total_text), "Max: %ld  Total: %ld", 
                    counter->max_combo, counter->total);
        } else {
            snprintf(total_text, sizeof(total_text), "Total: %ld", counter->total);
        }
        
        CLAY_TEXT(CLAY_ID("TotalDisplay"), CLAY_STRING(total_text),
                 CLAY_TEXT_CONFIG(.textColor = {0.5f, 0.5f, 0.5f, 1}, .fontSize = 10));
    }
}

static void render_quality_indicator(void) {
    CLAY(CLAY_ID("QualityIndicator"),
         CLAY_LAYOUT(.sizing = {CLAY_SIZING_GROW(), CLAY_SIZING_FIXED(20)},
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
                 CLAY_TEXT_CONFIG(.textColor = quality_colors[g_current_quality], .fontSize = 10));
    }
}

static void render_counter_stats(const Counter* counter) {
    if (!counter) return;
    
    uint32_t total_actions = counter_get_total_actions(counter);
    if (total_actions == 0) return;
    
    CLAY(CLAY_ID("StatsContainer"),
         CLAY_LAYOUT(.sizing = {CLAY_SIZING_GROW(), CLAY_SIZING_FIXED(32)},
                     .layoutDirection = CLAY_TOP_TO_BOTTOM, .childGap = 2)) {
        
        // Accuracy
        float accuracy = counter_get_accuracy(counter) * 100.0f;
        char acc_text[32];
        snprintf(acc_text, sizeof(acc_text), "Accuracy: %.1f%%", accuracy);
        
        CLAY_TEXT(CLAY_ID("AccuracyText"), CLAY_STRING(acc_text),
                 CLAY_TEXT_CONFIG(.textColor = {0.4f, 0.4f, 0.4f, 1}, .fontSize = 8));
        
        // Action breakdown
        char breakdown_text[64];
        snprintf(breakdown_text, sizeof(breakdown_text), 
                "P:%ld G:%ld Pa:%ld M:%ld", 
                counter->perfect_count, counter->good_count, 
                counter->partial_count, counter->miss_count);
        
        CLAY_TEXT(CLAY_ID("BreakdownText"), CLAY_STRING(breakdown_text),
                 CLAY_TEXT_CONFIG(.textColor = {0.4f, 0.4f, 0.4f, 1}, .fontSize = 8));
    }
}

static void render_settings_screen(void) {
    CLAY(CLAY_ID("SettingsScreen"),
         CLAY_LAYOUT(.sizing = {CLAY_SIZING_GROW(), CLAY_SIZING_GROW()},
                     .layoutDirection = CLAY_TOP_TO_BOTTOM, .padding = {8, 8})) {
        
        CLAY_TEXT(CLAY_ID("SettingsTitle"), CLAY_STRING("Settings"),
                 CLAY_TEXT_CONFIG(.textColor = {0, 0, 0, 1}, .fontSize = 14));
        
        // Settings options
        const char* settings[] = {
            "Add Counter",
            "Remove Counter", 
            "Save to Flash",
            "Load Preset",
            "Reset All",
            "Exit Settings"
        };
        
        for (int i = 0; i < 6; i++) {
            Clay_Color bg_color = (i == g_settings_selection) ?
                (Clay_Color){0.8f, 0.8f, 0.8f, 1} : (Clay_Color){1.0f, 1.0f, 1.0f, 1};
            
            CLAY(CLAY_IDI("SettingItem", i),
                 CLAY_RECTANGLE(.color = bg_color),
                 CLAY_LAYOUT(.sizing = {CLAY_SIZING_GROW(), CLAY_SIZING_FIXED(16)},
                             .padding = {4, 2})) {
                CLAY_TEXT(CLAY_IDI("SettingText", i), CLAY_STRING(settings[i]),
                         CLAY_TEXT_CONFIG(.textColor = {0, 0, 0, 1}, .fontSize = 10));
            }
        }
    }
}

// Button handling
static void handle_button_press(uint8_t button, bool is_press) {
    if (is_press) {
        g_button_press_time[button] = app_timer_cnt_get();
        g_button_held[button] = false;
        g_combo_device.total_button_presses++;
        device_wake_up(&g_combo_device);
    } else {
        uint32_t press_duration = app_timer_cnt_get() - g_button_press_time[button];
        bool was_hold = press_duration > APP_TIMER_TICKS(BUTTON_HOLD_MS);
        
        if (was_hold) {
            handle_button_hold(button);
        } else {
            // Short press handling
            if (g_settings_mode) {
                switch (button) {
                    case 0: // UP
                        g_settings_selection = (g_settings_selection > 0) ? g_settings_selection - 1 : 5;
                        break;
                    case 1: // DOWN  
                        g_settings_selection = (g_settings_selection < 5) ? g_settings_selection + 1 : 0;
                        break;
                    case 2: // SELECT
                        handle_settings_selection();
                        break;
                    case 3: // BACK
                        g_settings_mode = false;
                        break;
                }
            } else {
                switch (button) {
                    case 0: // UP - Next counter or quality up
                        if (g_combo_device.counter_count > 1) {
                            device_next_counter(&g_combo_device);
                        } else {
                            g_current_quality = (g_current_quality < QUALITY_PERFECT) ? 
                                              g_current_quality + 1 : QUALITY_PERFECT;
                        }
                        break;
                    case 1: // DOWN - Prev counter or quality down
                        if (g_combo_device.counter_count > 1) {
                            device_prev_counter(&g_combo_device);
                        } else {
                            g_current_quality = (g_current_quality > QUALITY_MISS) ? 
                                              g_current_quality - 1 : QUALITY_MISS;
                        }
                        break;
                    case 2: // SELECT - Increment counter
                        {
                            Counter* current = device_get_current_counter(&g_combo_device);
                            if (current) {
                                counter_increment(current, g_current_quality);
                                vibrate_feedback(50);
                                
                                // Send Bluetooth update if enabled
                                if (g_combo_device.bluetooth_enabled) {
                                    bluetooth_send_counter_update(current, g_combo_device.current_counter, g_current_quality);
                                }
                            }
                        }
                        break;
                    case 3: // BACK - Decrement counter
                        {
                            Counter* current = device_get_current_counter(&g_combo_device);
                            if (current) {
                                counter_decrement(current, g_combo_device.button_increment_amount);
                                vibrate_feedback(100);
                            }
                        }
                        break;
                }
            }
        }
        
        g_display_dirty = true;
    }
}

static void handle_button_hold(uint8_t button) {
    if (button == 3) { // BACK hold - Enter settings
        g_settings_mode = true;
        g_settings_selection = 0;
        vibrate_feedback(200);
    }
}

static void handle_settings_selection(void) {
    switch (g_settings_selection) {
        case 0: // Add Counter
            if (g_combo_device.counter_count < MAX_COUNTERS) {
                char label[16];
                snprintf(label, sizeof(label), "Count%d", g_combo_device.counter_count + 1);
                counter_add(&g_combo_device, label, COUNTER_TYPE_SIMPLE);
            }
            break;
        case 1: // Remove Counter
            if (g_combo_device.counter_count > 0) {
                counter_remove(&g_combo_device, g_combo_device.current_counter);
            }
            break;
        case 2: // Save to Flash
            device_save_to_flash(&g_combo_device);
            break;
        case 3: // Load Preset
            preset_workout_reps(&g_combo_device);
            break;
        case 4: // Reset All
            combo_device_init(&g_combo_device);
            setup_default_counters();
            break;
        case 5: // Exit Settings
            g_settings_mode = false;
            break;
    }
}

static void setup_default_counters(void) {
    // Start with a simple rep counter
    counter_add(&g_combo_device, "Reps", COUNTER_TYPE_SIMPLE);
    counter_configure_simple(&g_combo_device.counters[0], "Reps", 1);
    
    // Add a combo counter for streaks
    counter_add(&g_combo_device, "Streak", COUNTER_TYPE_COMBO);
    counter_configure_combo(&g_combo_device.counters[1], "Streak", 1, 3.0f, 0.1f);
}

// Button handlers
static void button_up_handler(uint8_t pin_no, uint8_t button_action) {
    handle_button_press(0, button_action == APP_BUTTON_PUSH);
}

static void button_down_handler(uint8_t pin_no, uint8_t button_action) {
    handle_button_press(1, button_action == APP_BUTTON_PUSH);
}

static void button_select_handler(uint8_t pin_no, uint8_t button_action) {
    handle_button_press(2, button_action == APP_BUTTON_PUSH);
}

static void button_back_handler(uint8_t pin_no, uint8_t button_action) {
    handle_button_press(3, button_action == APP_BUTTON_PUSH);
}

// Hardware initialization
static void hardware_init(void) {
    ret_code_t err_code;
    
    // Initialize SPI for e-paper display
    nrf_drv_spi_config_t spi_config = NRF_DRV_SPI_DEFAULT_CONFIG;
    spi_config.ss_pin = EPAPER_CS_PIN;
    spi_config.mosi_pin = EPAPER_MOSI_PIN;
    spi_config.sck_pin = EPAPER_SCK_PIN;
    spi_config.frequency = NRF_DRV_SPI_FREQ_4M;
    
    err_code = nrf_drv_spi_init(&g_spi, &spi_config, NULL, NULL);
    APP_ERROR_CHECK(err_code);
    g_spi_initialized = true;
    
    // Initialize GPIO pins
    nrf_gpio_cfg_output(EPAPER_DC_PIN);
    nrf_gpio_cfg_output(EPAPER_RST_PIN);
    nrf_gpio_cfg_input(EPAPER_BUSY_PIN, NRF_GPIO_PIN_PULLUP);
    nrf_gpio_cfg_output(VIBRATION_PIN);
    nrf_gpio_pin_clear(VIBRATION_PIN);
}

static void timers_init(void) {
    ret_code_t err_code;
    
    err_code = app_timer_init();
    APP_ERROR_CHECK(err_code);
    
    err_code = app_timer_create(&m_update_timer_id, APP_TIMER_MODE_REPEATED, combo_update_handler);
    APP_ERROR_CHECK(err_code);
    
    err_code = app_timer_create(&m_display_timer_id, APP_TIMER_MODE_SINGLE_SHOT, display_update_handler);
    APP_ERROR_CHECK(err_code);
}

static void buttons_init(void) {
    ret_code_t err_code;
    
    err_code = app_button_init(m_buttons, ARRAY_SIZE(m_buttons), 50);
    APP_ERROR_CHECK(err_code);
    
    err_code = app_button_enable();
    APP_ERROR_CHECK(err_code);
}

static void power_management_init(void) {
    ret_code_t err_code = nrf_pwr_mgmt_init();
    APP_ERROR_CHECK(err_code);
}

static void log_init(void) {
    ret_code_t err_code = NRF_LOG_INIT(NULL);
    APP_ERROR_CHECK(err_code);
    NRF_LOG_DEFAULT_BACKENDS_INIT();
}

static void vibrate_feedback(uint16_t duration_ms) {
    if (duration_ms == 0) return;
    
    nrf_gpio_pin_set(VIBRATION_PIN);
    nrf_delay_ms(duration_ms);
    nrf_gpio_pin_clear(VIBRATION_PIN);
}

static uint8_t read_battery_level(void) {
    // TODO: Implement ADC reading for actual battery level
    return 85; // Placeholder
}
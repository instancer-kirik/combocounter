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
#include "nrf_ble_gatt.h"
#include "nrf_ble_qwr.h"
#include "ble_advdata.h"
#include "ble_advertising.h"
#include "ble_conn_params.h"
#include "nrf_sdh.h"
#include "nrf_sdh_soc.h"
#include "nrf_sdh_ble.h"

// Clay UI
#define CLAY_IMPLEMENTATION
#include "../src/clay.h"

// Application includes
#include "fitness_core.h"
#include "clay_epaper_renderer.h"

// Hardware configuration
#define EPAPER_CS_PIN       8
#define EPAPER_DC_PIN       9
#define EPAPER_RST_PIN      10
#define EPAPER_BUSY_PIN     11
#define EPAPER_SCK_PIN      3
#define EPAPER_MOSI_PIN     4

#define BUTTON_UP_PIN       13
#define BUTTON_DOWN_PIN     14
#define BUTTON_SELECT_PIN   15
#define BUTTON_BACK_PIN     16

#define VIBRATION_PIN       17
#define BATTERY_ADC_PIN     2

// Timing constants
#define APP_TIMER_PRESCALER             0
#define APP_TIMER_OP_QUEUE_SIZE         4
#define SCHED_MAX_EVENT_DATA_SIZE       APP_TIMER_SCHED_EVENT_DATA_SIZE
#define SCHED_QUEUE_SIZE                10
#define UPDATE_INTERVAL                 APP_TIMER_TICKS(100)  // 100ms

// Memory allocation
#define CLAY_MEMORY_SIZE                (48 * 1024)  // 48KB for Clay UI
#define TOTAL_MEMORY_SIZE               (64 * 1024)  // 64KB total

// BLE configuration
#define DEVICE_NAME                     "FitnessTracker"
#define MANUFACTURER_NAME               "Combo Chracker"
#define APP_BLE_OBSERVER_PRIO           3
#define APP_BLE_CONN_CFG_TAG            1

// FDS configuration
#define CONFIG_FILE_ID                  0x8010
#define CONFIG_REC_KEY                  0x7010

// Global variables
static FitnessTracker g_fitness_tracker;
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
static uint8_t g_menu_selection = 0;
static bool g_button_pressed[4] = {false};
static uint32_t g_last_button_time = 0;

// Button configuration
static app_button_cfg_t m_buttons[] = {
    {BUTTON_UP_PIN, false, BUTTON_PULL, button_up_handler},
    {BUTTON_DOWN_PIN, false, BUTTON_PULL, button_down_handler},
    {BUTTON_SELECT_PIN, false, BUTTON_PULL, button_select_handler},
    {BUTTON_BACK_PIN, false, BUTTON_PULL, button_back_handler}
};

// Forward declarations
static void fitness_update_handler(void* p_context);
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
static ret_code_t fds_init_and_wait(void);
static void render_current_screen(void);
static void handle_navigation(uint8_t button, bool pressed);
static void vibrate(uint16_t duration_ms);
static uint8_t read_battery_level(void);

// Clay UI rendering functions
static void render_workout_select_screen(void);
static void render_exercise_list_screen(void);
static void render_active_set_screen(void);
static void render_rest_timer_screen(void);
static void render_statistics_screen(void);
static void render_settings_screen(void);
static void render_header(const char* title);
static void render_footer(const char* left_text, const char* right_text);

// BLE related functions
static void ble_stack_init(void);
static void gap_params_init(void);
static void gatt_init(void);
static void advertising_init(void);
static void services_init(void);
static void conn_params_init(void);
static void ble_evt_handler(ble_evt_t const * p_ble_evt, void * p_context);

// Clay text measurement callback
Clay_Dimensions measure_text_function(Clay_String* text, Clay_TextElementConfig* config) {
    return clay_epaper_measure_text(text, config);
}

// Application entry point
int main(void) {
    // Initialize logging
    log_init();
    
    NRF_LOG_INFO("Fitness Tracker starting...");
    
    // Initialize hardware
    hardware_init();
    
    // Initialize Clay UI
    if (!clay_epaper_init(&g_clay_context, g_clay_memory, CLAY_MEMORY_SIZE)) {
        NRF_LOG_ERROR("Failed to initialize Clay e-Paper context");
        APP_ERROR_HANDLER(NRF_ERROR_INTERNAL);
    }
    
    // Initialize Clay
    Clay_SetMeasureTextFunction(measure_text_function);
    Clay_Initialize(Clay_CreateArenaWithCapacityAndMemory(16384, g_app_memory));
    Clay_SetLayoutDimensions((Clay_Dimensions){EPAPER_WIDTH, EPAPER_HEIGHT});
    
    // Initialize timers
    timers_init();
    
    // Initialize buttons
    buttons_init();
    
    // Initialize power management
    power_management_init();
    
    // Initialize BLE stack (simplified for this example)
    // ble_stack_init();
    // gap_params_init();
    // gatt_init();
    // services_init();
    // advertising_init();
    // conn_params_init();
    
    // Initialize FDS (flash data storage)
    ret_code_t err_code = fds_init_and_wait();
    APP_ERROR_CHECK(err_code);
    
    // Initialize fitness tracker
    fitness_init(&g_fitness_tracker);
    
    // Start timers
    err_code = app_timer_start(m_update_timer_id, UPDATE_INTERVAL, NULL);
    APP_ERROR_CHECK(err_code);
    
    NRF_LOG_INFO("Fitness Tracker initialized successfully");
    
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

static void fitness_update_handler(void* p_context) {
    UNUSED_PARAMETER(p_context);
    
    // Update fitness tracker with 100ms delta
    fitness_update(&g_fitness_tracker, 0.1f);
    
    // Update battery level every 10 seconds (100 updates)
    static uint8_t battery_counter = 0;
    if (++battery_counter >= 100) {
        battery_counter = 0;
        uint8_t battery_level = read_battery_level();
        power_update_battery(&g_fitness_tracker, battery_level, false);
    }
    
    // Check if display needs updating
    if (display_needs_refresh(&g_fitness_tracker) || clay_epaper_needs_update(&g_clay_context)) {
        render_current_screen();
    }
}

static void display_update_handler(void* p_context) {
    UNUSED_PARAMETER(p_context);
    render_current_screen();
}

static void render_current_screen(void) {
    clay_epaper_begin_frame(&g_clay_context);
    
    Clay_BeginLayout();
    
    switch (g_fitness_tracker.current_screen) {
        case SCREEN_WORKOUT_SELECT:
            render_workout_select_screen();
            break;
        case SCREEN_EXERCISE_LIST:
            render_exercise_list_screen();
            break;
        case SCREEN_ACTIVE_SET:
            render_active_set_screen();
            break;
        case SCREEN_REST_TIMER:
            render_rest_timer_screen();
            break;
        case SCREEN_STATISTICS:
            render_statistics_screen();
            break;
        case SCREEN_SETTINGS:
            render_settings_screen();
            break;
        case SCREEN_SLEEP:
            // Minimal display for sleep mode
            CLAY(CLAY_ID("SleepScreen"), CLAY_LAYOUT(.sizing = {CLAY_SIZING_GROW(), CLAY_SIZING_GROW()})) {
                CLAY_TEXT(CLAY_ID("SleepText"), CLAY_STRING("Sleep Mode"), 
                         CLAY_TEXT_CONFIG(.textColor = {0, 0, 0, 1}, .fontSize = 16));
            }
            break;
    }
    
    Clay_RenderCommandArray render_commands = Clay_EndLayout();
    clay_epaper_render_commands(&g_clay_context, render_commands);
    clay_epaper_end_frame(&g_clay_context);
    display_mark_clean(&g_fitness_tracker);
}

static void render_header(const char* title) {
    CLAY(CLAY_ID("Header"), 
         CLAY_LAYOUT(.sizing = {CLAY_SIZING_GROW(), CLAY_SIZING_FIXED(24)}, 
                     .padding = {4, 4}, .childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER})) {
        
        // Title
        CLAY_TEXT(CLAY_ID("HeaderTitle"), CLAY_STRING(title), 
                 CLAY_TEXT_CONFIG(.textColor = {0, 0, 0, 1}, .fontSize = 12));
        
        // Battery level (right side)
        CLAY(CLAY_ID("BatteryContainer"), 
             CLAY_LAYOUT(.sizing = {CLAY_SIZING_FIXED(40), CLAY_SIZING_GROW()},
                         .childAlignment = {CLAY_ALIGN_X_RIGHT, CLAY_ALIGN_Y_CENTER})) {
            char battery_text[16];
            snprintf(battery_text, sizeof(battery_text), "%d%%", g_fitness_tracker.power.battery_level);
            CLAY_TEXT(CLAY_ID("BatteryText"), CLAY_STRING(battery_text),
                     CLAY_TEXT_CONFIG(.textColor = {0, 0, 0, 1}, .fontSize = 10));
        }
    }
}

static void render_footer(const char* left_text, const char* right_text) {
    CLAY(CLAY_ID("Footer"), 
         CLAY_LAYOUT(.sizing = {CLAY_SIZING_GROW(), CLAY_SIZING_FIXED(16)}, 
                     .padding = {4, 2})) {
        
        if (left_text) {
            CLAY_TEXT(CLAY_ID("FooterLeft"), CLAY_STRING(left_text),
                     CLAY_TEXT_CONFIG(.textColor = {0, 0, 0, 1}, .fontSize = 8));
        }
        
        if (right_text) {
            CLAY(CLAY_ID("FooterRightContainer"), 
                 CLAY_LAYOUT(.sizing = {CLAY_SIZING_GROW(), CLAY_SIZING_GROW()},
                             .childAlignment = {CLAY_ALIGN_X_RIGHT, CLAY_ALIGN_Y_CENTER})) {
                CLAY_TEXT(CLAY_ID("FooterRight"), CLAY_STRING(right_text),
                         CLAY_TEXT_CONFIG(.textColor = {0, 0, 0, 1}, .fontSize = 8));
            }
        }
    }
}

static void render_workout_select_screen(void) {
    CLAY(CLAY_ID("WorkoutSelectContainer"), 
         CLAY_LAYOUT(.sizing = {CLAY_SIZING_GROW(), CLAY_SIZING_GROW()}, 
                     .layoutDirection = CLAY_TOP_TO_BOTTOM)) {
        
        render_header("Select Workout");
        
        CLAY(CLAY_ID("WorkoutList"), 
             CLAY_LAYOUT(.sizing = {CLAY_SIZING_GROW(), CLAY_SIZING_GROW()}, 
                         .layoutDirection = CLAY_TOP_TO_BOTTOM, .padding = {8, 8}, .childGap = 4)) {
            
            const char* workouts[] = {"Push Day", "Pull Day", "Leg Day", "Custom"};
            for (int i = 0; i < 4; i++) {
                Clay_Color bg_color = (i == g_menu_selection) ? 
                    (Clay_Color){0.8f, 0.8f, 0.8f, 1.0f} : (Clay_Color){1.0f, 1.0f, 1.0f, 1.0f};
                
                CLAY(CLAY_IDI("WorkoutItem", i), 
                     CLAY_RECTANGLE(.color = bg_color),
                     CLAY_LAYOUT(.sizing = {CLAY_SIZING_GROW(), CLAY_SIZING_FIXED(20)}, 
                                 .padding = {4, 4})) {
                    CLAY_TEXT(CLAY_IDI("WorkoutText", i), CLAY_STRING(workouts[i]),
                             CLAY_TEXT_CONFIG(.textColor = {0, 0, 0, 1}, .fontSize = 12));
                }
            }
        }
        
        render_footer("UP/DOWN: Navigate", "SELECT: Choose");
    }
}

static void render_exercise_list_screen(void) {
    CLAY(CLAY_ID("ExerciseListContainer"), 
         CLAY_LAYOUT(.sizing = {CLAY_SIZING_GROW(), CLAY_SIZING_GROW()}, 
                     .layoutDirection = CLAY_TOP_TO_BOTTOM)) {
        
        render_header("Exercises");
        
        if (g_fitness_tracker.current_workout.active) {
            CLAY(CLAY_ID("ExerciseList"), 
                 CLAY_LAYOUT(.sizing = {CLAY_SIZING_GROW(), CLAY_SIZING_GROW()}, 
                             .layoutDirection = CLAY_TOP_TO_BOTTOM, .padding = {8, 8}, .childGap = 4)) {
                
                for (uint8_t i = 0; i < g_fitness_tracker.current_workout.total_exercises; i++) {
                    ExerciseSession* exercise = &g_fitness_tracker.current_workout.exercises[i];
                    Clay_Color bg_color = (i == g_fitness_tracker.current_workout.current_exercise) ? 
                        (Clay_Color){0.8f, 0.8f, 0.8f, 1.0f} : (Clay_Color){1.0f, 1.0f, 1.0f, 1.0f};
                    
                    CLAY(CLAY_IDI("ExerciseItem", i), 
                         CLAY_RECTANGLE(.color = bg_color),
                         CLAY_LAYOUT(.sizing = {CLAY_SIZING_GROW(), CLAY_SIZING_FIXED(24)}, 
                                     .padding = {4, 4})) {
                        CLAY_TEXT(CLAY_IDI("ExerciseText", i), CLAY_STRING(exercise->exercise.name),
                                 CLAY_TEXT_CONFIG(.textColor = {0, 0, 0, 1}, .fontSize = 12));
                        
                        // Show progress
                        char progress_text[32];
                        snprintf(progress_text, sizeof(progress_text), "%d/%d sets", 
                                exercise->current_set, exercise->total_sets);
                        CLAY_TEXT(CLAY_IDI("ExerciseProgress", i), CLAY_STRING(progress_text),
                                 CLAY_TEXT_CONFIG(.textColor = {0.5f, 0.5f, 0.5f, 1}, .fontSize = 10));
                    }
                }
            }
        }
        
        render_footer("SELECT: Start", "BACK: End Workout");
    }
}

static void render_active_set_screen(void) {
    if (!g_fitness_tracker.current_workout.active) return;
    
    uint8_t ex_idx = g_fitness_tracker.current_workout.current_exercise;
    ExerciseSession* exercise = &g_fitness_tracker.current_workout.exercises[ex_idx];
    Set* current_set = &exercise->sets[exercise->current_set];
    
    CLAY(CLAY_ID("ActiveSetContainer"), 
         CLAY_LAYOUT(.sizing = {CLAY_SIZING_GROW(), CLAY_SIZING_GROW()}, 
                     .layoutDirection = CLAY_TOP_TO_BOTTOM)) {
        
        render_header(exercise->exercise.name);
        
        CLAY(CLAY_ID("SetInfo"), 
             CLAY_LAYOUT(.sizing = {CLAY_SIZING_GROW(), CLAY_SIZING_GROW()}, 
                         .layoutDirection = CLAY_TOP_TO_BOTTOM, .padding = {16, 16}, 
                         .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}, .childGap = 8)) {
            
            // Set number
            char set_text[32];
            snprintf(set_text, sizeof(set_text), "Set %d", exercise->current_set + 1);
            CLAY_TEXT(CLAY_ID("SetNumber"), CLAY_STRING(set_text),
                     CLAY_TEXT_CONFIG(.textColor = {0, 0, 0, 1}, .fontSize = 14));
            
            // Reps counter (large)
            char reps_text[32];
            snprintf(reps_text, sizeof(reps_text), "%d / %d", 
                    current_set->reps_completed, exercise->exercise.target_reps);
            CLAY_TEXT(CLAY_ID("RepsCounter"), CLAY_STRING(reps_text),
                     CLAY_TEXT_CONFIG(.textColor = {0, 0, 0, 1}, .fontSize = 24));
            
            // Weight
            char weight_text[32];
            snprintf(weight_text, sizeof(weight_text), "%.1f kg", 
                    (float)current_set->weight_used / 10.0f);
            CLAY_TEXT(CLAY_ID("WeightDisplay"), CLAY_STRING(weight_text),
                     CLAY_TEXT_CONFIG(.textColor = {0, 0, 0, 1}, .fontSize = 16));
            
            // Rep quality breakdown
            char quality_text[64];
            snprintf(quality_text, sizeof(quality_text), "Perfect: %d  Good: %d  Partial: %d", 
                    current_set->perfect_reps, current_set->good_reps, current_set->partial_reps);
            CLAY_TEXT(CLAY_ID("QualityBreakdown"), CLAY_STRING(quality_text),
                     CLAY_TEXT_CONFIG(.textColor = {0.5f, 0.5f, 0.5f, 1}, .fontSize = 10));
        }
        
        render_footer("UP: Perfect Rep", "SELECT: Complete Set");
    }
}

static void render_rest_timer_screen(void) {
    CLAY(CLAY_ID("RestTimerContainer"), 
         CLAY_LAYOUT(.sizing = {CLAY_SIZING_GROW(), CLAY_SIZING_GROW()}, 
                     .layoutDirection = CLAY_TOP_TO_BOTTOM)) {
        
        render_header("Rest Timer");
        
        CLAY(CLAY_ID("TimerDisplay"), 
             CLAY_LAYOUT(.sizing = {CLAY_SIZING_GROW(), CLAY_SIZING_GROW()}, 
                         .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER})) {
            
            // Large timer display
            char timer_text[16];
            uint16_t minutes = g_fitness_tracker.rest_timer.remaining / 60;
            uint16_t seconds = g_fitness_tracker.rest_timer.remaining % 60;
            snprintf(timer_text, sizeof(timer_text), "%02d:%02d", minutes, seconds);
            
            Clay_Color timer_color = (g_fitness_tracker.rest_timer.remaining <= 10) ? 
                (Clay_Color){1.0f, 0.0f, 0.0f, 1.0f} : (Clay_Color){0, 0, 0, 1};
            
            CLAY_TEXT(CLAY_ID("TimerText"), CLAY_STRING(timer_text),
                     CLAY_TEXT_CONFIG(.textColor = timer_color, .fontSize = 32));
        }
        
        render_footer("SELECT: Skip Rest", "BACK: Return to Set");
    }
}

static void render_statistics_screen(void) {
    CLAY(CLAY_ID("StatsContainer"), 
         CLAY_LAYOUT(.sizing = {CLAY_SIZING_GROW(), CLAY_SIZING_GROW()}, 
                     .layoutDirection = CLAY_TOP_TO_BOTTOM)) {
        
        render_header("Statistics");
        
        CLAY(CLAY_ID("StatsList"), 
             CLAY_LAYOUT(.sizing = {CLAY_SIZING_GROW(), CLAY_SIZING_GROW()}, 
                         .layoutDirection = CLAY_TOP_TO_BOTTOM, .padding = {16, 16}, .childGap = 8)) {
            
            char stat_text[64];
            
            snprintf(stat_text, sizeof(stat_text), "Total Workouts: %lu", g_fitness_tracker.total_workouts);
            CLAY_TEXT(CLAY_ID("StatWorkouts"), CLAY_STRING(stat_text),
                     CLAY_TEXT_CONFIG(.textColor = {0, 0, 0, 1}, .fontSize = 12));
            
            snprintf(stat_text, sizeof(stat_text), "Total Sets: %lu", g_fitness_tracker.total_sets);
            CLAY_TEXT(CLAY_ID("StatSets"), CLAY_STRING(stat_text),
                     CLAY_TEXT_CONFIG(.textColor = {0, 0, 0, 1}, .fontSize = 12));
            
            snprintf(stat_text, sizeof(stat_text), "Total Reps: %lu", g_fitness_tracker.total_reps);
            CLAY_TEXT(CLAY_ID("StatReps"), CLAY_STRING(stat_text),
                     CLAY_TEXT_CONFIG(.textColor = {0, 0, 0, 1}, .fontSize = 12));
            
            snprintf(stat_text, sizeof(stat_text), "Total Volume: %.1f kg", 
                    (float)g_fitness_tracker.total_volume / 10.0f);
            CLAY_TEXT(CLAY_ID("StatVolume"), CLAY_STRING(stat_text),
                     CLAY_TEXT_CONFIG(.textColor = {0, 0, 0, 1}, .fontSize = 12));
        }
        
        render_footer("", "BACK: Return");
    }
}

static void render_settings_screen(void) {
    CLAY(CLAY_ID("SettingsContainer"), 
         CLAY_LAYOUT(.sizing = {CLAY_SIZING_GROW(), CLAY_SIZING_GROW()}, 
                     .layoutDirection = CLAY_TOP_TO_BOTTOM)) {
        
        render_header("Settings");
        
        CLAY(CLAY_ID("SettingsList"), 
             CLAY_LAYOUT(.sizing = {CLAY_SIZING_GROW(), CLAY_SIZING_GROW()}, 
                         .layoutDirection = CLAY_TOP_TO_BOTTOM, .padding = {16, 16}, .childGap = 8)) {
            
            // Auto rest setting
            char auto_rest_text[32];
            snprintf(auto_rest_text, sizeof(auto_rest_text), "Auto Rest: %s", 
                    g_fitness_tracker.auto_start_rest ? "ON" : "OFF");
            CLAY_TEXT(CLAY_ID("AutoRestSetting"), CLAY_STRING(auto_rest_text),
                     CLAY_TEXT_CONFIG(.textColor = {0, 0, 0, 1}, .fontSize = 12));
            
            // Vibration setting
            char vibration_text[32];
            snprintf(vibration_text, sizeof(vibration_text), "Vibration: %s", 
                    g_fitness_tracker.vibrate_enabled ? "ON" : "OFF");
            CLAY_TEXT(CLAY_ID("VibrationSetting"), CLAY_STRING(vibration_text),
                     CLAY_TEXT_CONFIG(.textColor = {0, 0, 0, 1}, .fontSize = 12));
        }
        
        render_footer("UP/DOWN: Navigate", "SELECT: Toggle");
    }
}

static void button_up_handler(uint8_t pin_no, uint8_t button_action) {
    handle_navigation(0, button_action == APP_BUTTON_PUSH);
}

static void button_down_handler(uint8_t pin_no, uint8_t button_action) {
    handle_navigation(1, button_action == APP_BUTTON_PUSH);
}

static void button_select_handler(uint8_t pin_no, uint8_t button_action) {
    handle_navigation(2, button_action == APP_BUTTON_PUSH);
}

static void button_back_handler(uint8_t pin_no, uint8_t button_action) {
    handle_navigation(3, button_action == APP_BUTTON_PUSH);
}

static void handle_navigation(uint8_t button, bool pressed) {
    if (!pressed) return;  // Only handle button press, not release
    
    g_fitness_tracker.last_interaction = app_timer_cnt_get();
    
    switch (g_fitness_tracker.current_screen) {
        case SCREEN_WORKOUT_SELECT:
            if (button == 0) {  // Up
                g_menu_selection = (g_menu_selection > 0) ? g_menu_selection - 1 : 3;
            } else if (button == 1) {  // Down
                g_menu_selection = (g_menu_selection < 3) ? g_menu_selection + 1 : 0;
            } else if (button == 2) {  // Select
                workout_start(&g_fitness_tracker, "Quick Workout");
                // Add some default exercises
                exercise_add(&g_fitness_tracker.current_workout, "Bench Press", 
                           EXERCISE_COMPOUND, 8, 800);  // 80kg
                exercise_add(&g_fitness_tracker.current_workout, "Squats", 
                           EXERCISE_COMPOUND, 10, 1000); // 100kg
            }
            break;
            
        case SCREEN_EXERCISE_LIST:
            if (button == 2) {  // Select
                if (g_fitness_tracker.current_workout.total_exercises > 0) {
                    exercise_start(&g_fitness_tracker, 0);
                    set_start(&g_fitness_tracker);
                }
            } else if (button == 3) {  // Back
                workout_end(&g_fitness_tracker);
            }
            break;
            
        case SCREEN_ACTIVE_SET:
            if (button == 0) {  // Up - perfect rep
                set_add_rep(&g_fitness_tracker, REP_QUALITY_PERFECT);
            } else if (button == 1) {  // Down - partial rep
                set_add_rep(&g_fitness_tracker, REP_QUALITY_PARTIAL);
            } else if (button == 2) {  // Select - complete set
                uint8_t ex_idx = g_fitness_tracker.current_workout.current_exercise;
                ExerciseSession* exercise = &g_fitness_tracker.current_workout.exercises[ex_idx];
                set_complete(&g_fitness_tracker, exercise->exercise.target_weight);
            } else if (button == 3) {  // Back
                exercise_complete(&g_fitness_tracker);
            }
            break;
            
        case SCREEN_REST_TIMER:
            if (button == 2) {  // Select - skip rest
                rest_timer_stop(&g_fitness_tracker.rest_timer);
                g_fitness_tracker.current_screen = SCREEN_ACTIVE_SET;
                set_start(&g_fitness_tracker);
            } else if (button == 3) {  // Back
                rest_timer_stop(&g_fitness_tracker.rest_timer);
                g_fitness_tracker.current_screen = SCREEN_ACTIVE_SET;
            }
            break;
            
        case SCREEN_SLEEP:
            power_wake_up(&g_fitness_tracker);
            break;
    }
    
    display_mark_dirty(&g_fitness_tracker);
}

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
    
    // Initialize GPIO for e-paper control pins
    nrf_gpio_cfg_output(EPAPER_DC_PIN);
    nrf_gpio_cfg_output(EPAPER_RST_PIN);
    nrf_gpio_cfg_input(EPAPER_BUSY_PIN, NRF_GPIO_PIN_PULLUP);
    
    // Initialize vibration motor pin
    nrf_gpio_cfg_output(VIBRATION_PIN);
    nrf_gpio_pin_clear(VIB
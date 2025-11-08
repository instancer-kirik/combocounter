#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>

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
#include "nrf_drv_saadc.h"

// Application includes
#include "simple_combo_core.h"
#include "epaper_display.h"

// Hardware configuration for nRF52840
#define EPAPER_CS_PIN       8
#define EPAPER_DC_PIN       9
#define EPAPER_RST_PIN      10
#define EPAPER_BUSY_PIN     11
#define EPAPER_SCK_PIN      3
#define EPAPER_MOSI_PIN     4

#define BUTTON_UP_PIN       13      // Navigate up / Quality up
#define BUTTON_DOWN_PIN     14      // Navigate down / Quality down
#define BUTTON_SELECT_PIN   15      // Increment / Confirm
#define BUTTON_BACK_PIN     16      // Back / Settings

#define VIBRATION_PIN       17
#define BATTERY_ADC_PIN     2

// Timing constants
#define UPDATE_INTERVAL_MS      100    // 100ms main loop
#define DISPLAY_REFRESH_MS      2000   // 2 second max refresh for e-ink
#define BUTTON_DEBOUNCE_MS      50     // 50ms debounce
#define BUTTON_HOLD_MS          1000   // 1 second hold for settings
#define SLEEP_TIMEOUT_MS        300000 // 5 minutes to sleep
#define BATTERY_CHECK_MS        60000  // Check battery every minute

// Screen definitions
typedef enum {
    SCREEN_COUNTER,
    SCREEN_SETTINGS,
    SCREEN_STATS,
    SCREEN_SLEEP
} ScreenType;

// Global state
static ComboDevice g_device;
static EpaperDisplay g_display;
static nrf_drv_spi_t g_spi = NRF_DRV_SPI_INSTANCE(0);

static ScreenType g_current_screen = SCREEN_COUNTER;
static uint32_t g_last_interaction = 0;
static uint32_t g_last_display_update = 0;
static uint8_t g_battery_level = 100;
static bool g_display_dirty = true;

// Button state tracking
static uint32_t g_button_press_time[4] = {0};
static bool g_button_held[4] = {false};

// App timer instances
APP_TIMER_DEF(m_update_timer);
APP_TIMER_DEF(m_battery_timer);

// SPI instance for e-paper
static bool g_spi_initialized = false;

// Function prototypes
static void timers_init(void);
static void buttons_init(void);
static void spi_init(void);
static void hardware_init(void);
static void power_management_init(void);
static void log_init(void);

static void update_timer_handler(void * p_context);
static void battery_timer_handler(void * p_context);

static void button_up_handler(uint8_t pin_no, uint8_t button_action);
static void button_down_handler(uint8_t pin_no, uint8_t button_action);
static void button_select_handler(uint8_t pin_no, uint8_t button_action);
static void button_back_handler(uint8_t pin_no, uint8_t button_action);

static void handle_button_press(uint8_t button_id);
static void handle_button_hold(uint8_t button_id);
static void handle_button_release(uint8_t button_id);

static void render_current_screen(void);
static void render_counter_screen(void);
static void render_settings_screen(void);
static void render_stats_screen(void);

static void setup_default_counters(void);
static void vibrate_feedback(uint16_t duration_ms);
static uint8_t read_battery_level(void);
static void check_sleep_conditions(void);
static void enter_sleep_mode(void);
static void wake_from_sleep(void);

// Main function
int main(void)
{
    // Initialize logging first
    log_init();
    NRF_LOG_INFO("Combo Chracker Embedded Starting...");

    // Initialize hardware
    hardware_init();
    spi_init();
    power_management_init();
    timers_init();
    buttons_init();

    // Initialize combo device
    combo_device_init(&g_device);
    setup_default_counters();

    // Initialize e-paper display
    if (!epaper_init(&g_display)) {
        NRF_LOG_ERROR("Failed to initialize e-paper display");
        // Continue anyway - maybe display will work later
    }

    // Clear display and show startup screen
    epaper_clear(&g_display);
    epaper_draw_text(&g_display, "Combo Chracker", 10, 10, FONT_LARGE, EPAPER_COLOR_BLACK, ALIGN_LEFT);
    epaper_draw_text(&g_display, "Starting...", 10, 40, FONT_MEDIUM, EPAPER_COLOR_BLACK, ALIGN_LEFT);
    epaper_update(&g_display, UPDATE_FULL);
    
    nrf_delay_ms(2000); // Show startup message

    // Start timers
    app_timer_start(m_update_timer, APP_TIMER_TICKS(UPDATE_INTERVAL_MS), NULL);
    app_timer_start(m_battery_timer, APP_TIMER_TICKS(BATTERY_CHECK_MS), NULL);

    g_last_interaction = app_timer_cnt_get();
    g_display_dirty = true;

    NRF_LOG_INFO("Combo Chracker Ready!");

    // Main loop
    while (true) {
        // Process scheduled events
        app_sched_execute();
        
        // Process logs
        NRF_LOG_PROCESS();
        
        // Update display if needed
        uint32_t current_time = app_timer_cnt_get();
        if (g_display_dirty && 
            (current_time - g_last_display_update) > APP_TIMER_TICKS(DISPLAY_REFRESH_MS)) {
            render_current_screen();
            g_last_display_update = current_time;
            g_display_dirty = false;
        }

        // Check sleep conditions
        check_sleep_conditions();

        // Enter low power mode
        nrf_pwr_mgmt_run();
    }
}

// Timer handlers
static void update_timer_handler(void * p_context)
{
    UNUSED_PARAMETER(p_context);
    
    // Update device logic
    combo_device_update(&g_device, UPDATE_INTERVAL_MS / 1000.0f);
    
    // Check for button holds
    uint32_t current_time = app_timer_cnt_get();
    for (int i = 0; i < 4; i++) {
        if (g_button_press_time[i] > 0 && !g_button_held[i]) {
            if ((current_time - g_button_press_time[i]) > APP_TIMER_TICKS(BUTTON_HOLD_MS)) {
                handle_button_hold(i);
                g_button_held[i] = true;
            }
        }
    }
}

static void battery_timer_handler(void * p_context)
{
    UNUSED_PARAMETER(p_context);
    g_battery_level = read_battery_level();
}

// Button handlers
static void button_up_handler(uint8_t pin_no, uint8_t button_action)
{
    if (button_action == APP_BUTTON_PUSH) {
        g_button_press_time[0] = app_timer_cnt_get();
        g_button_held[0] = false;
        handle_button_press(0);
    } else if (button_action == APP_BUTTON_RELEASE) {
        handle_button_release(0);
    }
}

static void button_down_handler(uint8_t pin_no, uint8_t button_action)
{
    if (button_action == APP_BUTTON_PUSH) {
        g_button_press_time[1] = app_timer_cnt_get();
        g_button_held[1] = false;
        handle_button_press(1);
    } else if (button_action == APP_BUTTON_RELEASE) {
        handle_button_release(1);
    }
}

static void button_select_handler(uint8_t pin_no, uint8_t button_action)
{
    if (button_action == APP_BUTTON_PUSH) {
        g_button_press_time[2] = app_timer_cnt_get();
        g_button_held[2] = false;
        handle_button_press(2);
    } else if (button_action == APP_BUTTON_RELEASE) {
        handle_button_release(2);
    }
}

static void button_back_handler(uint8_t pin_no, uint8_t button_action)
{
    if (button_action == APP_BUTTON_PUSH) {
        g_button_press_time[3] = app_timer_cnt_get();
        g_button_held[3] = false;
        handle_button_press(3);
    } else if (button_action == APP_BUTTON_RELEASE) {
        handle_button_release(3);
    }
}

// Button logic
static void handle_button_press(uint8_t button_id)
{
    g_last_interaction = app_timer_cnt_get();
    
    if (g_current_screen == SCREEN_SLEEP) {
        wake_from_sleep();
        return;
    }
    
    vibrate_feedback(50); // Short vibration feedback
    
    switch (g_current_screen) {
        case SCREEN_COUNTER:
            switch (button_id) {
                case 0: // UP
                    device_next_counter(&g_device);
                    break;
                case 1: // DOWN
                    device_prev_counter(&g_device);
                    break;
                case 2: // SELECT
                    {
                        Counter* counter = device_get_current_counter(&g_device);
                        if (counter) {
                            counter_increment(counter, QUALITY_GOOD);
                        }
                    }
                    break;
                case 3: // BACK
                    {
                        Counter* counter = device_get_current_counter(&g_device);
                        if (counter && counter->count > 0) {
                            counter_decrement(counter, 1);
                        }
                    }
                    break;
            }
            break;
            
        case SCREEN_SETTINGS:
            // Handle settings navigation
            break;
            
        case SCREEN_STATS:
            if (button_id == 3) { // BACK
                g_current_screen = SCREEN_COUNTER;
            }
            break;
            
        default:
            break;
    }
    
    g_display_dirty = true;
}

static void handle_button_hold(uint8_t button_id)
{
    vibrate_feedback(100); // Longer vibration for hold
    
    switch (button_id) {
        case 3: // BACK hold - go to settings
            if (g_current_screen == SCREEN_COUNTER) {
                g_current_screen = SCREEN_SETTINGS;
            } else if (g_current_screen == SCREEN_SETTINGS) {
                g_current_screen = SCREEN_COUNTER;
            }
            break;
            
        case 2: // SELECT hold - go to stats
            if (g_current_screen == SCREEN_COUNTER) {
                g_current_screen = SCREEN_STATS;
            }
            break;
            
        default:
            break;
    }
    
    g_display_dirty = true;
}

static void handle_button_release(uint8_t button_id)
{
    g_button_press_time[button_id] = 0;
    g_button_held[button_id] = false;
}

// Rendering functions
static void render_current_screen(void)
{
    if (g_current_screen == SCREEN_SLEEP) {
        return; // Don't update display when sleeping
    }
    
    epaper_clear(&g_display);
    
    // Draw header with battery and current time
    char header[32];
    snprintf(header, sizeof(header), "Battery: %d%%", g_battery_level);
    epaper_draw_text(&g_display, header, EPAPER_WIDTH - 100, 5, FONT_SMALL, EPAPER_COLOR_BLACK, ALIGN_RIGHT);
    
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
        default:
            break;
    }
    
    epaper_update(&g_display, UPDATE_PARTIAL);
}

static void render_counter_screen(void)
{
    Counter* current = device_get_current_counter(&g_device);
    if (!current) {
        epaper_draw_text(&g_display, "No Counters", 10, 50, FONT_LARGE, EPAPER_COLOR_BLACK, ALIGN_LEFT);
        return;
    }
    
    // Draw counter name
    epaper_draw_text(&g_display, current->label, 10, 30, FONT_MEDIUM, EPAPER_COLOR_BLACK, ALIGN_LEFT);
    
    // Draw current count (large)
    char count_str[16];
    snprintf(count_str, sizeof(count_str), "%ld", current->count);
    epaper_draw_text(&g_display, count_str, 10, 60, FONT_XLARGE, EPAPER_COLOR_BLACK, ALIGN_LEFT);
    
    // Draw multiplier if applicable
    if (current->type == COUNTER_TYPE_COMBO && current->multiplier > 1.0f) {
        char mult_str[16];
        snprintf(mult_str, sizeof(mult_str), "x%.1f", current->multiplier);
        epaper_draw_text(&g_display, mult_str, 150, 70, FONT_LARGE, EPAPER_COLOR_RED, ALIGN_LEFT);
    }
    
    // Draw total
    char total_str[32];
    snprintf(total_str, sizeof(total_str), "Total: %ld", current->total);
    epaper_draw_text(&g_display, total_str, 10, 100, FONT_SMALL, EPAPER_COLOR_BLACK, ALIGN_LEFT);
    
    // Draw max combo
    if (current->max_combo > 0) {
        char max_str[32];
        snprintf(max_str, sizeof(max_str), "Best: %ld", current->max_combo);
        epaper_draw_text(&g_display, max_str, 10, 120, FONT_SMALL, EPAPER_COLOR_BLACK, ALIGN_LEFT);
    }
    
    // Draw counter selection indicator
    if (g_device.counter_count > 1) {
        char indicator[16];
        snprintf(indicator, sizeof(indicator), "(%d/%d)", g_device.current_counter + 1, g_device.counter_count);
        epaper_draw_text(&g_display, indicator, EPAPER_WIDTH - 60, 30, FONT_SMALL, EPAPER_COLOR_BLACK, ALIGN_RIGHT);
    }
    
    // Draw button hints
    epaper_draw_text(&g_display, "UP/DN:Switch  SEL:+1  BACK:-1", 5, EPAPER_HEIGHT - 15, FONT_SMALL, EPAPER_COLOR_BLACK, ALIGN_LEFT);
}

static void render_settings_screen(void)
{
    epaper_draw_text(&g_display, "Settings", 10, 30, FONT_LARGE, EPAPER_COLOR_BLACK, ALIGN_LEFT);
    epaper_draw_text(&g_display, "Reset Counters", 10, 60, FONT_MEDIUM, EPAPER_COLOR_BLACK, ALIGN_LEFT);
    epaper_draw_text(&g_display, "Sleep Mode", 10, 80, FONT_MEDIUM, EPAPER_COLOR_BLACK, ALIGN_LEFT);
    epaper_draw_text(&g_display, "Device Info", 10, 100, FONT_MEDIUM, EPAPER_COLOR_BLACK, ALIGN_LEFT);
    
    epaper_draw_text(&g_display, "BACK:Exit", 5, EPAPER_HEIGHT - 15, FONT_SMALL, EPAPER_COLOR_BLACK, ALIGN_LEFT);
}

static void render_stats_screen(void)
{
    epaper_draw_text(&g_display, "Statistics", 10, 30, FONT_LARGE, EPAPER_COLOR_BLACK, ALIGN_LEFT);
    
    char stats[64];
    snprintf(stats, sizeof(stats), "Total Presses: %lu", g_device.total_button_presses);
    epaper_draw_text(&g_display, stats, 10, 60, FONT_MEDIUM, EPAPER_COLOR_BLACK, ALIGN_LEFT);
    
    snprintf(stats, sizeof(stats), "Uptime: %lu sec", g_device.device_uptime_sec);
    epaper_draw_text(&g_display, stats, 10, 80, FONT_MEDIUM, EPAPER_COLOR_BLACK, ALIGN_LEFT);
    
    epaper_draw_text(&g_display, "BACK:Exit", 5, EPAPER_HEIGHT - 15, FONT_SMALL, EPAPER_COLOR_BLACK, ALIGN_LEFT);
}

// Device setup
static void setup_default_counters(void)
{
    // Create a simple rep counter
    counter_add(&g_device, "Reps", COUNTER_TYPE_SIMPLE);
    counter_configure_simple(&g_device.counters[0], "Reps", 1);
    
    // Create a combo counter for streaks
    counter_add(&g_device, "Combo", COUNTER_TYPE_COMBO);
    counter_configure_combo(&g_device.counters[1], "Combo", 1, 3.0f, 0.1f);
    
    counter_set_active(&g_device, 0);
}

// Hardware initialization
static void hardware_init(void)
{
    // Initialize GPIO
    ret_code_t err_code = nrf_drv_gpiote_init();
    APP_ERROR_CHECK(err_code);
    
    // Configure vibration motor pin
    nrf_gpio_cfg_output(VIBRATION_PIN);
    nrf_gpio_pin_clear(VIBRATION_PIN);
    
    // Configure battery ADC
    nrf_drv_saadc_config_t saadc_config = NRF_DRV_SAADC_DEFAULT_CONFIG;
    err_code = nrf_drv_saadc_init(&saadc_config, NULL);
    APP_ERROR_CHECK(err_code);
    
    nrf_saadc_channel_config_t channel_config =
        NRF_DRV_SAADC_DEFAULT_CHANNEL_CONFIG_SE(NRF_SAADC_INPUT_AIN0 + BATTERY_ADC_PIN);
    err_code = nrf_drv_saadc_channel_init(0, &channel_config);
    APP_ERROR_CHECK(err_code);
}

static void spi_init(void)
{
    nrf_drv_spi_config_t spi_config = NRF_DRV_SPI_DEFAULT_CONFIG;
    spi_config.ss_pin   = EPAPER_CS_PIN;
    spi_config.miso_pin = NRF_DRV_SPI_PIN_NOT_USED;
    spi_config.mosi_pin = EPAPER_MOSI_PIN;
    spi_config.sck_pin  = EPAPER_SCK_PIN;
    spi_config.frequency = NRF_DRV_SPI_FREQ_4M;
    spi_config.mode = NRF_DRV_SPI_MODE_0;
    
    ret_code_t err_code = nrf_drv_spi_init(&g_spi, &spi_config, NULL, NULL);
    APP_ERROR_CHECK(err_code);
    
    g_spi_initialized = true;
}

static void timers_init(void)
{
    ret_code_t err_code = app_timer_init();
    APP_ERROR_CHECK(err_code);
    
    err_code = app_timer_create(&m_update_timer, APP_TIMER_MODE_REPEATED, update_timer_handler);
    APP_ERROR_CHECK(err_code);
    
    err_code = app_timer_create(&m_battery_timer, APP_TIMER_MODE_REPEATED, battery_timer_handler);
    APP_ERROR_CHECK(err_code);
}

static void buttons_init(void)
{
    ret_code_t err_code;
    
    static app_button_cfg_t buttons[] = {
        {BUTTON_UP_PIN,     false, BUTTON_PULL, button_up_handler},
        {BUTTON_DOWN_PIN,   false, BUTTON_PULL, button_down_handler},
        {BUTTON_SELECT_PIN, false, BUTTON_PULL, button_select_handler},
        {BUTTON_BACK_PIN,   false, BUTTON_PULL, button_back_handler},
    };
    
    err_code = app_button_init(buttons, ARRAY_SIZE(buttons), APP_TIMER_TICKS(BUTTON_DEBOUNCE_MS));
    APP_ERROR_CHECK(err_code);
    
    err_code = app_button_enable();
    APP_ERROR_CHECK(err_code);
}

static void power_management_init(void)
{
    ret_code_t err_code = nrf_pwr_mgmt_init();
    APP_ERROR_CHECK(err_code);
}

static void log_init(void)
{
    ret_code_t err_code = NRF_LOG_INIT(NULL);
    APP_ERROR_CHECK(err_code);
    
    NRF_LOG_DEFAULT_BACKENDS_INIT();
}

// Utility functions
static void vibrate_feedback(uint16_t duration_ms)
{
    nrf_gpio_pin_set(VIBRATION_PIN);
    nrf_delay_ms(duration_ms);
    nrf_gpio_pin_clear(VIBRATION_PIN);
}

static uint8_t read_battery_level(void)
{
    nrf_saadc_value_t adc_result;
    ret_code_t err_code = nrf_drv_saadc_sample_convert(0, &adc_result);
    
    if (err_code == NRF_SUCCESS) {
        // Convert ADC reading to battery percentage (simplified)
        // Assuming 3.7V nominal, 3.0V empty, 4.2V full
        uint16_t voltage_mv = (adc_result * 3600) / 1024; // Convert to mV
        if (voltage_mv >= 4200) return 100;
        if (voltage_mv <= 3000) return 0;
        return ((voltage_mv - 3000) * 100) / 1200;
    }
    
    return g_battery_level; // Return last known value on error
}

static void check_sleep_conditions(void)
{
    uint32_t current_time = app_timer_cnt_get();
    uint32_t idle_time = current_time - g_last_interaction;
    
    if (idle_time > APP_TIMER_TICKS(SLEEP_TIMEOUT_MS) && g_current_screen != SCREEN_SLEEP) {
        enter_sleep_mode();
    }
}

static void enter_sleep_mode(void)
{
    NRF_LOG_INFO("Entering sleep mode");
    
    g_current_screen = SCREEN_SLEEP;
    
    // Clear display and show sleep message
    epaper_clear(&g_display);
    epaper_draw_text(&g_display, "Sleeping...", 10, 70, FONT_LARGE, EPAPER_COLOR_BLACK, ALIGN_LEFT);
    epaper_draw_text(&g_display, "Press any button", 10, 100, FONT_MEDIUM, EPAPER_COLOR_BLACK, ALIGN_LEFT);
    epaper_update(&g_display, UPDATE_FULL);
    
    // Put display to sleep
    epaper_sleep(&g_display);
    
    // Stop non-essential timers
    app_timer_stop(m_update_timer);
}

static void wake_from_sleep(void)
{
    NRF_LOG_INFO("Waking from sleep");
    
    // Wake display
    epaper_wake(&g_display);
    
    // Restart timers
    app_timer_start(m_update_timer, APP_TIMER_TICKS(UPDATE_INTERVAL_MS), NULL);
    
    g_current_screen = SCREEN_COUNTER;
    g_display_dirty = true;
    g_last_interaction = app_timer_cnt_get();
}

// Low-level SPI functions for e-paper display
bool epaper_spi_init(void)
{
    return g_spi_initialized;
}

void epaper_spi_write_byte(uint8_t data)
{
    if (g_spi_initialized) {
        nrf_drv_spi_transfer(&g_spi, &data, 1, NULL, 0);
    }
}

void epaper_spi_write_buffer(const uint8_t* buffer, uint16_t length)
{
    if (g_spi_initialized) {
        nrf_drv_spi_transfer(&g_spi, buffer, length, NULL, 0);
    }
}

void epaper_gpio_dc_high(void) { nrf_gpio_pin_set(EPAPER_DC_PIN); }
void epaper_gpio_dc_low(void) { nrf_gpio_pin_clear(EPAPER_DC_PIN); }
void epaper_gpio_reset_high(void) { nrf_gpio_pin_set(EPAPER_RST_PIN); }
void epaper_gpio_reset_low(void) { nrf_gpio_pin_clear(EPAPER_RST_PIN); }
void epaper_gpio_cs_high(void) { nrf_gpio_pin_set(EPAPER_CS_PIN); }
void epaper_gpio_cs_low(void) { nrf_gpio_pin_clear(EPAPER_CS_PIN); }
bool epaper_gpio_busy_read(void) { return nrf_gpio_pin_read(EPAPER_BUSY_PIN); }
void epaper_delay_ms(uint32_t ms) { nrf_delay_ms(ms); }
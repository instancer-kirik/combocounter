#ifndef EPAPER_DISPLAY_H
#define EPAPER_DISPLAY_H

#include <stdint.h>
#include <stdbool.h>

// Display specifications for 2.15" e-Paper HAT+ (G)
#define EPAPER_WIDTH 296
#define EPAPER_HEIGHT 160
#define EPAPER_BUFFER_SIZE ((EPAPER_WIDTH * EPAPER_HEIGHT) / 4)  // 2 bits per pixel

// Color definitions (2 bits per pixel)
typedef enum {
    EPAPER_COLOR_BLACK = 0x00,   // 00
    EPAPER_COLOR_WHITE = 0x01,   // 01
    EPAPER_COLOR_RED = 0x02,     // 10
    EPAPER_COLOR_YELLOW = 0x03   // 11
} EpaperColor;

// Font sizes
typedef enum {
    FONT_SMALL = 8,
    FONT_MEDIUM = 12,
    FONT_LARGE = 16,
    FONT_XLARGE = 24
} FontSize;

// Text alignment
typedef enum {
    ALIGN_LEFT,
    ALIGN_CENTER,
    ALIGN_RIGHT
} TextAlign;

// Display update modes
typedef enum {
    UPDATE_FULL,        // Full refresh (slow but complete)
    UPDATE_PARTIAL,     // Partial refresh (faster but may have ghosting)
    UPDATE_FAST         // Fast refresh (fastest but limited colors)
} UpdateMode;

// Rectangle structure
typedef struct {
    uint16_t x;
    uint16_t y;
    uint16_t width;
    uint16_t height;
} EpaperRect;

// Point structure
typedef struct {
    uint16_t x;
    uint16_t y;
} EpaperPoint;

// Display context
typedef struct {
    uint8_t* framebuffer;       // 2-bit framebuffer
    bool dirty;                 // Needs refresh
    bool sleeping;              // Display in sleep mode
    uint32_t last_update;       // Timestamp of last update
    UpdateMode default_mode;    // Default update mode
} EpaperDisplay;

// Core display functions
bool epaper_init(EpaperDisplay* display);
void epaper_deinit(EpaperDisplay* display);
bool epaper_update(EpaperDisplay* display, UpdateMode mode);
void epaper_clear(EpaperDisplay* display);
void epaper_sleep(EpaperDisplay* display);
void epaper_wake(EpaperDisplay* display);

// Drawing primitives
void epaper_set_pixel(EpaperDisplay* display, uint16_t x, uint16_t y, EpaperColor color);
EpaperColor epaper_get_pixel(EpaperDisplay* display, uint16_t x, uint16_t y);
void epaper_draw_line(EpaperDisplay* display, uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, EpaperColor color);
void epaper_draw_rect(EpaperDisplay* display, EpaperRect rect, EpaperColor color, bool filled);
void epaper_draw_circle(EpaperDisplay* display, uint16_t cx, uint16_t cy, uint16_t radius, EpaperColor color, bool filled);

// Text rendering
void epaper_draw_text(EpaperDisplay* display, const char* text, uint16_t x, uint16_t y, 
                     FontSize size, EpaperColor color, TextAlign align);
void epaper_draw_text_in_rect(EpaperDisplay* display, const char* text, EpaperRect rect,
                             FontSize size, EpaperColor color, TextAlign align);
uint16_t epaper_get_text_width(const char* text, FontSize size);
uint16_t epaper_get_text_height(FontSize size);

// Fitness-specific UI elements
void epaper_draw_progress_bar(EpaperDisplay* display, EpaperRect rect, 
                             uint16_t current, uint16_t max, EpaperColor fg_color, EpaperColor bg_color);
void epaper_draw_rep_counter(EpaperDisplay* display, uint16_t x, uint16_t y, 
                            uint16_t reps, uint16_t target, EpaperColor color);
void epaper_draw_timer(EpaperDisplay* display, uint16_t x, uint16_t y, 
                      uint16_t minutes, uint16_t seconds, EpaperColor color);
void epaper_draw_weight_display(EpaperDisplay* display, uint16_t x, uint16_t y,
                               uint16_t weight_kg_x10, EpaperColor color);

// Battery and status indicators
void epaper_draw_battery(EpaperDisplay* display, uint16_t x, uint16_t y, 
                        uint8_t level, bool charging, EpaperColor color);
void epaper_draw_bluetooth_icon(EpaperDisplay* display, uint16_t x, uint16_t y, 
                               bool connected, EpaperColor color);

// Layout helpers
void epaper_draw_header(EpaperDisplay* display, const char* title, uint8_t battery_level, 
                       bool bluetooth_connected);
void epaper_draw_footer(EpaperDisplay* display, const char* left_text, const char* right_text);

// Screen layouts for fitness app
void epaper_render_workout_select(EpaperDisplay* display, const char** workout_names, 
                                 uint8_t count, uint8_t selected);
void epaper_render_exercise_list(EpaperDisplay* display, const char** exercise_names,
                                uint8_t count, uint8_t selected, uint8_t completed);
void epaper_render_active_set(EpaperDisplay* display, const char* exercise_name,
                             uint16_t current_reps, uint16_t target_reps, 
                             uint16_t weight_kg_x10, uint8_t set_number);
void epaper_render_rest_timer(EpaperDisplay* display, uint16_t seconds_remaining,
                             uint16_t total_seconds, const char* next_exercise);
void epaper_render_statistics(EpaperDisplay* display, uint32_t total_workouts,
                             uint32_t total_sets, uint32_t total_reps,
                             uint32_t total_volume_kg_x10);
void epaper_render_settings(EpaperDisplay* display, const char** settings,
                           bool* values, uint8_t count, uint8_t selected);

// Power management
bool epaper_should_sleep(EpaperDisplay* display, uint32_t idle_timeout_ms);
void epaper_set_update_mode(EpaperDisplay* display, UpdateMode mode);

// Low-level SPI interface (to be implemented per hardware)
bool epaper_spi_init(void);
void epaper_spi_write_byte(uint8_t data);
void epaper_spi_write_buffer(const uint8_t* buffer, uint16_t length);
void epaper_gpio_dc_high(void);
void epaper_gpio_dc_low(void);
void epaper_gpio_reset_high(void);
void epaper_gpio_reset_low(void);
void epaper_gpio_cs_high(void);
void epaper_gpio_cs_low(void);
bool epaper_gpio_busy_read(void);
void epaper_delay_ms(uint32_t ms);

// Memory management helpers
bool epaper_allocate_framebuffer(EpaperDisplay* display);
void epaper_free_framebuffer(EpaperDisplay* display);
void epaper_mark_dirty(EpaperDisplay* display);
void epaper_mark_clean(EpaperDisplay* display);

// Utility functions
uint8_t epaper_pack_pixels(EpaperColor c0, EpaperColor c1, EpaperColor c2, EpaperColor c3);
void epaper_unpack_pixels(uint8_t packed, EpaperColor* colors);
bool epaper_is_valid_coordinate(uint16_t x, uint16_t y);
EpaperRect epaper_clip_rect(EpaperRect rect);

// Error codes
typedef enum {
    EPAPER_OK = 0,
    EPAPER_ERROR_INIT_FAILED,
    EPAPER_ERROR_MEMORY_ALLOCATION,
    EPAPER_ERROR_SPI_FAILED,
    EPAPER_ERROR_TIMEOUT,
    EPAPER_ERROR_INVALID_PARAMETER
} EpaperError;

#endif // EPAPER_DISPLAY_H
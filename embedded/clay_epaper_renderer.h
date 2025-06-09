#ifndef CLAY_EPAPER_RENDERER_H
#define CLAY_EPAPER_RENDERER_H

#include <stdint.h>
#include <stdbool.h>
#include "../src/clay.h"

// E-Paper display specifications
#define EPAPER_WIDTH 296
#define EPAPER_HEIGHT 160
#define EPAPER_BUFFER_SIZE ((EPAPER_WIDTH * EPAPER_HEIGHT) / 4)  // 2 bits per pixel

// E-Paper color mapping (2 bits per pixel)
typedef enum {
    EPAPER_COLOR_BLACK = 0x00,   // 00
    EPAPER_COLOR_WHITE = 0x01,   // 01  
    EPAPER_COLOR_RED = 0x02,     // 10
    EPAPER_COLOR_YELLOW = 0x03   // 11
} EpaperColor;

// Font data structure for embedded font storage
typedef struct {
    const uint8_t* bitmap;      // Font bitmap data
    uint8_t width;              // Character width
    uint8_t height;             // Character height
    uint8_t first_char;         // First ASCII character
    uint8_t char_count;         // Number of characters
} EpaperFont;

// Clay-compatible e-Paper context
typedef struct {
    uint8_t* framebuffer;                   // 2-bit framebuffer
    bool dirty;                             // Needs refresh
    uint32_t last_update;                   // Last update timestamp
    
    // Font storage
    const EpaperFont* fonts[4];             // Small, medium, large, xlarge
    uint8_t current_font_id;
    
    // Clay integration
    Clay_Arena clay_arena;                  // Clay memory arena
    Clay_Dimensions screen_dimensions;      // Screen size for Clay
    
    // Render batching
    bool batch_mode;                        // Collecting render commands
    uint32_t render_command_count;
    
    // Color mapping
    EpaperColor current_text_color;
    EpaperColor current_bg_color;
    
    // Clipping
    Clay_BoundingBox clip_rect;
    bool clipping_enabled;
    
} ClayEpaperContext;

// Initialization and cleanup
bool clay_epaper_init(ClayEpaperContext* ctx, void* memory, size_t memory_size);
void clay_epaper_deinit(ClayEpaperContext* ctx);
bool clay_epaper_allocate_framebuffer(ClayEpaperContext* ctx);
void clay_epaper_free_framebuffer(ClayEpaperContext* ctx);

// Clay integration functions
void clay_epaper_begin_frame(ClayEpaperContext* ctx);
void clay_epaper_end_frame(ClayEpaperContext* ctx);
void clay_epaper_render_commands(ClayEpaperContext* ctx, Clay_RenderCommandArray commands);

// Clay callback implementations  
Clay_Dimensions clay_epaper_measure_text(Clay_String* text, Clay_TextElementConfig* config);
void clay_epaper_set_text_config(ClayEpaperContext* ctx, Clay_TextElementConfig* config);

// Font management
bool clay_epaper_load_font(ClayEpaperContext* ctx, uint8_t font_id, const EpaperFont* font);
const EpaperFont* clay_epaper_get_font(ClayEpaperContext* ctx, uint8_t font_id);
uint8_t clay_epaper_map_font_size(uint16_t clay_font_size);

// Color conversion utilities
EpaperColor clay_epaper_convert_color(Clay_Color clay_color);
Clay_Color clay_epaper_get_contrast_color(Clay_Color background);
bool clay_epaper_colors_equal(Clay_Color c1, Clay_Color c2);

// Low-level drawing primitives
void clay_epaper_clear(ClayEpaperContext* ctx, EpaperColor color);
void clay_epaper_set_pixel(ClayEpaperContext* ctx, uint16_t x, uint16_t y, EpaperColor color);
EpaperColor clay_epaper_get_pixel(ClayEpaperContext* ctx, uint16_t x, uint16_t y);

// Shape drawing
void clay_epaper_draw_rect(ClayEpaperContext* ctx, Clay_BoundingBox rect, EpaperColor color, bool filled);
void clay_epaper_draw_rounded_rect(ClayEpaperContext* ctx, Clay_BoundingBox rect, 
                                   Clay_CornerRadius radius, EpaperColor color, bool filled);
void clay_epaper_draw_line(ClayEpaperContext* ctx, int16_t x0, int16_t y0, 
                          int16_t x1, int16_t y1, EpaperColor color);
void clay_epaper_draw_circle(ClayEpaperContext* ctx, int16_t cx, int16_t cy, 
                            uint16_t radius, EpaperColor color, bool filled);

// Text rendering
void clay_epaper_draw_text(ClayEpaperContext* ctx, Clay_String text, Clay_BoundingBox bounds,
                          Clay_TextElementConfig* config);
void clay_epaper_draw_char(ClayEpaperContext* ctx, char c, uint16_t x, uint16_t y, 
                          const EpaperFont* font, EpaperColor color);
uint16_t clay_epaper_get_char_width(char c, const EpaperFont* font);

// Border rendering
void clay_epaper_draw_border(ClayEpaperContext* ctx, Clay_BoundingBox bounds,
                            Clay_BorderElementConfig* config);

// Clipping
void clay_epaper_set_clip_rect(ClayEpaperContext* ctx, Clay_BoundingBox rect);
void clay_epaper_clear_clip_rect(ClayEpaperContext* ctx);
bool clay_epaper_point_in_clip(ClayEpaperContext* ctx, uint16_t x, uint16_t y);

// Render command processors
void clay_epaper_process_rectangle_command(ClayEpaperContext* ctx, Clay_RenderCommand* command);
void clay_epaper_process_text_command(ClayEpaperContext* ctx, Clay_RenderCommand* command);
void clay_epaper_process_border_command(ClayEpaperContext* ctx, Clay_RenderCommand* command);
void clay_epaper_process_scissor_start(ClayEpaperContext* ctx, Clay_RenderCommand* command);
void clay_epaper_process_scissor_end(ClayEpaperContext* ctx, Clay_RenderCommand* command);

// Display hardware interface (to be implemented per hardware)
bool clay_epaper_hardware_init(void);
void clay_epaper_hardware_deinit(void);
bool clay_epaper_hardware_update_full(const uint8_t* framebuffer);
bool clay_epaper_hardware_update_partial(const uint8_t* framebuffer, Clay_BoundingBox region);
void clay_epaper_hardware_sleep(void);
void clay_epaper_hardware_wake(void);

// Utility functions
bool clay_epaper_bounds_valid(Clay_BoundingBox bounds);
Clay_BoundingBox clay_epaper_clip_bounds(ClayEpaperContext* ctx, Clay_BoundingBox bounds);
uint16_t clay_epaper_pack_pixels(EpaperColor c0, EpaperColor c1, EpaperColor c2, EpaperColor c3);
void clay_epaper_unpack_pixels(uint16_t packed, EpaperColor* colors);

// Fitness-specific UI helpers (built on Clay)
void clay_epaper_draw_progress_bar(ClayEpaperContext* ctx, Clay_BoundingBox bounds,
                                  float progress, EpaperColor fg_color, EpaperColor bg_color);
void clay_epaper_draw_battery_icon(ClayEpaperContext* ctx, uint16_t x, uint16_t y,
                                  uint8_t level, bool charging, EpaperColor color);
void clay_epaper_draw_bluetooth_icon(ClayEpaperContext* ctx, uint16_t x, uint16_t y,
                                    bool connected, EpaperColor color);

// Memory and performance optimization
void clay_epaper_mark_dirty(ClayEpaperContext* ctx);
void clay_epaper_mark_clean(ClayEpaperContext* ctx);
bool clay_epaper_needs_update(ClayEpaperContext* ctx);
size_t clay_epaper_get_memory_usage(ClayEpaperContext* ctx);

// Error handling
typedef enum {
    CLAY_EPAPER_OK = 0,
    CLAY_EPAPER_ERROR_INIT_FAILED,
    CLAY_EPAPER_ERROR_MEMORY_ALLOCATION,
    CLAY_EPAPER_ERROR_INVALID_FONT,
    CLAY_EPAPER_ERROR_HARDWARE_FAILURE,
    CLAY_EPAPER_ERROR_INVALID_PARAMETER
} ClayEpaperError;

ClayEpaperError clay_epaper_get_last_error(void);
const char* clay_epaper_error_string(ClayEpaperError error);

// Default embedded fonts (to be defined in implementation)
extern const EpaperFont CLAY_EPAPER_FONT_SMALL_8x8;
extern const EpaperFont CLAY_EPAPER_FONT_MEDIUM_8x12;
extern const EpaperFont CLAY_EPAPER_FONT_LARGE_8x16;
extern const EpaperFont CLAY_EPAPER_FONT_XLARGE_12x24;

// Configuration constants
#define CLAY_EPAPER_DEFAULT_ARENA_SIZE (32 * 1024)     // 32KB for Clay
#define CLAY_EPAPER_MAX_RENDER_COMMANDS 256
#define CLAY_EPAPER_MAX_TEXT_CACHE_SIZE 128
#define CLAY_EPAPER_UPDATE_THRESHOLD_MS 100            // Minimum time between updates

// Fitness tracker specific layouts using Clay
typedef struct {
    ClayEpaperContext* ctx;
    Clay_String title;
    uint8_t battery_level;
    bool bluetooth_connected;
} ClayEpaperHeader;

typedef struct {
    ClayEpaperContext* ctx;
    Clay_String exercise_name;
    uint16_t current_reps;
    uint16_t target_reps;
    uint16_t weight_kg_x10;
    uint8_t set_number;
} ClayEpaperActiveSet;

typedef struct {
    ClayEpaperContext* ctx;
    uint16_t seconds_remaining;
    uint16_t total_seconds;
    Clay_String next_exercise;
} ClayEpaperRestTimer;

// High-level layout functions
void clay_epaper_render_header(ClayEpaperHeader* header);
void clay_epaper_render_active_set(ClayEpaperActiveSet* set_data);
void clay_epaper_render_rest_timer(ClayEpaperRestTimer* timer_data);
void clay_epaper_render_menu(ClayEpaperContext* ctx, Clay_String* items, 
                            uint8_t item_count, uint8_t selected_index);

#endif // CLAY_EPAPER_RENDERER_H
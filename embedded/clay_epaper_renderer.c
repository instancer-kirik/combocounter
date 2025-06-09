#include "clay_epaper_renderer.h"
#include <string.h>
#include <math.h>

// Static error state
static ClayEpaperError g_last_error = CLAY_EPAPER_OK;

// Default embedded fonts (8x8 bitmap fonts for simplicity)
// In a real implementation, these would contain actual font bitmap data
static const uint8_t font_8x8_bitmap[] = {
    // Simplified ASCII font data (would be much larger in reality)
    // This is just a placeholder structure
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // Space
    0x18, 0x3C, 0x3C, 0x18, 0x18, 0x00, 0x18, 0x00, // !
    // ... rest of ASCII characters would go here
};

const EpaperFont CLAY_EPAPER_FONT_SMALL_8x8 = {
    .bitmap = font_8x8_bitmap,
    .width = 8,
    .height = 8,
    .first_char = 32,  // Space
    .char_count = 95   // Printable ASCII
};

const EpaperFont CLAY_EPAPER_FONT_MEDIUM_8x12 = {
    .bitmap = font_8x8_bitmap,  // Would be different bitmap in real implementation
    .width = 8,
    .height = 12,
    .first_char = 32,
    .char_count = 95
};

const EpaperFont CLAY_EPAPER_FONT_LARGE_8x16 = {
    .bitmap = font_8x8_bitmap,
    .width = 8,
    .height = 16,
    .first_char = 32,
    .char_count = 95
};

const EpaperFont CLAY_EPAPER_FONT_XLARGE_12x24 = {
    .bitmap = font_8x8_bitmap,
    .width = 12,
    .height = 24,
    .first_char = 32,
    .char_count = 95
};

// Helper macros
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define CLAMP(x, min, max) MIN(MAX(x, min), max)

// Color conversion utilities
EpaperColor clay_epaper_convert_color(Clay_Color clay_color) {
    // Convert RGBA to 4-color e-paper palette
    float luminance = 0.299f * clay_color.r + 0.587f * clay_color.g + 0.114f * clay_color.b;
    
    // Check for specific colors first
    if (clay_color.r > 0.8f && clay_color.g < 0.3f && clay_color.b < 0.3f) {
        return EPAPER_COLOR_RED;  // Red-ish colors
    }
    if (clay_color.r > 0.8f && clay_color.g > 0.8f && clay_color.b < 0.3f) {
        return EPAPER_COLOR_YELLOW;  // Yellow-ish colors
    }
    
    // Use luminance for black/white
    return luminance > 0.5f ? EPAPER_COLOR_WHITE : EPAPER_COLOR_BLACK;
}

Clay_Color clay_epaper_get_contrast_color(Clay_Color background) {
    float luminance = 0.299f * background.r + 0.587f * background.g + 0.114f * background.b;
    return luminance > 0.5f ? 
        (Clay_Color){0.0f, 0.0f, 0.0f, 1.0f} :  // Black text on light background
        (Clay_Color){1.0f, 1.0f, 1.0f, 1.0f};   // White text on dark background
}

bool clay_epaper_colors_equal(Clay_Color c1, Clay_Color c2) {
    const float epsilon = 0.01f;
    return fabsf(c1.r - c2.r) < epsilon &&
           fabsf(c1.g - c2.g) < epsilon &&
           fabsf(c1.b - c2.b) < epsilon &&
           fabsf(c1.a - c2.a) < epsilon;
}

// Font management
uint8_t clay_epaper_map_font_size(uint16_t clay_font_size) {
    if (clay_font_size <= 10) return 0;      // Small
    else if (clay_font_size <= 14) return 1; // Medium
    else if (clay_font_size <= 18) return 2; // Large
    else return 3;                            // XLarge
}

bool clay_epaper_load_font(ClayEpaperContext* ctx, uint8_t font_id, const EpaperFont* font) {
    if (!ctx || font_id >= 4 || !font) {
        g_last_error = CLAY_EPAPER_ERROR_INVALID_PARAMETER;
        return false;
    }
    
    ctx->fonts[font_id] = font;
    return true;
}

const EpaperFont* clay_epaper_get_font(ClayEpaperContext* ctx, uint8_t font_id) {
    if (!ctx || font_id >= 4) {
        return &CLAY_EPAPER_FONT_SMALL_8x8;  // Default fallback
    }
    
    return ctx->fonts[font_id] ? ctx->fonts[font_id] : &CLAY_EPAPER_FONT_SMALL_8x8;
}

// Framebuffer management
bool clay_epaper_allocate_framebuffer(ClayEpaperContext* ctx) {
    if (!ctx) {
        g_last_error = CLAY_EPAPER_ERROR_INVALID_PARAMETER;
        return false;
    }
    
    if (ctx->framebuffer) {
        return true;  // Already allocated
    }
    
    // Allocate from Clay arena
    ctx->framebuffer = Clay__Array_Allocate_Arena(&ctx->clay_arena, EPAPER_BUFFER_SIZE, uint8_t);
    if (!ctx->framebuffer) {
        g_last_error = CLAY_EPAPER_ERROR_MEMORY_ALLOCATION;
        return false;
    }
    
    // Clear to white
    memset(ctx->framebuffer, 0x55, EPAPER_BUFFER_SIZE);  // 0x55 = all white pixels
    return true;
}

void clay_epaper_free_framebuffer(ClayEpaperContext* ctx) {
    if (ctx && ctx->framebuffer) {
        // Clay arena memory will be automatically freed when arena is reset
        ctx->framebuffer = NULL;
    }
}

// Initialization
bool clay_epaper_init(ClayEpaperContext* ctx, void* memory, size_t memory_size) {
    if (!ctx || !memory || memory_size < CLAY_EPAPER_DEFAULT_ARENA_SIZE) {
        g_last_error = CLAY_EPAPER_ERROR_INVALID_PARAMETER;
        return false;
    }
    
    // Clear context
    memset(ctx, 0, sizeof(ClayEpaperContext));
    
    // Initialize Clay arena
    ctx->clay_arena = Clay_CreateArenaWithCapacityAndMemory(memory_size, memory);
    
    // Set up screen dimensions
    ctx->screen_dimensions.width = EPAPER_WIDTH;
    ctx->screen_dimensions.height = EPAPER_HEIGHT;
    
    // Load default fonts
    clay_epaper_load_font(ctx, 0, &CLAY_EPAPER_FONT_SMALL_8x8);
    clay_epaper_load_font(ctx, 1, &CLAY_EPAPER_FONT_MEDIUM_8x12);
    clay_epaper_load_font(ctx, 2, &CLAY_EPAPER_FONT_LARGE_8x16);
    clay_epaper_load_font(ctx, 3, &CLAY_EPAPER_FONT_XLARGE_12x24);
    
    // Set defaults
    ctx->current_text_color = EPAPER_COLOR_BLACK;
    ctx->current_bg_color = EPAPER_COLOR_WHITE;
    ctx->clipping_enabled = false;
    
    // Initialize hardware
    if (!clay_epaper_hardware_init()) {
        g_last_error = CLAY_EPAPER_ERROR_HARDWARE_FAILURE;
        return false;
    }
    
    return clay_epaper_allocate_framebuffer(ctx);
}

void clay_epaper_deinit(ClayEpaperContext* ctx) {
    if (ctx) {
        clay_epaper_free_framebuffer(ctx);
        clay_epaper_hardware_deinit();
        memset(ctx, 0, sizeof(ClayEpaperContext));
    }
}

// Low-level pixel operations
void clay_epaper_set_pixel(ClayEpaperContext* ctx, uint16_t x, uint16_t y, EpaperColor color) {
    if (!ctx || !ctx->framebuffer || x >= EPAPER_WIDTH || y >= EPAPER_HEIGHT) {
        return;
    }
    
    if (ctx->clipping_enabled && !clay_epaper_point_in_clip(ctx, x, y)) {
        return;
    }
    
    uint32_t pixel_index = y * EPAPER_WIDTH + x;
    uint32_t byte_index = pixel_index / 4;
    uint32_t bit_offset = (pixel_index % 4) * 2;
    
    uint8_t mask = ~(0x03 << bit_offset);
    ctx->framebuffer[byte_index] = (ctx->framebuffer[byte_index] & mask) | 
                                   ((color & 0x03) << bit_offset);
    
    ctx->dirty = true;
}

EpaperColor clay_epaper_get_pixel(ClayEpaperContext* ctx, uint16_t x, uint16_t y) {
    if (!ctx || !ctx->framebuffer || x >= EPAPER_WIDTH || y >= EPAPER_HEIGHT) {
        return EPAPER_COLOR_WHITE;
    }
    
    uint32_t pixel_index = y * EPAPER_WIDTH + x;
    uint32_t byte_index = pixel_index / 4;
    uint32_t bit_offset = (pixel_index % 4) * 2;
    
    return (EpaperColor)((ctx->framebuffer[byte_index] >> bit_offset) & 0x03);
}

void clay_epaper_clear(ClayEpaperContext* ctx, EpaperColor color) {
    if (!ctx || !ctx->framebuffer) return;
    
    uint8_t pattern = (color & 0x03) | ((color & 0x03) << 2) | 
                      ((color & 0x03) << 4) | ((color & 0x03) << 6);
    memset(ctx->framebuffer, pattern, EPAPER_BUFFER_SIZE);
    ctx->dirty = true;
}

// Shape drawing
void clay_epaper_draw_rect(ClayEpaperContext* ctx, Clay_BoundingBox rect, EpaperColor color, bool filled) {
    if (!ctx) return;
    
    int x1 = (int)rect.x;
    int y1 = (int)rect.y;
    int x2 = (int)(rect.x + rect.width);
    int y2 = (int)(rect.y + rect.height);
    
    x1 = CLAMP(x1, 0, EPAPER_WIDTH - 1);
    y1 = CLAMP(y1, 0, EPAPER_HEIGHT - 1);
    x2 = CLAMP(x2, 0, EPAPER_WIDTH);
    y2 = CLAMP(y2, 0, EPAPER_HEIGHT);
    
    if (filled) {
        for (int y = y1; y < y2; y++) {
            for (int x = x1; x < x2; x++) {
                clay_epaper_set_pixel(ctx, x, y, color);
            }
        }
    } else {
        // Draw outline
        for (int x = x1; x < x2; x++) {
            clay_epaper_set_pixel(ctx, x, y1, color);
            clay_epaper_set_pixel(ctx, x, y2 - 1, color);
        }
        for (int y = y1; y < y2; y++) {
            clay_epaper_set_pixel(ctx, x1, y, color);
            clay_epaper_set_pixel(ctx, x2 - 1, y, color);
        }
    }
}

void clay_epaper_draw_line(ClayEpaperContext* ctx, int16_t x0, int16_t y0, 
                          int16_t x1, int16_t y1, EpaperColor color) {
    if (!ctx) return;
    
    // Bresenham's line algorithm
    int dx = abs(x1 - x0);
    int dy = abs(y1 - y0);
    int sx = x0 < x1 ? 1 : -1;
    int sy = y0 < y1 ? 1 : -1;
    int err = dx - dy;
    
    while (true) {
        clay_epaper_set_pixel(ctx, x0, y0, color);
        
        if (x0 == x1 && y0 == y1) break;
        
        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x0 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y0 += sy;
        }
    }
}

// Text rendering
uint16_t clay_epaper_get_char_width(char c, const EpaperFont* font) {
    if (!font || c < font->first_char || c >= font->first_char + font->char_count) {
        return 0;
    }
    return font->width;
}

void clay_epaper_draw_char(ClayEpaperContext* ctx, char c, uint16_t x, uint16_t y, 
                          const EpaperFont* font, EpaperColor color) {
    if (!ctx || !font || c < font->first_char || c >= font->first_char + font->char_count) {
        return;
    }
    
    uint8_t char_index = c - font->first_char;
    const uint8_t* char_data = &font->bitmap[char_index * font->height];
    
    for (uint8_t row = 0; row < font->height; row++) {
        uint8_t byte = char_data[row];
        for (uint8_t col = 0; col < font->width; col++) {
            if (byte & (0x80 >> col)) {
                clay_epaper_set_pixel(ctx, x + col, y + row, color);
            }
        }
    }
}

void clay_epaper_draw_text(ClayEpaperContext* ctx, Clay_String text, Clay_BoundingBox bounds,
                          Clay_TextElementConfig* config) {
    if (!ctx || !text.chars || text.length == 0 || !config) return;
    
    uint8_t font_id = clay_epaper_map_font_size(config->fontSize);
    const EpaperFont* font = clay_epaper_get_font(ctx, font_id);
    EpaperColor text_color = clay_epaper_convert_color(config->textColor);
    
    uint16_t x = (uint16_t)bounds.x;
    uint16_t y = (uint16_t)bounds.y;
    uint16_t max_width = (uint16_t)bounds.width;
    
    for (uint32_t i = 0; i < text.length && i < 256; i++) {  // Limit for embedded
        char c = text.chars[i];
        
        if (c == '\n') {
            x = (uint16_t)bounds.x;
            y += font->height + 2;  // Line spacing
            continue;
        }
        
        uint16_t char_width = clay_epaper_get_char_width(c, font);
        if (x + char_width > bounds.x + max_width) {
            if (config->wrapMode == CLAY_TEXT_WRAP_WORDS) {
                x = (uint16_t)bounds.x;
                y += font->height + 2;
            } else {
                break;  // Clip text
            }
        }
        
        clay_epaper_draw_char(ctx, c, x, y, font, text_color);
        x += char_width + 1;  // Character spacing
    }
}

// Clay callback implementations
Clay_Dimensions clay_epaper_measure_text(Clay_String* text, Clay_TextElementConfig* config) {
    if (!text || !config || text->length == 0) {
        return (Clay_Dimensions){0, 0};
    }
    
    uint8_t font_id = clay_epaper_map_font_size(config->fontSize);
    const EpaperFont* font = (font_id < 4) ? 
        ((const EpaperFont*[]){&CLAY_EPAPER_FONT_SMALL_8x8, &CLAY_EPAPER_FONT_MEDIUM_8x12, 
                               &CLAY_EPAPER_FONT_LARGE_8x16, &CLAY_EPAPER_FONT_XLARGE_12x24})[font_id] :
        &CLAY_EPAPER_FONT_SMALL_8x8;
    
    uint16_t width = 0;
    uint16_t max_width = 0;
    uint16_t height = font->height;
    uint16_t lines = 1;
    
    for (uint32_t i = 0; i < text->length; i++) {
        char c = text->chars[i];
        if (c == '\n') {
            max_width = MAX(max_width, width);
            width = 0;
            lines++;
        } else {
            width += clay_epaper_get_char_width(c, font) + 1;
        }
    }
    
    max_width = MAX(max_width, width);
    height = lines * (font->height + 2) - 2;  // Remove spacing from last line
    
    return (Clay_Dimensions){(float)max_width, (float)height};
}

// Clipping
void clay_epaper_set_clip_rect(ClayEpaperContext* ctx, Clay_BoundingBox rect) {
    if (!ctx) return;
    
    ctx->clip_rect = rect;
    ctx->clipping_enabled = true;
}

void clay_epaper_clear_clip_rect(ClayEpaperContext* ctx) {
    if (!ctx) return;
    
    ctx->clipping_enabled = false;
}

bool clay_epaper_point_in_clip(ClayEpaperContext* ctx, uint16_t x, uint16_t y) {
    if (!ctx || !ctx->clipping_enabled) return true;
    
    return x >= ctx->clip_rect.x && x < ctx->clip_rect.x + ctx->clip_rect.width &&
           y >= ctx->clip_rect.y && y < ctx->clip_rect.y + ctx->clip_rect.height;
}

// Render command processing
void clay_epaper_process_rectangle_command(ClayEpaperContext* ctx, Clay_RenderCommand* command) {
    if (!ctx || !command) return;
    
    Clay_RectangleElementConfig* config = command->config.rectangleElementConfig;
    if (!config) return;
    
    EpaperColor color = clay_epaper_convert_color(config->color);
    clay_epaper_draw_rect(ctx, command->boundingBox, color, true);
}

void clay_epaper_process_text_command(ClayEpaperContext* ctx, Clay_RenderCommand* command) {
    if (!ctx || !command) return;
    
    Clay_TextElementConfig* config = command->config.textElementConfig;
    if (!config) return;
    
    clay_epaper_draw_text(ctx, command->text, command->boundingBox, config);
}

void clay_epaper_process_border_command(ClayEpaperContext* ctx, Clay_RenderCommand* command) {
    if (!ctx || !command) return;
    
    Clay_BorderElementConfig* config = command->config.borderElementConfig;
    if (!config) return;
    
    Clay_BoundingBox bounds = command->boundingBox;
    
    // Draw borders (simplified - just outline for now)
    EpaperColor border_color = clay_epaper_convert_color(config->left.color);
    clay_epaper_draw_rect(ctx, bounds, border_color, false);
}

void clay_epaper_process_scissor_start(ClayEpaperContext* ctx, Clay_RenderCommand* command) {
    if (!ctx || !command) return;
    
    clay_epaper_set_clip_rect(ctx, command->boundingBox);
}

void clay_epaper_process_scissor_end(ClayEpaperContext* ctx, Clay_RenderCommand* command) {
    if (!ctx) return;
    
    clay_epaper_clear_clip_rect(ctx);
}

// Frame management
void clay_epaper_begin_frame(ClayEpaperContext* ctx) {
    if (!ctx) return;
    
    ctx->batch_mode = true;
    ctx->render_command_count = 0;
    
    // Clear framebuffer to white
    clay_epaper_clear(ctx, EPAPER_COLOR_WHITE);
}

void clay_epaper_end_frame(ClayEpaperContext* ctx) {
    if (!ctx) return;
    
    ctx->batch_mode = false;
    
    // Update hardware display if dirty
    if (ctx->dirty) {
        clay_epaper_hardware_update_full(ctx->framebuffer);
        ctx->dirty = false;
        ctx->last_update = 0;  // Would be set to current time by caller
    }
}

void clay_epaper_render_commands(ClayEpaperContext* ctx, Clay_RenderCommandArray commands) {
    if (!ctx) return;
    
    for (uint32_t i = 0; i < commands.length; i++) {
        Clay_RenderCommand* command = Clay_RenderCommandArray_Get(&commands, i);
        if (!command) continue;
        
        switch (command->commandType) {
            case CLAY_RENDER_COMMAND_TYPE_RECTANGLE:
                clay_epaper_process_rectangle_command(ctx, command);
                break;
            case CLAY_RENDER_COMMAND_TYPE_TEXT:
                clay_epaper_process_text_command(ctx, command);
                break;
            case CLAY_RENDER_COMMAND_TYPE_BORDER:
                clay_epaper_process_border_command(ctx, command);
                break;
            case CLAY_RENDER_COMMAND_TYPE_SCISSOR_START:
                clay_epaper_process_scissor_start(ctx, command);
                break;
            case CLAY_RENDER_COMMAND_TYPE_SCISSOR_END:
                clay_epaper_process_scissor_end(ctx, command);
                break;
            default:
                break;
        }
    }
}

// Utility functions
bool clay_epaper_needs_update(ClayEpaperContext* ctx) {
    return ctx ? ctx->dirty : false;
}

void clay_epaper_mark_dirty(ClayEpaperContext* ctx) {
    if (ctx) ctx->dirty = true;
}

void clay_epaper_mark_clean(ClayEpaperContext* ctx) {
    if (ctx) ctx->dirty = false;
}

// Error handling
ClayEpaperError clay_epaper_get_last_error(void) {
    return g_last_error;
}

const char* clay_epaper_error_string(ClayEpaperError error) {
    switch (error) {
        case CLAY_EPAPER_OK: return "No error";
        case CLAY_EPAPER_ERROR_INIT_FAILED: return "Initialization failed";
        case CLAY_EPAPER_ERROR_MEMORY_ALLOCATION: return "Memory allocation failed";
        case CLAY_EPAPER_ERROR_INVALID_FONT: return "Invalid font";
        case CLAY_EPAPER_ERROR_HARDWARE_FAILURE: return "Hardware failure";
        case CLAY_EPAPER_ERROR_INVALID_PARAMETER: return "Invalid parameter";
        default: return "Unknown error";
    }
}

// Hardware interface stubs (to be implemented with Nordic SDK)
bool clay_epaper_hardware_init(void) {
    // TODO: Initialize SPI, GPIO pins, etc.
    return true;
}

void clay_epaper_hardware_deinit(void) {
    // TODO: Cleanup hardware resources
}

bool clay_epaper_hardware_update_full(const uint8_t* framebuffer) {
    // TODO: Send framebuffer to e-Paper display via SPI
    return true;
}

bool clay_epaper_hardware_update_partial(const uint8_t* framebuffer, Clay_BoundingBox region) {
    // TODO: Send partial update to e-Paper display
    return clay_epaper_hardware_update_full(framebuffer);  // Fallback to full update
}

void clay_epaper_hardware_sleep(void) {
    // TODO: Put e-Paper display in sleep mode
}

void clay_epaper_hardware_wake(void) {
    // TODO: Wake e-Paper display from sleep mode
}
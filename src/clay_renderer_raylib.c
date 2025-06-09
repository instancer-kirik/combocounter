#include "clay_renderer_raylib.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "clay.h"

void Clay_RenderCommandArray_RenderRaylib(Clay_RenderCommandArray commands) {
    if (!commands.internalArray || commands.length == 0) return;

    for (size_t i = 0; i < commands.length; i++) {
        Clay_RenderCommand* cmd = &commands.internalArray[i];
        if (!cmd) continue;

        switch (cmd->commandType) {
            case CLAY_RENDER_COMMAND_TYPE_RECTANGLE: {
                if (!cmd->config.rectangleElementConfig) continue;
                Color color = clay_to_raylib_color(cmd->config.rectangleElementConfig->color);
                DrawRectangle(
                    (int)cmd->boundingBox.x,
                    (int)cmd->boundingBox.y,
                    (int)cmd->boundingBox.width,
                    (int)cmd->boundingBox.height,
                    color
                );
                break;
            }

            case CLAY_RENDER_COMMAND_TYPE_TEXT: {
                if (!cmd->config.textElementConfig || !cmd->text.chars) continue;
                Color color = clay_to_raylib_color(cmd->config.textElementConfig->textColor);
                DrawText(
                    cmd->text.chars,
                    (int)cmd->boundingBox.x,
                    (int)cmd->boundingBox.y,
                    cmd->config.textElementConfig->fontSize,
                    color
                );
                break;
            }

            case CLAY_RENDER_COMMAND_TYPE_SCISSOR_START: {
                BeginScissorMode(
                    (int)cmd->boundingBox.x,
                    (int)cmd->boundingBox.y,
                    (int)cmd->boundingBox.width,
                    (int)cmd->boundingBox.height
                );
                break;
            }

            case CLAY_RENDER_COMMAND_TYPE_SCISSOR_END: {
                EndScissorMode();
                break;
            }
        }
    }
}

Clay_Dimensions Raylib_MeasureText(Clay_String* text, Clay_TextElementConfig* config) {
    Vector2 size = MeasureTextEx(GetFontDefault(), text->chars, config->fontSize, 1);
    return (Clay_Dimensions){
        .width = size.x,
        .height = size.y
    };
}

#define MAX_TEXTURES 100

typedef struct {
    char path[256];
    Texture2D texture;
} TextureCache;

static TextureCache texture_cache[MAX_TEXTURES];
static int texture_count = 0;

static Texture2D* get_or_load_texture(const char* path) {
    // Check if texture is already loaded
    for (int i = 0; i < texture_count; i++) {
        if (strcmp(texture_cache[i].path, path) == 0) {
            return &texture_cache[i].texture;
        }
    }
    
    // Load new texture if there's space
    if (texture_count < MAX_TEXTURES) {
        strncpy(texture_cache[texture_count].path, path, sizeof(texture_cache[texture_count].path) - 1);
        texture_cache[texture_count].texture = LoadTexture(path);
        return &texture_cache[texture_count++].texture;
    }
    
    return NULL;
}

static void Clay_DefaultErrorHandler(Clay_ErrorData error) {
    printf("Clay Error: %.*s\n", error.errorText.length, error.errorText.chars);
}

void Clay_Raylib_Initialize(int width, int height, const char* title, unsigned int flags) {
    // Set up text measurement only - Clay should already be initialized in main.c
    Clay_SetMeasureTextFunction(Raylib_MeasureText);
    
    // Set maximum element count to prevent array overflows
    Clay_SetMaxElementCount(10000);  // Increased from 1000
    Clay_SetMaxMeasureTextCacheWordCount(10000);  // Increased from 1000
    Clay_SetCullingEnabled(true);
}

Clay_ElementId Clay__GetClickedElement(Clay_RenderCommandArray commands, Clay_PointerData pointer) {
    Clay_ElementId result = {0};
    
    if (!commands.internalArray || commands.length == 0) {
        return result;
    }

    // Iterate through commands in reverse order (top-most elements first)
    for (int i = commands.length - 1; i >= 0; i--) {
        Clay_RenderCommand* cmd = &commands.internalArray[i];
        if (!cmd || cmd->commandType != CLAY_RENDER_COMMAND_TYPE_RECTANGLE) continue;

        // Check if point is inside bounding box
        if (pointer.position.x >= cmd->boundingBox.x && 
            pointer.position.x <= cmd->boundingBox.x + cmd->boundingBox.width &&
            pointer.position.y >= cmd->boundingBox.y && 
            pointer.position.y <= cmd->boundingBox.y + cmd->boundingBox.height) {
            
            result.id = cmd->id;
            // Don't use text for ID since we want to detect rectangle clicks
            result.stringId = (Clay_String){0};
            return result;
        }
    }
    
    return result;
} 
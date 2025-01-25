#include "clay_renderer_raylib.h"
#include <string.h>
#include "clay.h"

void Clay_RenderCommandArray_RenderRaylib(Clay_RenderCommandArray commands) {
    for (size_t i = 0; i < commands.length; i++) {
        Clay_RenderCommand* cmd = Clay_RenderCommandArray_Get(&commands, i);
        if (!cmd) continue;

        switch (cmd->commandType) {
            case CLAY_RENDER_COMMAND_TYPE_RECTANGLE: {
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

void Clay_Raylib_Initialize(int width, int height, const char* title, unsigned int flags) {
    SetConfigFlags(flags);
    InitWindow(width, height, title);
}

Clay_ElementId Clay__GetClickedElement(Clay_RenderCommandArray commands, Clay_PointerData pointer) {
    Clay_ElementId result = {
        .id = 0,
        .offset = 0,
        .baseId = 0,
        .stringId = {
            .chars = NULL,
            .length = 0
        }
    };
    
    // Iterate through commands in reverse order (top-most elements first)
    for (int i = commands.length - 1; i >= 0; i--) {
        Clay_RenderCommand* cmd = Clay_RenderCommandArray_Get(&commands, i);
        if (!cmd) continue;

        // Check if point is inside bounding box
        if (pointer.position.x >= cmd->boundingBox.x && 
            pointer.position.x <= cmd->boundingBox.x + cmd->boundingBox.width &&
            pointer.position.y >= cmd->boundingBox.y && 
            pointer.position.y <= cmd->boundingBox.y + cmd->boundingBox.height) {
            
            // Found a hit, return the command's ID
            result.id = cmd->id;
            if (cmd->commandType == CLAY_RENDER_COMMAND_TYPE_TEXT && cmd->text.chars) {
                result.stringId = cmd->text;
            }
            return result;
        }
    }
    
    return result;
} 
#ifndef CLAY_RENDERER_RAYLIB_H
#define CLAY_RENDERER_RAYLIB_H

#include "raylib.h"
#include "clay_types.h"

// Convert Clay_Color to Raylib Color
static inline Color clay_to_raylib_color(Clay_Color color) {
    return (Color){
        (unsigned char)(color.r * 255),
        (unsigned char)(color.g * 255),
        (unsigned char)(color.b * 255),
        (unsigned char)(color.a * 255)
    };
}

// Render Clay commands using Raylib
void Clay_RenderCommandArray_RenderRaylib(Clay_RenderCommandArray commands);

// Text measurement
Clay_Dimensions Raylib_MeasureText(Clay_String* text, Clay_TextElementConfig* config);

void Clay_Raylib_Initialize(int width, int height, const char* title, unsigned int flags);
void Clay_Raylib_Render(Clay_RenderCommandArray commands);

#endif // CLAY_RENDERER_RAYLIB_H 
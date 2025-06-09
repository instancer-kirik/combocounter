#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Include Clay's dependencies first
#include "raylib.h"

// Define implementation before including Clay
#define CLAY_IMPLEMENTATION
#define CLAY_RAYLIB_IMPLEMENTATION

// Now include Clay headers
#include "clay.h"
#include "clay_types.h"
#include "clay_renderer_raylib.h"

// Text measurement function for Clay
static Clay_Dimensions MeasureText(Clay_String* text, Clay_TextElementConfig* config) {
    Vector2 size = MeasureTextEx(GetFontDefault(), text->chars, config->fontSize, 1);
    return (Clay_Dimensions){
        .width = size.x,
        .height = size.y
    };
}

// Frame management functions
void Clay_BeginFrame(void) {
    // Just increment generation, layout begins in render
}

void Clay_EndFrame(void) {
    // Just reset arena, layout ends in render
}

// Global error handler
static Clay_ErrorHandler g_errorHandler;

void Clay_SetErrorHandler(Clay_ErrorHandler handler) {
    g_errorHandler = handler;
}

// Our application's initialization function
void Initialize_ComboCounter(Clay_Arena arena, Clay_Dimensions layoutDimensions, Clay_ErrorHandler errorHandler) {
    // Set higher element counts before initialization
    Clay_SetMaxElementCount(16384);  // Doubled from 8192
    Clay_SetMaxMeasureTextCacheWordCount(16384);  // Increased to match
    
    // Set the text measurement function
    Clay_SetMeasureTextFunction(MeasureText);
    
    // Initialize Clay with our settings
    Clay_Initialize(arena, layoutDimensions, errorHandler);
    
    // Initialize Raylib renderer
    Clay_Raylib_Initialize(layoutDimensions.width, layoutDimensions.height, "Combo Counter", 0);
} 
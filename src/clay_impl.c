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

// Frame management functions
void Clay_BeginFrame(void) {
    Clay_BeginLayout();
}

void Clay_EndFrame(void) {
    Clay_RenderCommandArray commands = Clay_EndLayout();
    // Process render commands if needed
}

void Clay_SetErrorHandler(Clay_ErrorHandler handler) {
    // Initialize Clay with default settings
    Clay_Initialize(
        Clay_CreateArenaWithCapacityAndMemory(Clay_MinMemorySize(), NULL),
        (Clay_Dimensions){800, 600}, // Default dimensions
        handler
    );
} 
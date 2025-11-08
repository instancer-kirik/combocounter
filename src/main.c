#include "raylib.h"
#include <stdio.h>
#include <stdlib.h>
#include "clay.h"
#include "clay_types.h"
#include "clay_renderer_raylib.h"
#include "clay_impl.h"
#include "ui.h"
#include "input.h"
#include "colors.h"
#include "timer.h"
#include "break_activities.h"
#include <string.h>

#define SCREEN_WIDTH 1280
#define SCREEN_HEIGHT 720

// Global UI context for widget access
ComboUI* g_ui_context = NULL;

void HandleClayErrors(Clay_ErrorData errorData) {
    printf("Clay Error: %s\n", errorData.errorText.chars);
}

int main(void) {
    // Initialize window
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Combo Counter");
    SetTargetFPS(60);
    
    // Initialize Clay with proper setup
    Clay_Arena arena = CLAY__INIT(Clay_Arena){
        .capacity = 64 * 1024 * 1024, // 64MB should be enough for complex UI
        .memory = malloc(64 * 1024 * 1024),
        .nextAllocation = 0
    };

    if (!arena.memory) {
        printf("Failed to allocate Clay arena memory (64MB)\n");
        CloseWindow();
        return 1;
    }

    Clay_Dimensions dimensions = {
        .width = SCREEN_WIDTH,
        .height = SCREEN_HEIGHT
    };
    
    // Create proper error handler structure
    Clay_ErrorHandler errorHandler = {
        .errorHandlerFunction = HandleClayErrors,
        .userData = 0
    };
    
    // Initialize Clay properly
    Initialize_ComboCounter(arena, dimensions, errorHandler);
    
    // Set debug mode to help track issues
    Clay_SetDebugModeEnabled(true);

    // Initialize UI after Clay is set up
    ComboUI ui = {0};
    init_ui(&ui);
    
    // Load saved tracker state
    load_ui_state(&ui);
    
    // Set global UI context for widget access
    g_ui_context = &ui;
    
    // Initialize break activities
    for (int i = 0; i < default_activities_count; i++) {
        break_menu_add_activity(&ui.break_menu, 
            default_activities[i].name,
            default_activities[i].description,
            default_activities[i].duration,
            default_activities[i].is_quick_break);
    }
    
    // Main game loop
    while (!WindowShouldClose()) {
        // Update
        float dt = GetFrameTime();
        
        // Update trackers
        for (int i = 0; i < ui.tracker_count; i++) {
            combo_update(&ui.trackers[i], dt);
        }
        
        // Handle input
        if (IsKeyPressed(KEY_ESCAPE)) {
            if (ui.break_menu.visible) {
                ui.break_menu.visible = false;
            }
            if (ui.tracker_form.form_visible) {
                ui.tracker_form.form_visible = false;
            }
            if (ui.interval_form.form_visible) {
                ui.interval_form.form_visible = false;
            }
        }

        // Handle keyboard input
        int key = GetCharPressed();
        while (key > 0) {
            handle_input(&ui, key);
            key = GetCharPressed();
        }

        // Handle special keys
        if (IsKeyPressed(KEY_ESCAPE)) {
            handle_input(&ui, KEY_ESCAPE);
        }
        if (IsKeyPressed(KEY_BACKSPACE)) {
            handle_input(&ui, KEY_BACKSPACE);
        }

        // Global shortcuts
        if (IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL)) {
            if (IsKeyPressed(KEY_N)) {
                ui.tracker_form.form_visible = true;
                ui.interval_form.form_visible = false;
                ui.break_menu.visible = false;
            }
            if (IsKeyPressed(KEY_I)) {
                ui.interval_form.form_visible = true;
                ui.tracker_form.form_visible = false;
                ui.break_menu.visible = false;
            }
            if (IsKeyPressed(KEY_B)) {
                ui.break_menu.visible = true;
                ui.tracker_form.form_visible = false;
                ui.interval_form.form_visible = false;
            }
        }

        // Handle mouse input
        Clay_Vector2 mousePos = { GetMouseX(), GetMouseY() };
        bool mousePressed = IsMouseButtonDown(MOUSE_BUTTON_LEFT);
        Clay_SetPointerState(mousePos, mousePressed);

        // Begin rendering
        BeginDrawing();
        
        // Clear background
        ClearBackground((Color){
            .r = (unsigned char)(COLOR_BG.r * 255),
            .g = (unsigned char)(COLOR_BG.g * 255),
            .b = (unsigned char)(COLOR_BG.b * 255),
            .a = (unsigned char)(COLOR_BG.a * 255)
        });
        
        // Generate and render UI
        Clay_RenderCommandArray commands = combo_ui_render(&ui);
        
        // Render the UI
        Clay_RenderCommandArray_RenderRaylib(commands);
        
        EndDrawing();
    }
    
    // Save UI state before cleanup
    save_ui_state(&ui);
    
    // Cleanup
    free(arena.memory);
    CloseWindow();
    return 0;
} 
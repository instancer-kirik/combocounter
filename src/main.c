#include "raylib.h"
#include <stdio.h>
#include "clay.h"
#include "clay_types.h"
#include "clay_renderer_raylib.h"
#include "ui.h"
#include "input.h"
#include "colors.h"
#include "timer.h"
#include "break_activities.h"
#include <string.h>
#include <stdlib.h>

#define SCREEN_WIDTH 1280
#define SCREEN_HEIGHT 720

void HandleClayErrors(Clay_ErrorData errorData) {
    printf("Clay Error: %s\n", errorData.errorText.chars);
}

int main(void) {
    // Initialize window
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Combo Counter");
    SetTargetFPS(60);
    
    // Initialize Clay
    Clay_ErrorHandler errorHandler = {
        .errorHandlerFunction = HandleClayErrors
    };
    Clay_SetErrorHandler(errorHandler);
    Clay_BeginFrame();
    
    // Initialize UI
    ComboUI ui = {0};
    init_ui(&ui);
    
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
        // Begin new Clay frame
        Clay_BeginFrame();
        
        // Update
        float dt = GetFrameTime();
        
        // Update trackers
        for (int i = 0; i < MAX_TRACKERS; i++) {
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

        if (IsKeyPressed(KEY_BACKSPACE)) {
            handle_input(&ui, KEY_BACKSPACE);
        }

        if (IsKeyPressed(KEY_ENTER)) {
            handle_input(&ui, KEY_ENTER);
        }

        int key = GetCharPressed();
        while (key > 0) {
            handle_input(&ui, key);
            key = GetCharPressed();
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
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            Clay_Vector2 mousePos = { GetMouseX(), GetMouseY() };
            Clay_PointerData pointer = { 
                .position = mousePos, 
                .state = CLAY_POINTER_DATA_PRESSED_THIS_FRAME 
            };
            Clay_BeginLayout();
            Clay_RenderCommandArray commands = combo_ui_render(&ui);
            Clay_ElementId clicked = Clay__GetClickedElement(commands, pointer);
            Clay_EndLayout();

            if (clicked.stringId.length > 0) {
                if (strncmp(clicked.stringId.chars, "add_tracker", clicked.stringId.length) == 0) {
                    ui.tracker_form.form_visible = true;
                    ui.interval_form.form_visible = false;
                    ui.break_menu.visible = false;
                }
                else if (strncmp(clicked.stringId.chars, "add_interval", clicked.stringId.length) == 0) {
                    ui.interval_form.form_visible = true;
                    ui.tracker_form.form_visible = false;
                    ui.break_menu.visible = false;
                }
                else if (strncmp(clicked.stringId.chars, "take_break", clicked.stringId.length) == 0) {
                    ui.break_menu.visible = true;
                    ui.tracker_form.form_visible = false;
                    ui.interval_form.form_visible = false;
                }
                else if (strncmp(clicked.stringId.chars, "add_tracker_submit", clicked.stringId.length) == 0) {
                    add_new_tracker(&ui);
                }
                else if (strncmp(clicked.stringId.chars, "add_interval_submit", clicked.stringId.length) == 0) {
                    add_new_interval(&ui);
                }
                else if (strncmp(clicked.stringId.chars, "cancel_form", clicked.stringId.length) == 0) {
                    ui.tracker_form.form_visible = false;
                    ui.interval_form.form_visible = false;
                }
                else if (strncmp(clicked.stringId.chars, "cancel_break", clicked.stringId.length) == 0) {
                    ui.break_menu.visible = false;
                }
                else {
                    // Check if a tracker was clicked
                    char tracker_prefix[] = "tracker_";
                    if (strncmp(clicked.stringId.chars, tracker_prefix, strlen(tracker_prefix)) == 0) {
                        int index = atoi(clicked.stringId.chars + strlen(tracker_prefix));
                        if (index >= 0 && index < MAX_TRACKERS) {
                            ui.selected_tracker = &ui.trackers[index];
                        }
                    }
                }
            }
        }
        
        // Render
        BeginDrawing();
        ClearBackground((Color){
            .r = (unsigned char)(COLOR_BG.r * 255),
            .g = (unsigned char)(COLOR_BG.g * 255),
            .b = (unsigned char)(COLOR_BG.b * 255),
            .a = (unsigned char)(COLOR_BG.a * 255)
        });

        Clay_BeginLayout();
        Clay_RenderCommandArray commands = combo_ui_render(&ui);
        Clay_RenderCommandArray_RenderRaylib(commands);
        Clay_EndLayout();
        
        EndDrawing();
        
        // End Clay frame
        Clay_EndFrame();
    }
    
    CloseWindow();
    return 0;
} 
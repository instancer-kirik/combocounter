#ifndef COMBO_UI_IMPL_H
#define COMBO_UI_IMPL_H

#include "core.h"
#include "common.h"
#include "clay.h"
#include "widgets.h"
#include "forms.h"
#include "input.h"

// Main container - fills the window
void combo_ui_render(ComboUI* ui) {
    // Register global click handler
    Clay_OnHover(handle_click, (intptr_t)ui);

    // Main container - fills the window
    CLAY(
        CLAY_ID("main_container"),
        CLAY_LAYOUT({
            .layoutDirection = CLAY_LEFT_TO_RIGHT,
            .childGap = 20,
            .childAlignment = {CLAY_ALIGN_Y_TOP}
        }),
        CLAY_RECTANGLE({.color = {.r = 0.1f, .g = 0.1f, .b = 0.12f, .a = 1.0f}})
    ) {
        // Left panel - Trackers
        CLAY(
            CLAY_ID("tracker_panel"),
            CLAY_LAYOUT({
                .sizing = {CLAY_SIZING_FIXED(300), CLAY_SIZING_GROW()},
                .childGap = 20,
                .childAlignment = {CLAY_ALIGN_X_CENTER}
            }),
            CLAY_RECTANGLE({.color = {.r = 0.15f, .g = 0.15f, .b = 0.17f, .a = 1.0f}})
        ) {
            // Header
            CLAY_TEXT(make_clay_string("Trackers"), CLAY_TEXT_CONFIG({
                .fontSize = 24,
                .textColor = ui->active_color
            }));

            // Add tracker button
            if (ui->tracker_count < MAX_TRACKERS) {
                CLAY(
                    CLAY_ID("add_tracker_btn"),
                    CLAY_LAYOUT({
                        .sizing = {CLAY_SIZING_FIXED(120), CLAY_SIZING_FIXED(30)}
                    }),
                    CLAY_RECTANGLE({.color = Clay_Hovered() ? ui->active_color : ui->paused_color})
                ) {
                    CLAY_TEXT(make_clay_string("[N] New"), CLAY_TEXT_CONFIG({
                        .fontSize = 16,
                        .textColor = {1, 1, 1, 1}
                    }));
                }
            }
            
            // Tracker list container
            CLAY(
                CLAY_ID("tracker_list"),
                CLAY_LAYOUT({
                    .sizing = {CLAY_SIZING_FIXED(280), CLAY_SIZING_GROW()},
                    .childGap = 20,
                    .childAlignment = {CLAY_ALIGN_X_CENTER}
                })
            ) {
                // Individual trackers
                for (int i = 0; i < ui->tracker_count; i++) {
                    widget_tracker_card(&ui->trackers[i], i, ui->active_color, ui->paused_color, ui->perfect_color);
                }
            }
        }
        
        // Right panel - Controls
        widget_controls_panel(ui->active_color, ui->paused_color, ui->break_color);
    }
    
    // Floating menus
    if (ui->break_menu && ui->break_menu->menu_visible) {
        widget_break_menu(ui->break_menu, ui);
    }
    
    if (ui->tracker_form.form_visible) {
        render_tracker_form(&ui->tracker_form, ui);
    }
    
    if (ui->interval_form.form_visible) {
        render_interval_form(&ui->interval_form, ui);
    }
}

#endif // COMBO_UI_IMPL_H 
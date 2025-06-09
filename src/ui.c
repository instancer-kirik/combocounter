#include "ui.h"
#include "forms.h"
#include "widgets.h"
#include "colors.h"
#include <string.h>
#include <stdlib.h>
#include "clay.h"
#include "clay_renderer_raylib.h"
void init_ui(ComboUI* ui) {
    // Initialize colors
    ui->active_color = COLOR_ACTIVE;
    ui->paused_color = COLOR_PAUSED;
    ui->perfect_color = COLOR_PERFECT;
    ui->miss_color = COLOR_MISS;
    ui->error_color = COLOR_ERROR;
    ui->break_color = COLOR_BREAK;
    ui->quick_break_color = COLOR_QUICK_BREAK;
    ui->objective_color = COLOR_OBJECTIVE;
    ui->completed_color = COLOR_COMPLETED;

    // Initialize forms
    ui->tracker_form.form_visible = false;
    ui->tracker_form.error_visible = false;
    ui->tracker_form.error_message = NULL;
    memset(ui->tracker_form.label_buffer, 0, MAX_LABEL_LENGTH);

    ui->interval_form.form_visible = false;
    ui->interval_form.error_visible = false;
    ui->interval_form.error_message = NULL;
    memset(ui->interval_form.label_buffer, 0, MAX_LABEL_LENGTH);
    memset(ui->interval_form.duration_buffer, 0, 16);
    memset(ui->interval_form.reps_buffer, 0, 8);

    // Initialize break menu
    ui->break_menu.visible = false;
    ui->break_menu.activity_count = 0;

    // Initialize tracker count
    ui->tracker_count = 0;
    ui->selected_tracker = NULL;
}

Clay_RenderCommandArray combo_ui_render(ComboUI* ui) {
    Clay_BeginLayout();

    // Render break menu if visible
    if (ui->break_menu.visible) {
        widget_break_menu(&ui->break_menu, ui);
        return Clay_EndLayout();
    }

    // Render forms if visible
    if (ui->tracker_form.form_visible) {
        render_tracker_form(&ui->tracker_form, ui);
        return Clay_EndLayout();
    }

    if (ui->interval_form.form_visible) {
        render_interval_form(&ui->interval_form, ui);
        return Clay_EndLayout();
    }

    // Main UI layout
    CLAY(
        CLAY_IDI("main_ui_container", 0),
        CLAY_LAYOUT(CLAY__INIT(Clay_LayoutConfig) {
            .layoutDirection = CLAY_LEFT_TO_RIGHT,
            .sizing = CLAY__INIT(Clay_Sizing) {
                .width = CLAY_SIZING_FIXED(1280),
                .height = CLAY_SIZING_FIXED(720)
            }
        })
    ) {
        // Left panel - trackers
        CLAY(
            CLAY_IDI("trackers_panel_container", 0),
            CLAY_LAYOUT(CLAY__INIT(Clay_LayoutConfig) {
                .layoutDirection = CLAY_TOP_TO_BOTTOM,
                .sizing = CLAY__INIT(Clay_Sizing) {
                    .width = CLAY_SIZING_FIXED(960),
                    .height = CLAY_SIZING_FIXED(720)
                }
            })
        ) {
            // Header
            CLAY(
                CLAY_IDI("trackers_header", 0),
                CLAY_TEXT(
                    CLAY_STRING("Trackers"),
                    CLAY_TEXT_CONFIG(CLAY__INIT(Clay_TextElementConfig) {
                        .textColor = COLOR_ACTIVE,
                        .fontSize = 24
                    })
                )
            );

            // Render tracker cards with unique indices
            for (int i = 0; i < ui->tracker_count; i++) {
                widget_tracker_card(&ui->trackers[i], i, ui->active_color, ui->paused_color, ui->perfect_color);
            }
        }

        // Right panel - controls
        CLAY(
            CLAY_IDI("controls_panel_container", 0),
            CLAY_LAYOUT(CLAY__INIT(Clay_LayoutConfig) {
                .layoutDirection = CLAY_TOP_TO_BOTTOM,
                .sizing = CLAY__INIT(Clay_Sizing) {
                    .width = CLAY_SIZING_FIXED(320),
                    .height = CLAY_SIZING_FIXED(720)
                }
            })
        ) {
            // Header
            CLAY(
                CLAY_IDI("controls_header", 0),
                CLAY_TEXT(
                    CLAY_STRING("Controls"),
                    CLAY_TEXT_CONFIG(CLAY__INIT(Clay_TextElementConfig) {
                        .textColor = COLOR_ACTIVE,
                        .fontSize = 24
                    })
                )
            );

            widget_controls_panel(0, ui->active_color, ui->paused_color, ui->break_color);
        }
    }

    return Clay_EndLayout();
}

void add_new_tracker(ComboUI* ui) {
    if (ui->tracker_count >= MAX_TRACKERS) {
        ui->tracker_form.error_visible = true;
        ui->tracker_form.error_message = "Maximum number of trackers reached";
        return;
    }

    if (strlen(ui->tracker_form.label_buffer) == 0) {
        ui->tracker_form.error_visible = true;
        ui->tracker_form.error_message = "Label cannot be empty";
        return;
    }

    // Initialize new tracker
    combo_init(&ui->trackers[ui->tracker_count], ui->tracker_form.label_buffer);
    ui->tracker_count++;

    // Hide form and clear error
    ui->tracker_form.form_visible = false;
    ui->tracker_form.error_visible = false;
    ui->tracker_form.error_message = NULL;
    memset(ui->tracker_form.label_buffer, 0, MAX_LABEL_LENGTH);
}

void add_new_interval(ComboUI* ui) {
    if (!ui->selected_tracker) {
        ui->interval_form.error_visible = true;
        ui->interval_form.error_message = "No tracker selected";
        return;
    }

    // Parse duration and reps
    int duration = atoi(ui->interval_form.duration_buffer);
    if (duration < 30) {
        ui->interval_form.error_visible = true;
        ui->interval_form.error_message = "Duration must be at least 30 seconds";
        return;
    }

    int reps = atoi(ui->interval_form.reps_buffer);
    if (reps < 1) reps = 1;

    // Use label if provided, otherwise use "Exercise"
    const char* label = strlen(ui->interval_form.label_buffer) > 0 ? 
                       ui->interval_form.label_buffer : "Exercise";

    // Add interval to selected tracker
    interval_tracker_add(&ui->selected_tracker->interval_tracker, label, duration, reps);

    // Hide form and clear error
    ui->interval_form.form_visible = false;
    ui->interval_form.error_visible = false;
    ui->interval_form.error_message = NULL;
    memset(ui->interval_form.label_buffer, 0, MAX_LABEL_LENGTH);
    memset(ui->interval_form.duration_buffer, 0, 16);
    memset(ui->interval_form.reps_buffer, 0, 8);
} 
#include "forms.h"
#include <string.h>

static Clay_String make_clay_string(const char* str) {
    return (Clay_String){ .chars = str, .length = strlen(str) };
}

void render_tracker_form(TrackerForm* form, ComboUI* ui) {
    if (!form->form_visible) return;

    CLAY(
        CLAY_ID("tracker_form_container"),
        CLAY_LAYOUT({
            .sizing = {CLAY_SIZING_FIXED(300), CLAY_SIZING_FIXED(200)},
            .childAlignment = {CLAY_ALIGN_X_CENTER},
            .childGap = 20
        }),
        CLAY_FLOATING({
            .attachment = {.element = CLAY_ATTACH_POINT_CENTER_CENTER},
            .offset = {0, 0}
        }),
        CLAY_RECTANGLE({.color = {.r = 0.1f, .g = 0.1f, .b = 0.12f, .a = 0.95f}})
    ) {
        // Header
        CLAY_TEXT(make_clay_string("Add New Tracker"), CLAY_TEXT_CONFIG({
            .fontSize = 24,
            .textColor = ui->active_color
        }));

        // Label input field
        CLAY(
            CLAY_LAYOUT({
                .sizing = {CLAY_SIZING_FIXED(260), CLAY_SIZING_FIXED(40)},
                .childGap = 5
            })
        ) {
            CLAY_TEXT(make_clay_string("Label:"), CLAY_TEXT_CONFIG({
                .fontSize = 16,
                .textColor = ui->active_color
            }));

            CLAY(
                CLAY_RECTANGLE({.color = {.r = 0.2f, .g = 0.2f, .b = 0.2f, .a = 1.0f}})
            ) {
                CLAY_TEXT(make_clay_string(form->label_buffer), CLAY_TEXT_CONFIG({
                    .fontSize = 16,
                    .textColor = ui->active_color
                }));
            }
        }

        if (form->error_visible) {
            CLAY_TEXT(make_clay_string(form->error_message), CLAY_TEXT_CONFIG({
                .fontSize = 14,
                .textColor = ui->error_color
            }));
        }

        // Buttons
        CLAY(
            CLAY_LAYOUT({
                .sizing = {CLAY_SIZING_FIXED(260), CLAY_SIZING_FIXED(40)},
                .layoutDirection = CLAY_LEFT_TO_RIGHT,
                .childGap = 10
            })
        ) {
            CLAY(
                CLAY_ID("tracker_form_submit"),
                CLAY_LAYOUT({
                    .sizing = {CLAY_SIZING_FIXED(120), CLAY_SIZING_FIXED(30)}
                }),
                CLAY_RECTANGLE({.color = Clay_Hovered() ? ui->active_color : ui->paused_color})
            ) {
                CLAY_TEXT(make_clay_string("Add"), CLAY_TEXT_CONFIG({
                    .fontSize = 16,
                    .textColor = {1, 1, 1, 1}
                }));
            }

            CLAY(
                CLAY_ID("tracker_form_cancel"),
                CLAY_LAYOUT({
                    .sizing = {CLAY_SIZING_FIXED(120), CLAY_SIZING_FIXED(30)}
                }),
                CLAY_RECTANGLE({.color = Clay_Hovered() ? ui->error_color : ui->paused_color})
            ) {
                CLAY_TEXT(make_clay_string("Cancel"), CLAY_TEXT_CONFIG({
                    .fontSize = 16,
                    .textColor = {1, 1, 1, 1}
                }));
            }
        }
    }
}

void render_interval_form(IntervalForm* form, ComboUI* ui) {
    if (!form->form_visible) return;

    CLAY(
        CLAY_ID("interval_form_container"),
        CLAY_LAYOUT({
            .sizing = {CLAY_SIZING_FIXED(300), CLAY_SIZING_FIXED(300)},
            .childAlignment = {CLAY_ALIGN_X_CENTER},
            .childGap = 20
        }),
        CLAY_FLOATING({
            .attachment = {.element = CLAY_ATTACH_POINT_CENTER_CENTER},
            .offset = {0, 0}
        }),
        CLAY_RECTANGLE({.color = {.r = 0.1f, .g = 0.1f, .b = 0.12f, .a = 0.95f}})
    ) {
        // Header
        CLAY_TEXT(make_clay_string("Add New Interval"), CLAY_TEXT_CONFIG({
            .fontSize = 24,
            .textColor = ui->active_color
        }));

        // Label input field
        CLAY(
            CLAY_LAYOUT({
                .sizing = {CLAY_SIZING_FIXED(260), CLAY_SIZING_FIXED(40)},
                .childGap = 5
            })
        ) {
            CLAY_TEXT(make_clay_string("Label:"), CLAY_TEXT_CONFIG({
                .fontSize = 16,
                .textColor = {1, 1, 1, 1}
            }));

            CLAY(
                CLAY_RECTANGLE({.color = {.r = 0.2f, .g = 0.2f, .b = 0.2f, .a = 1.0f}})
            ) {
                CLAY_TEXT(make_clay_string(form->label_buffer), CLAY_TEXT_CONFIG({
                    .fontSize = 16,
                    .textColor = {1, 1, 1, 1}
                }));
            }
        }

        // Duration input field
        CLAY(
            CLAY_LAYOUT({
                .sizing = {CLAY_SIZING_FIXED(260), CLAY_SIZING_FIXED(40)},
                .childGap = 5
            })
        ) {
            CLAY_TEXT(make_clay_string("Duration (seconds):"), CLAY_TEXT_CONFIG({
                .fontSize = 16,
                .textColor = {1, 1, 1, 1}
            }));

            CLAY(
                CLAY_RECTANGLE({.color = {.r = 0.2f, .g = 0.2f, .b = 0.2f, .a = 1.0f}})
            ) {
                CLAY_TEXT(make_clay_string(form->duration_buffer), CLAY_TEXT_CONFIG({
                    .fontSize = 16,
                    .textColor = {1, 1, 1, 1}
                }));
            }
        }

        // Reps input field
        CLAY(
            CLAY_LAYOUT({
                .sizing = {CLAY_SIZING_FIXED(260), CLAY_SIZING_FIXED(40)},
                .childGap = 5
            })
        ) {
            CLAY_TEXT(make_clay_string("Reps:"), CLAY_TEXT_CONFIG({
                .fontSize = 16,
                .textColor = {1, 1, 1, 1}
            }));

            CLAY(
                CLAY_RECTANGLE({.color = {.r = 0.2f, .g = 0.2f, .b = 0.2f, .a = 1.0f}})
            ) {
                CLAY_TEXT(make_clay_string(form->reps_buffer), CLAY_TEXT_CONFIG({
                    .fontSize = 16,
                    .textColor = {1, 1, 1, 1}
                }));
            }
        }

        if (form->error_visible) {
            CLAY_TEXT(make_clay_string(form->error_message), CLAY_TEXT_CONFIG({
                .fontSize = 14,
                .textColor = ui->error_color
            }));
        }

        // Buttons
        CLAY(
            CLAY_LAYOUT({
                .childAlignment = {CLAY_ALIGN_X_CENTER},
                .childGap = 10,
                .sizing = {CLAY_SIZING_FIXED(260), CLAY_SIZING_FIXED(40)}
            })
        ) {
            // Add button
            CLAY(
                CLAY_ID("interval_form_submit"),
                CLAY_LAYOUT({.sizing = {CLAY_SIZING_FIXED(120), CLAY_SIZING_FIXED(30)}}),
                CLAY_RECTANGLE({.color = Clay_Hovered() ? ui->active_color : ui->paused_color})
            ) {
                CLAY_TEXT(make_clay_string("Add"), CLAY_TEXT_CONFIG({
                    .fontSize = 16,
                    .textColor = {1, 1, 1, 1}
                }));
            }

            // Cancel button
            CLAY(
                CLAY_ID("interval_form_cancel"),
                CLAY_LAYOUT({.sizing = {CLAY_SIZING_FIXED(120), CLAY_SIZING_FIXED(30)}}),
                CLAY_RECTANGLE({.color = Clay_Hovered() ? ui->error_color : ui->paused_color})
            ) {
                CLAY_TEXT(make_clay_string("Cancel"), CLAY_TEXT_CONFIG({
                    .fontSize = 16,
                    .textColor = {1, 1, 1, 1}
                }));
            }
        }
    }
} 
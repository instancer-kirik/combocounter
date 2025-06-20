#include "widgets.h"
#include "timer.h"
#include <stdio.h>
#include <string.h>

#define CLAY_IMPLEMENTATION
#include "clay.h"

static Clay_String make_clay_string(const char* str) {
    return (Clay_String){ .chars = str, .length = strlen(str) };
}

// Forward declaration
extern void handle_click(Clay_ElementId elementId, Clay_PointerData pointerData, intptr_t userData);

void widget_break_menu(BreakMenu* menu, ComboUI* ui) {
    CLAY(
        CLAY_ID("break_menu_main"),
        CLAY_LAYOUT(CLAY__INIT(Clay_LayoutConfig) {
            .layoutDirection = CLAY_TOP_TO_BOTTOM,
            .sizing = CLAY__INIT(Clay_Sizing) {
                .width = CLAY_SIZING_FIXED(400),
                .height = CLAY_SIZING_FIXED(600)
            },
            .childGap = 16,
            .padding = {16, 16}
        }),
        CLAY_FLOATING(CLAY__INIT(Clay_FloatingElementConfig) {
            .attachment = {
                .element = CLAY_ATTACH_POINT_CENTER_CENTER,
                .parent = CLAY_ATTACH_POINT_CENTER_CENTER
            }
        }),
        CLAY_RECTANGLE(CLAY__INIT(Clay_RectangleElementConfig) {
            .color = (Clay_Color){0.1f, 0.1f, 0.12f, 0.95f}
        })
    ) {
        // Header
        CLAY(
            CLAY_ID("break_menu_header"),
            CLAY_TEXT(
                make_clay_string("Break Menu"),
                CLAY_TEXT_CONFIG(CLAY__INIT(Clay_TextElementConfig) {
                    .textColor = ui->break_color,
                    .fontSize = 24
                })
            )
        );

        // Activity list
        for (int i = 0; i < menu->activity_count; i++) {
            char activity_id[64];
            snprintf(activity_id, sizeof(activity_id), "break_activity_%d", i);
            CLAY(
                CLAY_ID(activity_id),
                CLAY_LAYOUT(CLAY__INIT(Clay_LayoutConfig) {
                    .layoutDirection = CLAY_TOP_TO_BOTTOM,
                    .sizing = CLAY__INIT(Clay_Sizing) {
                        .width = CLAY_SIZING_FIXED(380),
                        .height = CLAY_SIZING_FIXED(80)
                    },
                    .childGap = 8,
                    .padding = {8, 8}
                }),
                CLAY_RECTANGLE(CLAY__INIT(Clay_RectangleElementConfig) {
                    .color = (Clay_Color){0.15f, 0.15f, 0.17f, 1.0f}
                })
            ) {
                // Activity name
                snprintf(activity_id, sizeof(activity_id), "break_activity_name_%d", i);
                CLAY(
                    CLAY_ID(activity_id),
                    CLAY_TEXT(
                        make_clay_string(menu->activities[i].name),
                        CLAY_TEXT_CONFIG(CLAY__INIT(Clay_TextElementConfig) {
                            .textColor = menu->activities[i].is_quick_break ? ui->quick_break_color : ui->break_color,
                            .fontSize = 18
                        })
                    )
                );

                // Activity description
                snprintf(activity_id, sizeof(activity_id), "break_activity_desc_%d", i);
                CLAY(
                    CLAY_ID(activity_id),
                    CLAY_TEXT(
                        make_clay_string(menu->activities[i].description),
                        CLAY_TEXT_CONFIG(CLAY__INIT(Clay_TextElementConfig) {
                            .textColor = ui->paused_color,
                            .fontSize = 14
                        })
                    )
                );

                // Duration
                char duration_text[32];
                int minutes = menu->activities[i].duration / 60;
                snprintf(duration_text, sizeof(duration_text), "%d minutes", minutes);
                snprintf(activity_id, sizeof(activity_id), "break_activity_duration_%d", i);
                CLAY(
                    CLAY_ID(activity_id),
                    CLAY_TEXT(
                        make_clay_string(duration_text),
                        CLAY_TEXT_CONFIG(CLAY__INIT(Clay_TextElementConfig) {
                            .textColor = ui->paused_color,
                            .fontSize = 14
                        })
                    )
                );
            }
        }
    }
}

void widget_objective_progress(ComboState* tracker, int index, Clay_Color objective_color, Clay_Color completed_color, Clay_Color paused_color) {
    char progress_id[64];
    snprintf(progress_id, sizeof(progress_id), "objective_progress_%d", index);
    CLAY(
        CLAY_ID(progress_id),
        CLAY_LAYOUT(CLAY__INIT(Clay_LayoutConfig) {
            .layoutDirection = CLAY_LEFT_TO_RIGHT,
            .sizing = CLAY__INIT(Clay_Sizing) {
                .width = CLAY_SIZING_FIXED(280),
                .height = CLAY_SIZING_FIXED(24)
            },
            .childGap = 8,
            .padding = {8, 8}
        })
    ) {
        // Progress bar background
        snprintf(progress_id, sizeof(progress_id), "obj_progress_bg_%d", index);
        CLAY(
            CLAY_ID(progress_id),
            CLAY_LAYOUT(CLAY__INIT(Clay_LayoutConfig) {
                .sizing = CLAY__INIT(Clay_Sizing) {
                    .width = CLAY_SIZING_FIXED(260),
                    .height = CLAY_SIZING_FIXED(24)
                }
            }),
            CLAY_RECTANGLE(CLAY__INIT(Clay_RectangleElementConfig) {
                .color = paused_color
            })
        ) {
            // Progress bar fill
            float progress = 0.0f;
            if (tracker->active_objective_index < tracker->objective_count) {
                progress = tracker->objectives[tracker->active_objective_index].current_score / 
                          (float)tracker->objectives[tracker->active_objective_index].target_score;
            }
            snprintf(progress_id, sizeof(progress_id), "obj_progress_fill_%d", index);
            CLAY(
                CLAY_ID(progress_id),
                CLAY_LAYOUT(CLAY__INIT(Clay_LayoutConfig) {
                    .sizing = CLAY__INIT(Clay_Sizing) {
                        .width = CLAY_SIZING_FIXED(260 * progress),
                        .height = CLAY_SIZING_FIXED(24)
                    }
                }),
                CLAY_RECTANGLE(CLAY__INIT(Clay_RectangleElementConfig) {
                    .color = tracker->paused ? completed_color : objective_color
                })
            );
        }
    }
}

void widget_interval_tracker(IntervalTracker* intervals, int index, Clay_Color active_color, Clay_Color paused_color) {
    if (!intervals->interval_active || intervals->current_interval_index >= intervals->interval_count) return;

    char interval_id[64];
    snprintf(interval_id, sizeof(interval_id), "interval_tracker_%d", index);
    CLAY(
        CLAY_ID(interval_id),
        CLAY_LAYOUT(CLAY__INIT(Clay_LayoutConfig) {
            .layoutDirection = CLAY_TOP_TO_BOTTOM,
            .sizing = CLAY__INIT(Clay_Sizing) {
                .width = CLAY_SIZING_FIXED(280),
                .height = CLAY_SIZING_FIXED(80)
            },
            .childGap = 4,
            .padding = {8, 8}
        })
    ) {
        char timer_text[32];
        const char* formatted_time = format_time(intervals->current_time);
        strncpy(timer_text, formatted_time, sizeof(timer_text) - 1);
        timer_text[sizeof(timer_text) - 1] = '\0';

        snprintf(interval_id, sizeof(interval_id), "interval_timer_%d", index);
        CLAY(
            CLAY_ID(interval_id),
            CLAY_TEXT(
                make_clay_string(timer_text),
                CLAY_TEXT_CONFIG(CLAY__INIT(Clay_TextElementConfig) {
                    .fontSize = 24,
                    .textColor = intervals->is_running ? active_color : paused_color
                })
            )
        );

        char interval_text[128];
        snprintf(interval_text, sizeof(interval_text), "%s (%d/%d)", 
                intervals->current_interval.label,
                intervals->current_rep,
                intervals->current_interval.reps);

        snprintf(interval_id, sizeof(interval_id), "interval_text_%d", index);
        CLAY(
            CLAY_ID(interval_id),
            CLAY_TEXT(
                make_clay_string(interval_text),
                CLAY_TEXT_CONFIG(CLAY__INIT(Clay_TextElementConfig) {
                    .fontSize = 16,
                    .textColor = intervals->is_running ? active_color : paused_color
                })
            )
        );
    }
}

void widget_tracker_card(ComboState* tracker, int index, Clay_Color active_color, Clay_Color paused_color, Clay_Color perfect_color) {
    char id_buffer[64];
    
    snprintf(id_buffer, sizeof(id_buffer), "tracker_card_%d", index);
    CLAY(
        CLAY_ID(id_buffer),
        CLAY_LAYOUT(CLAY__INIT(Clay_LayoutConfig) {
            .layoutDirection = CLAY_TOP_TO_BOTTOM,
            .sizing = CLAY__INIT(Clay_Sizing) {
                .width = CLAY_SIZING_FIXED(280),
                .height = CLAY_SIZING_FIXED(120)
            },
            .childGap = 8,
            .padding = {8, 8}
        }),
        CLAY_RECTANGLE(CLAY__INIT(Clay_RectangleElementConfig) {
            .color = (Clay_Color){0.15f, 0.15f, 0.17f, 1.0f}
        })
    ) {
        // Header with label and score
        snprintf(id_buffer, sizeof(id_buffer), "tracker_header_%d", index);
        CLAY(
            CLAY_ID(id_buffer),
            CLAY_LAYOUT(CLAY__INIT(Clay_LayoutConfig) {
                .layoutDirection = CLAY_LEFT_TO_RIGHT,
                .sizing = CLAY__INIT(Clay_Sizing) {
                    .width = CLAY_SIZING_FIXED(260),
                    .height = CLAY_SIZING_FIXED(32)
                },
                .childGap = 8
            })
        ) {
            snprintf(id_buffer, sizeof(id_buffer), "tracker_label_%d", index);
            CLAY(
                CLAY_ID(id_buffer),
                CLAY_TEXT(
                    make_clay_string(tracker->label),
                    CLAY_TEXT_CONFIG(CLAY__INIT(Clay_TextElementConfig) {
                        .fontSize = 24,
                        .textColor = tracker->paused ? paused_color : active_color
                    })
                )
            );

            snprintf(id_buffer, sizeof(id_buffer), "tracker_score_%d", index);
            char score_text[32];
            snprintf(score_text, sizeof(score_text), "%d", tracker->score);
            CLAY(
                CLAY_ID(id_buffer),
                CLAY_TEXT(
                    make_clay_string(score_text),
                    CLAY_TEXT_CONFIG(CLAY__INIT(Clay_TextElementConfig) {
                        .fontSize = 24,
                        .textColor = tracker->perfect_hits > 0 ? perfect_color : 
                                    (tracker->paused ? paused_color : active_color)
                    })
                )
            );
        }

        // Objective progress
        widget_objective_progress(tracker, index, active_color, perfect_color, paused_color);

        // Interval tracker if active
        if (tracker->interval_tracker.has_interval) {
            widget_interval_tracker(&tracker->interval_tracker, index, active_color, paused_color);
        }
    }
}

void widget_controls_panel(int index, Clay_Color active_color, Clay_Color paused_color, Clay_Color break_color) {
    extern ComboUI* g_ui_context;

    CLAY(
        CLAY_IDI("controls_panel", index),
        CLAY_LAYOUT(CLAY__INIT(Clay_LayoutConfig) {
            .layoutDirection = CLAY_TOP_TO_BOTTOM,
            .sizing = CLAY__INIT(Clay_Sizing) {
                .width = CLAY_SIZING_FIXED(320),
                .height = CLAY_SIZING_FIXED(720)
            },
            .childGap = 8,
            .padding = {8, 8}
        })
    ) {
        // Add tracker button
        CLAY(
            CLAY_ID("add_tracker_btn"),
            CLAY_LAYOUT(CLAY__INIT(Clay_LayoutConfig) {
                .sizing = CLAY__INIT(Clay_Sizing) {
                    .width = CLAY_SIZING_FIXED(280),
                    .height = CLAY_SIZING_FIXED(40)
                }
            }),
            CLAY_RECTANGLE(CLAY__INIT(Clay_RectangleElementConfig) {
                .color = Clay_Hovered() ? active_color : paused_color
            }),
            Clay_OnHover(handle_click, (intptr_t)g_ui_context)
        ) {
            CLAY_TEXT(
                make_clay_string("Add Tracker"),
                CLAY_TEXT_CONFIG(CLAY__INIT(Clay_TextElementConfig) {
                    .fontSize = 16,
                    .textColor = (Clay_Color){1, 1, 1, 1}
                })
            );
        }

        // Add interval button
        CLAY(
            CLAY_ID("add_interval_btn"),
            CLAY_LAYOUT(CLAY__INIT(Clay_LayoutConfig) {
                .sizing = CLAY__INIT(Clay_Sizing) {
                    .width = CLAY_SIZING_FIXED(280),
                    .height = CLAY_SIZING_FIXED(40)
                }
            }),
            CLAY_RECTANGLE(CLAY__INIT(Clay_RectangleElementConfig) {
                .color = Clay_Hovered() ? active_color : paused_color
            }),
            Clay_OnHover(handle_click, (intptr_t)g_ui_context)
        ) {
            CLAY_TEXT(
                make_clay_string("Add Interval"),
                CLAY_TEXT_CONFIG(CLAY__INIT(Clay_TextElementConfig) {
                    .fontSize = 16,
                    .textColor = (Clay_Color){1, 1, 1, 1}
                })
            );
        }

        // Take break button
        // Break menu button
        CLAY(
            CLAY_ID("break_menu_btn"),
            CLAY_LAYOUT(CLAY__INIT(Clay_LayoutConfig) {
                .sizing = CLAY__INIT(Clay_Sizing) {
                    .width = CLAY_SIZING_FIXED(280),
                    .height = CLAY_SIZING_FIXED(40)
                }
            }),
            CLAY_RECTANGLE(CLAY__INIT(Clay_RectangleElementConfig) {
                .color = Clay_Hovered() ? break_color : paused_color
            }),
            Clay_OnHover(handle_click, (intptr_t)g_ui_context)
        ) {
            CLAY_TEXT(
                make_clay_string("Break Menu"),
                CLAY_TEXT_CONFIG(CLAY__INIT(Clay_TextElementConfig) {
                    .fontSize = 16,
                    .textColor = (Clay_Color){1, 1, 1, 1}
                })
            );
        }
    }
} 
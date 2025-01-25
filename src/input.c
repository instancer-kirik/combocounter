#include "input.h"
#include "raylib.h"
#include <ctype.h>
#include <string.h>
#include <stdlib.h>

void handle_tracker_form_input(TrackerForm* form, int key) {
    if (!form->form_visible) return;

    int len = strlen(form->label_buffer);

    if (key == KEY_BACKSPACE) {
        if (len > 0) {
            form->label_buffer[len - 1] = '\0';
        }
        return;
    }

    if (isprint(key) && len < MAX_LABEL_LENGTH - 1) {
        form->label_buffer[len] = (char)key;
        form->label_buffer[len + 1] = '\0';
    }
}

void handle_interval_form_input(IntervalForm* form, int key) {
    if (!form->form_visible) return;

    int len = strlen(form->label_buffer);
    int duration_len = strlen(form->duration_buffer);
    int reps_len = strlen(form->reps_buffer);

    if (key == KEY_BACKSPACE) {
        if (len > 0) {
            form->label_buffer[len - 1] = '\0';
        } else if (duration_len > 0) {
            form->duration_buffer[duration_len - 1] = '\0';
        } else if (reps_len > 0) {
            form->reps_buffer[reps_len - 1] = '\0';
        }
        return;
    }

    // Handle label input
    if (isprint(key) && len < MAX_LABEL_LENGTH - 1) {
        form->label_buffer[len] = (char)key;
        form->label_buffer[len + 1] = '\0';
        return;
    }

    // Handle duration input
    if (isdigit(key) && duration_len < 15) {
        form->duration_buffer[duration_len] = (char)key;
        form->duration_buffer[duration_len + 1] = '\0';
        return;
    }

    // Handle reps input
    if (isdigit(key) && reps_len < 7) {
        form->reps_buffer[reps_len] = (char)key;
        form->reps_buffer[reps_len + 1] = '\0';
        return;
    }
}

void handle_click(Clay_ElementId elementId, Clay_PointerData pointerData, intptr_t userData) {
    if (pointerData.state != CLAY_POINTER_DATA_PRESSED_THIS_FRAME) return;
    
    ComboUI* ui = (ComboUI*)userData;
    const char* id = elementId.stringId.chars;
    
    if (strcmp(id, "add_tracker_button") == 0) {
        ui->tracker_form.form_visible = true;
        ui->tracker_form.error_visible = false;
        memset(ui->tracker_form.label_buffer, 0, MAX_LABEL_LENGTH);
    }
    else if (strcmp(id, "add_interval_button") == 0) {
        ui->interval_form.form_visible = true;
        ui->interval_form.error_visible = false;
        memset(ui->interval_form.duration_buffer, 0, sizeof(ui->interval_form.duration_buffer));
        memset(ui->interval_form.reps_buffer, 0, sizeof(ui->interval_form.reps_buffer));
        memset(ui->interval_form.label_buffer, 0, MAX_LABEL_LENGTH);
    }
    else if (strcmp(id, "cancel_tracker_button") == 0) {
        ui->tracker_form.form_visible = false;
        ui->tracker_form.error_visible = false;
    }
    else if (strcmp(id, "cancel_interval_button") == 0) {
        ui->interval_form.form_visible = false;
        ui->interval_form.error_visible = false;
    }
}

void handle_input(ComboUI* ui, int key) {
    // Global shortcuts
    if (key == KEY_ESCAPE) {
        // Hide any visible forms
        if (ui->tracker_form.form_visible) {
            ui->tracker_form.form_visible = false;
            ui->tracker_form.error_visible = false;
            ui->tracker_form.error_message = NULL;
            memset(ui->tracker_form.label_buffer, 0, MAX_LABEL_LENGTH);
            return;
        }

        if (ui->interval_form.form_visible) {
            ui->interval_form.form_visible = false;
            ui->interval_form.error_visible = false;
            ui->interval_form.error_message = NULL;
            memset(ui->interval_form.label_buffer, 0, MAX_LABEL_LENGTH);
            memset(ui->interval_form.duration_buffer, 0, 16);
            memset(ui->interval_form.reps_buffer, 0, 8);
            return;
        }

        if (ui->break_menu.visible) {
            ui->break_menu.visible = false;
            return;
        }
    }

    // Handle form input
    handle_tracker_form_input(&ui->tracker_form, key);
    handle_interval_form_input(&ui->interval_form, key);

    // Global shortcuts when no forms are open
    if (!ui->tracker_form.form_visible && !ui->interval_form.form_visible && !ui->break_menu.visible) {
        switch (key) {
            case 'n':
            case 'N':
                if (ui->tracker_count < MAX_TRACKERS) {
                    ui->tracker_form.form_visible = true;
                }
                break;

            case 'i':
            case 'I':
                if (ui->selected_tracker) {
                    ui->interval_form.form_visible = true;
                }
                break;

            case 'b':
            case 'B':
                ui->break_menu.visible = true;
                break;
        }
    }
} 
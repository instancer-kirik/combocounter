#include "ui.h"

void init_ui(ComboUI* ui) {
    ui->perfect_color = COLOR_PERFECT;
    ui->good_color = COLOR_GOOD;
    ui->miss_color = COLOR_MISS;
    ui->paused_color = COLOR_PAUSED;
    ui->active_color = COLOR_ACTIVE;
    ui->objective_color = COLOR_OBJECTIVE;
    ui->completed_color = COLOR_COMPLETED;
    ui->break_color = COLOR_BREAK;
    ui->quick_break_color = COLOR_QUICK_BREAK;
    ui->error_color = COLOR_ERROR;
    ui->tracker_count = 0;
    ui->animation_time = 0.0f;
    
    // Initialize forms
    ui->tracker_form.form_visible = false;
    ui->tracker_form.error_visible = false;
    ui->tracker_form.error_message = NULL;
    memset(ui->tracker_form.label_buffer, 0, MAX_LABEL_LENGTH);
    
    ui->interval_form.form_visible = false;
    ui->interval_form.error_visible = false;
    ui->interval_form.error_message = NULL;
    memset(ui->interval_form.label_buffer, 0, MAX_LABEL_LENGTH);
    memset(ui->interval_form.duration_buffer, 0, sizeof(ui->interval_form.duration_buffer));
    memset(ui->interval_form.reps_buffer, 0, sizeof(ui->interval_form.reps_buffer));
    ui->interval_form.selected_tracker = -1;
    
    // Enable Clay debug features
    Clay_SetDebugModeEnabled(true);
    Clay_SetCullingEnabled(true);
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
    
    combo_init(&ui->trackers[ui->tracker_count], ui->tracker_form.label_buffer);
    ui->tracker_count++;
    
    ui->tracker_form.form_visible = false;
    ui->tracker_form.error_visible = false;
    memset(ui->tracker_form.label_buffer, 0, MAX_LABEL_LENGTH);
}

void add_new_interval(ComboUI* ui) {
    // Parse duration from buffer
    float duration = 0.0f;
    if (strlen(ui->interval_form.duration_buffer) > 0) {
        duration = atof(ui->interval_form.duration_buffer);
    }
    
    if (duration < 30.0f) {
        ui->interval_form.error_visible = true;
        ui->interval_form.error_message = "Duration must be at least 30 seconds";
        return;
    }
    
    // Parse reps from buffer
    uint32_t reps = 1;
    if (strlen(ui->interval_form.reps_buffer) > 0) {
        reps = atoi(ui->interval_form.reps_buffer);
    }
    
    if (ui->interval_form.selected_tracker < 0 || ui->interval_form.selected_tracker >= ui->tracker_count) {
        ui->interval_form.error_visible = true;
        ui->interval_form.error_message = "Invalid tracker selected";
        return;
    }
    
    // Use label if provided, otherwise use default
    const char* label = strlen(ui->interval_form.label_buffer) > 0 ? 
        ui->interval_form.label_buffer : "Exercise";
    
    interval_tracker_add(
        &ui->trackers[ui->interval_form.selected_tracker].interval_tracker,
        reps,
        duration,
        label
    );
    
    ui->interval_form.form_visible = false;
    ui->interval_form.error_visible = false;
    memset(ui->interval_form.label_buffer, 0, MAX_LABEL_LENGTH);
    memset(ui->interval_form.duration_buffer, 0, sizeof(ui->interval_form.duration_buffer));
    memset(ui->interval_form.reps_buffer, 0, sizeof(ui->interval_form.reps_buffer));
    ui->interval_form.selected_tracker = -1;
} 
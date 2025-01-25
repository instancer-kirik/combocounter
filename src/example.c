// Create multiple trackers
ComboState coding_streak = {};
ComboState exercise_streak = {};
ComboState focus_time = {};

// Initialize with labels
combo_init(&coding_streak, "Coding Streak");
combo_init(&exercise_streak, "Exercise Streak"); 
combo_init(&focus_time, "Focus Time");

// Background tracking example
while (running) {
    float dt = get_delta_time();
    
    // Handle alt-tab/focus changes
    if (window_lost_focus) {
        combo_handle_focus_change(&coding_streak, false);
        // Exercise streak keeps going
        combo_handle_focus_change(&focus_time, false);
    }
    
    // Increment on events
    if (git_commit_detected) {
        combo_increment(&coding_streak, 1);
    }
    
    if (exercise_detected) {
        combo_increment(&exercise_streak, 1);
    }
    
    // Update all trackers
    combo_update(&coding_streak, dt);
    combo_update(&exercise_streak, dt);
    combo_update(&focus_time, dt);
}

void render_combo_ui(ComboUI* ui) {
    CLAY(CLAY_ID("combo_container"),
         CLAY_LAYOUT({.sizing = {CLAY_SIZING_FIXED(200), CLAY_SIZING_GROW({})}})) {
        
        // Render each active tracker
        for (int i = 0; i < ui->tracker_count; i++) {
            ComboState* tracker = &ui->trackers[i];
            
            CLAY(CLAY_LAYOUT({.childAlignment = {CLAY_ALIGN_X_CENTER}})) {
                // Label
                CLAY_TEXT(tracker->label, CLAY_TEXT_CONFIG({.fontSize = 16}));
                
                // Combo count with multiplier
                char combo_text[32];
                snprintf(combo_text, sizeof(combo_text), "%dx", tracker->combo);
                CLAY_TEXT(combo_text, CLAY_TEXT_CONFIG({
                    .fontSize = 24,
                    .textColor = tracker->paused ? ui->paused_color : ui->active_color
                }));
                
                // Decay timer bar
                if (!tracker->paused) {
                    render_decay_bar(tracker);
                }
            }
        }
    }
}

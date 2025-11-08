#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "core.h"

// Constants for combo mechanics
#define COMBO_DECAY_TIME 5.0f  // Time in seconds before combo starts decaying
#define COMBO_DECAY_RATE 1.0f  // How many points lost per second
#define BASE_MULTIPLIER 1.0f
#define MULTIPLIER_INCREASE 0.1f
#define MAX_MULTIPLIER 3.0f

void combo_init(ComboState* state, const char* label) {
    strncpy(state->label, label, MAX_LABEL_LENGTH - 1);
    state->label[MAX_LABEL_LENGTH - 1] = '\0';
    state->score = 0;
    state->combo = 0;
    state->max_combo = 0;
    state->paused = true;
    state->has_objective = false;
    state->objective = 0;
    state->completed_intervals = 0;
    state->multiplier = BASE_MULTIPLIER;
    state->decay_pause = 0.0f;
    state->total_hits = 0;
    state->perfect_hits = 0;
    state->miss_hits = 0;
    state->objectives = NULL;
    state->objective_count = 0;
    state->active_objective_index = 0;
    interval_tracker_init(&state->interval_tracker);
}

void combo_increment(ComboState* state, uint32_t amount) {
    if (state->paused) return;

    state->total_hits++;
    state->perfect_hits++;  // For now, all hits are perfect
    
    // Update score with multiplier
    state->score += (uint32_t)(amount * state->multiplier);
    
    // Increase combo and multiplier
    state->combo++;
    if (state->combo > state->max_combo) {
        state->max_combo = state->combo;
    }
    
    state->multiplier = BASE_MULTIPLIER + (MULTIPLIER_INCREASE * state->combo);
    if (state->multiplier > MAX_MULTIPLIER) {
        state->multiplier = MAX_MULTIPLIER;
    }
    
    // Reset decay timer
    state->decay_pause = COMBO_DECAY_TIME;
    
    // Update objective progress if any
    combo_update_objective_progress(state, amount);
}

void combo_decrement(ComboState* state, uint32_t amount) {
    if (state->paused) return;

    state->miss_hits++;
    if (state->score >= amount) {
        state->score -= amount;
    } else {
        state->score = 0;
    }
    
    // Reset combo and multiplier
    state->combo = 0;
    state->multiplier = BASE_MULTIPLIER;
}

void combo_pause(ComboState* state) {
    state->paused = true;
    if (state->interval_tracker.has_interval) {
        state->interval_tracker.is_running = false;
    }
}

void combo_resume(ComboState* state) {
    state->paused = false;
    if (state->interval_tracker.has_interval) {
        state->interval_tracker.is_running = true;
    }
}

void combo_update(ComboState* state, float dt) {
    if (state->paused) return;

    if (state->interval_tracker.has_interval && state->interval_tracker.is_running) {
        state->interval_tracker.current_time -= dt;

        if (state->interval_tracker.current_time <= 0) {
            // Interval completed
            state->score += 10;
            state->combo++;

            if (state->has_objective) {
                state->completed_intervals++;
                if (state->completed_intervals >= state->objective) {
                    // Objective completed
                    state->score += 50;
                }
            }

            if (state->interval_tracker.current_rep < state->interval_tracker.current_interval.reps) {
                // Start next rep
                state->interval_tracker.current_rep++;
                state->interval_tracker.current_time = state->interval_tracker.current_interval.duration;
            } else {
                // All reps completed
                state->interval_tracker.has_interval = false;
            }
        }
    }
}

void combo_handle_focus_change(ComboState* state, bool focused) {
    if (!focused) {
        combo_pause(state);
    }
}

void combo_set_objectives(ComboState* state, Objective* objectives, uint32_t count) {
    // Free existing objectives if any
    if (state->objectives) {
        free(state->objectives);
    }
    
    // Allocate and copy new objectives
    state->objectives = malloc(sizeof(Objective) * count);
    state->objective_count = count;
    state->active_objective_index = 0;
    
    for (uint32_t i = 0; i < count; i++) {
        strcpy(state->objectives[i].name, objectives[i].name);
        strcpy(state->objectives[i].description, objectives[i].description);
        state->objectives[i].target_score = objectives[i].target_score;
        state->objectives[i].current_score = objectives[i].current_score;
        state->objectives[i].completed = objectives[i].completed;
    }
}

void combo_switch_objective(ComboState* state, uint32_t index) {
    if (index < state->objective_count) {
        state->active_objective_index = index;
    }
}

void combo_update_objective_progress(ComboState* state, uint32_t score_increment) {
    if (state->objective_count == 0) return;
    
    Objective* current = &state->objectives[state->active_objective_index];
    current->current_score += score_increment;
    
    if (current->current_score >= current->target_score) {
        current->completed = true;
    }
}

void combo_save_state(ComboState* state, const char* file) {
    FILE* f = fopen(file, "wb");
    if (!f) return;
    
    // Write basic state
    fwrite(state, sizeof(ComboState), 1, f);
    
    // Write label
    size_t label_len = strlen(state->label);
    fwrite(&label_len, sizeof(size_t), 1, f);
    fwrite(state->label, 1, label_len, f);
    
    // Write objectives
    for (uint32_t i = 0; i < state->objective_count; i++) {
        Objective* obj = &state->objectives[i];
        
        size_t name_len = strlen(obj->name);
        size_t desc_len = strlen(obj->description);
        
        fwrite(&name_len, sizeof(size_t), 1, f);
        fwrite(obj->name, 1, name_len, f);
        
        fwrite(&desc_len, sizeof(size_t), 1, f);
        fwrite(obj->description, 1, desc_len, f);
        
        fwrite(&obj->target_score, sizeof(uint32_t), 1, f);
        fwrite(&obj->current_score, sizeof(uint32_t), 1, f);
        fwrite(&obj->completed, sizeof(bool), 1, f);
    }
    
    fclose(f);
}

void combo_load_state(ComboState* state, const char* file) {
    FILE* f = fopen(file, "rb");
    if (!f) return;
    
    // Read basic state
    ComboState temp;
    if (fread(&temp, sizeof(ComboState), 1, f) != 1) {
        fclose(f);
        return;
    }
    
    // Read label
    size_t label_len;
    if (fread(&label_len, sizeof(size_t), 1, f) != 1) {
        fclose(f);
        return;
    }
    
    char* label = malloc(label_len + 1);
    if (fread(label, 1, label_len, f) != label_len) {
        free(label);
        fclose(f);
        return;
    }
    label[label_len] = '\0';
    
    // Free existing state
    if (state->objectives) {
        free(state->objectives);
    }
    
    // Copy new state
    *state = temp;
    strcpy(state->label, label);
    free(label);
    
    state->objectives = malloc(sizeof(Objective) * state->objective_count);
    
    // Read objectives
    for (uint32_t i = 0; i < state->objective_count; i++) {
        Objective* obj = &state->objectives[i];
        
        size_t name_len, desc_len;
        char temp_str[MAX_LABEL_LENGTH];
        
        // Read name
        if (fread(&name_len, sizeof(size_t), 1, f) != 1) break;
        if (name_len >= MAX_LABEL_LENGTH) name_len = MAX_LABEL_LENGTH - 1;
        if (fread(temp_str, 1, name_len, f) != name_len) break;
        temp_str[name_len] = '\0';
        strcpy(obj->name, temp_str);
        
        // Read description
        if (fread(&desc_len, sizeof(size_t), 1, f) != 1) break;
        if (desc_len >= MAX_LABEL_LENGTH) desc_len = MAX_LABEL_LENGTH - 1;
        if (fread(temp_str, 1, desc_len, f) != desc_len) break;
        temp_str[desc_len] = '\0';
        strcpy(obj->description, temp_str);
        
        if (fread(&obj->target_score, sizeof(uint32_t), 1, f) != 1) break;
        if (fread(&obj->current_score, sizeof(uint32_t), 1, f) != 1) break;
        if (fread(&obj->completed, sizeof(bool), 1, f) != 1) break;
    }
    
    fclose(f);
}

void interval_tracker_init(IntervalTracker* tracker) {
    tracker->has_interval = false;
    tracker->is_running = false;
    tracker->current_time = 0;
    tracker->current_rep = 0;
    memset(&tracker->current_interval, 0, sizeof(Interval));
}

void interval_tracker_add(IntervalTracker* tracker, const char* label, int duration, int reps) {
    strncpy(tracker->current_interval.label, label, MAX_LABEL_LENGTH - 1);
    tracker->current_interval.label[MAX_LABEL_LENGTH - 1] = '\0';
    tracker->current_interval.duration = duration;
    tracker->current_interval.reps = reps;
    tracker->has_interval = true;
    tracker->is_running = false;
    tracker->current_time = duration;
    tracker->current_rep = 1;
}

void interval_tracker_start(IntervalTracker* tracker) {
    if (tracker->interval_count == 0) return;
    
    tracker->interval_active = true;
    tracker->current_time = tracker->intervals[tracker->current_interval_index].duration;
}

void interval_tracker_stop(IntervalTracker* tracker) {
    tracker->interval_active = false;
}

void interval_tracker_reset(IntervalTracker* tracker) {
    tracker->current_interval_index = 0;
    tracker->current_time = 0;
    tracker->interval_active = false;
}

void interval_tracker_update(IntervalTracker* tracker, float dt) {
    if (!tracker->interval_active || tracker->interval_count == 0) return;
    
    tracker->current_time -= dt;
    
    if (tracker->current_time <= 0) {
        // Move to next interval
        tracker->current_interval_index = (tracker->current_interval_index + 1) % tracker->interval_count;
        tracker->current_time = tracker->intervals[tracker->current_interval_index].duration;
    }
}

void interval_tracker_clear(IntervalTracker* tracker) {
    if (tracker->intervals) {
        for (uint32_t i = 0; i < tracker->interval_count; i++) {
            free(tracker->intervals[i].label);
        }
        free(tracker->intervals);
    }
    interval_tracker_init(tracker);
}

void objective_init(Objective* objective, const char* name, const char* description, int target_score) {
    strncpy(objective->name, name, MAX_LABEL_LENGTH - 1);
    objective->name[MAX_LABEL_LENGTH - 1] = '\0';
    strncpy(objective->description, description, MAX_LABEL_LENGTH - 1);
    objective->description[MAX_LABEL_LENGTH - 1] = '\0';
    objective->target_score = target_score;
    objective->current_score = 0;
    objective->completed = false;
}

void objective_update(Objective* objective, int score) {
    objective->current_score = score;
    objective->completed = score >= objective->target_score;
}

// Multi-tracker save/load functions
void combo_save_all_trackers(ComboState* trackers, int tracker_count, const char* file) {
    FILE* f = fopen(file, "wb");
    if (!f) return;
    
    // Write tracker count
    fwrite(&tracker_count, sizeof(int), 1, f);
    
    // Write each tracker
    for (int i = 0; i < tracker_count; i++) {
        ComboState* state = &trackers[i];
        
        // Write basic state (excluding pointers)
        fwrite(&state->score, sizeof(int), 1, f);
        fwrite(&state->combo, sizeof(int), 1, f);
        fwrite(&state->max_combo, sizeof(int), 1, f);
        fwrite(&state->paused, sizeof(bool), 1, f);
        fwrite(&state->has_objective, sizeof(bool), 1, f);
        fwrite(&state->objective, sizeof(int), 1, f);
        fwrite(&state->completed_intervals, sizeof(int), 1, f);
        fwrite(&state->multiplier, sizeof(float), 1, f);
        fwrite(&state->decay_pause, sizeof(float), 1, f);
        fwrite(&state->total_hits, sizeof(uint32_t), 1, f);
        fwrite(&state->perfect_hits, sizeof(uint32_t), 1, f);
        fwrite(&state->miss_hits, sizeof(uint32_t), 1, f);
        fwrite(&state->objective_count, sizeof(uint32_t), 1, f);
        fwrite(&state->active_objective_index, sizeof(uint32_t), 1, f);
        
        // Write label
        size_t label_len = strlen(state->label);
        fwrite(&label_len, sizeof(size_t), 1, f);
        fwrite(state->label, 1, label_len, f);
        
        // Write objectives
        for (uint32_t j = 0; j < state->objective_count; j++) {
            Objective* obj = &state->objectives[j];
            
            size_t name_len = strlen(obj->name);
            size_t desc_len = strlen(obj->description);
            
            fwrite(&name_len, sizeof(size_t), 1, f);
            fwrite(obj->name, 1, name_len, f);
            
            fwrite(&desc_len, sizeof(size_t), 1, f);
            fwrite(obj->description, 1, desc_len, f);
            
            fwrite(&obj->target_score, sizeof(int), 1, f);
            fwrite(&obj->current_score, sizeof(int), 1, f);
            fwrite(&obj->completed, sizeof(bool), 1, f);
        }
        
        // Write interval tracker
        fwrite(&state->interval_tracker.has_interval, sizeof(bool), 1, f);
        fwrite(&state->interval_tracker.is_running, sizeof(bool), 1, f);
        fwrite(&state->interval_tracker.interval_active, sizeof(bool), 1, f);
        fwrite(&state->interval_tracker.current_time, sizeof(int), 1, f);
        fwrite(&state->interval_tracker.current_rep, sizeof(int), 1, f);
        fwrite(&state->interval_tracker.interval_count, sizeof(int), 1, f);
        fwrite(&state->interval_tracker.current_interval_index, sizeof(int), 1, f);
        
        // Write current interval
        size_t interval_label_len = strlen(state->interval_tracker.current_interval.label);
        fwrite(&interval_label_len, sizeof(size_t), 1, f);
        fwrite(state->interval_tracker.current_interval.label, 1, interval_label_len, f);
        fwrite(&state->interval_tracker.current_interval.duration, sizeof(int), 1, f);
        fwrite(&state->interval_tracker.current_interval.reps, sizeof(int), 1, f);
    }
    
    fclose(f);
}

int combo_load_all_trackers(ComboState* trackers, int max_trackers, const char* file) {
    FILE* f = fopen(file, "rb");
    if (!f) return 0;
    
    int tracker_count;
    if (fread(&tracker_count, sizeof(int), 1, f) != 1) {
        fclose(f);
        return 0;
    }
    
    // Limit to max_trackers
    if (tracker_count > max_trackers) {
        tracker_count = max_trackers;
    }
    
    // Load each tracker
    for (int i = 0; i < tracker_count; i++) {
        ComboState* state = &trackers[i];
        
        // Initialize the state first
        memset(state, 0, sizeof(ComboState));
        
        // Read basic state
        if (fread(&state->score, sizeof(int), 1, f) != 1) break;
        if (fread(&state->combo, sizeof(int), 1, f) != 1) break;
        if (fread(&state->max_combo, sizeof(int), 1, f) != 1) break;
        if (fread(&state->paused, sizeof(bool), 1, f) != 1) break;
        if (fread(&state->has_objective, sizeof(bool), 1, f) != 1) break;
        if (fread(&state->objective, sizeof(int), 1, f) != 1) break;
        if (fread(&state->completed_intervals, sizeof(int), 1, f) != 1) break;
        if (fread(&state->multiplier, sizeof(float), 1, f) != 1) break;
        if (fread(&state->decay_pause, sizeof(float), 1, f) != 1) break;
        if (fread(&state->total_hits, sizeof(uint32_t), 1, f) != 1) break;
        if (fread(&state->perfect_hits, sizeof(uint32_t), 1, f) != 1) break;
        if (fread(&state->miss_hits, sizeof(uint32_t), 1, f) != 1) break;
        if (fread(&state->objective_count, sizeof(uint32_t), 1, f) != 1) break;
        if (fread(&state->active_objective_index, sizeof(uint32_t), 1, f) != 1) break;
        
        // Read label
        size_t label_len;
        if (fread(&label_len, sizeof(size_t), 1, f) != 1) break;
        if (label_len >= MAX_LABEL_LENGTH) label_len = MAX_LABEL_LENGTH - 1;
        if (fread(state->label, 1, label_len, f) != label_len) break;
        state->label[label_len] = '\0';
        
        // Allocate and read objectives
        if (state->objective_count > 0) {
            state->objectives = malloc(sizeof(Objective) * state->objective_count);
            for (uint32_t j = 0; j < state->objective_count; j++) {
                Objective* obj = &state->objectives[j];
                
                size_t name_len, desc_len;
                char temp_str[MAX_LABEL_LENGTH];
                
                // Read name
                if (fread(&name_len, sizeof(size_t), 1, f) != 1) break;
                if (name_len >= MAX_LABEL_LENGTH) name_len = MAX_LABEL_LENGTH - 1;
                if (fread(temp_str, 1, name_len, f) != name_len) break;
                temp_str[name_len] = '\0';
                strcpy(obj->name, temp_str);
                
                // Read description
                if (fread(&desc_len, sizeof(size_t), 1, f) != 1) break;
                if (desc_len >= MAX_LABEL_LENGTH) desc_len = MAX_LABEL_LENGTH - 1;
                if (fread(temp_str, 1, desc_len, f) != desc_len) break;
                temp_str[desc_len] = '\0';
                strcpy(obj->description, temp_str);
                
                if (fread(&obj->target_score, sizeof(int), 1, f) != 1) break;
                if (fread(&obj->current_score, sizeof(int), 1, f) != 1) break;
                if (fread(&obj->completed, sizeof(bool), 1, f) != 1) break;
            }
        }
        
        // Read interval tracker
        if (fread(&state->interval_tracker.has_interval, sizeof(bool), 1, f) != 1) break;
        if (fread(&state->interval_tracker.is_running, sizeof(bool), 1, f) != 1) break;
        if (fread(&state->interval_tracker.interval_active, sizeof(bool), 1, f) != 1) break;
        if (fread(&state->interval_tracker.current_time, sizeof(int), 1, f) != 1) break;
        if (fread(&state->interval_tracker.current_rep, sizeof(int), 1, f) != 1) break;
        if (fread(&state->interval_tracker.interval_count, sizeof(int), 1, f) != 1) break;
        if (fread(&state->interval_tracker.current_interval_index, sizeof(int), 1, f) != 1) break;
        
        // Read current interval
        size_t interval_label_len;
        if (fread(&interval_label_len, sizeof(size_t), 1, f) != 1) break;
        if (interval_label_len >= MAX_LABEL_LENGTH) interval_label_len = MAX_LABEL_LENGTH - 1;
        if (fread(state->interval_tracker.current_interval.label, 1, interval_label_len, f) != interval_label_len) break;
        state->interval_tracker.current_interval.label[interval_label_len] = '\0';
        if (fread(&state->interval_tracker.current_interval.duration, sizeof(int), 1, f) != 1) break;
        if (fread(&state->interval_tracker.current_interval.reps, sizeof(int), 1, f) != 1) break;
    }
    
    fclose(f);
    return tracker_count;
}

// Server sync functions will be implemented later when we add the networking code
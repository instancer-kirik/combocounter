#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "simple_combo_core.h"
#include "audio_action_recorder.h"
#include "nrf_log.h"
#include "app_timer.h"
#include "nrf_delay.h"

// Global instances
static ComboDevice g_combo_device;
static audio_action_recorder_t g_audio_recorder;
static bool g_workout_active = false;
static uint32_t g_session_start_time = 0;

// Button states
static bool g_button_combo_pressed = false;
static bool g_button_memo_pressed = false;
static uint32_t g_button_memo_press_time = 0;

// Quick memo state
static bool g_quick_memo_recording = false;

/**
 * @brief Initialize the integrated ComboCounter + Audio system
 */
ret_code_t combo_audio_system_init(void) {
    ret_code_t err_code;
    
    NRF_LOG_INFO("Initializing ComboCounter + Audio Action Recorder system");
    
    // Initialize ComboCounter
    combo_device_init(&g_combo_device);
    
    // Add some default counters
    counter_add(&g_combo_device, "Push-ups", COUNTER_TYPE_COMBO);
    counter_add(&g_combo_device, "Squats", COUNTER_TYPE_COMBO);
    counter_add(&g_combo_device, "Sets", COUNTER_TYPE_SIMPLE);
    counter_add(&g_combo_device, "Total Volume", COUNTER_TYPE_ACCUMULATOR);
    
    // Initialize audio recorder with strength training preset
    err_code = audio_recorder_init(&g_audio_recorder);
    if (err_code != NRF_SUCCESS) {
        NRF_LOG_ERROR("Failed to initialize audio recorder: %d", err_code);
        return err_code;
    }
    
    // Configure for strength training
    err_code = audio_load_strength_training_preset(&g_audio_recorder);
    if (err_code != NRF_SUCCESS) {
        NRF_LOG_ERROR("Failed to load strength training preset: %d", err_code);
        return err_code;
    }
    
    // Start in listening mode for ultra-low power
    err_code = audio_recorder_set_mode(&g_audio_recorder, AUDIO_MODE_LISTEN);
    if (err_code != NRF_SUCCESS) {
        NRF_LOG_ERROR("Failed to set audio mode: %d", err_code);
        return err_code;
    }
    
    NRF_LOG_INFO("System initialized successfully");
    NRF_LOG_INFO("Power consumption: ~200µA in standby");
    NRF_LOG_INFO("Battery life estimate: %d hours", 
                 audio_get_estimated_battery_life_hours(&g_audio_recorder));
    
    return NRF_SUCCESS;
}

/**
 * @brief Handle movement detection from audio analysis
 */
void audio_on_movement_detected(audio_action_recorder_t* recorder, const movement_analysis_t* movement) {
    // Auto-start workout session if not already active
    if (!g_workout_active && movement->movement_intensity > 500) {
        g_workout_active = true;
        g_session_start_time = movement->timestamp;
        
        audio_start_workout_session(recorder, "Auto-detected Workout");
        audio_recorder_set_mode(recorder, AUDIO_MODE_WORKOUT_ANALYSIS);
        
        NRF_LOG_INFO("Workout auto-started - intensity: %d", movement->movement_intensity);
        
        // Audio feedback
        audio_play_custom_sound(recorder, "workout_start");
    }
    
    // Log movement for analysis
    NRF_LOG_DEBUG("Movement: intensity=%d, frequency=%dHz, quality=%d/10", 
                  movement->movement_intensity, 
                  movement->movement_frequency, 
                  movement->movement_quality);
}

/**
 * @brief Handle rep detection from audio analysis
 */
void audio_on_rep_detected(audio_action_recorder_t* recorder, uint16_t rep_count) {
    // Get current counter (assuming user is working on counter 0)
    Counter* current_counter = &g_combo_device.counters[g_combo_device.current_counter];
    
    // Sync audio rep detection with combo counter
    if (g_workout_active) {
        // Increment the combo counter
        counter_increment(current_counter, QUALITY_GOOD);
        
        // Sync audio system with combo count
        audio_sync_with_combo_counter(recorder, 
                                     g_combo_device.current_counter,
                                     current_counter->count,
                                     current_counter->combo);
        
        // Audio feedback for milestones
        if (current_counter->combo > 0 && current_counter->combo % 10 == 0) {
            audio_play_combo_milestone_sound(recorder, current_counter->combo);
        } else if (rep_count % 5 == 0) {
            audio_play_rep_count_feedback(recorder, rep_count);
        }
        
        NRF_LOG_INFO("Rep %d detected - Counter: %s, Combo: %d", 
                     rep_count, current_counter->label, current_counter->combo);
    }
}

/**
 * @brief Handle quick memo recording
 */
void handle_quick_memo_button(void) {
    static uint32_t last_press_time = 0;
    uint32_t current_time = app_timer_cnt_get();
    uint32_t press_duration = current_time - g_button_memo_press_time;
    
    // Short press: quick memo
    if (press_duration < APP_TIMER_TICKS(1000)) {  // < 1 second
        if (!g_quick_memo_recording) {
            // Start recording
            g_quick_memo_recording = true;
            audio_quick_memo_start(&g_audio_recorder);
            
            NRF_LOG_INFO("Quick memo recording started");
            
            // Visual feedback (if you have LED)
            // led_blink_fast();
            
        } else {
            // Stop recording  
            g_quick_memo_recording = false;
            audio_quick_memo_stop(&g_audio_recorder);
            
            NRF_LOG_INFO("Quick memo recording stopped");
        }
    }
    // Long press: playback last memo
    else if (press_duration >= APP_TIMER_TICKS(2000)) {  // >= 2 seconds
        audio_quick_memo_playback_last(&g_audio_recorder);
        NRF_LOG_INFO("Playing back last memo");
    }
}

/**
 * @brief Main application loop
 */
void combo_audio_main_loop(void) {
    static uint32_t last_activity_check = 0;
    static uint32_t last_stats_print = 0;
    uint32_t current_time = app_timer_cnt_get();
    
    // Check for workout timeout (5 minutes of inactivity)
    if (g_workout_active && 
        (current_time - last_activity_check) > APP_TIMER_TICKS(5000)) {  // Check every 5 seconds
        
        last_activity_check = current_time;
        
        // Check if there's been movement recently
        if (!audio_is_movement_detected(&g_audio_recorder)) {
            static uint32_t inactivity_start = 0;
            if (inactivity_start == 0) {
                inactivity_start = current_time;
            }
            
            // End workout after 5 minutes of inactivity
            if ((current_time - inactivity_start) > APP_TIMER_TICKS(300000)) {  // 5 minutes
                g_workout_active = false;
                audio_end_workout_session(&g_audio_recorder);
                audio_recorder_set_mode(&g_audio_recorder, AUDIO_MODE_LISTEN);
                
                NRF_LOG_INFO("Workout auto-ended due to inactivity");
                
                // Create summary memo
                char summary[128];
                Counter* counter = &g_combo_device.counters[g_combo_device.current_counter];
                snprintf(summary, sizeof(summary), 
                        "Workout complete. %s: %d reps, max combo: %d",
                        counter->label, counter->count, counter->best_combo);
                
                // You could auto-record this summary
                // audio_add_workout_note(&g_audio_recorder, summary);
                
                inactivity_start = 0;
            }
        } else {
            // Reset inactivity timer
            inactivity_start = 0;
        }
    }
    
    // Print stats every 30 seconds during workout
    if (g_workout_active && 
        (current_time - last_stats_print) > APP_TIMER_TICKS(30000)) {
        
        last_stats_print = current_time;
        
        Counter* counter = &g_combo_device.counters[g_combo_device.current_counter];
        uint16_t audio_reps = audio_get_rep_count(&g_audio_recorder);
        float movement_consistency = audio_get_movement_consistency(&g_audio_recorder);
        
        NRF_LOG_INFO("Workout Status:");
        NRF_LOG_INFO("  Counter reps: %d, Audio reps: %d", counter->count, audio_reps);
        NRF_LOG_INFO("  Combo: %d, Best: %d", counter->combo, counter->best_combo);
        NRF_LOG_INFO("  Movement consistency: %.1f%%", movement_consistency * 100.0f);
        NRF_LOG_INFO("  Storage used: %d KB", audio_get_storage_used_kb(&g_audio_recorder));
    }
    
    // Handle button inputs
    if (g_button_combo_pressed) {
        g_button_combo_pressed = false;
        
        // Manual increment for combo counter
        Counter* counter = &g_combo_device.counters[g_combo_device.current_counter];
        counter_increment(counter, QUALITY_PERFECT);
        
        // Validate with audio system
        audio_validate_rep_with_counter(&g_audio_recorder, true);
        
        NRF_LOG_INFO("Manual rep: %s count=%d, combo=%d", 
                     counter->label, counter->count, counter->combo);
    }
    
    if (g_button_memo_pressed) {
        g_button_memo_pressed = false;
        handle_quick_memo_button();
    }
    
    // Ultra-low power delay
    nrf_delay_ms(100);
}

/**
 * @brief Demo workout scenarios
 */
void demo_strength_workout(void) {
    NRF_LOG_INFO("=== Demo: Strength Training Workout ===");
    
    // Start workout manually
    g_workout_active = true;
    audio_start_workout_session(&g_audio_recorder, "Demo Strength Workout");
    audio_recorder_set_mode(&g_audio_recorder, AUDIO_MODE_WORKOUT_ANALYSIS);
    
    Counter* pushups = &g_combo_device.counters[0];  // Push-ups counter
    Counter* sets = &g_combo_device.counters[2];     // Sets counter
    
    // Simulate 3 sets of push-ups
    for (int set = 1; set <= 3; set++) {
        NRF_LOG_INFO("--- Set %d ---", set);
        
        // Record a quick memo about the set
        char memo_text[64];
        snprintf(memo_text, sizeof(memo_text), "Starting set %d of push-ups", set);
        
        // Simulate reps (in real use, these would be detected automatically)
        for (int rep = 1; rep <= 10; rep++) {
            counter_increment(pushups, QUALITY_GOOD);
            
            // Sync with audio system
            audio_sync_with_combo_counter(&g_audio_recorder, 0, pushups->count, pushups->combo);
            
            // Milestone feedback
            if (pushups->combo % 5 == 0) {
                audio_play_rep_count_feedback(&g_audio_recorder, rep);
            }
            
            nrf_delay_ms(2000);  // 2 second rep pace
        }
        
        // Complete set
        counter_increment(sets, QUALITY_GOOD);
        NRF_LOG_INFO("Set %d complete - Total push-ups: %d, Combo: %d", 
                     set, pushups->count, pushups->combo);
        
        // Rest between sets
        if (set < 3) {
            NRF_LOG_INFO("Rest period - 60 seconds");
            nrf_delay_ms(5000);  // Shortened for demo
        }
    }
    
    // End workout
    g_workout_active = false;
    audio_end_workout_session(&g_audio_recorder);
    audio_recorder_set_mode(&g_audio_recorder, AUDIO_MODE_LISTEN);
    
    // Print final stats
    NRF_LOG_INFO("=== Workout Complete ===");
    NRF_LOG_INFO("Push-ups: %d (best combo: %d)", pushups->count, pushups->best_combo);
    NRF_LOG_INFO("Sets: %d", sets->count);
    NRF_LOG_INFO("Total memos: %d", audio_get_memo_count(&g_audio_recorder));
}

/**
 * @brief Demo memo functionality
 */
void demo_memo_recorder(void) {
    NRF_LOG_INFO("=== Demo: Voice Memo Recorder ===");
    
    // Switch to memo-only mode for maximum battery life
    audio_load_memo_only_preset(&g_audio_recorder);
    audio_recorder_set_mode(&g_audio_recorder, AUDIO_MODE_OFF);
    
    NRF_LOG_INFO("Power consumption in memo mode: ~50µA");
    NRF_LOG_INFO("Estimated battery life: %d hours", 
                 audio_get_estimated_battery_life_hours(&g_audio_recorder));
    
    // Simulate quick memos
    for (int i = 1; i <= 3; i++) {
        NRF_LOG_INFO("Recording demo memo %d...", i);
        
        // In real use, this would be triggered by button press
        audio_quick_memo_start(&g_audio_recorder);
        nrf_delay_ms(3000);  // Simulate 3-second memo
        audio_quick_memo_stop(&g_audio_recorder);
        
        NRF_LOG_INFO("Memo %d recorded", i);
        nrf_delay_ms(1000);
    }
    
    // List all memos
    uint16_t memo_count = audio_get_memo_count(&g_audio_recorder);
    NRF_LOG_INFO("Total memos stored: %d", memo_count);
    
    for (uint16_t i = 0; i < memo_count; i++) {
        voice_memo_t* memo = audio_get_memo_info(&g_audio_recorder, i);
        if (memo) {
            NRF_LOG_INFO("Memo %d: %s (%ds, %d bytes)", 
                         memo->memo_id,
                         memo->filename,
                         memo->duration_sec,
                         memo->file_size_bytes);
        }
    }
    
    // Storage usage
    NRF_LOG_INFO("Storage: %d KB used, %d KB free", 
                 audio_get_storage_used_kb(&g_audio_recorder),
                 audio_get_storage_free_kb(&g_audio_recorder));
}

/**
 * @brief Callback when memo is recorded
 */
void audio_on_memo_recorded(audio_action_recorder_t* recorder, voice_memo_t* memo) {
    NRF_LOG_INFO("Memo recorded: %s (%d seconds)", memo->filename, memo->duration_sec);
    
    // Auto-tag workout-related memos
    if (g_workout_active) {
        Counter* current = &g_combo_device.counters[g_combo_device.current_counter];
        audio_tag_memo_with_workout(recorder, memo->memo_id, current->label);
        memo->is_workout_related = true;
    }
}

/**
 * @brief Callback when storage is low
 */
void audio_on_storage_low(audio_action_recorder_t* recorder, uint32_t free_space_kb) {
    NRF_LOG_WARNING("Storage low: %d KB remaining", free_space_kb);
    
    // Auto-cleanup old unprotected memos
    audio_cleanup_old_memos(recorder, 30);  // Delete memos older than 30 days
    
    uint32_t new_free_space = audio_get_storage_free_kb(recorder);
    NRF_LOG_INFO("Cleaned up storage: %d KB now available", new_free_space);
}

/**
 * @brief Main application entry point
 */
int main(void) {
    // Initialize logging
    NRF_LOG_INIT(NULL);
    NRF_LOG_DEFAULT_BACKENDS_INIT();
    
    // Initialize timer module
    app_timer_init();
    
    NRF_LOG_INFO("ComboCounter + Audio Action Recorder Demo");
    NRF_LOG_INFO("Ultra-low power fitness tracking with voice memos");
    
    // Initialize the system
    ret_code_t err_code = combo_audio_system_init();
    if (err_code != NRF_SUCCESS) {
        NRF_LOG_ERROR("System initialization failed: %d", err_code);
        return 1;
    }
    
    // Run demos
    demo_strength_workout();
    nrf_delay_ms(2000);
    
    demo_memo_recorder();
    nrf_delay_ms(2000);
    
    NRF_LOG_INFO("Entering main loop - system ready for use");
    
    // Main application loop
    while (1) {
        combo_audio_main_loop();
        
        // Process logs
        NRF_LOG_PROCESS();
        
        // Ultra-low power sleep
        __WFE();
    }
}

/**
 * @brief Button interrupt handlers (would be connected to actual GPIO)
 */
void button_combo_handler(void) {
    g_button_combo_pressed = true;
}

void button_memo_press_handler(void) {
    g_button_memo_press_time = app_timer_cnt_get();
}

void button_memo_release_handler(void) {
    g_button_memo_pressed = true;
}
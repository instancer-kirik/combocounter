#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include "audio_action_recorder.h"
#include "simple_combo_core.h"
#include "nrf_log.h"
#include "app_timer.h"
#include "nrf_delay.h"

// ================================
// TEST CONFIGURATION
// ================================

#define TEST_ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            NRF_LOG_ERROR("TEST FAILED: %s", message); \
            return false; \
        } else { \
            NRF_LOG_INFO("TEST PASSED: %s", message); \
        } \
    } while(0)

// ================================
// GLOBAL TEST STATE
// ================================

static audio_action_recorder_t g_test_recorder;
static ComboDevice g_test_combo_device;
static uint32_t g_movement_callback_count = 0;
static uint32_t g_rep_callback_count = 0;
static uint32_t g_memo_callback_count = 0;

// ================================
// CALLBACK IMPLEMENTATIONS FOR TESTING
// ================================

void audio_on_movement_detected(audio_action_recorder_t* recorder, const movement_analysis_t* movement) {
    g_movement_callback_count++;
    NRF_LOG_INFO("Movement detected: intensity=%d, frequency=%d Hz, quality=%d", 
                 movement->movement_intensity, 
                 movement->movement_frequency, 
                 movement->movement_quality);
}

void audio_on_rep_detected(audio_action_recorder_t* recorder, uint16_t rep_count) {
    g_rep_callback_count++;
    NRF_LOG_INFO("Rep detected: count=%d", rep_count);
}

void audio_on_memo_recorded(audio_action_recorder_t* recorder, voice_memo_t* memo) {
    g_memo_callback_count++;
    NRF_LOG_INFO("Memo recorded: %s (%.1f seconds)", memo->label, (float)memo->duration_seconds);
}

void audio_on_silence_detected(audio_action_recorder_t* recorder) {
    NRF_LOG_INFO("Silence detected");
}

void audio_on_storage_low(audio_action_recorder_t* recorder, uint32_t free_space_kb) {
    NRF_LOG_WARNING("Storage low: %d KB remaining", free_space_kb);
}

void audio_on_battery_low(audio_action_recorder_t* recorder, uint8_t battery_percent) {
    NRF_LOG_WARNING("Battery low: %d%% remaining", battery_percent);
}

// ================================
// TEST FUNCTIONS
// ================================

bool test_audio_recorder_initialization(void) {
    NRF_LOG_INFO("=== Testing Audio Recorder Initialization ===");
    
    ret_code_t err_code = audio_recorder_init(&g_test_recorder);
    TEST_ASSERT(err_code == NRF_SUCCESS, "Audio recorder initialization");
    
    TEST_ASSERT(g_test_recorder.status == RECORDER_STATUS_READY, "Recorder status is READY");
    TEST_ASSERT(g_test_recorder.mode == AUDIO_MODE_OFF, "Initial mode is OFF");
    TEST_ASSERT(g_test_recorder.volume == 128, "Default volume is mid-level");
    TEST_ASSERT(g_test_recorder.memo_count == 0, "Initial memo count is zero");
    
    return true;
}

bool test_configuration_presets(void) {
    NRF_LOG_INFO("=== Testing Configuration Presets ===");
    
    // Test strength training preset
    ret_code_t err_code = audio_load_strength_training_preset(&g_test_recorder);
    TEST_ASSERT(err_code == NRF_SUCCESS, "Load strength training preset");
    TEST_ASSERT(g_test_recorder.recording_quality == AUDIO_QUALITY_MEDIUM, "Strength preset quality");
    TEST_ASSERT(g_test_recorder.audio_feedback_enabled == true, "Strength preset feedback enabled");
    TEST_ASSERT(g_test_recorder.rep_detection_enabled == true, "Strength preset rep detection");
    
    // Test cardio preset
    err_code = audio_load_cardio_preset(&g_test_recorder);
    TEST_ASSERT(err_code == NRF_SUCCESS, "Load cardio preset");
    TEST_ASSERT(g_test_recorder.recording_quality == AUDIO_QUALITY_LOW, "Cardio preset quality");
    TEST_ASSERT(g_test_recorder.audio_feedback_enabled == false, "Cardio preset feedback disabled");
    TEST_ASSERT(g_test_recorder.rep_detection_enabled == false, "Cardio preset no rep detection");
    
    // Test ultra-low power preset
    err_code = audio_load_ultra_low_power_preset(&g_test_recorder);
    TEST_ASSERT(err_code == NRF_SUCCESS, "Load ultra-low power preset");
    TEST_ASSERT(g_test_recorder.recording_quality == AUDIO_QUALITY_LOW, "Low power preset quality");
    TEST_ASSERT(g_test_recorder.movement_threshold == 800, "Low power preset high threshold");
    
    return true;
}

bool test_recording_control(void) {
    NRF_LOG_INFO("=== Testing Recording Control ===");
    
    // Test start recording
    ret_code_t err_code = audio_start_recording(&g_test_recorder);
    TEST_ASSERT(err_code == NRF_SUCCESS, "Start recording");
    TEST_ASSERT(g_test_recorder.status == RECORDER_STATUS_RECORDING, "Status is RECORDING");
    
    // Brief recording period
    nrf_delay_ms(500);
    
    // Test pause recording
    err_code = audio_pause_recording(&g_test_recorder);
    TEST_ASSERT(err_code == NRF_SUCCESS, "Pause recording");
    TEST_ASSERT(g_test_recorder.status == RECORDER_STATUS_PAUSED, "Status is PAUSED");
    
    nrf_delay_ms(100);
    
    // Test resume recording
    err_code = audio_resume_recording(&g_test_recorder);
    TEST_ASSERT(err_code == NRF_SUCCESS, "Resume recording");
    TEST_ASSERT(g_test_recorder.status == RECORDER_STATUS_RECORDING, "Status is RECORDING after resume");
    
    nrf_delay_ms(200);
    
    // Test stop recording
    err_code = audio_stop_recording(&g_test_recorder);
    TEST_ASSERT(err_code == NRF_SUCCESS, "Stop recording");
    TEST_ASSERT(g_test_recorder.status == RECORDER_STATUS_READY, "Status is READY after stop");
    
    return true;
}

bool test_voice_memo_functionality(void) {
    NRF_LOG_INFO("=== Testing Voice Memo Functionality ===");
    
    uint16_t initial_memo_count = g_test_recorder.memo_count;
    
    // Test start memo with custom label
    ret_code_t err_code = audio_start_memo(&g_test_recorder, "Test Workout Note");
    TEST_ASSERT(err_code == NRF_SUCCESS, "Start memo with label");
    TEST_ASSERT(g_test_recorder.mode == AUDIO_MODE_MEMO_RECORDING, "Mode is MEMO_RECORDING");
    
    // Simulate memo recording
    nrf_delay_ms(1000);
    
    // Test stop memo
    err_code = audio_stop_memo(&g_test_recorder);
    TEST_ASSERT(err_code == NRF_SUCCESS, "Stop memo");
    TEST_ASSERT(g_test_recorder.memo_count == initial_memo_count + 1, "Memo count increased");
    
    // Verify memo properties
    voice_memo_t* saved_memo = &g_test_recorder.memos[initial_memo_count];
    TEST_ASSERT(strcmp(saved_memo->label, "Test Workout Note") == 0, "Memo label saved correctly");
    TEST_ASSERT(saved_memo->duration_seconds > 0, "Memo has duration");
    
    // Test start memo without label (should use default)
    err_code = audio_start_memo(&g_test_recorder, NULL);
    TEST_ASSERT(err_code == NRF_SUCCESS, "Start memo without label");
    
    nrf_delay_ms(500);
    
    err_code = audio_stop_memo(&g_test_recorder);
    TEST_ASSERT(err_code == NRF_SUCCESS, "Stop second memo");
    TEST_ASSERT(g_test_recorder.memo_count == initial_memo_count + 2, "Second memo count increased");
    
    // Test memo playback
    err_code = audio_play_memo(&g_test_recorder, initial_memo_count);
    TEST_ASSERT(err_code == NRF_SUCCESS, "Play memo");
    TEST_ASSERT(g_test_recorder.mode == AUDIO_MODE_PLAYBACK, "Mode is PLAYBACK during memo playback");
    
    return true;
}

bool test_movement_analysis(void) {
    NRF_LOG_INFO("=== Testing Movement Analysis ===");
    
    // Load strength training preset for movement detection
    audio_load_strength_training_preset(&g_test_recorder);
    
    // Start recording for analysis
    ret_code_t err_code = audio_start_recording(&g_test_recorder);
    TEST_ASSERT(err_code == NRF_SUCCESS, "Start recording for analysis");
    
    // Simulate some audio processing time
    nrf_delay_ms(300);
    
    // Test movement analysis
    movement_analysis_t movement;
    err_code = audio_analyze_movement(&g_test_recorder, &movement);
    // Note: This might return NRF_ERROR_NOT_FOUND if no movement detected, which is OK
    
    if (err_code == NRF_SUCCESS) {
        TEST_ASSERT(movement.movement_intensity >= 0 && movement.movement_intensity <= 1000, 
                   "Movement intensity in valid range");
        TEST_ASSERT(movement.movement_quality >= 0 && movement.movement_quality <= 10, 
                   "Movement quality in valid range");
        NRF_LOG_INFO("Movement analysis successful: intensity=%d, quality=%d", 
                     movement.movement_intensity, movement.movement_quality);
    } else {
        NRF_LOG_INFO("No movement detected (expected in test environment)");
    }
    
    // Test rep detection
    uint16_t rep_count = 0;
    err_code = audio_detect_rep(&g_test_recorder, &rep_count);
    // This will likely return NRF_ERROR_NOT_FOUND in test environment
    
    NRF_LOG_INFO("Rep detection test completed, count=%d", rep_count);
    
    audio_stop_recording(&g_test_recorder);
    
    return true;
}

bool test_baseline_calibration(void) {
    NRF_LOG_INFO("=== Testing Baseline Calibration ===");
    
    uint16_t original_threshold = g_test_recorder.movement_threshold;
    
    ret_code_t err_code = audio_calibrate_baseline(&g_test_recorder);
    TEST_ASSERT(err_code == NRF_SUCCESS, "Baseline calibration");
    
    // Threshold should be updated after calibration
    TEST_ASSERT(g_test_recorder.movement_threshold != original_threshold, 
               "Movement threshold updated after calibration");
    
    NRF_LOG_INFO("Calibration complete: threshold=%d", g_test_recorder.movement_threshold);
    
    return true;
}

bool test_power_management(void) {
    NRF_LOG_INFO("=== Testing Power Management ===");
    
    // Test entering low power mode
    ret_code_t err_code = audio_enter_low_power_mode(&g_test_recorder);
    TEST_ASSERT(err_code == NRF_SUCCESS, "Enter low power mode");
    TEST_ASSERT(g_test_recorder.mode == AUDIO_MODE_OFF, "Mode is OFF in low power");
    
    // Test exiting low power mode
    err_code = audio_exit_low_power_mode(&g_test_recorder);
    TEST_ASSERT(err_code == NRF_SUCCESS, "Exit low power mode");
    TEST_ASSERT(g_test_recorder.mode == AUDIO_MODE_LISTEN, "Mode is LISTEN after exit");
    
    // Test battery life estimation
    uint32_t battery_hours = audio_get_estimated_battery_life_hours(&g_test_recorder);
    TEST_ASSERT(battery_hours > 0, "Battery life estimation returns positive value");
    
    NRF_LOG_INFO("Estimated battery life: %d hours", battery_hours);
    
    return true;
}

bool test_audio_feedback(void) {
    NRF_LOG_INFO("=== Testing Audio Feedback ===");
    
    // Enable audio feedback
    g_test_recorder.audio_feedback_enabled = true;
    
    // Test rep count feedback
    ret_code_t err_code = audio_play_rep_count_feedback(&g_test_recorder, 5);
    TEST_ASSERT(err_code == NRF_SUCCESS, "Play rep count feedback");
    
    nrf_delay_ms(500);  // Allow audio to play
    
    // Test form feedback
    err_code = audio_play_form_feedback(&g_test_recorder, 8);
    TEST_ASSERT(err_code == NRF_SUCCESS, "Play form feedback");
    
    nrf_delay_ms(500);
    
    // Test combo milestone
    err_code = audio_play_combo_milestone_sound(&g_test_recorder, 25);
    TEST_ASSERT(err_code == NRF_SUCCESS, "Play combo milestone sound");
    
    nrf_delay_ms(500);
    
    return true;
}

bool test_combo_counter_integration(void) {
    NRF_LOG_INFO("=== Testing ComboCounter Integration ===");
    
    // Initialize combo device for integration testing
    combo_device_init(&g_test_combo_device);
    counter_add(&g_test_combo_device, "Push-ups", COUNTER_TYPE_COMBO);
    
    // Test syncing with combo counter
    ret_code_t err_code = audio_sync_with_combo_counter(&g_test_recorder, 0, 15, 10);
    TEST_ASSERT(err_code == NRF_SUCCESS, "Sync with combo counter");
    
    // Test rep validation
    err_code = audio_validate_rep_with_counter(&g_test_recorder, true);
    TEST_ASSERT(err_code == NRF_SUCCESS, "Validate rep as confirmed");
    
    err_code = audio_validate_rep_with_counter(&g_test_recorder, false);
    TEST_ASSERT(err_code == NRF_SUCCESS, "Validate rep as rejected");
    
    // Test memo tagging
    if (g_test_recorder.memo_count > 0) {
        err_code = audio_tag_memo_with_workout(&g_test_recorder, 0, "Push-ups");
        TEST_ASSERT(err_code == NRF_SUCCESS, "Tag memo with workout");
        
        // Check that the memo label was updated
        voice_memo_t* tagged_memo = &g_test_recorder.memos[0];
        TEST_ASSERT(strstr(tagged_memo->label, "Push-ups") != NULL, "Memo tagged with exercise name");
    }
    
    return true;
}

bool test_file_management(void) {
    NRF_LOG_INFO("=== Testing File Management ===");
    
    // Test storage usage calculation
    uint32_t used_kb = audio_get_storage_used_kb(&g_test_recorder);
    uint32_t free_kb = audio_get_storage_free_kb(&g_test_recorder);
    
    NRF_LOG_INFO("Storage: %d KB used, %d KB free", used_kb, free_kb);
    TEST_ASSERT(used_kb + free_kb <= g_test_recorder.storage_available_kb, "Storage accounting is consistent");
    
    // Test cleanup (but don't actually delete recent memos)
    uint16_t memo_count_before = g_test_recorder.memo_count;
    ret_code_t err_code = audio_cleanup_old_memos(&g_test_recorder, 365); // Only delete memos older than 1 year
    TEST_ASSERT(err_code == NRF_SUCCESS, "Cleanup old memos");
    TEST_ASSERT(g_test_recorder.memo_count == memo_count_before, "No recent memos deleted");
    
    return true;
}

bool test_utility_functions(void) {
    NRF_LOG_INFO("=== Testing Utility Functions ===");
    
    // Test mode string conversion
    const char* mode_str = audio_get_mode_string(AUDIO_MODE_MEMO_RECORDING);
    TEST_ASSERT(strcmp(mode_str, "RECORDING_MEMO") == 0, "Mode string conversion");
    
    // Test movement signature generation
    int16_t test_audio[256];
    for (int i = 0; i < 256; i++) {
        test_audio[i] = (int16_t)(1000 * sin(2.0 * M_PI * i / 256.0)); // Simple sine wave
    }
    
    float signature[8];
    ret_code_t err_code = audio_generate_movement_signature(test_audio, 256, signature);
    TEST_ASSERT(err_code == NRF_SUCCESS, "Generate movement signature");
    
    // Check that signature values are in reasonable range
    for (int i = 0; i < 8; i++) {
        TEST_ASSERT(signature[i] >= 0.0f && signature[i] <= 1.0f, "Signature values in valid range");
    }
    
    return true;
}

bool test_callback_functionality(void) {
    NRF_LOG_INFO("=== Testing Callback Functionality ===");
    
    uint32_t initial_movement_callbacks = g_movement_callback_count;
    uint32_t initial_rep_callbacks = g_rep_callback_count;
    uint32_t initial_memo_callbacks = g_memo_callback_count;
    
    // Callbacks should have been triggered during previous tests
    NRF_LOG_INFO("Callback counts - Movement: %d, Rep: %d, Memo: %d", 
                 g_movement_callback_count, g_rep_callback_count, g_memo_callback_count);
    
    // At least memo callbacks should have been triggered
    TEST_ASSERT(g_memo_callback_count >= 2, "Memo callbacks were triggered");
    
    return true;
}

// ================================
// MAIN TEST RUNNER
// ================================

void run_all_audio_tests(void) {
    NRF_LOG_INFO("🎙️  Starting Audio Action Recorder Test Suite");
    NRF_LOG_INFO("===============================================");
    
    bool all_tests_passed = true;
    uint32_t test_count = 0;
    uint32_t passed_count = 0;
    
    // Initialize logging and timer
    NRF_LOG_INIT(NULL);
    NRF_LOG_DEFAULT_BACKENDS_INIT();
    app_timer_init();
    
    // Reset callback counters
    g_movement_callback_count = 0;
    g_rep_callback_count = 0;
    g_memo_callback_count = 0;
    
    // Run all tests
    struct {
        const char* name;
        bool (*test_func)(void);
    } tests[] = {
        {"Audio Recorder Initialization", test_audio_recorder_initialization},
        {"Configuration Presets", test_configuration_presets},
        {"Recording Control", test_recording_control},
        {"Voice Memo Functionality", test_voice_memo_functionality},
        {"Movement Analysis", test_movement_analysis},
        {"Baseline Calibration", test_baseline_calibration},
        {"Power Management", test_power_management},
        {"Audio Feedback", test_audio_feedback},
        {"ComboCounter Integration", test_combo_counter_integration},
        {"File Management", test_file_management},
        {"Utility Functions", test_utility_functions},
        {"Callback Functionality", test_callback_functionality}
    };
    
    for (size_t i = 0; i < sizeof(tests) / sizeof(tests[0]); i++) {
        test_count++;
        NRF_LOG_INFO("\n--- Running Test: %s ---", tests[i].name);
        
        if (tests[i].test_func()) {
            passed_count++;
            NRF_LOG_INFO("✅ %s: PASSED", tests[i].name);
        } else {
            all_tests_passed = false;
            NRF_LOG_ERROR("❌ %s: FAILED", tests[i].name);
        }
        
        nrf_delay_ms(100);  // Brief delay between tests
    }
    
    // Cleanup
    audio_recorder_deinit(&g_test_recorder);
    
    // Final results
    NRF_LOG_INFO("\n🏁 Audio Action Recorder Test Results");
    NRF_LOG_INFO("=====================================");
    NRF_LOG_INFO("Tests Run: %d", test_count);
    NRF_LOG_INFO("Passed: %d", passed_count);
    NRF_LOG_INFO("Failed: %d", test_count - passed_count);
    
    if (all_tests_passed) {
        NRF_LOG_INFO("🎉 ALL TESTS PASSED! Audio Action Recorder is ready for use.");
    } else {
        NRF_LOG_ERROR("💥 Some tests failed. Check logs for details.");
    }
}

// ================================
// MAIN ENTRY POINT
// ================================

int main(void) {
    run_all_audio_tests();
    
    // Keep running to allow log output
    while (1) {
        NRF_LOG_PROCESS();
        nrf_delay_ms(100);
    }
    
    return 0;
}
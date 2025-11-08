#include "audio_action_recorder.h"
#include "musicmaker_integration.h"
#include "nrf_log.h"
#include "nrf_delay.h"
#include "app_error.h"
#include "app_timer.h"
#include "nrf_drv_pdm.h"
#include "nrf_drv_spi.h"
#include "nrf_gpio.h"
#include "ff.h"
#include <string.h>
#include <math.h>

// ================================
// GLOBAL STATE
// ================================

static bool g_audio_system_initialized = false;
static int16_t g_audio_buffer[AUDIO_BUFFER_SIZE];
static int16_t g_analysis_buffer[AUDIO_ANALYSIS_WINDOW];
static uint32_t g_buffer_position = 0;
static bool g_recording_active = false;

// Timer instances
APP_TIMER_DEF(m_analysis_timer);
APP_TIMER_DEF(m_memo_timeout_timer);

// File system
static FATFS g_fs;
static bool g_sd_card_mounted = false;

// Movement detection state
static float g_baseline_noise_level = 0.0f;
static uint32_t g_last_movement_time = 0;
static uint16_t g_rep_count_session = 0;
static float g_last_movement_signature[8] = {0};

// ================================
// PRIVATE FUNCTION DECLARATIONS
// ================================

static ret_code_t pdm_init(void);
static ret_code_t sd_card_init(void);
static void pdm_event_handler(nrf_drv_pdm_evt_t const * p_evt);
static void analysis_timer_handler(void * p_context);
static void memo_timeout_handler(void * p_context);
static float calculate_rms_energy(const int16_t* data, uint16_t length);
static float calculate_spectral_centroid(const int16_t* data, uint16_t length);
static bool detect_movement_pattern(const int16_t* data, uint16_t length, movement_analysis_t* result);
static ret_code_t save_memo_to_file(const voice_memo_t* memo);
static ret_code_t load_memo_from_file(uint16_t memo_id, voice_memo_t* memo);
static void generate_unique_filename(char* buffer, size_t buffer_size, const char* prefix);

// ================================
// CORE INITIALIZATION
// ================================

ret_code_t audio_recorder_init(audio_action_recorder_t* recorder) {
    ret_code_t err_code;
    
    if (recorder == NULL) {
        return NRF_ERROR_NULL;
    }
    
    NRF_LOG_INFO("Audio Action Recorder: Initializing...");
    
    // Initialize structure
    memset(recorder, 0, sizeof(audio_action_recorder_t));
    recorder->mode = AUDIO_MODE_OFF;
    recorder->volume = 128;  // Mid-level
    recorder->recording_quality = AUDIO_QUALITY_MEDIUM;
    recorder->movement_threshold = AUDIO_THRESHOLD_MOVEMENT;
    recorder->silence_threshold = AUDIO_THRESHOLD_SILENCE;
    
    // Initialize MusicMaker for playback
    if (!musicmaker_init()) {
        NRF_LOG_ERROR("Failed to initialize MusicMaker");
        return NRF_ERROR_INTERNAL;
    }
    
    // Initialize PDM microphone
    err_code = pdm_init();
    if (err_code != NRF_SUCCESS) {
        NRF_LOG_ERROR("Failed to initialize PDM: %d", err_code);
        return err_code;
    }
    
    // Initialize SD card for storage
    err_code = sd_card_init();
    if (err_code != NRF_SUCCESS) {
        NRF_LOG_WARNING("SD card not available, using internal storage only");
        recorder->storage_available_kb = 64;  // Limited internal storage
    } else {
        recorder->storage_available_kb = 8192;  // 8MB typical SD card space
    }
    
    // Create analysis timer
    err_code = app_timer_create(&m_analysis_timer, 
                               APP_TIMER_MODE_REPEATED, 
                               analysis_timer_handler);
    APP_ERROR_CHECK(err_code);
    
    // Create memo timeout timer
    err_code = app_timer_create(&m_memo_timeout_timer, 
                               APP_TIMER_MODE_SINGLE_SHOT, 
                               memo_timeout_handler);
    APP_ERROR_CHECK(err_code);
    
    recorder->status = RECORDER_STATUS_READY;
    g_audio_system_initialized = true;
    
    NRF_LOG_INFO("Audio Action Recorder initialized successfully");
    return NRF_SUCCESS;
}

ret_code_t audio_recorder_deinit(audio_action_recorder_t* recorder) {
    if (recorder == NULL) {
        return NRF_ERROR_NULL;
    }
    
    // Stop any active recording
    audio_stop_recording(recorder);
    
    // Stop timers
    app_timer_stop(m_analysis_timer);
    app_timer_stop(m_memo_timeout_timer);
    
    // Deinitialize PDM
    nrf_drv_pdm_uninit();
    
    // Deinitialize MusicMaker
    musicmaker_deinit();
    
    // Unmount SD card
    if (g_sd_card_mounted) {
        f_mount(NULL, "", 0);
        g_sd_card_mounted = false;
    }
    
    recorder->status = RECORDER_STATUS_UNINITIALIZED;
    g_audio_system_initialized = false;
    
    return NRF_SUCCESS;
}

// ================================
// RECORDING CONTROL
// ================================

ret_code_t audio_start_recording(audio_action_recorder_t* recorder) {
    ret_code_t err_code;
    
    if (recorder == NULL) {
        return NRF_ERROR_NULL;
    }
    
    if (!g_audio_system_initialized) {
        return NRF_ERROR_INVALID_STATE;
    }
    
    if (g_recording_active) {
        return NRF_SUCCESS;  // Already recording
    }
    
    NRF_LOG_INFO("Starting audio recording");
    
    // Clear buffers
    memset(g_audio_buffer, 0, sizeof(g_audio_buffer));
    memset(g_analysis_buffer, 0, sizeof(g_analysis_buffer));
    g_buffer_position = 0;
    
    // Configure PDM for recording
    nrf_drv_pdm_config_t pdm_config = {
        .mode               = NRF_PDM_MODE_MONO,
        .edge               = NRF_PDM_EDGE_LEFTFALLING,
        .pin_clk            = PDM_CLK_PIN,
        .pin_din            = PDM_DATA_PIN,
        .clock_freq         = NRF_PDM_FREQ_1280K,
        .ratio              = NRF_PDM_RATIO_80X,
        .interrupt_priority = 6,
        .gain_l             = NRF_PDM_GAIN_MAXIMUM,
        .gain_r             = NRF_PDM_GAIN_MAXIMUM
    };
    
    err_code = nrf_drv_pdm_init(&pdm_config, pdm_event_handler);
    if (err_code != NRF_SUCCESS) {
        NRF_LOG_ERROR("PDM init failed: %d", err_code);
        return err_code;
    }
    
    // Start PDM sampling
    err_code = nrf_drv_pdm_start();
    if (err_code != NRF_SUCCESS) {
        NRF_LOG_ERROR("PDM start failed: %d", err_code);
        return err_code;
    }
    
    // Start analysis timer (10Hz analysis rate)
    err_code = app_timer_start(m_analysis_timer, APP_TIMER_TICKS(100), recorder);
    if (err_code != NRF_SUCCESS) {
        NRF_LOG_ERROR("Analysis timer start failed: %d", err_code);
        return err_code;
    }
    
    g_recording_active = true;
    recorder->status = RECORDER_STATUS_RECORDING;
    recorder->recording_start_time = app_timer_cnt_get();
    
    return NRF_SUCCESS;
}

ret_code_t audio_stop_recording(audio_action_recorder_t* recorder) {
    if (recorder == NULL) {
        return NRF_ERROR_NULL;
    }
    
    if (!g_recording_active) {
        return NRF_SUCCESS;  // Already stopped
    }
    
    NRF_LOG_INFO("Stopping audio recording");
    
    // Stop timers
    app_timer_stop(m_analysis_timer);
    app_timer_stop(m_memo_timeout_timer);
    
    // Stop PDM
    nrf_drv_pdm_stop();
    nrf_drv_pdm_uninit();
    
    g_recording_active = false;
    recorder->status = RECORDER_STATUS_READY;
    
    uint32_t recording_duration = app_timer_cnt_diff_compute(app_timer_cnt_get(), 
                                                           recorder->recording_start_time);
    recorder->total_recording_time_ms += recording_duration;
    
    return NRF_SUCCESS;
}

ret_code_t audio_pause_recording(audio_action_recorder_t* recorder) {
    if (recorder == NULL) {
        return NRF_ERROR_NULL;
    }
    
    if (recorder->status != RECORDER_STATUS_RECORDING) {
        return NRF_ERROR_INVALID_STATE;
    }
    
    nrf_drv_pdm_stop();
    app_timer_stop(m_analysis_timer);
    
    recorder->status = RECORDER_STATUS_PAUSED;
    
    return NRF_SUCCESS;
}

ret_code_t audio_resume_recording(audio_action_recorder_t* recorder) {
    if (recorder == NULL) {
        return NRF_ERROR_NULL;
    }
    
    if (recorder->status != RECORDER_STATUS_PAUSED) {
        return NRF_ERROR_INVALID_STATE;
    }
    
    nrf_drv_pdm_start();
    app_timer_start(m_analysis_timer, APP_TIMER_TICKS(100), recorder);
    
    recorder->status = RECORDER_STATUS_RECORDING;
    
    return NRF_SUCCESS;
}

// ================================
// VOICE MEMO FUNCTIONS
// ================================

ret_code_t audio_start_memo(audio_action_recorder_t* recorder, const char* label) {
    ret_code_t err_code;
    
    if (recorder == NULL) {
        return NRF_ERROR_NULL;
    }
    
    if (recorder->memo_count >= MAX_MEMOS_STORED) {
        return NRF_ERROR_NO_MEM;
    }
    
    // Find next available memo slot
    uint16_t memo_index = recorder->memo_count;
    voice_memo_t* memo = &recorder->memos[memo_index];
    
    // Initialize memo
    memo->id = memo_index;
    memo->timestamp = app_timer_cnt_get();
    memo->duration_seconds = 0;
    memo->file_size_bytes = 0;
    memo->is_protected = false;
    
    if (label != NULL) {
        strncpy(memo->label, label, sizeof(memo->label) - 1);
        memo->label[sizeof(memo->label) - 1] = '\0';
    } else {
        snprintf(memo->label, sizeof(memo->label), "Memo_%d", memo->id);
    }
    
    // Generate unique filename
    generate_unique_filename(memo->filename, sizeof(memo->filename), "memo");
    
    // Start recording for this memo
    recorder->current_memo_id = memo->id;
    recorder->mode = AUDIO_MODE_MEMO_RECORDING;
    
    err_code = audio_start_recording(recorder);
    if (err_code != NRF_SUCCESS) {
        return err_code;
    }
    
    // Start memo timeout timer
    err_code = app_timer_start(m_memo_timeout_timer, 
                              APP_TIMER_TICKS(MAX_MEMO_DURATION_SEC * 1000), 
                              recorder);
    if (err_code != NRF_SUCCESS) {
        NRF_LOG_WARNING("Failed to start memo timeout timer");
    }
    
    NRF_LOG_INFO("Started memo recording: %s", memo->label);
    
    return NRF_SUCCESS;
}

ret_code_t audio_stop_memo(audio_action_recorder_t* recorder) {
    if (recorder == NULL) {
        return NRF_ERROR_NULL;
    }
    
    if (recorder->mode != AUDIO_MODE_MEMO_RECORDING) {
        return NRF_ERROR_INVALID_STATE;
    }
    
    voice_memo_t* memo = &recorder->memos[recorder->current_memo_id];
    
    // Stop recording
    audio_stop_recording(recorder);
    
    // Calculate memo duration
    uint32_t end_time = app_timer_cnt_get();
    uint32_t duration_ticks = app_timer_cnt_diff_compute(end_time, memo->timestamp);
    memo->duration_seconds = duration_ticks / APP_TIMER_TICKS(1000);
    
    // Save memo to file
    ret_code_t err_code = save_memo_to_file(memo);
    if (err_code != NRF_SUCCESS) {
        NRF_LOG_WARNING("Failed to save memo to file: %d", err_code);
    }
    
    recorder->memo_count++;
    recorder->mode = AUDIO_MODE_LISTEN;
    
    // Call user callback
    audio_on_memo_recorded(recorder, memo);
    
    NRF_LOG_INFO("Memo saved: %s (%.1f seconds)", memo->label, 
                 (float)memo->duration_seconds);
    
    return NRF_SUCCESS;
}

ret_code_t audio_play_memo(audio_action_recorder_t* recorder, uint16_t memo_id) {
    if (recorder == NULL) {
        return NRF_ERROR_NULL;
    }
    
    if (memo_id >= recorder->memo_count) {
        return NRF_ERROR_NOT_FOUND;
    }
    
    voice_memo_t* memo = &recorder->memos[memo_id];
    
    // Play memo file using MusicMaker
    bool success = musicmaker_play_file(memo->filename, false);
    if (!success) {
        return NRF_ERROR_INTERNAL;
    }
    
    recorder->mode = AUDIO_MODE_PLAYBACK;
    
    return NRF_SUCCESS;
}

// ================================
// MOVEMENT ANALYSIS
// ================================

ret_code_t audio_analyze_movement(audio_action_recorder_t* recorder, movement_analysis_t* result) {
    if (recorder == NULL || result == NULL) {
        return NRF_ERROR_NULL;
    }
    
    if (!g_recording_active) {
        return NRF_ERROR_INVALID_STATE;
    }
    
    // Analyze the current audio buffer for movement patterns
    bool movement_detected = detect_movement_pattern(g_analysis_buffer, 
                                                   AUDIO_ANALYSIS_WINDOW, 
                                                   result);
    
    if (movement_detected) {
        result->timestamp = app_timer_cnt_get();
        
        // Generate audio signature for this movement
        audio_generate_movement_signature(g_analysis_buffer, 
                                        AUDIO_ANALYSIS_WINDOW, 
                                        result->audio_signature);
        
        // Update movement statistics
        recorder->total_movements_detected++;
        g_last_movement_time = result->timestamp;
        memcpy(g_last_movement_signature, result->audio_signature, sizeof(result->audio_signature));
        
        // Call user callback
        audio_on_movement_detected(recorder, result);
        
        NRF_LOG_DEBUG("Movement detected: intensity=%d, frequency=%dHz", 
                     result->movement_intensity, result->movement_frequency);
    }
    
    return movement_detected ? NRF_SUCCESS : NRF_ERROR_NOT_FOUND;
}

ret_code_t audio_detect_rep(audio_action_recorder_t* recorder, uint16_t* rep_count) {
    if (recorder == NULL || rep_count == NULL) {
        return NRF_ERROR_NULL;
    }
    
    movement_analysis_t movement;
    ret_code_t err_code = audio_analyze_movement(recorder, &movement);
    
    if (err_code != NRF_SUCCESS) {
        return err_code;
    }
    
    // Simple rep detection based on movement patterns
    // This is a simplified algorithm - real implementation would be more sophisticated
    bool is_rep = false;
    
    if (movement.is_rep_detected) {
        // Check if this movement is significantly different from the last one
        // indicating a complete rep cycle
        float similarity = audio_calculate_movement_similarity(&movement, 
                                                              (movement_analysis_t*)g_last_movement_signature);
        
        if (similarity > 0.7f && movement.movement_quality > 6) {
            g_rep_count_session++;
            is_rep = true;
            
            // Update recorder statistics
            recorder->total_reps_detected = g_rep_count_session;
            
            // Play audio feedback
            audio_play_rep_count_feedback(recorder, g_rep_count_session);
            
            // Call user callback
            audio_on_rep_detected(recorder, g_rep_count_session);
            
            NRF_LOG_INFO("Rep detected: count=%d, quality=%d", 
                        g_rep_count_session, movement.movement_quality);
        }
    }
    
    *rep_count = g_rep_count_session;
    return is_rep ? NRF_SUCCESS : NRF_ERROR_NOT_FOUND;
}

ret_code_t audio_calibrate_baseline(audio_action_recorder_t* recorder) {
    if (recorder == NULL) {
        return NRF_ERROR_NULL;
    }
    
    NRF_LOG_INFO("Calibrating baseline noise level...");
    
    // Start recording for calibration
    ret_code_t err_code = audio_start_recording(recorder);
    if (err_code != NRF_SUCCESS) {
        return err_code;
    }
    
    // Collect samples for 2 seconds
    nrf_delay_ms(2000);
    
    // Calculate baseline noise level
    g_baseline_noise_level = calculate_rms_energy(g_audio_buffer, AUDIO_BUFFER_SIZE);
    
    // Set movement threshold relative to baseline
    recorder->movement_threshold = (uint16_t)(g_baseline_noise_level * 2.5f);
    recorder->silence_threshold = (uint16_t)(g_baseline_noise_level * 1.1f);
    
    audio_stop_recording(recorder);
    
    NRF_LOG_INFO("Baseline calibration complete: noise=%.1f, threshold=%d", 
                g_baseline_noise_level, recorder->movement_threshold);
    
    return NRF_SUCCESS;
}

// ================================
// AUDIO FEEDBACK
// ================================

ret_code_t audio_play_rep_count_feedback(audio_action_recorder_t* recorder, uint16_t rep_count) {
    if (recorder == NULL) {
        return NRF_ERROR_NULL;
    }
    
    // Play count feedback if enabled
    if (!recorder->audio_feedback_enabled) {
        return NRF_SUCCESS;
    }
    
    AudioClip clip = AUDIO_CLIP_NONE;
    
    // Select appropriate clip based on count
    if (rep_count <= 10) {
        clip = AUDIO_CLIP_COUNT_1 + (rep_count - 1);
    } else if (rep_count % 5 == 0) {
        // Play count for every 5th rep after 10
        clip = AUDIO_CLIP_BEEP_SUCCESS;
    }
    
    if (clip != AUDIO_CLIP_NONE) {
        musicmaker_play_clip(clip, false);
    }
    
    return NRF_SUCCESS;
}

ret_code_t audio_play_form_feedback(audio_action_recorder_t* recorder, uint8_t quality_score) {
    if (recorder == NULL) {
        return NRF_ERROR_NULL;
    }
    
    if (!recorder->audio_feedback_enabled) {
        return NRF_SUCCESS;
    }
    
    AudioClip clip = AUDIO_CLIP_NONE;
    
    if (quality_score >= 9) {
        clip = AUDIO_CLIP_PERFECT;
    } else if (quality_score >= 7) {
        clip = AUDIO_CLIP_GOOD;
    } else if (quality_score >= 5) {
        clip = AUDIO_CLIP_PARTIAL;
    } else {
        clip = AUDIO_CLIP_MISS;
    }
    
    musicmaker_play_clip(clip, false);
    
    return NRF_SUCCESS;
}

ret_code_t audio_play_combo_milestone_sound(audio_action_recorder_t* recorder, uint32_t combo) {
    if (recorder == NULL) {
        return NRF_ERROR_NULL;
    }
    
    if (!recorder->audio_feedback_enabled) {
        return NRF_SUCCESS;
    }
    
    AudioClip clip = AUDIO_CLIP_NONE;
    
    if (combo >= 100) {
        clip = AUDIO_CLIP_COMBO_100;
    } else if (combo >= 50) {
        clip = AUDIO_CLIP_COMBO_50;
    } else if (combo >= 25) {
        clip = AUDIO_CLIP_COMBO_25;
    } else if (combo >= 10) {
        clip = AUDIO_CLIP_COMBO_10;
    }
    
    if (clip != AUDIO_CLIP_NONE) {
        musicmaker_play_clip(clip, false);
    }
    
    return NRF_SUCCESS;
}

// ================================
// CONFIGURATION PRESETS
// ================================

ret_code_t audio_load_strength_training_preset(audio_action_recorder_t* recorder) {
    if (recorder == NULL) {
        return NRF_ERROR_NULL;
    }
    
    recorder->recording_quality = AUDIO_QUALITY_MEDIUM;
    recorder->movement_threshold = 500;
    recorder->silence_threshold = 100;
    recorder->audio_feedback_enabled = true;
    recorder->auto_memo_enabled = false;
    recorder->rep_detection_enabled = true;
    
    NRF_LOG_INFO("Loaded strength training preset");
    
    return NRF_SUCCESS;
}

ret_code_t audio_load_cardio_preset(audio_action_recorder_t* recorder) {
    if (recorder == NULL) {
        return NRF_ERROR_NULL;
    }
    
    recorder->recording_quality = AUDIO_QUALITY_LOW;
    recorder->movement_threshold = 300;
    recorder->silence_threshold = 50;
    recorder->audio_feedback_enabled = false;  // Less feedback for cardio
    recorder->auto_memo_enabled = true;
    recorder->rep_detection_enabled = false;   // Reps don't apply to cardio
    
    NRF_LOG_INFO("Loaded cardio preset");
    
    return NRF_SUCCESS;
}

ret_code_t audio_load_ultra_low_power_preset(audio_action_recorder_t* recorder) {
    if (recorder == NULL) {
        return NRF_ERROR_NULL;
    }
    
    recorder->recording_quality = AUDIO_QUALITY_LOW;
    recorder->movement_threshold = 800;  // Higher threshold to reduce false triggers
    recorder->silence_threshold = 200;
    recorder->audio_feedback_enabled = false;
    recorder->auto_memo_enabled = false;
    recorder->rep_detection_enabled = false;
    
    NRF_LOG_INFO("Loaded ultra-low power preset");
    
    return NRF_SUCCESS;
}

// ================================
// UTILITY FUNCTIONS
// ================================

const char* audio_get_mode_string(audio_mode_t mode) {
    switch (mode) {
        case AUDIO_MODE_OFF: return "OFF";
        case AUDIO_MODE_LISTEN: return "LISTENING";
        case AUDIO_MODE_MEMO_RECORDING: return "RECORDING_MEMO";
        case AUDIO_MODE_WORKOUT_ANALYSIS: return "ANALYZING";
        case AUDIO_MODE_PLAYBACK: return "PLAYBACK";
        case AUDIO_MODE_PROCESSING: return "PROCESSING";
        default: return "UNKNOWN";
    }
}

float audio_calculate_movement_similarity(const movement_analysis_t* move1, const movement_analysis_t* move2) {
    if (move1 == NULL || move2 == NULL) {
        return 0.0f;
    }
    
    float similarity = 0.0f;
    
    // Compare audio signatures
    for (int i = 0; i < 8; i++) {
        float diff = fabsf(move1->audio_signature[i] - move2->audio_signature[i]);
        similarity += (1.0f - diff);  // Simplified similarity calculation
    }
    
    similarity /= 8.0f;
    
    // Factor in intensity and frequency similarity
    float intensity_sim = 1.0f - fabsf((float)move1->movement_intensity - (float)move2->movement_intensity) / 1000.0f;
    float freq_sim = 1.0f - fabsf((float)move1->movement_frequency - (float)move2->movement_frequency) / 1000.0f;
    
    similarity = (similarity + intensity_sim + freq_sim) / 3.0f;
    
    return fmaxf(0.0f, fminf(1.0f, similarity));
}

ret_code_t audio_generate_movement_signature(const int16_t* audio_data, uint16_t length, float* signature) {
    if (audio_data == NULL || signature == NULL || length == 0) {
        return NRF_ERROR_NULL;
    }
    
    // Initialize signature
    memset(signature, 0, 8 * sizeof(float));
    
    // Simple frequency domain analysis
    // This is a simplified implementation - real world would use FFT
    
    uint16_t bin_size = length / 8;
    
    for (int bin = 0; bin < 8; bin++) {
        float energy = 0.0f;
        uint16_t start = bin * bin_size;
        uint16_t end = fminf(start + bin_size, length);
        
        for (uint16_t i = start; i < end; i++) {
            energy += (float)(audio_data[i] * audio_data[i]);
        }
        
        signature[bin] = sqrtf(energy / bin_size) / 32768.0f;  // Normalize
    }
    
    return NRF_SUCCESS;
}

// ================================
// PRIVATE FUNCTION IMPLEMENTATIONS
// ================================

static ret_code_t pdm_init(void) {
    // Enable PDM power pin if available
    nrf_gpio_cfg_output(PDM_POWER_PIN);
    nrf_gpio_pin_set(PDM_POWER_PIN);
    
    nrf_delay_ms(10);  // Allow power to stabilize
    
    return NRF_SUCCESS;
}

static ret_code_t sd_card_init(void) {
    FRESULT ff_result = f_mount(&g_fs, "", 1);
    if (ff_result != FR_OK) {
        NRF_LOG_WARNING("SD card mount failed: %d", ff_result);
        return NRF_ERROR_INTERNAL;
    }
    
    g_sd_card_mounted = true;
    
    // Create audio directories if they don't exist
    f_mkdir("audio");
    f_mkdir("audio/memos");
    f_mkdir("audio/workouts");
    
    return NRF_SUCCESS;
}

static void pdm_event_handler(nrf_drv_pdm_evt_t const * p_evt) {
    if (p_evt->buffer_requested) {
        nrf_drv_pdm_buffer_set(g_audio_buffer, AUDIO_BUFFER_SIZE);
    } else if (p_evt->buffer_released != NULL) {
        // Copy data to analysis buffer for processing
        if (g_buffer_position + p_evt->buffer_released_size <= AUDIO_ANALYSIS_WINDOW) {
            memcpy(&g_analysis_buffer[g_buffer_position], 
                   p_evt->buffer_released, 
                   p_evt->buffer_released_size * sizeof(int16_t));
            g_buffer_position += p_evt->buffer_released_size;
        } else {
            // Buffer full, shift data and add new samples
            memmove(g_analysis_buffer, 
                   &g_analysis_buffer[p_evt->buffer_released_size],
                   (AUDIO_ANALYSIS_WINDOW - p_evt->buffer_released_size) * sizeof(int16_t));
            memcpy(&g_analysis_buffer[AUDIO_ANALYSIS_WINDOW - p_evt->buffer_released_size],
                   p_evt->buffer_released,
                   p_evt->buffer_released_size * sizeof(int16_t));
        }
    }
}

static void analysis_timer_handler(void * p_context) {
    audio_action_recorder_t* recorder = (audio_action_recorder_t*)p_context;
    
    if (recorder == NULL || !g_recording_active) {
        return;
    }
    
    // Perform movement analysis
    movement_analysis_t movement;
    if (audio_analyze_movement(recorder, &movement) == NRF_SUCCESS) {
        // Check for rep detection if enabled
        if (recorder->rep_detection_enabled) {
            uint16_t rep_count;
            audio_detect_rep(recorder, &rep_count);
        }
    }
}

static void memo_timeout_handler(void * p_context) {
    audio_action_recorder_t* recorder = (audio_action_recorder_t*)p_context;
    
    if (recorder != NULL && recorder->mode == AUDIO_MODE_MEMO_RECORDING) {
        NRF_LOG_INFO("Memo timeout - stopping recording");
        audio_stop_memo(recorder);
    }
}

static float calculate_rms_energy(const int16_t* data, uint16_t length) {
    if (data == NULL || length == 0) {
        return 0.0f;
    }
    
    float sum = 0.0f;
    for (uint16_t i = 0; i < length; i++) {
        sum += (float)(data[i] * data[i]);
    }
    
    return sqrtf(sum / length);
}

static bool detect_movement_pattern(const int16_t* data, uint16_t length, movement_analysis_t* result) {
    if (data == NULL || result == NULL || length == 0) {
        return false;
    }
    
    // Calculate energy level
    float energy = calculate_rms_energy(data, length);
    
    // Check if energy exceeds movement threshold
    if (energy < g_baseline_noise_level * 1.5f) {
        return false;  // Below movement threshold
    }
    
    // Fill in movement analysis results
    result->movement_intensity = (uint16_t)fminf(energy, 1000.0f);
    result->movement_frequency = calculate_spectral_centroid(data, length);
    result->movement_duration_ms = 100;  // Analysis window duration
    result->movement_quality = (uint8_t)(energy / (g_baseline_noise_level * 10.0f));
    result->movement_quality = fminf(10, fmaxf(0, result->movement_quality));
    
    // Simple rep detection based on pattern
    result->is_rep_detected = (result->movement_intensity > 600 && 
                              result->movement_quality > 6 &&
                              result->movement_frequency > 20 && 
                              result->movement_frequency < 200);
    
    // Calculate tempo regularity (simplified)
    result->tempo_regularity = 8;  // Placeholder - would need history for real calculation
    
    return true;
}

static float calculate_spectral_centroid(const int16_t* data, uint16_t length) {
    if (data == NULL || length == 0) {
        return 0.0f;
    }
    
    // Simplified spectral centroid calculation
    // Real implementation would use FFT
    float weighted_sum = 0.0f;
    float magnitude_sum = 0.0f;
    
    for (uint16_t i = 0; i < length; i++) {
        float magnitude = fabsf((float)data[i]);
        weighted_sum += magnitude * i;
        magnitude_sum += magnitude;
    }
    
    if (magnitude_sum > 0) {
        return (weighted_sum / magnitude_sum) * (AUDIO_SAMPLE_RATE / (2.0f * length));
    }
    
    return 0.0f;
}

static ret_code_t save_memo_to_file(const voice_memo_t* memo) {
    if (memo == NULL) {
        return NRF_ERROR_NULL;
    }
    
    if (!g_sd_card_mounted) {
        NRF_LOG_WARNING("SD card not available - memo not saved to file");
        return NRF_ERROR_INVALID_STATE;
    }
    
    FIL file;
    char filepath[64];
    snprintf(filepath, sizeof(filepath), "audio/memos/%s.wav", memo->filename);
    
    FRESULT ff_result = f_open(&file, filepath, FA_WRITE | FA_CREATE_ALWAYS);
    if (ff_result != FR_OK) {
        NRF_LOG_ERROR("Failed to create memo file: %d", ff_result);
        return NRF_ERROR_INTERNAL;
    }
    
    // Write WAV header (simplified)
    uint8_t wav_header[44] = {
        'R', 'I', 'F', 'F',
        0, 0, 0, 0,  // File size (will be filled)
        'W', 'A', 'V', 'E',
        'f', 'm', 't', ' ',
        16, 0, 0, 0,  // Subchunk1Size
        1, 0,         // AudioFormat (PCM)
        1, 0,         // NumChannels (mono)
        0x80, 0x3E, 0, 0,  // SampleRate (16000)
        0, 0x7D, 0, 0,     // ByteRate
        2, 0,         // BlockAlign
        16, 0,        // BitsPerSample
        'd', 'a', 't', 'a',
        0, 0, 0, 0    // Subchunk2Size (will be filled)
    };
    
    UINT bytes_written;
    ff_result = f_write(&file, wav_header, sizeof(wav_header), &bytes_written);
    if (ff_result != FR_OK) {
        f_close(&file);
        return NRF_ERROR_INTERNAL;
    }
    
    // Write audio data (placeholder - would write actual recorded data)
    uint32_t data_size = memo->duration_seconds * AUDIO_SAMPLE_RATE * 2;  // 16-bit samples
    
    f_close(&file);
    
    return NRF_SUCCESS;
}

static ret_code_t load_memo_from_file(uint16_t memo_id, voice_memo_t* memo) {
    if (memo == NULL) {
        return NRF_ERROR_NULL;
    }
    
    // Placeholder implementation
    // Would load memo metadata and prepare for playback
    
    return NRF_SUCCESS;
}

static void generate_unique_filename(char* buffer, size_t buffer_size, const char* prefix) {
    if (buffer == NULL || prefix == NULL) {
        return;
    }
    
    uint32_t timestamp = app_timer_cnt_get();
    snprintf(buffer, buffer_size, "%s_%08lx", prefix, timestamp);
}

// ================================
// POWER MANAGEMENT
// ================================

ret_code_t audio_enter_low_power_mode(audio_action_recorder_t* recorder) {
    if (recorder == NULL) {
        return NRF_ERROR_NULL;
    }
    
    // Stop any active recording
    if (g_recording_active) {
        audio_stop_recording(recorder);
    }
    
    // Power down PDM microphone
    nrf_gpio_pin_clear(PDM_POWER_PIN);
    
    // Set MusicMaker to low power
    musicmaker_set_volume(0);  // Mute
    
    recorder->mode = AUDIO_MODE_OFF;
    
    NRF_LOG_INFO("Entered low power mode");
    
    return NRF_SUCCESS;
}

ret_code_t audio_exit_low_power_mode(audio_action_recorder_t* recorder) {
    if (recorder == NULL) {
        return NRF_ERROR_NULL;
    }
    
    // Power up PDM microphone
    nrf_gpio_pin_set(PDM_POWER_PIN);
    nrf_delay_ms(10);
    
    // Restore MusicMaker volume
    musicmaker_set_volume(recorder->volume);
    
    recorder->mode = AUDIO_MODE_LISTEN;
    
    NRF_LOG_INFO("Exited low power mode");
    
    return NRF_SUCCESS;
}

uint32_t audio_get_estimated_battery_life_hours(audio_action_recorder_t* recorder) {
    if (recorder == NULL) {
        return 0;
    }
    
    // Simplified battery life calculation
    // Based on typical usage patterns and power consumption
    
    uint32_t base_consumption_ua = 200;  // Standby power
    uint32_t recording_overhead_ua = 1800;  // Additional when recording
    
    // Assume 3.7V, 2000mAh battery
    uint32_t battery_capacity_uah = 2000000;
    
    float avg_consumption = base_consumption_ua;
    if (recorder->mode != AUDIO_MODE_OFF) {
        avg_consumption += recording_overhead_ua * 0.1f;  // 10% duty cycle estimate
    }
    
    uint32_t estimated_hours = (uint32_t)(battery_capacity_uah / avg_consumption);
    
    return estimated_hours;
}

// ================================
// FILE MANAGEMENT
// ================================

ret_code_t audio_cleanup_old_memos(audio_action_recorder_t* recorder, uint32_t days_old) {
    if (recorder == NULL) {
        return NRF_ERROR_NULL;
    }
    
    uint32_t current_time = app_timer_cnt_get();
    uint32_t cutoff_time = current_time - (days_old * 24 * 60 * 60 * 1000);  // Convert to ms
    
    uint16_t deleted_count = 0;
    
    for (uint16_t i = 0; i < recorder->memo_count; i++) {
        voice_memo_t* memo = &recorder->memos[i];
        
        // Skip protected memos
        if (memo->is_protected) {
            continue;
        }
        
        // Check if memo is old enough
        if (memo->timestamp < cutoff_time) {
            // Delete file if on SD card
            if (g_sd_card_mounted) {
                char filepath[64];
                snprintf(filepath, sizeof(filepath), "audio/memos/%s.wav", memo->filename);
                f_unlink(filepath);
            }
            
            // Remove from array by shifting remaining memos
            memmove(&recorder->memos[i], &recorder->memos[i + 1], 
                   (recorder->memo_count - i - 1) * sizeof(voice_memo_t));
            recorder->memo_count--;
            deleted_count++;
            i--;  // Check this index again since we shifted
        }
    }
    
    NRF_LOG_INFO("Cleaned up %d old memos", deleted_count);
    
    return NRF_SUCCESS;
}

uint32_t audio_get_storage_used_kb(audio_action_recorder_t* recorder) {
    if (recorder == NULL) {
        return 0;
    }
    
    uint32_t total_size = 0;
    
    for (uint16_t i = 0; i < recorder->memo_count; i++) {
        total_size += recorder->memos[i].file_size_bytes;
    }
    
    return total_size / 1024;
}

uint32_t audio_get_storage_free_kb(audio_action_recorder_t* recorder) {
    if (recorder == NULL) {
        return 0;
    }
    
    return recorder->storage_available_kb - audio_get_storage_used_kb(recorder);
}

// ================================
// INTEGRATION FUNCTIONS
// ================================

ret_code_t audio_sync_with_combo_counter(audio_action_recorder_t* recorder, 
                                        uint8_t counter_id, 
                                        uint32_t count, 
                                        uint32_t combo) {
    if (recorder == NULL) {
        return NRF_ERROR_NULL;
    }
    
    // Sync rep count with combo counter
    if (count > recorder->total_reps_detected) {
        recorder->total_reps_detected = count;
    }
    
    // Play milestone sounds for combo achievements
    if (combo > 0 && combo % 10 == 0) {
        audio_play_combo_milestone_sound(recorder, combo);
    }
    
    return NRF_SUCCESS;
}

ret_code_t audio_validate_rep_with_counter(audio_action_recorder_t* recorder, bool rep_confirmed) {
    if (recorder == NULL) {
        return NRF_ERROR_NULL;
    }
    
    if (rep_confirmed) {
        // Play confirmation sound
        audio_play_form_feedback(recorder, 8);  // Good quality
    } else {
        // Rep was not valid, adjust detection sensitivity
        recorder->movement_threshold += 50;  // Make detection slightly less sensitive
    }
    
    return NRF_SUCCESS;
}

ret_code_t audio_tag_memo_with_workout(audio_action_recorder_t* recorder, 
                                      uint16_t memo_id, 
                                      const char* exercise_name) {
    if (recorder == NULL || exercise_name == NULL) {
        return NRF_ERROR_NULL;
    }
    
    if (memo_id >= recorder->memo_count) {
        return NRF_ERROR_NOT_FOUND;
    }
    
    voice_memo_t* memo = &recorder->memos[memo_id];
    
    // Add exercise tag to memo label
    char tagged_label[sizeof(memo->label)];
    snprintf(tagged_label, sizeof(tagged_label), "[%s] %s", exercise_name, memo->label);
    strncpy(memo->label, tagged_label, sizeof(memo->label) - 1);
    memo->label[sizeof(memo->label) - 1] = '\0';
    
    return NRF_SUCCESS;
}

// ================================
// WEAK CALLBACK IMPLEMENTATIONS
// ================================

__WEAK void audio_on_movement_detected(audio_action_recorder_t* recorder, const movement_analysis_t* movement) {
    // Default implementation - user can override
    NRF_LOG_DEBUG("Movement detected callback - override in user code");
}

__WEAK void audio_on_rep_detected(audio_action_recorder_t* recorder, uint16_t rep_count) {
    // Default implementation - user can override
    NRF_LOG_DEBUG("Rep detected callback - override in user code");
}

__WEAK void audio_on_memo_recorded(audio_action_recorder_t* recorder, voice_memo_t* memo) {
    // Default implementation - user can override
    NRF_LOG_DEBUG("Memo recorded callback - override in user code");
}

__WEAK void audio_on_silence_detected(audio_action_recorder_t* recorder) {
    // Default implementation - user can override
    NRF_LOG_DEBUG("Silence detected callback - override in user code");
}

__WEAK void audio_on_storage_low(audio_action_recorder_t* recorder, uint32_t free_space_kb) {
    // Default implementation - user can override
    NRF_LOG_WARNING("Storage low: %d KB remaining", free_space_kb);
}

__WEAK void audio_on_battery_low(audio_action_recorder_t* recorder, uint8_t battery_percent) {
    // Default implementation - user can override
    NRF_LOG_WARNING("Battery low: %d%% remaining", battery_percent);
}
#include "musicmaker_integration.h"
#include "nrf_log.h"
#include "nrf_delay.h"
#include "app_error.h"
#include <string.h>

// ================================
// GLOBAL STATE
// ================================

static MusicMakerStatus g_musicmaker_status = {0};
static MusicMakerCallback g_playback_callback = NULL;
static MusicMakerError g_last_error = MUSICMAKER_ERROR_NONE;

// ================================
// EMBEDDED AUDIO DATA
// ================================

// Simple beep tones (basic sine wave data)
static const uint8_t BEEP_SHORT_DATA[] = {
    0xFF, 0xF3, 0x60, 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x49, 0x6E, 0x66, 0x6F, 0x00, 0x00, 0x00, 0x0F, 0x00, 0x00, 0x00, 0x12, 0x00, 0x00, 0x04,
    // Simplified MP3 frame data for a 100ms beep at 1kHz
};

static const uint8_t BEEP_LONG_DATA[] = {
    0xFF, 0xF3, 0x60, 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x49, 0x6E, 0x66, 0x6F, 0x00, 0x00, 0x00, 0x0F, 0x00, 0x00, 0x00, 0x24, 0x00, 0x00, 0x08,
    // Simplified MP3 frame data for a 500ms beep at 1kHz
};

// Audio clip information table
static const AudioClipInfo g_audio_clips[] = {
    {AUDIO_CLIP_NONE, NULL, 0, false, NULL},
    
    // Counting clips (would be loaded from SD card)
    {AUDIO_CLIP_COUNT_1, "audio/count/1.mp3", 0, false, NULL},
    {AUDIO_CLIP_COUNT_2, "audio/count/2.mp3", 0, false, NULL},
    {AUDIO_CLIP_COUNT_3, "audio/count/3.mp3", 0, false, NULL},
    {AUDIO_CLIP_COUNT_4, "audio/count/4.mp3", 0, false, NULL},
    {AUDIO_CLIP_COUNT_5, "audio/count/5.mp3", 0, false, NULL},
    {AUDIO_CLIP_COUNT_6, "audio/count/6.mp3", 0, false, NULL},
    {AUDIO_CLIP_COUNT_7, "audio/count/7.mp3", 0, false, NULL},
    {AUDIO_CLIP_COUNT_8, "audio/count/8.mp3", 0, false, NULL},
    {AUDIO_CLIP_COUNT_9, "audio/count/9.mp3", 0, false, NULL},
    {AUDIO_CLIP_COUNT_10, "audio/count/10.mp3", 0, false, NULL},
    
    // Quality feedback
    {AUDIO_CLIP_PERFECT, "audio/feedback/perfect.mp3", 0, false, NULL},
    {AUDIO_CLIP_GOOD, "audio/feedback/good.mp3", 0, false, NULL},
    {AUDIO_CLIP_PARTIAL, "audio/feedback/partial.mp3", 0, false, NULL},
    {AUDIO_CLIP_MISS, "audio/feedback/miss.mp3", 0, false, NULL},
    
    // Combo milestones
    {AUDIO_CLIP_COMBO_10, "audio/combo/combo_10.mp3", 0, false, NULL},
    {AUDIO_CLIP_COMBO_25, "audio/combo/combo_25.mp3", 0, false, NULL},
    {AUDIO_CLIP_COMBO_50, "audio/combo/combo_50.mp3", 0, false, NULL},
    {AUDIO_CLIP_COMBO_100, "audio/combo/combo_100.mp3", 0, false, NULL},
    
    // System events
    {AUDIO_CLIP_COMBO_BROKEN, "audio/system/combo_broken.mp3", 0, false, NULL},
    {AUDIO_CLIP_NEW_RECORD, "audio/system/new_record.mp3", 0, false, NULL},
    {AUDIO_CLIP_WORKOUT_COMPLETE, "audio/system/workout_complete.mp3", 0, false, NULL},
    {AUDIO_CLIP_REST_TIME, "audio/system/rest_time.mp3", 0, false, NULL},
    {AUDIO_CLIP_SET_COMPLETE, "audio/system/set_complete.mp3", 0, false, NULL},
    
    // Simple beeps (embedded)
    {AUDIO_CLIP_BEEP_SHORT, NULL, sizeof(BEEP_SHORT_DATA), true, BEEP_SHORT_DATA},
    {AUDIO_CLIP_BEEP_LONG, NULL, sizeof(BEEP_LONG_DATA), true, BEEP_LONG_DATA},
    {AUDIO_CLIP_BEEP_SUCCESS, "audio/beeps/success.mp3", 0, false, NULL},
    {AUDIO_CLIP_BEEP_ERROR, "audio/beeps/error.mp3", 0, false, NULL},
};

// ================================
// PRIVATE FUNCTIONS
// ================================

static bool vs1053_wait_for_dreq(uint32_t timeout_ms)
{
    uint32_t start_time = 0; // Would use app_timer_cnt_get() in real implementation
    
    while (!musicmaker_ready_for_data()) {
        if (timeout_ms > 0 && (start_time + timeout_ms) < start_time) { // Simplified timeout check
            return false;
        }
        nrf_delay_us(10);
    }
    return true;
}

static bool vs1053_send_data(const uint8_t* data, uint16_t length)
{
    if (!dual_spi_musicmaker_ready()) {
        return false;
    }
    
    // Wait for VS1053 to be ready
    if (!vs1053_wait_for_dreq(100)) {
        g_last_error = MUSICMAKER_ERROR_COMMUNICATION;
        return false;
    }
    
    // Select VS1053 for data
    musicmaker_cs_select(true);
    
    // Send data via SPI
    ret_code_t err_code = dual_spi_musicmaker_transfer(data, length, NULL, 0);
    
    // Deselect
    musicmaker_cs_select(false);
    
    if (err_code != NRF_SUCCESS) {
        g_last_error = MUSICMAKER_ERROR_COMMUNICATION;
        return false;
    }
    
    return true;
}

// ================================
// CORE FUNCTIONS
// ================================

bool musicmaker_init(void)
{
    NRF_LOG_INFO("MusicMaker: Initializing...");
    
    // Initialize status
    memset(&g_musicmaker_status, 0, sizeof(g_musicmaker_status));
    g_musicmaker_status.state = MUSICMAKER_STATE_INITIALIZING;
    g_musicmaker_status.volume = 128; // Mid-level volume
    
    // Initialize dual SPI if not already done
    if (!dual_spi_init()) {
        NRF_LOG_ERROR("MusicMaker: Failed to initialize SPI");
        g_last_error = MUSICMAKER_ERROR_SPI_INIT;
        g_musicmaker_status.state = MUSICMAKER_STATE_ERROR;
        return false;
    }
    
    // Hardware reset
    musicmaker_hardware_reset();
    
    // Wait for VS1053 to boot
    nrf_delay_ms(100);
    
    // Test communication
    if (!musicmaker_test_communication()) {
        NRF_LOG_ERROR("MusicMaker: Communication test failed");
        g_last_error = MUSICMAKER_ERROR_COMMUNICATION;
        g_musicmaker_status.state = MUSICMAKER_STATE_ERROR;
        return false;
    }
    
    // Configure VS1053
    if (!musicmaker_software_reset()) {
        NRF_LOG_ERROR("MusicMaker: Software reset failed");
        g_musicmaker_status.state = MUSICMAKER_STATE_ERROR;
        return false;
    }
    
    // Set default volume
    if (!musicmaker_set_volume(g_musicmaker_status.volume)) {
        NRF_LOG_WARNING("MusicMaker: Failed to set initial volume");
    }
    
    g_musicmaker_status.state = MUSICMAKER_STATE_READY;
    NRF_LOG_INFO("MusicMaker: Initialization complete");
    
    return true;
}

void musicmaker_deinit(void)
{
    NRF_LOG_INFO("MusicMaker: Deinitializing...");
    
    // Stop any playback
    musicmaker_stop_playback();
    
    // Reset VS1053
    musicmaker_hardware_reset();
    
    // Clear status
    memset(&g_musicmaker_status, 0, sizeof(g_musicmaker_status));
    g_musicmaker_status.state = MUSICMAKER_STATE_UNINITIALIZED;
    
    g_playback_callback = NULL;
    g_last_error = MUSICMAKER_ERROR_NONE;
}

const MusicMakerStatus* musicmaker_get_status(void)
{
    return &g_musicmaker_status;
}

bool musicmaker_set_volume(uint8_t volume)
{
    // VS1053 volume: 0x00 = loudest, 0xFF = silent
    // Convert to VS1053 format (both channels same volume)
    uint16_t vs1053_volume = (volume << 8) | volume;
    
    if (musicmaker_write_register(VS1053_REG_VOL, vs1053_volume)) {
        g_musicmaker_status.volume = volume;
        return true;
    }
    
    return false;
}

uint8_t musicmaker_get_volume(void)
{
    return g_musicmaker_status.volume;
}

// ================================
// AUDIO PLAYBACK
// ================================

bool musicmaker_play_clip(AudioClip clip, bool blocking)
{
    if (clip >= AUDIO_CLIP_MAX) {
        g_last_error = MUSICMAKER_ERROR_INVALID_FORMAT;
        return false;
    }
    
    const AudioClipInfo* clip_info = &g_audio_clips[clip];
    
    if (clip_info->is_embedded && clip_info->data) {
        // Play embedded data
        return musicmaker_play_data(clip_info->data, clip_info->size_bytes, blocking);
    } else if (clip_info->filename) {
        // Play from SD card
        return musicmaker_play_file(clip_info->filename, blocking);
    }
    
    g_last_error = MUSICMAKER_ERROR_FILE_NOT_FOUND;
    return false;
}

bool musicmaker_play_file(const char* filename, bool blocking)
{
    if (!filename) {
        g_last_error = MUSICMAKER_ERROR_FILE_NOT_FOUND;
        return false;
    }
    
    if (g_musicmaker_status.state != MUSICMAKER_STATE_READY) {
        g_last_error = MUSICMAKER_ERROR_COMMUNICATION;
        return false;
    }
    
    NRF_LOG_INFO("MusicMaker: Playing file: %s", filename);
    
    // In a real implementation, this would:
    // 1. Open file from SD card
    // 2. Read chunks of audio data
    // 3. Stream to VS1053
    // For now, just simulate playback
    
    g_musicmaker_status.is_playing = true;
    g_musicmaker_status.state = MUSICMAKER_STATE_PLAYING;
    g_musicmaker_status.clips_played++;
    
    // Simulate playback duration
    if (blocking) {
        nrf_delay_ms(500); // Simulate 500ms audio clip
        musicmaker_stop_playback();
    }
    
    return true;
}

bool musicmaker_play_data(const uint8_t* data, uint32_t size, bool blocking)
{
    if (!data || size == 0) {
        g_last_error = MUSICMAKER_ERROR_INVALID_FORMAT;
        return false;
    }
    
    if (g_musicmaker_status.state != MUSICMAKER_STATE_READY) {
        g_last_error = MUSICMAKER_ERROR_COMMUNICATION;
        return false;
    }
    
    NRF_LOG_INFO("MusicMaker: Playing embedded data (%d bytes)", size);
    
    g_musicmaker_status.is_playing = true;
    g_musicmaker_status.state = MUSICMAKER_STATE_PLAYING;
    
    // Stream data to VS1053
    const uint32_t chunk_size = 32; // VS1053 typical buffer size
    uint32_t bytes_sent = 0;
    
    while (bytes_sent < size) {
        uint32_t bytes_to_send = (size - bytes_sent > chunk_size) ? chunk_size : (size - bytes_sent);
        
        if (!vs1053_send_data(&data[bytes_sent], bytes_to_send)) {
            musicmaker_stop_playback();
            return false;
        }
        
        bytes_sent += bytes_to_send;
        
        if (!blocking) {
            // Store remaining data for later streaming
            g_musicmaker_status.playback_position = bytes_sent;
            break;
        }
        
        nrf_delay_ms(1); // Small delay between chunks
    }
    
    if (blocking || bytes_sent >= size) {
        // Wait for playback completion
        nrf_delay_ms(100);
        musicmaker_stop_playback();
    }
    
    g_musicmaker_status.clips_played++;
    
    return true;
}

void musicmaker_stop_playback(void)
{
    if (g_musicmaker_status.is_playing) {
        NRF_LOG_INFO("MusicMaker: Stopping playback");
        
        // Cancel playback on VS1053
        uint16_t mode = musicmaker_read_register(VS1053_REG_MODE);
        musicmaker_write_register(VS1053_REG_MODE, mode | VS1053_MODE_SM_CANCEL);
        
        // Clear cancel bit after short delay
        nrf_delay_ms(10);
        mode = musicmaker_read_register(VS1053_REG_MODE);
        musicmaker_write_register(VS1053_REG_MODE, mode & ~VS1053_MODE_SM_CANCEL);
        
        g_musicmaker_status.is_playing = false;
        g_musicmaker_status.state = MUSICMAKER_STATE_READY;
        g_musicmaker_status.current_clip = AUDIO_CLIP_NONE;
        
        if (g_playback_callback) {
            g_playback_callback(g_musicmaker_status.current_clip, true);
        }
    }
}

void musicmaker_pause_playback(void)
{
    // VS1053 doesn't have native pause, so we implement by stopping
    musicmaker_stop_playback();
}

void musicmaker_resume_playback(void)
{
    // Would need to store position and restart from there
    // For now, just a placeholder
}

bool musicmaker_is_playing(void)
{
    return g_musicmaker_status.is_playing;
}

// ================================
// UTILITY FUNCTIONS
// ================================

uint16_t musicmaker_read_register(uint8_t reg)
{
    if (!dual_spi_musicmaker_ready()) {
        return 0xFFFF;
    }
    
    uint8_t cmd[4] = {VS1053_CMD_READ, reg, 0x00, 0x00};
    uint8_t response[4] = {0};
    
    // Wait for VS1053 to be ready
    if (!vs1053_wait_for_dreq(100)) {
        return 0xFFFF;
    }
    
    // Select VS1053 for command
    musicmaker_cs_select(true);
    
    // Send command and receive response
    ret_code_t err_code = dual_spi_musicmaker_transfer(cmd, 4, response, 4);
    
    // Deselect
    musicmaker_cs_select(false);
    
    if (err_code != NRF_SUCCESS) {
        return 0xFFFF;
    }
    
    return (response[2] << 8) | response[3];
}

bool musicmaker_write_register(uint8_t reg, uint16_t value)
{
    if (!dual_spi_musicmaker_ready()) {
        return false;
    }
    
    uint8_t cmd[4] = {
        VS1053_CMD_WRITE,
        reg,
        (uint8_t)(value >> 8),
        (uint8_t)(value & 0xFF)
    };
    
    // Wait for VS1053 to be ready
    if (!vs1053_wait_for_dreq(100)) {
        return false;
    }
    
    // Select VS1053 for command
    musicmaker_cs_select(true);
    
    // Send command
    ret_code_t err_code = dual_spi_musicmaker_transfer(cmd, 4, NULL, 0);
    
    // Deselect
    musicmaker_cs_select(false);
    
    return err_code == NRF_SUCCESS;
}

void musicmaker_hardware_reset(void)
{
    NRF_LOG_INFO("MusicMaker: Hardware reset");
    
    // Pull reset pin low
    musicmaker_reset_set(true);
    nrf_delay_ms(10);
    
    // Release reset pin
    musicmaker_reset_set(false);
    nrf_delay_ms(100);
}

bool musicmaker_software_reset(void)
{
    NRF_LOG_INFO("MusicMaker: Software reset");
    
    // Set software reset bit
    if (!musicmaker_write_register(VS1053_REG_MODE, VS1053_MODE_SM_SDINEW | VS1053_MODE_SM_RESET)) {
        return false;
    }
    
    // Wait for reset completion
    nrf_delay_ms(10);
    
    // Clear reset bit and set standard mode
    return musicmaker_write_register(VS1053_REG_MODE, VS1053_MODE_SM_SDINEW);
}

bool musicmaker_test_communication(void)
{
    NRF_LOG_INFO("MusicMaker: Testing communication");
    
    // Test by reading status register
    uint16_t status = musicmaker_read_register(VS1053_REG_STATUS);
    
    if (status == 0xFFFF) {
        return false;
    }
    
    // Test by writing and reading back a register
    const uint16_t test_value = 0x1234;
    if (!musicmaker_write_register(VS1053_REG_WRAMADDR, test_value)) {
        return false;
    }
    
    uint16_t read_back = musicmaker_read_register(VS1053_REG_WRAMADDR);
    return read_back == test_value;
}

void musicmaker_update(void)
{
    // Handle ongoing playback, buffer management, etc.
    if (g_musicmaker_status.is_playing) {
        // Check if playback is complete
        if (!musicmaker_ready_for_data()) {
            // Still playing
            return;
        }
        
        // Playback might be complete
        // In a real implementation, check actual VS1053 status
        
        // For now, just simulate playback completion
        static uint32_t playback_counter = 0;
        playback_counter++;
        
        if (playback_counter > 1000) { // Arbitrary completion check
            musicmaker_stop_playback();
            playback_counter = 0;
        }
    }
}

// ================================
// AUDIO CLIP MANAGEMENT
// ================================

const AudioClipInfo* musicmaker_get_clip_info(AudioClip clip)
{
    if (clip >= AUDIO_CLIP_MAX) {
        return NULL;
    }
    
    return &g_audio_clips[clip];
}

bool musicmaker_register_clip(AudioClip clip_id, const char* filename, 
                             const uint8_t* data, uint32_t size)
{
    // In a real implementation, this would allow dynamic registration
    // For now, just return success for valid parameters
    return (clip_id < AUDIO_CLIP_MAX && (filename || data));
}

bool musicmaker_preload_clip(AudioClip clip)
{
    // In a real implementation, this would load the audio file into memory
    // For now, just validate the clip exists
    return musicmaker_get_clip_info(clip) != NULL;
}

// ================================
// ERROR HANDLING
// ================================

MusicMakerError musicmaker_get_last_error(void)
{
    return g_last_error;
}

void musicmaker_clear_error(void)
{
    g_last_error = MUSICMAKER_ERROR_NONE;
}

const char* musicmaker_error_to_string(MusicMakerError error)
{
    switch (error) {
        case MUSICMAKER_ERROR_NONE:
            return "No error";
        case MUSICMAKER_ERROR_SPI_INIT:
            return "SPI initialization failed";
        case MUSICMAKER_ERROR_RESET_TIMEOUT:
            return "Reset timeout";
        case MUSICMAKER_ERROR_COMMUNICATION:
            return "Communication error";
        case MUSICMAKER_ERROR_SD_CARD:
            return "SD card error";
        case MUSICMAKER_ERROR_FILE_NOT_FOUND:
            return "Audio file not found";
        case MUSICMAKER_ERROR_INVALID_FORMAT:
            return "Invalid audio format";
        case MUSICMAKER_ERROR_BUFFER_FULL:
            return "Audio buffer full";
        case MUSICMAKER_ERROR_PLAYBACK_FAILED:
            return "Playback failed";
        default:
            return "Unknown error";
    }
}

// ================================
// CALLBACK SYSTEM
// ================================

void musicmaker_set_callback(MusicMakerCallback callback)
{
    g_playback_callback = callback;
}
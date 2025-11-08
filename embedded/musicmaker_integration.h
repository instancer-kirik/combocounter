#ifndef MUSICMAKER_INTEGRATION_H
#define MUSICMAKER_INTEGRATION_H

#include <stdint.h>
#include <stdbool.h>
#include "dual_spi_config.h"

#ifdef __cplusplus
extern "C" {
#endif

// ================================
// MUSICMAKER VS1053 CONSTANTS
// ================================

// VS1053 Command Codes
#define VS1053_CMD_WRITE          0x02
#define VS1053_CMD_READ           0x03

// VS1053 Register Addresses
#define VS1053_REG_MODE           0x00
#define VS1053_REG_STATUS         0x01
#define VS1053_REG_BASS           0x02
#define VS1053_REG_CLOCKF         0x03
#define VS1053_REG_DECODE_TIME    0x04
#define VS1053_REG_AUDATA         0x05
#define VS1053_REG_WRAM           0x06
#define VS1053_REG_WRAMADDR       0x07
#define VS1053_REG_HDAT0          0x08
#define VS1053_REG_HDAT1          0x09
#define VS1053_REG_AIADDR         0x0A
#define VS1053_REG_VOL            0x0B
#define VS1053_REG_AICTRL0        0x0C
#define VS1053_REG_AICTRL1        0x0D
#define VS1053_REG_AICTRL2        0x0E
#define VS1053_REG_AICTRL3        0x0F

// VS1053 Mode Register Bits
#define VS1053_MODE_SM_DIFF       0x0001
#define VS1053_MODE_SM_LAYER12    0x0002
#define VS1053_MODE_SM_RESET      0x0004
#define VS1053_MODE_SM_CANCEL     0x0008
#define VS1053_MODE_SM_EARSPEAKER_LO 0x0010
#define VS1053_MODE_SM_TESTS      0x0020
#define VS1053_MODE_SM_STREAM     0x0040
#define VS1053_MODE_SM_EARSPEAKER_HI 0x0080
#define VS1053_MODE_SM_DACT       0x0100
#define VS1053_MODE_SM_SDIORD     0x0200
#define VS1053_MODE_SM_SDISHARE   0x0400
#define VS1053_MODE_SM_SDINEW     0x0800
#define VS1053_MODE_SM_ADPCM      0x1000
#define VS1053_MODE_SM_LINE1      0x4000
#define VS1053_MODE_SM_CLK_RANGE  0x8000

// Audio buffer size
#define AUDIO_BUFFER_SIZE         32

// ================================
// AUDIO CLIP DEFINITIONS
// ================================

typedef enum {
    AUDIO_CLIP_NONE = 0,
    
    // Counting clips (1-10)
    AUDIO_CLIP_COUNT_1,
    AUDIO_CLIP_COUNT_2,
    AUDIO_CLIP_COUNT_3,
    AUDIO_CLIP_COUNT_4,
    AUDIO_CLIP_COUNT_5,
    AUDIO_CLIP_COUNT_6,
    AUDIO_CLIP_COUNT_7,
    AUDIO_CLIP_COUNT_8,
    AUDIO_CLIP_COUNT_9,
    AUDIO_CLIP_COUNT_10,
    
    // Quality feedback
    AUDIO_CLIP_PERFECT,
    AUDIO_CLIP_GOOD,
    AUDIO_CLIP_PARTIAL,
    AUDIO_CLIP_MISS,
    
    // Combo milestones
    AUDIO_CLIP_COMBO_10,
    AUDIO_CLIP_COMBO_25,
    AUDIO_CLIP_COMBO_50,
    AUDIO_CLIP_COMBO_100,
    
    // System events
    AUDIO_CLIP_COMBO_BROKEN,
    AUDIO_CLIP_NEW_RECORD,
    AUDIO_CLIP_WORKOUT_COMPLETE,
    AUDIO_CLIP_REST_TIME,
    AUDIO_CLIP_SET_COMPLETE,
    
    // Simple beeps
    AUDIO_CLIP_BEEP_SHORT,
    AUDIO_CLIP_BEEP_LONG,
    AUDIO_CLIP_BEEP_SUCCESS,
    AUDIO_CLIP_BEEP_ERROR,
    
    AUDIO_CLIP_MAX
} AudioClip;

typedef enum {
    MUSICMAKER_STATE_UNINITIALIZED = 0,
    MUSICMAKER_STATE_INITIALIZING,
    MUSICMAKER_STATE_READY,
    MUSICMAKER_STATE_PLAYING,
    MUSICMAKER_STATE_ERROR
} MusicMakerState;

typedef struct {
    AudioClip clip_id;
    const char* filename;
    uint32_t size_bytes;
    bool is_embedded;           // True if data is in flash, false if on SD card
    const uint8_t* data;        // Pointer to embedded audio data (if applicable)
} AudioClipInfo;

typedef struct {
    MusicMakerState state;
    uint8_t volume;             // 0-255 (VS1053 volume scale)
    bool is_playing;
    AudioClip current_clip;
    uint32_t playback_position;
    
    // Buffer for streaming audio
    uint8_t audio_buffer[AUDIO_BUFFER_SIZE];
    uint16_t buffer_position;
    bool buffer_needs_refill;
    
    // Statistics
    uint32_t clips_played;
    uint32_t playback_errors;
} MusicMakerStatus;

// ================================
// CORE FUNCTIONS
// ================================

/**
 * Initialize the MusicMaker FeatherWing
 * Sets up SPI communication, resets the VS1053, and configures basic settings
 * @return true on success, false on failure
 */
bool musicmaker_init(void);

/**
 * Deinitialize the MusicMaker and release resources
 */
void musicmaker_deinit(void);

/**
 * Get current MusicMaker status
 * @return Pointer to status structure (read-only)
 */
const MusicMakerStatus* musicmaker_get_status(void);

/**
 * Set master volume (0 = loudest, 255 = silent)
 * @param volume Volume level (0-255)
 * @return true on success
 */
bool musicmaker_set_volume(uint8_t volume);

/**
 * Get current volume setting
 * @return Volume level (0-255)
 */
uint8_t musicmaker_get_volume(void);

// ================================
// AUDIO PLAYBACK
// ================================

/**
 * Play an audio clip by ID
 * @param clip Audio clip to play
 * @param blocking If true, wait for playback to complete
 * @return true if playback started successfully
 */
bool musicmaker_play_clip(AudioClip clip, bool blocking);

/**
 * Play audio from a file on SD card
 * @param filename Path to audio file (e.g., "audio/beep.mp3")
 * @param blocking If true, wait for playback to complete
 * @return true if playback started successfully
 */
bool musicmaker_play_file(const char* filename, bool blocking);

/**
 * Play embedded audio data from flash memory
 * @param data Pointer to audio data in flash
 * @param size Size of audio data in bytes
 * @param blocking If true, wait for playback to complete
 * @return true if playback started successfully
 */
bool musicmaker_play_data(const uint8_t* data, uint32_t size, bool blocking);

/**
 * Stop current playback
 */
void musicmaker_stop_playback(void);

/**
 * Pause current playback
 */
void musicmaker_pause_playback(void);

/**
 * Resume paused playback
 */
void musicmaker_resume_playback(void);

/**
 * Check if audio is currently playing
 * @return true if playing, false if stopped
 */
bool musicmaker_is_playing(void);

// ================================
// ADVANCED FEATURES
// ================================

/**
 * Set bass boost settings
 * @param bass_enhancement Bass enhancement (0-15)
 * @param bass_freq Bass frequency limit (2-15, where 2=60Hz, 15=150Hz)
 * @return true on success
 */
bool musicmaker_set_bass(uint8_t bass_enhancement, uint8_t bass_freq);

/**
 * Set treble control
 * @param treble_enhancement Treble enhancement (0-15)
 * @param treble_freq Treble frequency limit (1-15, where 1=1000Hz, 15=8000Hz)
 * @return true on success
 */
bool musicmaker_set_treble(uint8_t treble_enhancement, uint8_t treble_freq);

/**
 * Enable/disable differential output mode
 * @param enable True to enable differential output
 * @return true on success
 */
bool musicmaker_set_differential(bool enable);

// ================================
// UTILITY FUNCTIONS
// ================================

/**
 * Read VS1053 register
 * @param reg Register address
 * @return Register value, or 0xFFFF on error
 */
uint16_t musicmaker_read_register(uint8_t reg);

/**
 * Write VS1053 register
 * @param reg Register address
 * @param value Value to write
 * @return true on success
 */
bool musicmaker_write_register(uint8_t reg, uint16_t value);

/**
 * Perform hardware reset of VS1053
 */
void musicmaker_hardware_reset(void);

/**
 * Perform software reset of VS1053
 * @return true on success
 */
bool musicmaker_software_reset(void);

/**
 * Test VS1053 communication
 * @return true if communication is working
 */
bool musicmaker_test_communication(void);

/**
 * Update MusicMaker state (call from main loop)
 * Handles buffering, state management, etc.
 */
void musicmaker_update(void);

// ================================
// AUDIO CLIP MANAGEMENT
// ================================

/**
 * Get information about an audio clip
 * @param clip Audio clip ID
 * @return Pointer to clip info, or NULL if invalid
 */
const AudioClipInfo* musicmaker_get_clip_info(AudioClip clip);

/**
 * Register a custom audio clip
 * @param clip_id Custom clip ID (should be >= AUDIO_CLIP_MAX)
 * @param filename Filename on SD card
 * @param data Embedded data pointer (can be NULL)
 * @param size Size of audio data
 * @return true on success
 */
bool musicmaker_register_clip(AudioClip clip_id, const char* filename, 
                             const uint8_t* data, uint32_t size);

/**
 * Load audio clip from SD card into memory
 * @param clip Audio clip to preload
 * @return true on success
 */
bool musicmaker_preload_clip(AudioClip clip);

// ================================
// ERROR HANDLING
// ================================

typedef enum {
    MUSICMAKER_ERROR_NONE = 0,
    MUSICMAKER_ERROR_SPI_INIT,
    MUSICMAKER_ERROR_RESET_TIMEOUT,
    MUSICMAKER_ERROR_COMMUNICATION,
    MUSICMAKER_ERROR_SD_CARD,
    MUSICMAKER_ERROR_FILE_NOT_FOUND,
    MUSICMAKER_ERROR_INVALID_FORMAT,
    MUSICMAKER_ERROR_BUFFER_FULL,
    MUSICMAKER_ERROR_PLAYBACK_FAILED
} MusicMakerError;

/**
 * Get last error code
 * @return Last error that occurred
 */
MusicMakerError musicmaker_get_last_error(void);

/**
 * Clear error state
 */
void musicmaker_clear_error(void);

/**
 * Get error description string
 * @param error Error code
 * @return Human-readable error description
 */
const char* musicmaker_error_to_string(MusicMakerError error);

// ================================
// CALLBACK SYSTEM
// ================================

typedef void (*MusicMakerCallback)(AudioClip clip, bool playback_complete);

/**
 * Set callback for playback events
 * @param callback Function to call when playback starts/stops
 */
void musicmaker_set_callback(MusicMakerCallback callback);

#ifdef __cplusplus
}
#endif

#endif // MUSICMAKER_INTEGRATION_H
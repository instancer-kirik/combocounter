#ifndef TURSO_LOCAL_H
#define TURSO_LOCAL_H

#include <stdint.h>
#include <stdbool.h>
#include "simple_combo_core.h"

// Turso-compatible local database for nRF52840
// Designed for energy efficiency and BTLE sync

#define TURSO_LOCAL_VERSION "1.0.0"
#define MAX_SYNC_QUEUE_SIZE 32
#define MAX_DEVICE_ID_LENGTH 16
#define TURSO_MAGIC_BYTES 0xC0FFEE42

// Energy-conscious settings
#define BATCH_WRITE_THRESHOLD 5     // Write after 5 changes to save flash cycles
#define SYNC_HEARTBEAT_INTERVAL_MS 30000  // 30 seconds between BTLE sync attempts
#define LOW_POWER_SYNC_INTERVAL_MS 300000 // 5 minutes in low power mode

// Turso-compatible record types
typedef enum {
    RECORD_TYPE_COUNTER = 1,
    RECORD_TYPE_SESSION = 2,
    RECORD_TYPE_AUDIO_CONFIG = 3,
    RECORD_TYPE_CUSTOM_LABEL = 4,
    RECORD_TYPE_SYNC_STATE = 5,
    RECORD_TYPE_DEVICE_INFO = 6
} TursoRecordType;

// Sync operation types for BTLE
typedef enum {
    SYNC_OP_CREATE = 1,
    SYNC_OP_UPDATE = 2,
    SYNC_OP_DELETE = 3,
    SYNC_OP_READ = 4
} TursoSyncOperation;

// Lightweight sync record for BTLE transmission
typedef struct {
    uint32_t timestamp_ms;
    uint16_t record_id;
    TursoRecordType type;
    TursoSyncOperation operation;
    uint8_t data[32];              // Compact payload for BTLE
    uint16_t crc16;                // Data integrity check
    bool pending_sync;             // Needs to be sent over BTLE
} __attribute__((packed)) TursoSyncRecord;

// Local database state
typedef struct {
    char device_id[MAX_DEVICE_ID_LENGTH];
    uint32_t last_sync_timestamp;
    uint32_t local_sequence_number;
    uint16_t pending_sync_count;
    bool btle_connected;
    bool low_power_mode;
    
    // Sync queue for BTLE transmission
    TursoSyncRecord sync_queue[MAX_SYNC_QUEUE_SIZE];
    uint8_t sync_queue_head;
    uint8_t sync_queue_tail;
    
    // Energy monitoring
    uint32_t total_writes;
    uint32_t last_flash_write_ms;
    uint8_t dirty_record_count;    // Batch writes when this hits threshold
} TursoLocalDB;

// Counter state record (Turso-compatible schema)
typedef struct {
    uint16_t record_id;
    uint32_t created_at;
    uint32_t updated_at;
    char label[MAX_LABEL_LENGTH];
    CounterType type;
    int32_t count;
    int32_t total;
    int32_t max_combo;
    float multiplier;
    uint32_t session_count;        // For analytics
    bool active;
} __attribute__((packed)) TursoCounterRecord;

// Session tracking record
typedef struct {
    uint16_t record_id;
    uint32_t started_at;
    uint32_t ended_at;
    uint16_t counter_id;
    uint32_t total_reps;
    uint32_t perfect_reps;
    uint32_t good_reps;
    uint32_t partial_reps;
    uint32_t miss_reps;
    float avg_multiplier;
    uint32_t max_combo_achieved;
} __attribute__((packed)) TursoSessionRecord;

// Audio configuration record
typedef struct {
    uint16_t record_id;
    uint32_t updated_at;
    uint8_t audio_mode;
    uint8_t volume;
    bool count_aloud;
    bool form_feedback;
    bool combo_announcements;
    bool milestone_sounds;
    char custom_labels[10][32];    // User-defined counting phrases
    char quality_labels[4][32];    // Custom quality feedback
} __attribute__((packed)) TursoAudioRecord;

// Function declarations
bool turso_local_init(const char* device_id);
void turso_local_shutdown(void);

// Counter operations (energy-optimized)
bool turso_save_counter(const Counter* counter, bool force_immediate_write);
bool turso_load_counter(uint16_t counter_id, Counter* counter);
bool turso_delete_counter(uint16_t counter_id);
bool turso_load_all_counters(Counter* counters, uint8_t max_count, uint8_t* actual_count);

// Session tracking
uint16_t turso_start_session(uint16_t counter_id);
bool turso_end_session(uint16_t session_id, const TursoSessionRecord* session_data);
bool turso_get_session_stats(uint16_t counter_id, uint32_t* total_sessions, float* avg_accuracy);

// Audio configuration
bool turso_save_audio_config(const TursoAudioRecord* audio_config);
bool turso_load_audio_config(TursoAudioRecord* audio_config);

// BTLE sync operations
bool turso_queue_sync_operation(TursoRecordType type, uint16_t record_id, 
                               TursoSyncOperation op, const void* data, uint8_t data_size);
bool turso_get_next_sync_record(TursoSyncRecord* record);
void turso_mark_sync_complete(uint16_t record_id);
uint16_t turso_get_pending_sync_count(void);

// Remote database sync (for later BTLE implementation)
typedef void (*turso_sync_callback_t)(TursoSyncRecord* record, bool success);
void turso_set_sync_callback(turso_sync_callback_t callback);

// Energy management
void turso_enter_low_power_mode(void);
void turso_exit_low_power_mode(void);
void turso_force_flush_pending_writes(void);
uint32_t turso_get_flash_write_count(void);

// BTLE connection status
void turso_set_btle_connected(bool connected);
bool turso_is_btle_connected(void);

// Compact data serialization for BTLE (minimal energy)
uint8_t turso_serialize_counter(const TursoCounterRecord* counter, uint8_t* buffer, uint8_t max_size);
bool turso_deserialize_counter(const uint8_t* buffer, uint8_t size, TursoCounterRecord* counter);

// Database maintenance (run periodically to optimize storage)
void turso_compact_database(void);
bool turso_verify_database_integrity(void);

// Statistics and monitoring
typedef struct {
    uint32_t total_records;
    uint32_t pending_sync_records;
    uint32_t total_flash_writes;
    uint32_t last_sync_timestamp;
    uint16_t database_size_kb;
    bool integrity_ok;
    bool btle_sync_healthy;
} TursoDatabaseStats;

bool turso_get_database_stats(TursoDatabaseStats* stats);

// Error handling
typedef enum {
    TURSO_OK = 0,
    TURSO_ERROR_NOT_INITIALIZED = -1,
    TURSO_ERROR_FLASH_WRITE_FAILED = -2,
    TURSO_ERROR_RECORD_NOT_FOUND = -3,
    TURSO_ERROR_SYNC_QUEUE_FULL = -4,
    TURSO_ERROR_INVALID_RECORD = -5,
    TURSO_ERROR_LOW_POWER_MODE = -6,
    TURSO_ERROR_BTLE_DISCONNECTED = -7
} TursoError;

TursoError turso_get_last_error(void);
const char* turso_error_string(TursoError error);

// Development/debug helpers (remove in production)
#ifdef DEBUG
void turso_dump_database_state(void);
void turso_simulate_sync_failure(bool enable);
void turso_force_sync_queue_full(void);
#endif

#endif // TURSO_LOCAL_H
#include "turso_local.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

// Nordic SDK includes (simulated for now)
#define NRF_LOG_INFO(fmt, ...) printf("[TURSO] " fmt "\n", ##__VA_ARGS__)
#define NRF_LOG_ERROR(fmt, ...) printf("[TURSO ERROR] " fmt "\n", ##__VA_ARGS__)
#define NRF_LOG_DEBUG(fmt, ...) printf("[TURSO DEBUG] " fmt "\n", ##__VA_ARGS__)

// Flash storage simulation (replace with Nordic flash API)
#define FLASH_PAGE_SIZE 4096
#define FLASH_SECTOR_SIZE 64
#define TURSO_FLASH_BASE_ADDR 0x80000
static uint8_t flash_simulation[FLASH_PAGE_SIZE * 4];

// Global database state
static TursoLocalDB g_db;
static bool g_db_initialized = false;
static TursoError g_last_error = TURSO_OK;

// Energy-conscious write batching
static TursoCounterRecord g_pending_counters[MAX_COUNTERS];
static bool g_counter_dirty[MAX_COUNTERS];
static uint8_t g_dirty_counter_count = 0;

// CRC16 calculation for data integrity
static uint16_t calculate_crc16(const uint8_t* data, uint16_t length) {
    uint16_t crc = 0xFFFF;
    for (uint16_t i = 0; i < length; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (uint8_t bit = 0; bit < 8; bit++) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ 0x1021;
            } else {
                crc = crc << 1;
            }
        }
    }
    return crc;
}

// Get current timestamp (milliseconds since boot)
static uint32_t get_timestamp_ms(void) {
    // In real nRF52840, use app_timer or RTC
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

// Flash write wrapper with energy monitoring
static bool flash_write_sector(uint32_t addr, const void* data, uint16_t size) {
    // In real nRF52840, use nrf_fstorage or fds (Flash Data Storage)
    if (addr < TURSO_FLASH_BASE_ADDR || addr + size >= TURSO_FLASH_BASE_ADDR + sizeof(flash_simulation)) {
        g_last_error = TURSO_ERROR_FLASH_WRITE_FAILED;
        return false;
    }
    
    memcpy(&flash_simulation[addr - TURSO_FLASH_BASE_ADDR], data, size);
    g_db.total_writes++;
    g_db.last_flash_write_ms = get_timestamp_ms();
    
    NRF_LOG_DEBUG("Flash write: addr=0x%08X, size=%d, total_writes=%d", 
                  addr, size, g_db.total_writes);
    return true;
}

// Flash read wrapper
static bool flash_read_sector(uint32_t addr, void* data, uint16_t size) {
    if (addr < TURSO_FLASH_BASE_ADDR || addr + size >= TURSO_FLASH_BASE_ADDR + sizeof(flash_simulation)) {
        return false;
    }
    
    memcpy(data, &flash_simulation[addr - TURSO_FLASH_BASE_ADDR], size);
    return true;
}

// Add record to sync queue for BTLE transmission
static bool add_to_sync_queue(TursoRecordType type, uint16_t record_id, 
                             TursoSyncOperation op, const void* data, uint8_t data_size) {
    if (g_db.pending_sync_count >= MAX_SYNC_QUEUE_SIZE) {
        g_last_error = TURSO_ERROR_SYNC_QUEUE_FULL;
        NRF_LOG_ERROR("Sync queue full!");
        return false;
    }
    
    TursoSyncRecord* record = &g_db.sync_queue[g_db.sync_queue_tail];
    record->timestamp_ms = get_timestamp_ms();
    record->record_id = record_id;
    record->type = type;
    record->operation = op;
    record->pending_sync = true;
    
    // Copy data with size limit for BTLE efficiency
    uint8_t copy_size = (data_size > sizeof(record->data)) ? sizeof(record->data) : data_size;
    if (data && copy_size > 0) {
        memcpy(record->data, data, copy_size);
    }
    
    record->crc16 = calculate_crc16(record->data, copy_size);
    
    g_db.sync_queue_tail = (g_db.sync_queue_tail + 1) % MAX_SYNC_QUEUE_SIZE;
    g_db.pending_sync_count++;
    
    NRF_LOG_DEBUG("Added to sync queue: type=%d, id=%d, op=%d", type, record_id, op);
    return true;
}

// Initialize Turso local database
bool turso_local_init(const char* device_id) {
    if (g_db_initialized) {
        return true;
    }
    
    // Clear database state
    memset(&g_db, 0, sizeof(g_db));
    
    // Set device ID
    if (device_id) {
        strncpy(g_db.device_id, device_id, MAX_DEVICE_ID_LENGTH - 1);
        g_db.device_id[MAX_DEVICE_ID_LENGTH - 1] = '\0';
    } else {
        strcpy(g_db.device_id, "nrf52840_001");
    }
    
    g_db.local_sequence_number = 1;
    g_db.btle_connected = false;
    g_db.low_power_mode = false;
    
    // Initialize flash simulation (in real nRF52840, check flash integrity)
    memset(flash_simulation, 0xFF, sizeof(flash_simulation));
    
    g_db_initialized = true;
    g_last_error = TURSO_OK;
    
    NRF_LOG_INFO("Turso local DB initialized for device: %s", g_db.device_id);
    return true;
}

// Shutdown database with energy-conscious cleanup
void turso_local_shutdown(void) {
    if (!g_db_initialized) {
        return;
    }
    
    // Force flush any pending writes to save data
    turso_force_flush_pending_writes();
    
    NRF_LOG_INFO("Turso local DB shutdown. Total flash writes: %d", g_db.total_writes);
    g_db_initialized = false;
}

// Save counter with batched writes for energy efficiency
bool turso_save_counter(const Counter* counter, bool force_immediate_write) {
    if (!g_db_initialized) {
        g_last_error = TURSO_ERROR_NOT_INITIALIZED;
        return false;
    }
    
    if (!counter) {
        g_last_error = TURSO_ERROR_INVALID_RECORD;
        return false;
    }
    
    // Convert to Turso record format
    TursoCounterRecord turso_counter = {0};
    turso_counter.record_id = (uint16_t)(counter - (Counter*)0); // Simple ID based on pointer offset
    turso_counter.created_at = get_timestamp_ms();
    turso_counter.updated_at = turso_counter.created_at;
    
    strncpy(turso_counter.label, counter->label, MAX_LABEL_LENGTH - 1);
    turso_counter.type = counter->type;
    turso_counter.count = counter->count;
    turso_counter.total = counter->total;
    turso_counter.max_combo = counter->max_combo;
    turso_counter.multiplier = counter->multiplier;
    turso_counter.active = counter->active;
    
    // Energy-conscious batched writing
    uint8_t counter_index = turso_counter.record_id % MAX_COUNTERS;
    g_pending_counters[counter_index] = turso_counter;
    
    if (!g_counter_dirty[counter_index]) {
        g_counter_dirty[counter_index] = true;
        g_dirty_counter_count++;
    }
    
    // Queue for BTLE sync
    add_to_sync_queue(RECORD_TYPE_COUNTER, turso_counter.record_id, 
                     SYNC_OP_UPDATE, &turso_counter, sizeof(turso_counter));
    
    // Write immediately if forced or threshold reached
    if (force_immediate_write || g_dirty_counter_count >= BATCH_WRITE_THRESHOLD) {
        turso_force_flush_pending_writes();
    }
    
    NRF_LOG_DEBUG("Counter saved (batched): %s, dirty_count=%d", 
                  counter->label, g_dirty_counter_count);
    return true;
}

// Load counter from flash
bool turso_load_counter(uint16_t counter_id, Counter* counter) {
    if (!g_db_initialized || !counter) {
        g_last_error = TURSO_ERROR_NOT_INITIALIZED;
        return false;
    }
    
    // First check if it's in pending writes
    uint8_t counter_index = counter_id % MAX_COUNTERS;
    if (g_counter_dirty[counter_index]) {
        TursoCounterRecord* turso_counter = &g_pending_counters[counter_index];
        
        strncpy(counter->label, turso_counter->label, MAX_LABEL_LENGTH);
        counter->type = turso_counter->type;
        counter->count = turso_counter->count;
        counter->total = turso_counter->total;
        counter->max_combo = turso_counter->max_combo;
        counter->multiplier = turso_counter->multiplier;
        counter->active = turso_counter->active;
        
        return true;
    }
    
    // Load from flash (simplified - in real implementation, use proper addressing)
    TursoCounterRecord turso_counter;
    uint32_t addr = TURSO_FLASH_BASE_ADDR + (counter_id * sizeof(TursoCounterRecord));
    
    if (!flash_read_sector(addr, &turso_counter, sizeof(turso_counter))) {
        g_last_error = TURSO_ERROR_RECORD_NOT_FOUND;
        return false;
    }
    
    // Convert back to Counter format
    strncpy(counter->label, turso_counter.label, MAX_LABEL_LENGTH);
    counter->type = turso_counter.type;
    counter->count = turso_counter.count;
    counter->total = turso_counter.total;
    counter->max_combo = turso_counter.max_combo;
    counter->multiplier = turso_counter.multiplier;
    counter->active = turso_counter.active;
    
    return true;
}

// Force flush pending writes (energy-conscious batch operation)
void turso_force_flush_pending_writes(void) {
    if (!g_db_initialized || g_dirty_counter_count == 0) {
        return;
    }
    
    NRF_LOG_INFO("Flushing %d pending counter writes to flash", g_dirty_counter_count);
    
    for (uint8_t i = 0; i < MAX_COUNTERS; i++) {
        if (g_counter_dirty[i]) {
            uint32_t addr = TURSO_FLASH_BASE_ADDR + (i * sizeof(TursoCounterRecord));
            flash_write_sector(addr, &g_pending_counters[i], sizeof(TursoCounterRecord));
            g_counter_dirty[i] = false;
        }
    }
    
    g_dirty_counter_count = 0;
    NRF_LOG_DEBUG("Flash write batch complete");
}

// BTLE sync operations
bool turso_queue_sync_operation(TursoRecordType type, uint16_t record_id, 
                               TursoSyncOperation op, const void* data, uint8_t data_size) {
    return add_to_sync_queue(type, record_id, op, data, data_size);
}

bool turso_get_next_sync_record(TursoSyncRecord* record) {
    if (!g_db_initialized || !record || g_db.pending_sync_count == 0) {
        return false;
    }
    
    *record = g_db.sync_queue[g_db.sync_queue_head];
    return true;
}

// Save audio configuration
bool turso_save_audio_config(const TursoAudioRecord* audio_config) {
    if (!g_db_initialized || !audio_config) {
        g_last_error = TURSO_ERROR_NOT_INITIALIZED;
        return false;
    }
    
    uint32_t addr = TURSO_FLASH_BASE_ADDR + (MAX_COUNTERS * sizeof(TursoCounterRecord));
    
    if (!flash_write_sector(addr, audio_config, sizeof(TursoAudioRecord))) {
        g_last_error = TURSO_ERROR_FLASH_WRITE_FAILED;
        return false;
    }
    
    // Queue for BTLE sync
    add_to_sync_queue(RECORD_TYPE_AUDIO_CONFIG, audio_config->record_id,
                     SYNC_OP_UPDATE, audio_config, sizeof(TursoAudioRecord));
    
    NRF_LOG_DEBUG("Audio config saved to database");
    return true;
}

// Load audio configuration
bool turso_load_audio_config(TursoAudioRecord* audio_config) {
    if (!g_db_initialized || !audio_config) {
        g_last_error = TURSO_ERROR_NOT_INITIALIZED;
        return false;
    }
    
    uint32_t addr = TURSO_FLASH_BASE_ADDR + (MAX_COUNTERS * sizeof(TursoCounterRecord));
    
    if (!flash_read_sector(addr, audio_config, sizeof(TursoAudioRecord))) {
        g_last_error = TURSO_ERROR_RECORD_NOT_FOUND;
        return false;
    }
    
    // Check if record is valid (non-zero record_id indicates saved data)
    if (audio_config->record_id == 0) {
        g_last_error = TURSO_ERROR_RECORD_NOT_FOUND;
        return false;
    }
    
    NRF_LOG_DEBUG("Audio config loaded from database");
    return true;
}

void turso_mark_sync_complete(uint16_t record_id) {
    if (!g_db_initialized || g_db.pending_sync_count == 0) {
        return;
    }
    
    // Find and mark as synced
    for (uint8_t i = 0; i < MAX_SYNC_QUEUE_SIZE; i++) {
        if (g_db.sync_queue[i].record_id == record_id && g_db.sync_queue[i].pending_sync) {
            g_db.sync_queue[i].pending_sync = false;
            
            // Move queue head if this was the next record
            if (i == g_db.sync_queue_head) {
                g_db.sync_queue_head = (g_db.sync_queue_head + 1) % MAX_SYNC_QUEUE_SIZE;
                g_db.pending_sync_count--;
            }
            
            NRF_LOG_DEBUG("Sync marked complete for record_id=%d", record_id);
            break;
        }
    }
}

uint16_t turso_get_pending_sync_count(void) {
    return g_db_initialized ? g_db.pending_sync_count : 0;
}

// Energy management
void turso_enter_low_power_mode(void) {
    if (!g_db_initialized) {
        return;
    }
    
    // Flush any pending writes before entering low power
    turso_force_flush_pending_writes();
    
    g_db.low_power_mode = true;
    NRF_LOG_INFO("Turso DB entering low power mode");
}

void turso_exit_low_power_mode(void) {
    if (!g_db_initialized) {
        return;
    }
    
    g_db.low_power_mode = false;
    NRF_LOG_INFO("Turso DB exiting low power mode");
}

// BTLE connection management
void turso_set_btle_connected(bool connected) {
    if (!g_db_initialized) {
        return;
    }
    
    bool was_connected = g_db.btle_connected;
    g_db.btle_connected = connected;
    
    if (connected && !was_connected) {
        NRF_LOG_INFO("BTLE connected - %d records pending sync", g_db.pending_sync_count);
    } else if (!connected && was_connected) {
        NRF_LOG_INFO("BTLE disconnected");
    }
}

bool turso_is_btle_connected(void) {
    return g_db_initialized ? g_db.btle_connected : false;
}

// Compact serialization for BTLE transmission
uint8_t turso_serialize_counter(const TursoCounterRecord* counter, uint8_t* buffer, uint8_t max_size) {
    if (!counter || !buffer || max_size < 32) {
        return 0;
    }
    
    // Pack essential data for BTLE efficiency
    uint8_t pos = 0;
    
    // Record ID (2 bytes)
    buffer[pos++] = (counter->record_id >> 8) & 0xFF;
    buffer[pos++] = counter->record_id & 0xFF;
    
    // Timestamp (4 bytes)
    buffer[pos++] = (counter->updated_at >> 24) & 0xFF;
    buffer[pos++] = (counter->updated_at >> 16) & 0xFF;
    buffer[pos++] = (counter->updated_at >> 8) & 0xFF;
    buffer[pos++] = counter->updated_at & 0xFF;
    
    // Label (truncated to 8 chars for BTLE efficiency)
    uint8_t label_len = strlen(counter->label);
    if (label_len > 8) label_len = 8;
    buffer[pos++] = label_len;
    memcpy(&buffer[pos], counter->label, label_len);
    pos += label_len;
    
    // Counter data (8 bytes)
    buffer[pos++] = counter->type;
    buffer[pos++] = (counter->count >> 24) & 0xFF;
    buffer[pos++] = (counter->count >> 16) & 0xFF;
    buffer[pos++] = (counter->count >> 8) & 0xFF;
    buffer[pos++] = counter->count & 0xFF;
    buffer[pos++] = (counter->total >> 16) & 0xFF;
    buffer[pos++] = (counter->total >> 8) & 0xFF;
    buffer[pos++] = counter->total & 0xFF;
    
    return pos;
}

// Add turso_shutdown function that was referenced
void turso_shutdown(void) {
    turso_local_shutdown();
}

// Database statistics
bool turso_get_database_stats(TursoDatabaseStats* stats) {
    if (!g_db_initialized || !stats) {
        return false;
    }
    
    memset(stats, 0, sizeof(TursoDatabaseStats));
    
    stats->total_records = MAX_COUNTERS; // Simplified
    stats->pending_sync_records = g_db.pending_sync_count;
    stats->total_flash_writes = g_db.total_writes;
    stats->last_sync_timestamp = g_db.last_sync_timestamp;
    stats->database_size_kb = (sizeof(flash_simulation) / 1024);
    stats->integrity_ok = true; // Simplified
    stats->btle_sync_healthy = g_db.btle_connected && (g_db.pending_sync_count < MAX_SYNC_QUEUE_SIZE / 2);
    
    return true;
}

// Error handling
TursoError turso_get_last_error(void) {
    return g_last_error;
}

const char* turso_error_string(TursoError error) {
    switch (error) {
        case TURSO_OK: return "OK";
        case TURSO_ERROR_NOT_INITIALIZED: return "Database not initialized";
        case TURSO_ERROR_FLASH_WRITE_FAILED: return "Flash write failed";
        case TURSO_ERROR_RECORD_NOT_FOUND: return "Record not found";
        case TURSO_ERROR_SYNC_QUEUE_FULL: return "Sync queue full";
        case TURSO_ERROR_INVALID_RECORD: return "Invalid record";
        case TURSO_ERROR_LOW_POWER_MODE: return "Operation not allowed in low power mode";
        case TURSO_ERROR_BTLE_DISCONNECTED: return "BTLE disconnected";
        default: return "Unknown error";
    }
}

// Database maintenance
void turso_compact_database(void) {
    if (!g_db_initialized) {
        return;
    }
    
    NRF_LOG_INFO("Database compaction started (placeholder)");
    // In real implementation: defragment flash, remove deleted records, etc.
}

uint32_t turso_get_flash_write_count(void) {
    return g_db_initialized ? g_db.total_writes : 0;
}

#ifdef DEBUG
void turso_dump_database_state(void) {
    if (!g_db_initialized) {
        printf("Turso DB not initialized\n");
        return;
    }
    
    printf("\n=== Turso Local DB State ===\n");
    printf("Device ID: %s\n", g_db.device_id);
    printf("BTLE Connected: %s\n", g_db.btle_connected ? "Yes" : "No");
    printf("Low Power Mode: %s\n", g_db.low_power_mode ? "Yes" : "No");
    printf("Pending Sync: %d records\n", g_db.pending_sync_count);
    printf("Dirty Counters: %d\n", g_dirty_counter_count);
    printf("Total Flash Writes: %d\n", g_db.total_writes);
    printf("Last Flash Write: %d ms ago\n", get_timestamp_ms() - g_db.last_flash_write_ms);
    printf("===========================\n\n");
}
#endif
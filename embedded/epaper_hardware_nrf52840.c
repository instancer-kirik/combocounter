#include "clay_epaper_renderer.h"
#include "nrf_drv_spi.h"
#include "nrf_gpio.h"
#include "nrf_delay.h"
#include "nrf_log.h"
#include <string.h>

// E-Paper command definitions for 2.15" 4-color display
#define EPAPER_CMD_PANEL_SETTING           0x00
#define EPAPER_CMD_POWER_SETTING           0x01
#define EPAPER_CMD_POWER_OFF               0x02
#define EPAPER_CMD_POWER_OFF_SEQUENCE      0x03
#define EPAPER_CMD_POWER_ON                0x04
#define EPAPER_CMD_POWER_ON_MEASURE        0x05
#define EPAPER_CMD_BOOSTER_SOFT_START      0x06
#define EPAPER_CMD_DEEP_SLEEP              0x07
#define EPAPER_CMD_DATA_START_TRANSMISSION_1  0x10
#define EPAPER_CMD_DATA_STOP               0x11
#define EPAPER_CMD_DISPLAY_REFRESH         0x12
#define EPAPER_CMD_DATA_START_TRANSMISSION_2  0x13
#define EPAPER_CMD_PLL_CONTROL             0x30
#define EPAPER_CMD_TEMPERATURE_SENSOR      0x40
#define EPAPER_CMD_TEMPERATURE_CALIBRATION 0x41
#define EPAPER_CMD_TEMPERATURE_SENSOR_WRITE 0x42
#define EPAPER_CMD_TEMPERATURE_SENSOR_READ 0x43
#define EPAPER_CMD_VCOM_AND_DATA_SETTING   0x50
#define EPAPER_CMD_TCON_SETTING            0x60
#define EPAPER_CMD_TCON_RESOLUTION         0x61
#define EPAPER_CMD_SOURCE_AND_GATE_START   0x62
#define EPAPER_CMD_GET_STATUS              0x71
#define EPAPER_CMD_AUTO_MEASURE_VCOM       0x80
#define EPAPER_CMD_VCOM_VALUE              0x81
#define EPAPER_CMD_VCM_DC_SETTING          0x82
#define EPAPER_CMD_PROGRAM_MODE            0xA0
#define EPAPER_CMD_ACTIVE_PROGRAM          0xA1
#define EPAPER_CMD_READ_OTP_DATA           0xA2

// Hardware pin definitions (should match main application)
#define EPAPER_CS_PIN       8
#define EPAPER_DC_PIN       9
#define EPAPER_RST_PIN      10
#define EPAPER_BUSY_PIN     11
#define EPAPER_SCK_PIN      3
#define EPAPER_MOSI_PIN     4

// External SPI instance (should be initialized by main application)
extern nrf_drv_spi_t g_spi;
extern bool g_spi_initialized;

// Static functions
static void epaper_send_command(uint8_t command);
static void epaper_send_data(uint8_t data);
static void epaper_send_data_buffer(const uint8_t* buffer, uint16_t length);
static void epaper_wait_busy(void);
static void epaper_reset_sequence(void);
static bool epaper_init_sequence(void);
static void epaper_set_memory_area(uint16_t x_start, uint16_t y_start, uint16_t x_end, uint16_t y_end);
static void epaper_set_memory_pointer(uint16_t x, uint16_t y);

// Hardware interface implementation
bool clay_epaper_hardware_init(void) {
    if (!g_spi_initialized) {
        NRF_LOG_ERROR("SPI not initialized");
        return false;
    }
    
    // Configure control pins
    nrf_gpio_cfg_output(EPAPER_CS_PIN);
    nrf_gpio_cfg_output(EPAPER_DC_PIN);
    nrf_gpio_cfg_output(EPAPER_RST_PIN);
    nrf_gpio_cfg_input(EPAPER_BUSY_PIN, NRF_GPIO_PIN_PULLUP);
    
    // Set initial pin states
    nrf_gpio_pin_set(EPAPER_CS_PIN);      // CS high (inactive)
    nrf_gpio_pin_clear(EPAPER_DC_PIN);    // DC low (command mode)
    nrf_gpio_pin_set(EPAPER_RST_PIN);     // RST high (not reset)
    
    // Reset and initialize the display
    epaper_reset_sequence();
    
    if (!epaper_init_sequence()) {
        NRF_LOG_ERROR("E-Paper initialization failed");
        return false;
    }
    
    NRF_LOG_INFO("E-Paper display initialized successfully");
    return true;
}

void clay_epaper_hardware_deinit(void) {
    // Put display in deep sleep
    epaper_send_command(EPAPER_CMD_DEEP_SLEEP);
    epaper_send_data(0xA5);  // Check code for deep sleep
    
    // Set pins to safe state
    nrf_gpio_pin_set(EPAPER_CS_PIN);
    nrf_gpio_pin_clear(EPAPER_DC_PIN);
    nrf_gpio_pin_clear(EPAPER_RST_PIN);
}

bool clay_epaper_hardware_update_full(const uint8_t* framebuffer) {
    if (!framebuffer) {
        NRF_LOG_ERROR("Null framebuffer");
        return false;
    }
    
    NRF_LOG_DEBUG("Starting full display update");
    
    // Set memory area to full screen
    epaper_set_memory_area(0, 0, EPAPER_WIDTH - 1, EPAPER_HEIGHT - 1);
    epaper_set_memory_pointer(0, 0);
    
    // Send black/white data (first transmission)
    epaper_send_command(EPAPER_CMD_DATA_START_TRANSMISSION_1);
    
    // Convert 2-bit framebuffer to display format
    // For 4-color display, we need to send data differently
    uint8_t bw_buffer[EPAPER_BUFFER_SIZE];
    uint8_t red_buffer[EPAPER_BUFFER_SIZE];
    
    // Split 2-bit data into black/white and red/yellow channels
    for (uint32_t i = 0; i < EPAPER_BUFFER_SIZE; i++) {
        uint8_t packed = framebuffer[i];
        uint8_t bw_byte = 0;
        uint8_t red_byte = 0;
        
        for (int bit = 0; bit < 4; bit++) {
            uint8_t pixel = (packed >> (bit * 2)) & 0x03;
            uint8_t bw_bit = 0;
            uint8_t red_bit = 0;
            
            switch (pixel) {
                case 0x00:  // Black
                    bw_bit = 0;
                    red_bit = 0;
                    break;
                case 0x01:  // White
                    bw_bit = 1;
                    red_bit = 0;
                    break;
                case 0x02:  // Red
                    bw_bit = 1;
                    red_bit = 1;
                    break;
                case 0x03:  // Yellow
                    bw_bit = 1;
                    red_bit = 1;
                    break;
            }
            
            bw_byte |= (bw_bit << bit);
            red_byte |= (red_bit << bit);
        }
        
        bw_buffer[i] = bw_byte;
        red_buffer[i] = red_byte;
    }
    
    // Send black/white data
    epaper_send_data_buffer(bw_buffer, EPAPER_BUFFER_SIZE);
    
    // Send red/yellow data (second transmission)
    epaper_send_command(EPAPER_CMD_DATA_START_TRANSMISSION_2);
    epaper_send_data_buffer(red_buffer, EPAPER_BUFFER_SIZE);
    
    // Refresh display
    epaper_send_command(EPAPER_CMD_DISPLAY_REFRESH);
    nrf_delay_ms(100);  // Give some time before checking busy
    epaper_wait_busy();
    
    NRF_LOG_DEBUG("Display update completed");
    return true;
}

bool clay_epaper_hardware_update_partial(const uint8_t* framebuffer, Clay_BoundingBox region) {
    // For simplicity, fall back to full update
    // Partial updates are more complex and display-specific
    return clay_epaper_hardware_update_full(framebuffer);
}

void clay_epaper_hardware_sleep(void) {
    epaper_send_command(EPAPER_CMD_DEEP_SLEEP);
    epaper_send_data(0xA5);  // Check code for deep sleep
    nrf_delay_ms(2);
}

void clay_epaper_hardware_wake(void) {
    // Wake up from deep sleep requires reset sequence
    epaper_reset_sequence();
    epaper_init_sequence();
}

// Static helper functions
static void epaper_send_command(uint8_t command) {
    nrf_gpio_pin_clear(EPAPER_DC_PIN);  // Command mode
    nrf_gpio_pin_clear(EPAPER_CS_PIN);  // Select device
    
    ret_code_t err_code = nrf_drv_spi_transfer(&g_spi, &command, 1, NULL, 0);
    if (err_code != NRF_SUCCESS) {
        NRF_LOG_ERROR("SPI command transfer failed: %d", err_code);
    }
    
    nrf_gpio_pin_set(EPAPER_CS_PIN);    // Deselect device
}

static void epaper_send_data(uint8_t data) {
    nrf_gpio_pin_set(EPAPER_DC_PIN);    // Data mode
    nrf_gpio_pin_clear(EPAPER_CS_PIN);  // Select device
    
    ret_code_t err_code = nrf_drv_spi_transfer(&g_spi, &data, 1, NULL, 0);
    if (err_code != NRF_SUCCESS) {
        NRF_LOG_ERROR("SPI data transfer failed: %d", err_code);
    }
    
    nrf_gpio_pin_set(EPAPER_CS_PIN);    // Deselect device
}

static void epaper_send_data_buffer(const uint8_t* buffer, uint16_t length) {
    if (!buffer || length == 0) return;
    
    nrf_gpio_pin_set(EPAPER_DC_PIN);    // Data mode
    nrf_gpio_pin_clear(EPAPER_CS_PIN);  // Select device
    
    // Send data in chunks to avoid SPI driver limitations
    const uint16_t CHUNK_SIZE = 64;
    uint16_t offset = 0;
    
    while (offset < length) {
        uint16_t chunk_len = (length - offset > CHUNK_SIZE) ? CHUNK_SIZE : (length - offset);
        
        ret_code_t err_code = nrf_drv_spi_transfer(&g_spi, &buffer[offset], chunk_len, NULL, 0);
        if (err_code != NRF_SUCCESS) {
            NRF_LOG_ERROR("SPI buffer transfer failed: %d", err_code);
            break;
        }
        
        offset += chunk_len;
    }
    
    nrf_gpio_pin_set(EPAPER_CS_PIN);    // Deselect device
}

static void epaper_wait_busy(void) {
    uint32_t timeout = 0;
    const uint32_t MAX_TIMEOUT = 5000;  // 5 seconds timeout
    
    while (nrf_gpio_pin_read(EPAPER_BUSY_PIN) == 0) {
        nrf_delay_ms(10);
        timeout += 10;
        
        if (timeout >= MAX_TIMEOUT) {
            NRF_LOG_WARNING("E-Paper busy timeout");
            break;
        }
    }
}

static void epaper_reset_sequence(void) {
    nrf_gpio_pin_set(EPAPER_RST_PIN);
    nrf_delay_ms(200);
    nrf_gpio_pin_clear(EPAPER_RST_PIN);
    nrf_delay_ms(2);
    nrf_gpio_pin_set(EPAPER_RST_PIN);
    nrf_delay_ms(200);
}

static bool epaper_init_sequence(void) {
    epaper_wait_busy();
    
    // Panel setting
    epaper_send_command(EPAPER_CMD_PANEL_SETTING);
    epaper_send_data(0x0F);  // KW-3f, KWR-2F, BWROTP-0f, BWOTP-1f
    
    // PLL control
    epaper_send_command(EPAPER_CMD_PLL_CONTROL);
    epaper_send_data(0x29);  // 3A 100HZ, 29 150Hz, 39 200HZ, 31 171HZ
    
    // Power setting
    epaper_send_command(EPAPER_CMD_POWER_SETTING);
    epaper_send_data(0x03);  // VDS_EN, VDG_EN
    epaper_send_data(0x00);  // VCOM_HV, VGHL_LV[1], VGHL_LV[0]
    epaper_send_data(0x2B);  // VDH
    epaper_send_data(0x2B);  // VDL
    epaper_send_data(0x09);  // VDHR
    
    // Booster soft start
    epaper_send_command(EPAPER_CMD_BOOSTER_SOFT_START);
    epaper_send_data(0x07);
    epaper_send_data(0x07);
    epaper_send_data(0x17);
    
    // Power on
    epaper_send_command(EPAPER_CMD_POWER_ON);
    nrf_delay_ms(100);
    epaper_wait_busy();
    
    // TCON setting
    epaper_send_command(EPAPER_CMD_TCON_SETTING);
    epaper_send_data(0x22);
    
    // Resolution setting
    epaper_send_command(EPAPER_CMD_TCON_RESOLUTION);
    epaper_send_data(EPAPER_WIDTH >> 8);    // Source width high byte
    epaper_send_data(EPAPER_WIDTH & 0xFF);  // Source width low byte
    epaper_send_data(EPAPER_HEIGHT >> 8);   // Gate height high byte
    epaper_send_data(EPAPER_HEIGHT & 0xFF); // Gate height low byte
    
    // VCOM and data interval setting
    epaper_send_command(EPAPER_CMD_VCOM_AND_DATA_SETTING);
    epaper_send_data(0x10);  // VCOM DC value
    epaper_send_data(0x00);  // Data polarity
    
    return true;
}

static void epaper_set_memory_area(uint16_t x_start, uint16_t y_start, uint16_t x_end, uint16_t y_end) {
    // This command may not be needed for all displays
    // Implementation depends on specific display controller
}

static void epaper_set_memory_pointer(uint16_t x, uint16_t y) {
    // This command may not be needed for all displays
    // Implementation depends on specific display controller
}

// Low-level interface functions called by Clay renderer
void epaper_spi_write_byte(uint8_t data) {
    if (g_spi_initialized) {
        ret_code_t err_code = nrf_drv_spi_transfer(&g_spi, &data, 1, NULL, 0);
        if (err_code != NRF_SUCCESS) {
            NRF_LOG_ERROR("SPI write byte failed: %d", err_code);
        }
    }
}

void epaper_spi_write_buffer(const uint8_t* buffer, uint16_t length) {
    epaper_send_data_buffer(buffer, length);
}

void epaper_gpio_dc_high(void) { 
    nrf_gpio_pin_set(EPAPER_DC_PIN); 
}

void epaper_gpio_dc_low(void) { 
    nrf_gpio_pin_clear(EPAPER_DC_PIN); 
}

void epaper_gpio_reset_high(void) { 
    nrf_gpio_pin_set(EPAPER_RST_PIN); 
}

void epaper_gpio_reset_low(void) { 
    nrf_gpio_pin_clear(EPAPER_RST_PIN); 
}

void epaper_gpio_cs_high(void) { 
    nrf_gpio_pin_set(EPAPER_CS_PIN); 
}

void epaper_gpio_cs_low(void) { 
    nrf_gpio_pin_clear(EPAPER_CS_PIN); 
}

bool epaper_gpio_busy_read(void) { 
    return nrf_gpio_pin_read(EPAPER_BUSY_PIN) == 0; 
}

void epaper_delay_ms(uint32_t ms) { 
    nrf_delay_ms(ms); 
}
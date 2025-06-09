/**
 * Copyright (c) 2017 - 2021, Nordic Semiconductor ASA
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice, this
 *    list of conditions and the following disclaimer in the documentation and/or
 *    other materials provided with the distribution.
 *
 * 3. Neither the name of Nordic Semiconductor ASA nor the names of its
 *    contributors may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef SDK_CONFIG_H
#define SDK_CONFIG_H

// <<< Use Configuration Wizard in Context Menu >>>

// <h> Application

// <h> nRF_Libraries

// <e> APP_BUTTON_ENABLED - app_button - buttons handling module
//==========================================================
#ifndef APP_BUTTON_ENABLED
#define APP_BUTTON_ENABLED 1
#endif

// <o> APP_BUTTON_WITH_PUSH_NO_PULL - Included push action without pull action
#ifndef APP_BUTTON_WITH_PUSH_NO_PULL
#define APP_BUTTON_WITH_PUSH_NO_PULL 0
#endif

// </e>

// <e> APP_SCHEDULER_ENABLED - app_scheduler - events scheduler
//==========================================================
#ifndef APP_SCHEDULER_ENABLED
#define APP_SCHEDULER_ENABLED 1
#endif

// <o> APP_SCHEDULER_MAX_EVENT_SIZE - Maximum size of the scheduler event data.
#ifndef APP_SCHEDULER_MAX_EVENT_SIZE
#define APP_SCHEDULER_MAX_EVENT_SIZE 32
#endif

// <o> APP_SCHEDULER_QUEUE_SIZE - Size of the scheduler queue.
#ifndef APP_SCHEDULER_QUEUE_SIZE
#define APP_SCHEDULER_QUEUE_SIZE 16
#endif

// </e>

// <e> APP_TIMER_ENABLED - app_timer - Application timer functionality
//==========================================================
#ifndef APP_TIMER_ENABLED
#define APP_TIMER_ENABLED 1
#endif

// <o> APP_TIMER_CONFIG_RTC_FREQUENCY - Configure RTC prescaler.
#ifndef APP_TIMER_CONFIG_RTC_FREQUENCY
#define APP_TIMER_CONFIG_RTC_FREQUENCY 0
#endif

// <o> APP_TIMER_CONFIG_IRQ_PRIORITY - Interrupt priority
#ifndef APP_TIMER_CONFIG_IRQ_PRIORITY
#define APP_TIMER_CONFIG_IRQ_PRIORITY 6
#endif

// <o> APP_TIMER_CONFIG_OP_QUEUE_SIZE - Capacity of timer requests queue.
#ifndef APP_TIMER_CONFIG_OP_QUEUE_SIZE
#define APP_TIMER_CONFIG_OP_QUEUE_SIZE 10
#endif

// <q> APP_TIMER_CONFIG_USE_SCHEDULER - Enable app_scheduler integration
#ifndef APP_TIMER_CONFIG_USE_SCHEDULER
#define APP_TIMER_CONFIG_USE_SCHEDULER 0
#endif

// <q> APP_TIMER_KEEPS_RTC_ACTIVE - Enable RTC always on
#ifndef APP_TIMER_KEEPS_RTC_ACTIVE
#define APP_TIMER_KEEPS_RTC_ACTIVE 0
#endif

// </e>

// <e> NRF_BALLOC_ENABLED - nrf_balloc - Block allocator module
//==========================================================
#ifndef NRF_BALLOC_ENABLED
#define NRF_BALLOC_ENABLED 1
#endif

// </e>

// <e> NRF_FPRINTF_ENABLED - nrf_fprintf - fprintf function.
//==========================================================
#ifndef NRF_FPRINTF_ENABLED
#define NRF_FPRINTF_ENABLED 1
#endif

// <q> NRF_FPRINTF_FLAG_AUTOMATIC_CR_ON_LF_ENABLED - For each printed LF, function will add CR.
#ifndef NRF_FPRINTF_FLAG_AUTOMATIC_CR_ON_LF_ENABLED
#define NRF_FPRINTF_FLAG_AUTOMATIC_CR_ON_LF_ENABLED 0
#endif

// </e>

// <e> NRF_MEMOBJ_ENABLED - nrf_memobj - Linked memory allocator module
//==========================================================
#ifndef NRF_MEMOBJ_ENABLED
#define NRF_MEMOBJ_ENABLED 1
#endif

// </e>

// <e> NRF_PWR_MGMT_ENABLED - nrf_pwr_mgmt - Power management module
//==========================================================
#ifndef NRF_PWR_MGMT_ENABLED
#define NRF_PWR_MGMT_ENABLED 1
#endif

// <o> NRF_PWR_MGMT_CONFIG_IRQ_PRIORITY - Interrupt priority
#ifndef NRF_PWR_MGMT_CONFIG_IRQ_PRIORITY
#define NRF_PWR_MGMT_CONFIG_IRQ_PRIORITY 7
#endif

// <q> NRF_PWR_MGMT_CONFIG_CPU_USAGE_MONITOR_ENABLED - Enables CPU usage monitor.
#ifndef NRF_PWR_MGMT_CONFIG_CPU_USAGE_MONITOR_ENABLED
#define NRF_PWR_MGMT_CONFIG_CPU_USAGE_MONITOR_ENABLED 0
#endif

// </e>

// <e> NRF_RINGBUF_ENABLED - nrf_ringbuf - Ring buffer
//==========================================================
#ifndef NRF_RINGBUF_ENABLED
#define NRF_RINGBUF_ENABLED 1
#endif

// </e>

// <e> NRF_SECTION_ITER_ENABLED - nrf_section_iter - Section iterator
//==========================================================
#ifndef NRF_SECTION_ITER_ENABLED
#define NRF_SECTION_ITER_ENABLED 1
#endif

// </e>

// <e> NRF_STRERROR_ENABLED - nrf_strerror - Library for converting error code to string.
//==========================================================
#ifndef NRF_STRERROR_ENABLED
#define NRF_STRERROR_ENABLED 1
#endif

// </e>

// </h>

// <h> nRF_Log

// <e> NRF_LOG_ENABLED - nrf_log - Logger
//==========================================================
#ifndef NRF_LOG_ENABLED
#define NRF_LOG_ENABLED 1
#endif

// <h> Log message pool - Configuration of log message pool

// <o> NRF_LOG_MSGPOOL_ELEMENT_SIZE - Size of a single element in the pool of memory objects.
#ifndef NRF_LOG_MSGPOOL_ELEMENT_SIZE
#define NRF_LOG_MSGPOOL_ELEMENT_SIZE 20
#endif

// <o> NRF_LOG_MSGPOOL_ELEMENT_COUNT - Number of elements in the pool of memory objects
#ifndef NRF_LOG_MSGPOOL_ELEMENT_COUNT
#define NRF_LOG_MSGPOOL_ELEMENT_COUNT 8
#endif

// </h>

// <q> NRF_LOG_ALLOW_OVERFLOW - Configures behavior when circular buffer is full.
#ifndef NRF_LOG_ALLOW_OVERFLOW
#define NRF_LOG_ALLOW_OVERFLOW 1
#endif

// <o> NRF_LOG_BUFSIZE - Size of the buffer for storing logs [bytes].
#ifndef NRF_LOG_BUFSIZE
#define NRF_LOG_BUFSIZE 1024
#endif

// <o> NRF_LOG_DEFAULT_LEVEL - Default Severity level
#ifndef NRF_LOG_DEFAULT_LEVEL
#define NRF_LOG_DEFAULT_LEVEL 3
#endif

// <e> NRF_LOG_BACKEND_RTT_ENABLED - Enable RTT backend.
//==========================================================
#ifndef NRF_LOG_BACKEND_RTT_ENABLED
#define NRF_LOG_BACKEND_RTT_ENABLED 1
#endif

// <o> NRF_LOG_BACKEND_RTT_TEMP_BUFFER_SIZE - Size of buffer for partially processed strings.
#ifndef NRF_LOG_BACKEND_RTT_TEMP_BUFFER_SIZE
#define NRF_LOG_BACKEND_RTT_TEMP_BUFFER_SIZE 64
#endif

// <o> NRF_LOG_BACKEND_RTT_TX_BUFFER_SIZE - Size of RTT TX buffer.
#ifndef NRF_LOG_BACKEND_RTT_TX_BUFFER_SIZE
#define NRF_LOG_BACKEND_RTT_TX_BUFFER_SIZE 4096
#endif

// </e>

// <e> NRF_LOG_BACKEND_UART_ENABLED - Enable UART backend.
//==========================================================
#ifndef NRF_LOG_BACKEND_UART_ENABLED
#define NRF_LOG_BACKEND_UART_ENABLED 0
#endif

// </e>

// </e>

// </h>

// <h> nRF_Drivers

// <e> GPIOTE_ENABLED - nrf_drv_gpiote - GPIOTE peripheral driver - legacy layer
//==========================================================
#ifndef GPIOTE_ENABLED
#define GPIOTE_ENABLED 1
#endif

// <o> GPIOTE_CONFIG_IRQ_PRIORITY - Interrupt priority
#ifndef GPIOTE_CONFIG_IRQ_PRIORITY
#define GPIOTE_CONFIG_IRQ_PRIORITY 6
#endif

// </e>

// <e> POWER_ENABLED - nrf_drv_power - POWER peripheral driver - legacy layer
//==========================================================
#ifndef POWER_ENABLED
#define POWER_ENABLED 1
#endif

// <o> POWER_CONFIG_IRQ_PRIORITY - Interrupt priority
#ifndef POWER_CONFIG_IRQ_PRIORITY
#define POWER_CONFIG_IRQ_PRIORITY 6
#endif

// </e>

// <e> SAADC_ENABLED - nrf_drv_saadc - SAADC peripheral driver - legacy layer
//==========================================================
#ifndef SAADC_ENABLED
#define SAADC_ENABLED 1
#endif

// <o> SAADC_CONFIG_IRQ_PRIORITY - Interrupt priority
#ifndef SAADC_CONFIG_IRQ_PRIORITY
#define SAADC_CONFIG_IRQ_PRIORITY 6
#endif

// </e>

// <e> SPI_ENABLED - nrf_drv_spi - SPI/SPIM peripheral driver - legacy layer
//==========================================================
#ifndef SPI_ENABLED
#define SPI_ENABLED 1
#endif

// <o> SPI_DEFAULT_CONFIG_IRQ_PRIORITY - Interrupt priority
#ifndef SPI_DEFAULT_CONFIG_IRQ_PRIORITY
#define SPI_DEFAULT_CONFIG_IRQ_PRIORITY 6
#endif

// <e> SPI0_ENABLED - Enable SPI0 instance
//==========================================================
#ifndef SPI0_ENABLED
#define SPI0_ENABLED 1
#endif

// </e>

// </e>

// </h>

// <h> nRFx

// <e> NRFX_CLOCK_ENABLED - nrfx_clock - CLOCK peripheral driver
//==========================================================
#ifndef NRFX_CLOCK_ENABLED
#define NRFX_CLOCK_ENABLED 1
#endif

// <o> NRFX_CLOCK_CONFIG_IRQ_PRIORITY - Interrupt priority
#ifndef NRFX_CLOCK_CONFIG_IRQ_PRIORITY
#define NRFX_CLOCK_CONFIG_IRQ_PRIORITY 6
#endif

// </e>

// <e> NRFX_GPIOTE_ENABLED - nrfx_gpiote - GPIOTE peripheral driver
//==========================================================
#ifndef NRFX_GPIOTE_ENABLED
#define NRFX_GPIOTE_ENABLED 1
#endif

// <o> NRFX_GPIOTE_CONFIG_IRQ_PRIORITY - Interrupt priority
#ifndef NRFX_GPIOTE_CONFIG_IRQ_PRIORITY
#define NRFX_GPIOTE_CONFIG_IRQ_PRIORITY 6
#endif

// </e>

// <e> NRFX_POWER_ENABLED - nrfx_power - POWER peripheral driver
//==========================================================
#ifndef NRFX_POWER_ENABLED
#define NRFX_POWER_ENABLED 1
#endif

// <o> NRFX_POWER_CONFIG_IRQ_PRIORITY - Interrupt priority
#ifndef NRFX_POWER_CONFIG_IRQ_PRIORITY
#define NRFX_POWER_CONFIG_IRQ_PRIORITY 6
#endif

// </e>

// <e> NRFX_SAADC_ENABLED - nrfx_saadc - SAADC peripheral driver
//==========================================================
#ifndef NRFX_SAADC_ENABLED
#define NRFX_SAADC_ENABLED 1
#endif

// <o> NRFX_SAADC_CONFIG_IRQ_PRIORITY - Interrupt priority
#ifndef NRFX_SAADC_CONFIG_IRQ_PRIORITY
#define NRFX_SAADC_CONFIG_IRQ_PRIORITY 6
#endif

// </e>

// <e> NRFX_SPI_ENABLED - nrfx_spi - SPI peripheral driver
//==========================================================
#ifndef NRFX_SPI_ENABLED
#define NRFX_SPI_ENABLED 1
#endif

// <o> NRFX_SPI_DEFAULT_CONFIG_IRQ_PRIORITY - Interrupt priority
#ifndef NRFX_SPI_DEFAULT_CONFIG_IRQ_PRIORITY
#define NRFX_SPI_DEFAULT_CONFIG_IRQ_PRIORITY 6
#endif

// <e> NRFX_SPI0_ENABLED - Enable SPI0 instance
//==========================================================
#ifndef NRFX_SPI0_ENABLED
#define NRFX_SPI0_ENABLED 1
#endif

// </e>

// </e>

// <e> NRFX_SPIM_ENABLED - nrfx_spim - SPIM peripheral driver
//==========================================================
#ifndef NRFX_SPIM_ENABLED
#define NRFX_SPIM_ENABLED 1
#endif

// <o> NRFX_SPIM_DEFAULT_CONFIG_IRQ_PRIORITY - Interrupt priority
#ifndef NRFX_SPIM_DEFAULT_CONFIG_IRQ_PRIORITY
#define NRFX_SPIM_DEFAULT_CONFIG_IRQ_PRIORITY 6
#endif

// <e> NRFX_SPIM0_ENABLED - Enable SPIM0 instance
//==========================================================
#ifndef NRFX_SPIM0_ENABLED
#define NRFX_SPIM0_ENABLED 1
#endif

// </e>

// </e>

// </h>

// <h> Segger RTT

// <o> SEGGER_RTT_CONFIG_BUFFER_SIZE_UP - Size of upstream buffer.
#ifndef SEGGER_RTT_CONFIG_BUFFER_SIZE_UP
#define SEGGER_RTT_CONFIG_BUFFER_SIZE_UP 4096
#endif

// <o> SEGGER_RTT_CONFIG_MAX_NUM_UP_BUFFERS - Maximum number of upstream buffers.
#ifndef SEGGER_RTT_CONFIG_MAX_NUM_UP_BUFFERS
#define SEGGER_RTT_CONFIG_MAX_NUM_UP_BUFFERS 2
#endif

// <o> SEGGER_RTT_CONFIG_BUFFER_SIZE_DOWN - Size of downstream buffer.
#ifndef SEGGER_RTT_CONFIG_BUFFER_SIZE_DOWN
#define SEGGER_RTT_CONFIG_BUFFER_SIZE_DOWN 16
#endif

// <o> SEGGER_RTT_CONFIG_MAX_NUM_DOWN_BUFFERS - Maximum number of downstream buffers.
#ifndef SEGGER_RTT_CONFIG_MAX_NUM_DOWN_BUFFERS
#define SEGGER_RTT_CONFIG_MAX_NUM_DOWN_BUFFERS 2
#endif

// </h>

// <<< end of configuration section >>>
#endif //SDK_CONFIG_H
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "GPS.h"
#include "GPS_parser.h"
#include "esp_timer.h"
#include "freertos/portmacro.h"
#include "sys/time.h"
#include "driver/gpio.h"

static const char *TAG = "GPS";

gps_time_sync_t gps_time = {0};
gps_display_data_t gps_display_data = {0};
static portMUX_TYPE gps_data_lock = portMUX_INITIALIZER_UNLOCKED;

typedef struct
{
    volatile uint64_t last_pps_local_us;
    volatile bool new_pps;
    volatile uint32_t pps_count;
} pps_state_t;

static pps_state_t g_pps = {0};

void gps_get_display_data(gps_display_data_t *out)
{
    if (!out)
    {
        return;
    }

    portENTER_CRITICAL(&gps_data_lock);
    *out = gps_display_data;
    portEXIT_CRITICAL(&gps_data_lock);
}

static void IRAM_ATTR pps_isr_handler(void *arg) //records the exact ESP32 timer value the moment the PPS signal edge goes high
{
    g_pps.last_pps_local_us = (uint64_t)esp_timer_get_time();
    g_pps.new_pps = true;
    g_pps.pps_count++;
}

static void gps_uart_init(void)
{
    const uart_config_t uart_config = {
        .baud_rate = 9600,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    };

    uart_driver_install(GPS_UART, GPS_BUF_SIZE * 2, 0, 0, NULL, 0);
    uart_param_config(GPS_UART, &uart_config);
    uart_set_pin(GPS_UART, GPS_TX_PIN, GPS_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

    vTaskDelay(pdMS_TO_TICKS(1000));
    const char *pps_enable_cmd = "$PMTK255,1*2D\r\n";
    uart_write_bytes(GPS_UART, pps_enable_cmd, strlen(pps_enable_cmd));
}

void pps_gpio_init(gpio_num_t pps_gpio)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << pps_gpio),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_POSEDGE};
    gpio_config(&io_conf);

    gpio_install_isr_service(0); //enables ESP32 GPIO interrupt system
    gpio_isr_handler_add(pps_gpio, pps_isr_handler, NULL);  //connects PPS pin to interrupt_isr handler routine
}

void gps_time_update_from_pps(uint64_t utc_second_us, uint64_t local_pps_us)
{
    portENTER_CRITICAL(&gps_data_lock);
    gps_time.valid = true;
    gps_time.utc_sync_us = utc_second_us;
    gps_time.local_sync_us = local_pps_us;
    portEXIT_CRITICAL(&gps_data_lock);
}
void gps_factory_reset(void)
{
    const char *reset_cmd = "$PMTK104*37\r\n";
    uart_write_bytes(GPS_UART, reset_cmd, strlen(reset_cmd));
}

bool gps_get_sync_snapshot(bool *valid, uint64_t *utc_sync_us, uint64_t *local_sync_us)
{
    if (!valid || !utc_sync_us || !local_sync_us) {
        return false;
    }

    portENTER_CRITICAL(&gps_data_lock);
    *valid = gps_time.valid;
    *utc_sync_us = gps_time.utc_sync_us;
    *local_sync_us = gps_time.local_sync_us;
    portEXIT_CRITICAL(&gps_data_lock);

    return true;
}

bool utc_ms_to_parts(int64_t utc_ms, sample_t *out){
    if (!out) {
        return false;
    }

    time_t seconds = utc_ms / 1000;
    struct tm tm_time;

    gmtime_r(&seconds, &tm_time);

    out->year = tm_time.tm_year + 1900;
    out->month = tm_time.tm_mon + 1;
    out->day = tm_time.tm_mday;
    out->hour = tm_time.tm_hour;
    out->minute = tm_time.tm_min;
    out->second = tm_time.tm_sec;
    out->millisecond = utc_ms % 1000;

    return true;
}

void gps_task(void *arg)
{
    uint8_t rx_buf[128];
    char line[128];
    int idx = 0;
    bool in_sentence = false;
    uint64_t sentence_start_us = 0;
    nmea_data_t nmea_data; //for storing the parsed GPS data from the NMEA sentences

    gps_uart_init();
    pps_gpio_init(GPS_PPS_PIN);


    while (1)
    {
        int len = uart_read_bytes(GPS_UART, rx_buf, sizeof(rx_buf), pdMS_TO_TICKS(100));

        if (len <= 0)   //empty rx buffer
        {
            continue;
        }

        for (int i = 0; i < len; i++) //parsing byte by byte thtrough NMEA sentence
        {
            char c = (char)rx_buf[i]; //cast uint8_t to char
            if (c == '$') //start of new NMEA sentence
            {
                idx = 0;
                in_sentence = true;
                sentence_start_us = (uint64_t)esp_timer_get_time();
                line[idx++] = c;
                continue;
            }
            if (!in_sentence)
            {
                continue;
            }
            if (c == '\n') //end of NMEA sentence
            {
                if (idx > 0 && line[idx - 1] == '\r')
                {
                    idx--;
                }
                line[idx] = '\0';
                memset(&nmea_data, 0, sizeof(nmea_data));
                if (nmea_parse(line, &nmea_data)) //converts the NMEA sentence into structured data in the nmea_data_t struct
                {
                    if (nmea_data.rmc.valid)
                    {
                        uint64_t parsed_utc_us = 0;

                        if (rmc_datetime_to_epoch_us(nmea_data.rmc.date, nmea_data.rmc.time, &parsed_utc_us))
                        { 
                            // Atomically snapshot and clear the PPS state
                            portENTER_CRITICAL(&gps_data_lock);
                            uint64_t last_pps_us = g_pps.last_pps_local_us;
                            g_pps.new_pps = false;
                            portEXIT_CRITICAL(&gps_data_lock);

                            // PPS belongs to this second if it fired within 600ms before the sentence arrived.
                            // This handles the case where NMEA is processed before the PPS flag is set.
                            bool pps_recent = (sentence_start_us >= last_pps_us) &&
                                            (sentence_start_us - last_pps_us < 900000ULL);

                            if (pps_recent)
                            {
                                gps_time_update_from_pps(parsed_utc_us, last_pps_us);
                                ESP_LOGI(TAG, "PPS sync established: %s %s", nmea_data.rmc.date, nmea_data.rmc.time);
                            }
                            else
                            {
                                portENTER_CRITICAL(&gps_data_lock);
                                if (gps_time.valid) {
                                    uint64_t sub_us = (gps_time.utc_sync_us + (sentence_start_us - gps_time.local_sync_us)) % 1000000ULL;
                                    gps_time.utc_sync_us = (parsed_utc_us / 1000000ULL) * 1000000ULL + sub_us;
                                } else {
                                    gps_time.utc_sync_us = parsed_utc_us;
                                }
                                gps_time.valid = true;
                                gps_time.local_sync_us = sentence_start_us;
                                portEXIT_CRITICAL(&gps_data_lock);

                                ESP_LOGW(TAG, "GPS time updated from NMEA only");
                            }

                            /*
                            portENTER_CRITICAL(&gps_data_lock);
                            gps_display_data.valid = true;
                            gps_display_data.latitude = nmea_data.rmc.lat;
                            gps_display_data.longitude = nmea_data.rmc.lon;

                            strncpy(gps_display_data.utc_time,
                                    nmea_data.rmc.time,
                                    sizeof(gps_display_data.utc_time) - 1);
                            gps_display_data.utc_time[sizeof(gps_display_data.utc_time) - 1] = '\0';
                            portEXIT_CRITICAL(&gps_data_lock);

                            if (g_pps.new_pps) //using PPS sychronization
                            {
                                portENTER_CRITICAL(&gps_data_lock);   // ← disable interrupts
                                bool pps_fired = g_pps.new_pps;       // ← snapshot the flag
                                uint64_t local_pps_us = g_pps.last_pps_local_us;  // ← snapshot the timestamp
                                g_pps.new_pps = false;                // ← clear the flag
                                portEXIT_CRITICAL(&gps_data_lock);    // ← re-enable interrupts
                                if (pps_fired)                        // ← now check the snapshot, not the live variable
                                {
                                    uint64_t pps_utc_us = parsed_utc_us;
                                    gps_time_update_from_pps(pps_utc_us, local_pps_us);
                                    ESP_LOGI(TAG, "PPS sync established: %s %s", nmea_data.rmc.date, nmea_data.rmc.time);
                                }
                            }
                            else //fallback to NMEA syncronization
                            {
                                portENTER_CRITICAL(&gps_data_lock);
                                if (gps_time.valid) {
                                    // Use GPS for the seconds field, but preserve the sub-second
                                    // offset from the current interpolation to avoid backward jumps.
                                    // The NMEA sentence always reports 0ms (integer seconds), but
                                    // arrives hundreds of ms after the actual second boundary.
                                    uint64_t sub_us = (gps_time.utc_sync_us + (sentence_start_us - gps_time.local_sync_us)) % 1000000ULL;
                                    gps_time.utc_sync_us = (parsed_utc_us / 1000000ULL) * 1000000ULL + sub_us;
                                } else {
                                    gps_time.utc_sync_us = parsed_utc_us;
                                }
                                gps_time.valid = true;
                                gps_time.local_sync_us = sentence_start_us;
                                portEXIT_CRITICAL(&gps_data_lock);

                                ESP_LOGW(TAG, "GPS time updated from NMEA only");
                            } */
                        }
                    }
                }
                idx = 0;
                in_sentence = false;
                continue;
            }
            if (idx < sizeof(line) - 1) {
                line[idx++] = c;
            } else {
                idx = 0;
                in_sentence = false;
            }
        }
    }
}

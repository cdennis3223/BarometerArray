#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "GPS.h"
#include "GPS_parser.h"
#include "esp_timer.h"
#include "freertos/portmacro.h"
#include "driver/gpio.h"

static const char *TAG = "GPS";

gps_time_sync_t gps_time = {0};
gps_display_data_t gps_display_data = {0};
static portMUX_TYPE gps_data_lock = portMUX_INITIALIZER_UNLOCKED;

typedef struct
{
    volatile int64_t last_pps_local_us;
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

static void IRAM_ATTR pps_isr_handler(void *arg)
{
    g_pps.last_pps_local_us = esp_timer_get_time();
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
}

void pps_gpio_init(gpio_num_t pps_gpio)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << pps_gpio),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_POSEDGE};
    gpio_config(&io_conf);

    gpio_install_isr_service(0);
    gpio_isr_handler_add(pps_gpio, pps_isr_handler, NULL);
}

void gps_time_update_from_pps(uint64_t utc_second_us, int64_t local_pps_us)
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

bool gps_get_sync_snapshot(bool *valid, uint64_t *utc_sync_us, int64_t *local_sync_us)
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

void gps_task(void *arg)
{
    uint8_t rx_buf[128];
    char line[128];
    int idx = 0;
    bool in_sentence = false;
    nmea_data_t data;

    gps_uart_init();

    while (1)
    {
        int len = uart_read_bytes(GPS_UART, rx_buf, sizeof(rx_buf), pdMS_TO_TICKS(100));

        if (len <= 0)
        {
            continue;
        }

        for (int i = 0; i < len; i++)
        {
            char c = (char)rx_buf[i];

            if (c == '$')
            {
                idx = 0;
                in_sentence = true;
                line[idx++] = c;
                continue;
            }

            if (!in_sentence)
            {
                continue;
            }

            if (c == '\n')
            {
                if (idx > 0 && line[idx - 1] == '\r')
                {
                    idx--;
                }

                line[idx] = '\0';

                memset(&data, 0, sizeof(data));

                if (nmea_parse(line, &data))
                {
                    if (data.rmc.valid)
                    {
                        uint64_t parsed_utc_us = 0;

                        if (rmc_datetime_to_epoch_us(data.rmc.date, data.rmc.time, &parsed_utc_us))
                        {

                            portENTER_CRITICAL(&gps_data_lock);
                            gps_display_data.valid = true;
                            gps_display_data.latitude = data.rmc.lat;
                            gps_display_data.longitude = data.rmc.lon;

                            strncpy(gps_display_data.utc_time,
                                    data.rmc.time,
                                    sizeof(gps_display_data.utc_time) - 1);
                            gps_display_data.utc_time[sizeof(gps_display_data.utc_time) - 1] = '\0';
                            portEXIT_CRITICAL(&gps_data_lock);

                            if (g_pps.new_pps)
                            {
                                int64_t local_pps_us = g_pps.last_pps_local_us;
                                g_pps.new_pps = false;

                                // Start with this assumption.
                                // If your timestamps end up exactly 1 second off, change to:
                                // uint64_t pps_utc_us = parsed_utc_us + 1000000ULL;
                                uint64_t pps_utc_us = parsed_utc_us;

                                gps_time_update_from_pps(pps_utc_us, local_pps_us);

                                ESP_LOGI(TAG, "PPS sync established: %s %s",
                                         data.rmc.date, data.rmc.time);
                            }
                            else
                            {
                                portENTER_CRITICAL(&gps_data_lock);
                                gps_time.valid = true;
                                gps_time.utc_sync_us = parsed_utc_us;
                                gps_time.local_sync_us = esp_timer_get_time();
                                portEXIT_CRITICAL(&gps_data_lock);

                                ESP_LOGW(TAG, "GPS time updated from NMEA only");
                            }
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

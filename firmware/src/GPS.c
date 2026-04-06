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

static const char *TAG = "GPS";

gps_time_sync_t gps_time = {0};

void gps_uart_init(void)
{
    const uart_config_t uart_config = {
        .baud_rate = 9600,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    };

    uart_driver_install(GPS_UART, GPS_BUF_SIZE * 2, 0, 0, NULL, 0);
    uart_param_config(GPS_UART, &uart_config);
    uart_set_pin(GPS_UART, GPS_TX_PIN, GPS_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
}

void gps_factory_reset(void)
{
    const char *reset_cmd = "$PMTK104*37\r\n";
    uart_write_bytes(GPS_UART, reset_cmd, strlen(reset_cmd));
}



void gps_task(void *arg)
{
    uint8_t rx_buf[128];
    char line[128];
    int idx = 0;
    bool in_sentence = false;
    nmea_data_t data;

    while (1) {
        int len = uart_read_bytes(GPS_UART, rx_buf, sizeof(rx_buf), pdMS_TO_TICKS(100));

        if (len <= 0) {
            continue;
        }

        for (int i = 0; i < len; i++) {
            char c = (char)rx_buf[i];

            if (c == '$') {
                idx = 0;
                in_sentence = true;
                line[idx++] = c;
                continue;
            }

            if (!in_sentence) {
                continue;
            }

            if (c == '\n') {
                if (idx > 0 && line[idx - 1] == '\r') {
                    idx--;
                }

                line[idx] = '\0';

                memset(&data, 0, sizeof(data));

                // Optional raw debug
                // ESP_LOGI(TAG, "%s", line);

                /*
                if (nmea_parse(line, &data)) {
                    if (data.rmc.valid) {
                        printf("RMC: %s %s (%u sec)\n",
                               data.rmc.date,
                               data.rmc.time,
                               (unsigned)rmc_time_to_seconds(data.rmc.time));
                    }
                }
                */

                if (nmea_parse(line, &data)) {
                    if (data.rmc.valid) {
                        uint32_t sec = rmc_time_to_seconds(data.rmc.time);

                        gps_time.utc_sync_us = (uint64_t)sec * 1000000ULL;
                        gps_time.local_sync_us = esp_timer_get_time();
                        gps_time.valid = true;

                        //printf("RMC: %s %s (%u sec)\n",
                            //data.rmc.date,
                            //data.rmc.time,
                            //(unsigned)sec);
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
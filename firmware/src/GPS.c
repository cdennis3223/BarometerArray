#include <stdio.h>
#include <string.h>
#include "driver/uart.h"
#include "esp_log.h"
#include "GPS.h"



static const char *TAG = "GPS";





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


void gps_factory_reset()
{
    const char *reset_cmd = "$PMTK104*37\r\n";
    uart_write_bytes(GPS_UART, reset_cmd, strlen(reset_cmd));
}


void gps_task(void *arg)
{
    uint8_t data[128];
    char line[256];
    int idx = 0;
    bool in_sentence = false;

    while (1) {
        int len = uart_read_bytes(GPS_UART, data, sizeof(data), pdMS_TO_TICKS(100));

        for (int i = 0; i < len; i++) {
            char c = data[i];

            // Start of a new NMEA sentence
            if (c == '$') {
                idx = 0;
                in_sentence = true;
                line[idx++] = c;
                continue;
            }

            if (!in_sentence) {
                continue;  // ignore everything until '$'
            }

            if (c == '\n') {
                if (idx > 0 && line[idx - 1] == '\r') idx--;
                line[idx] = '\0';

                ESP_LOGI(TAG, "%s", line);

                idx = 0;
                in_sentence = false;
                continue;
            }

            if (idx < sizeof(line) - 1) {
                line[idx++] = c;
            } else {
                // overflow → abandon this sentence
                idx = 0;
                in_sentence = false;
            }
        }
    }
}


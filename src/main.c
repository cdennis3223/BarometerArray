/*This program reads NMEA sentences from the GPS module over UART
and prints them out to the serial port */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "esp_log.h"

#define GPS_UART        UART_NUM_2
#define GPS_TX_PIN      17      //this is TX2 on the devkit
#define GPS_RX_PIN      16      //this is RX2 on the devkit
#define GPS_BUF_SIZE    1024

static const char *TAG = "GPS";

static void gps_uart_init(void)
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

static void gps_task(void *arg){



    
    uint8_t data[128];
    char line[256];
    int idx = 0;
    while (1) {
        //printf("Hello printf\n");
        //ESP_LOGI(TAG, "Hello ESP_LOGI");
        //vTaskDelay(pdMS_TO_TICKS(1000));

        int len = uart_read_bytes(GPS_UART, data, sizeof(data), pdMS_TO_TICKS(100));

        for (int i = 0; i < len; i++) {
            char c = data[i];
            if (c == '\n') {
                // remove trailing carriage return
                if (idx > 0 && line[idx-1] == '\r') idx--;
                line[idx] = '\0';
                ESP_LOGI(TAG, "%s", line);
                idx = 0;
                }
            else if (idx < sizeof(line) - 1) {
                line[idx++] = c;
                }
            else {
                // buffer full, reset
                idx = 0;
            }
        }
    } 
}


void app_main() {
    /*
    uart_config_t uart_config = {
        .baud_rate = 9600, // <-- If gibberish, change this to 115200
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    uart_driver_install(UART_NUM_2, 1024, 0, 0, NULL, 0);
    uart_param_config(UART_NUM_2, &uart_config);
    uart_set_pin(UART_NUM_2, 17, 16, -1, -1);

    uint8_t byte;
    while (1) {
        // Read 1 byte at a time and print it immediately
        if (uart_read_bytes(UART_NUM_2, &byte, 1, portMAX_DELAY) > 0) {
            putchar(byte); 
            fflush(stdout); 
        }
    }
    */


    gps_uart_init();
    xTaskCreate(gps_task, "gps_task", 4096, NULL, 5, NULL);

}
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "GPS.h"
#include "DPS.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include <inttypes.h>
#include "collate.h"



static const char *TAG = "MAIN";

static spi_device_handle_t spi_handle;
static dps368_t dps;
static ring_buffer_t pressure_buffer;




void app_main(void) {
    //spi_device_handle_t spi_handle;
    //dps368_t dps;
    //ring_buffer_t pressure_buffer;

    ESP_LOGI(TAG, "before spi_bus_init");
    spi_bus_init(&spi_handle);

    ESP_LOGI(TAG, "before dps368_init");
    dps368_init(&dps, spi_handle);

    ESP_LOGI(TAG, "before ring_buffer_init");
    ring_buffer_init(&pressure_buffer);

    // sample every 100 ms = 10 Hz
    ESP_LOGI(TAG, "before collate_start_task");
    collate_start_task(&pressure_buffer, &dps, 100);

    ESP_LOGI(TAG, "after collate_start_task");

    while (1) {
        sample_t s;

        while (ring_buffer_pop(&pressure_buffer, &s)) {
            ESP_LOGI(TAG,
                    "P = %.2f Pa, T = %.2f C, t = %lu ms",
                    s.pressure,
                    s.temperature,
                    (unsigned long)s.timestamp_ms);
        }

        ESP_LOGI(TAG, "------------------------");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}


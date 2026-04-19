#include <stdio.h>
#include <string.h>
#include <inttypes.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_log.h"

#include "GPS.h"
#include "DPS.h"
#include "collate.h"
#include "screen.h"
#include "SD.h"
#include "BatMGMT.h"
#include "logger.h"
#include "GlobalWatch.h"

static const char *TAG = "MAIN";


static spi_device_handle_t spi_handle;
static dps368_t dps;
static ring_buffer_t pressure_buffer;
volatile bool shutdown_requested = false;
volatile bool logger_done = false;


void app_main(void)
{
    
    esp_log_level_set("*", ESP_LOG_INFO);

    BatMGMT_init();
    sd_init();

    ring_buffer_init(&pressure_buffer);

    if (spi_bus_init(&spi_handle) != ESP_OK) {
        printf("SPI init failed\n");
        return;
    }

    dps368_init(&dps, spi_handle);

    sd_log_open("log.csv");

    // =====task creation==================================
    collate_start_task(&pressure_buffer, &dps, 33);

    xTaskCreate(logger_task, "logger_task", 4096, &pressure_buffer, 4, NULL);

    xTaskCreate(gps_task, "gps_task", 4096, NULL, 5,NULL);

    xTaskCreate(button_task, "button", 4096, &pressure_buffer, 1, NULL);

}

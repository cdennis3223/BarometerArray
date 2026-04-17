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

static const char *TAG = "MAIN";


static spi_device_handle_t spi_handle;
static dps368_t dps;
static ring_buffer_t pressure_buffer;


void app_main(void)
{
    gps_uart_init();
    BatMGMT_init();
    sd_init();

    ring_buffer_init(&pressure_buffer);

    if (spi_bus_init(&spi_handle) != ESP_OK) {
        printf("SPI init failed\n");
        return;
    }

    dps368_init(&dps, spi_handle);

    sd_log_open("log.csv");

    collate_start_task(&pressure_buffer, &dps, 33);
    logger_start_task(&pressure_buffer);
    esp_err_t ret;

    xTaskCreate(button_task, "button", 4096, NULL, 1, NULL);

   /*
    xTaskCreate(
        gps_task,
        "gps_task",
        4096,
        NULL,
        5,
        NULL
    );*/
    
    
}


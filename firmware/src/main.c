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

static const char *TAG = "MAIN";

static spi_device_handle_t spi_handle;
static dps368_t dps;
static ring_buffer_t pressure_buffer;

void app_main(void)
{
    printf("HELLO FROM APP_MAIN\n");

    gps_uart_init();

    ring_buffer_init(&pressure_buffer);

    if (spi_bus_init(&spi_handle) != ESP_OK) {
        printf("SPI init failed\n");
        return;
    }

    dps368_init(&dps, spi_handle);

    collate_start_task(&pressure_buffer, &dps, 33);

    xTaskCreate(
        gps_task,
        "gps_task",
        4096,
        NULL,
        5,
        NULL
    );
}


/*

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
#include "screen.h"



static const char *TAG = "MAIN";
static spi_device_handle_t spi_handle;
static dps368_t dps;
static ring_buffer_t pressure_buffer;


void app_main(void) {
    
    
    //button_init();
    //sh1107_init();

    gps_uart_init();
    //vTaskDelay(pdMS_TO_TICKS(1000));
    //gps_factory_reset();
    printf("HELLO FROM APP_MAIN\n");
    



    spi_device_handle_t spi_handle;

    // 1. init ring buffer
    ring_buffer_init(&rb);

    // 2. init SPI bus and get device handle
    if (spi_bus_init(&spi_handle) != ESP_OK) {
        printf("SPI init failed\n");
        return;
    }


    // 3. init DPS368 struct
    dps368_init(&dps, spi_handle);

     // 4. start collate task at about 30 Hz
    collate_start_task(&rb, &dps, 33);

        xTaskCreate(
        gps_task,
        "gps_task",
        4096,
        NULL,          // task argument
        5,             // priority
        NULL
    );

    TaskCreate(
        button_task,   // task function
        "button_task", // name (for debugging)
        4096,          // stack size (bytes)
        NULL,          // task argument
        5,             // priority
        NULL           // task handle (optional)
    );
}
*/
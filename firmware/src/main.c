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
    
    button_init();
    sh1107_init();

    xTaskCreate(
        button_task,      // task function
        "button_task",    // name (for debugging)
        4096,             // stack size (bytes)
        NULL,             // task argument
        5,                // priority
        NULL              // task handle (optional)
    );


}


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

static const char *TAG = "MAIN";


//static spi_device_handle_t spi_handle;
//static dps368_t dps;
//static ring_buffer_t pressure_buffer;


void app_main(void)
{
    vTaskDelay(pdMS_TO_TICKS(3000));
    printf("HELLO FROM APP_MAIN\n");


    /*
    gps_uart_init();

    ring_buffer_init(&pressure_buffer);

    if (spi_bus_init(&spi_handle) != ESP_OK) {
        printf("SPI init failed\n");
        return;
    }

    dps368_init(&dps, spi_handle);

    collate_start_task(&pressure_buffer, &dps, 33);
    


    esp_err_t ret;

    ret = sd_init();
    printf("sd_init returned: %s\n", esp_err_to_name(ret));
    if (ret != ESP_OK) {
        printf("SD init failed\n");
        return;
    }

    ret = sd_log_open("Wassup.csv");
    printf("sd_log_open returned: %s\n", esp_err_to_name(ret));
    if (ret != ESP_OK) {
        printf("SD log open failed\n");
        return;
    }

    ret = sd_log_sample(420.69f, 666);
    printf("sd_log_sample returned: %s\n", esp_err_to_name(ret));
    */

    //sh1107_init();
    xTaskCreate(button_task, "button", 4096, NULL, 5, NULL);

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


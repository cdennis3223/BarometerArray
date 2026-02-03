/*This program reads NMEA sentences from the GPS module over UART
and prints them out to the serial port */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "GPS.h"


void app_main() {
    
    
    gps_uart_init();
    vTaskDelay(pdMS_TO_TICKS(500));    
    gps_factory_reset();

    xTaskCreate(gps_task, "gps_task", 4096, NULL, 5, NULL);

}
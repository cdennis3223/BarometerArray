/*This program reads NMEA sentences from the GPS module over UART
and prints them out to the serial port */

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






void app_main() {

    
    
    // 1. Declare the local handle
    spi_device_handle_t dev_sensor1 = NULL;
    uint8_t buf[1] = {0};

    // 2. Pass the ADDRESS (&) so spi_bus_init can set dev_sensor1
    esp_err_t ret = spi_bus_init(&dev_sensor1);
    
    if (ret == ESP_OK && dev_sensor1 != NULL) {
        // 3. Now dev_sensor1 is valid and can be used
        dps368_read(dev_sensor1, 0x0D, buf, 1);
        ESP_LOGI("MAIN", "Sensor ID: 0x%02X", buf[0]);
    } else {
        ESP_LOGE("MAIN", "Failed to initialize SPI sensor!");
    }
    

    while(1){
        vTaskDelay(pdMS_TO_TICKS(500));
        printf("\n\n--- SERIAL TEST START ---\n\n");
        dps368_read(dev_sensor1, 0x0D, buf, 1);
        printf("Senso2 PROD/REV: 0x%02x/n", buf[0]);
    }
   

}


    /*
    


    
    gps_uart_init();
      
    gps_factory_reset();

    xTaskCreate(gps_task, "gps_task", 4096, NULL, 5, NULL);
    */

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


void app_main(void) {

    //1. SPI Setup
    spi_device_handle_t spi_handle_1;
     if (spi_bus_init(&spi_handle_1) != ESP_OK) {
        ESP_LOGE("MAIN", "SPI Init Failed");
        return;
    }
    
    //2. DPS368 sensor setup
    dps368_t pressure_sensor_1;
    dps368_init(&pressure_sensor_1, spi_handle_1);



    while(1) {

        // 1. Force a "Wake up" by reading a random register twice
        //uint8_t dummy;
        //dps368_read(pressure_sensor_1.spi, 0x0D, &dummy, 1); 
        //vTaskDelay(pdMS_TO_TICKS(500));
        
        // 2. Now try the real ID read
        //uint8_t id = 0;
        //dps368_read(dev_sensor1, 0x0D, &id, 1);
        //printf("Device ID: 0x%02X\n", id);

        
        
        // This prints the raw decimal value
        // To get hPa (e.g. 1013.25), you need the calibration coefficients
        //printf("Raw Pressure Value:%" PRId32 "\n", raw_p);
        //printf("Raw Temperature Value:%" PRId32"\n", raw_t);

        float corrected_p = corrected_pressure(&pressure_sensor_1);
        printf("Corrected Pressure Value: %d Pa\n", (int)corrected_p);

        int corrected_t = corrected_temperature(&pressure_sensor_1);
        printf("Corrected Temperature Value: %d °C\n", corrected_t);

        vTaskDelay(pdMS_TO_TICKS(800));

        
    
    // Will need to implement a shutdown procedure eventually
    //Close the file and deinitialize SD Card and turn off sensors etc.
    }


}
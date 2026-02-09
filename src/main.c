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


void app_main(void) {
    spi_device_handle_t dev_sensor1 = NULL;
    //uint8_t buf[1];

    // 1. Setup SPI
    if (spi_bus_init(&dev_sensor1) != ESP_OK) {
        ESP_LOGE("MAIN", "SPI Init Failed");
        return;
    }

    // 2. Configure Sensor (Set to Background Mode, High Precision)
    // Register 0x08 (MEAS_CFG): Set bits to enable pressure and temperature
    //uint8_t config_val = 0x07; // Continuous measurement mode
    // Note: You'll need a dps368_write function similar to your read function
    // For now, let's assume it's configured or using default one-shot.

    // Register 0x08 is MEAS_CFG. 
    // Setting it to 0x07 starts continuous pressure and temperature measurement.
    dps368_write(dev_sensor1, 0x08, 0x07);


    //spi_device_handle_t dev_sensor1 = NULL;
    //spi_bus_init(&dev_sensor1);

    


    while(1) {

        // 1. Force a "Wake up" by reading a random register twice
        uint8_t dummy;
        dps368_read(dev_sensor1, 0x0D, &dummy, 1); 
        vTaskDelay(pdMS_TO_TICKS(100));
        
        // 2. Now try the real ID read
        uint8_t id = 0;
        dps368_read(dev_sensor1, 0x0D, &id, 1);
        printf("Device ID: 0x%02X\n", id);


        /*
        uint8_t id = 0;
        dps368_read(dev_sensor1, 0x0D, &id, 1);
        printf("Device ID: 0x%02X\n", id);


        float raw_p = dps368_get_raw_pressure(dev_sensor1);
        
        // This prints the raw decimal value
        // To get hPa (e.g. 1013.25), you need the calibration coefficients
        printf("Raw Pressure Value: %.2f\n", raw_p);

        vTaskDelay(pdMS_TO_TICKS(500));
        */
    }
}
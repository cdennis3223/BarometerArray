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
    spi_device_handle_t dev_sensor1 = NULL;
    //uint8_t buf[1];

    // 1. Setup SPI
    if (spi_bus_init(&dev_sensor1) != ESP_OK) {
        ESP_LOGE("MAIN", "SPI Init Failed");
        return;
    }

    vTaskDelay(pdMS_TO_TICKS(3000)); //Give time to enter monitor

    // 2. Configure Sensor (Set to Background Mode, High Precision)
    // Register 0x08 (MEAS_CFG): Set bits to enable pressure and temperature
    //uint8_t config_val = 0x07; // Continuous measurement mode
    // Note: You'll need a dps368_write function similar to your read function
    // For now, let's assume it's configured or using default one-shot.

    //Configure pressure and temperature measurement OSR(OverSampling Rate)
    dps368_write(dev_sensor1, 0x06, 0x14); // Set pressure OSR to 16
    dps368_write(dev_sensor1, 0x07, 0x94); // Set temperature OSR to 16


    // Register 0x08 is MEAS_CFG. 
    // Setting it to 0x07 starts continuous pressure and temperature measurement.
    dps368_write(dev_sensor1, 0x08, 0x07);
    dps368_write(dev_sensor1, 0x09, 0x0C); // Set CFG_REG to allow pressure/temp shift

    

    vTaskDelay(pdMS_TO_TICKS(100)); // Wait for sensor to stabilize

    dps368_read(dev_sensor1, 0x28, &v, 1); ESP_LOGI("CFG","INT_STS  0x28 = 0x%02X", v);
    //bit 7 of 0x28 is a 1 therefore we are using external temp_sensor

    // 3. Read Calibration Coefficients there are 8 coefficients, c0 and c1 are 12 bits, the rest are 16 bits
    int32_t coeffs[9];
    dps368_get_coeff(dev_sensor1, coeffs);

    //spi_device_handle_t dev_sensor1 = NULL;
    //spi_bus_init(&dev_sensor1);

    


    while(1) {

        // 1. Force a "Wake up" by reading a random register twice
        uint8_t dummy;
        dps368_read(dev_sensor1, 0x0D, &dummy, 1); 
        vTaskDelay(pdMS_TO_TICKS(500));
        
        // 2. Now try the real ID read
        uint8_t id = 0;
        dps368_read(dev_sensor1, 0x0D, &id, 1);
        printf("Device ID: 0x%02X\n", id);

        int32_t raw_p = dps368_get_raw_pressure(dev_sensor1);
        int32_t raw_t = dps368_get_raw_temp(dev_sensor1);
        
        // This prints the raw decimal value
        // To get hPa (e.g. 1013.25), you need the calibration coefficients
        printf("Raw Pressure Value:%" PRId32 "\n", raw_p);
        printf("Raw Temperature Value:%" PRId32"\n", raw_t);

        int corrected_p = corrected_pressure(raw_p, raw_t, coeffs);
        printf("Corrected Pressure Value: %d Pa\n", corrected_p);

        int corrected_t = corrected_temperature(raw_t, coeffs);
        printf("Corrected Temperature Value: %d °C\n", corrected_t);

        vTaskDelay(pdMS_TO_TICKS(500));
        
    }
}
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
    // 1. Setup SPI
    spi_device_handle_t dev_sensor1 = NULL;

    if (spi_bus_init(&dev_sensor1) != ESP_OK) {
        ESP_LOGE("MAIN", "SPI Init Failed");
        return;
    }

    // 2. Initialize the sensor
    dps368_init(dev_sensor1);

    // 3. Read Calibration Coefficients there are 8 coefficients, c0 and c1 are 12 bits, the rest are 16 bits
    uint8_t check;
    check = (dps368_read(dev_sensor1, 0x08, &check, 1) & 0x80);//ensures coefficients are ready to read
    if (check == 0x80) {
        int32_t coeffs[9];
        dps368_get_coeff(dev_sensor1, coeffs);
    }

    //4. Setup file for reading and writing to SD card
     if (sd_init() != ESP_OK) {
        ESP_LOGE("MAIN", "SD Card Init Failed");
        return;
    }

    FILE *f = sd_log_open("data.txt");
    if (f == NULL) {
        ESP_LOGE("MAIN", "Failed to open file for writing");
        return;
    }

    fprintf(f, "Corrected Pressure (Pa),Corrected Temperature (°C)\n");

    while(1) {

        // 1. Force a "Wake up" by reading a random register twice
        uint8_t dummy;
        dps368_read(dev_sensor1, 0x0D, &dummy, 1); 
        vTaskDelay(pdMS_TO_TICKS(500));
        
        int32_t raw_p = dps368_get_raw_pressure(dev_sensor1);
        int32_t raw_t = dps368_get_raw_temp(dev_sensor1);
        
        // This prints the raw decimal value
        // To get hPa (e.g. 1013.25), you need the calibration coefficients

        int corrected_p = corrected_pressure(raw_p, raw_t, coeffs);

        int corrected_t = corrected_temperature(raw_t, coeffs);

        // Log the corrected pressure and temperature to the SD card
        sd_log_sample((float)corrected_p, (uint32_t)corrected_t);

        vTaskDelay(pdMS_TO_TICKS(500));

        
    
    // Will need to implement a shutdown procedure eventually
    //Close the file and deinitialize SD Card and turn off sensors etc.
    }


}
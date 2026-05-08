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
#include "GlobalWatch.h"


static spi_device_handle_t spi1;
static spi_device_handle_t spi2;
static dps368_t dps_internal;
static dps368_t dps_external;
static ring_buffer_t pressure_buffer;
volatile bool shutdown_requested = false;
volatile bool logger_done = false;


void app_main(void)
{

    // ====Initialization=====================================================
    esp_log_level_set("*", ESP_LOG_INFO);

    //establishes I2C communication with the MAX17043 battery monitor chip. Need this to see the battery voltage as well as estimated state of charge (SOC)
    BatMGMT_init();
    
    //The ring buffer is an intermediary step for storing timestamped measurements before logging to SD card
    ring_buffer_init(&pressure_buffer);

    //Sets up the SPI bus before dsp368 configuration
    if (spi_bus_init_two(&spi1, &spi2) != ESP_OK) {
        printf("SPI init failed\n");
        return;
    }
    dps368_init(&dps_internal, spi1); //connects to DPS368 pressure sensor internal
    dps368_init(&dps_external, spi2); //connects to DPS368 pressure sensor external

    sd_log_open("log.csv", "w"); //opens csv file on the SD card to save measurements to


    // =====task creation==================================
    collate_start_task(&pressure_buffer, &dps_internal, &dps_external, 33); //this task starts the collate_task which handles taking measurements and saving to the ring buffer

    xTaskCreate(logger_task, "logger_task", 4096, &pressure_buffer, 4, NULL); //this task takes measurements from the ring buffers, pops them off, and saves them to the SD card

    xTaskCreate(gps_task, "gps_task", 4096, NULL, 5,NULL); //this task continuously parses NMEA sentences from the GPS handler, it also handles PPS synchronization

    xTaskCreate(userInterface_task, "button", 4096, &pressure_buffer, 1, NULL); //this task responds to button presses and cycles through 4 different informational readouts on the UI screen.
}

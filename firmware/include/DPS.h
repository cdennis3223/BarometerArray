#pragma once

#include "driver/spi_master.h"

// Your existing pin definitions
#define PIN_NUM_MISO 46
#define PIN_NUM_MOSI 3
#define PIN_NUM_CLK  9
#define PIN_NUM_CS1  8
#define PIN_NUM_CS2  18

typedef struct{
    spi_device_handle_t spi;
    int32_t coeffs[9];
} dps368_t;



esp_err_t spi_bus_init(spi_device_handle_t *handle_out);

void dps368_init(dps368_t *dev, spi_device_handle_t spi_handle);

float corrected_pressure(dps368_t *dev);

float corrected_temperature(dps368_t *dev);
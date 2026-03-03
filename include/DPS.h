#pragma once

#include "driver/spi_master.h"

// Your existing pin definitions
#define PIN_NUM_MISO 12
#define PIN_NUM_MOSI 13
#define PIN_NUM_CLK  14
#define PIN_NUM_CS1  15

typedef struct{
    spi_device_handle_t spi;
    int32_t coeffs[9];
} dps368_t;



esp_err_t spi_bus_init(spi_device_handle_t *handle_out);

void dps368_init(dps368_t *dev, spi_device_handle_t spi_handle);

float corrected_pressure(dps368_t *dev);

float corrected_temperature(dps368_t *dev);

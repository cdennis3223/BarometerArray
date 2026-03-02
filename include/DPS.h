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

// UPDATE: Note the asterisk (*). We pass a pointer to the handle.
esp_err_t spi_bus_init(spi_device_handle_t *handle_out);

// This matches your signature
//esp_err_t dps368_read(spi_device_handle_t dev, uint8_t reg, uint8_t *out, size_t len);

void dps368_init(dps368_t *dev, spi_device_handle_t spi_handle);

//This gets the coefficients required for pressure calc. stores them in a provided array
//void dps368_get_coeff(spi_device_handle_t dev, int32_t *coeffs);


float corrected_pressure(dps368_t *dev);
//float corrected_temperature(float raw_temp, dps368_t *dev);

//int32_t dps368_get_raw_temp(spi_device_handle_t dev);

//int32_t dps368_get_raw_pressure(spi_device_handle_t dev);

//esp_err_t dps368_write(spi_device_handle_t dev, uint8_t reg, uint8_t val);

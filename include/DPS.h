#ifndef DPS_H
#define DPS_H

#include "driver/spi_master.h"

// Your existing pin definitions
#define PIN_NUM_MISO 12  //was 12 originally
#define PIN_NUM_MOSI 13     //was 13 originally
#define PIN_NUM_CLK  14
#define PIN_NUM_CS1  15

// UPDATE: Note the asterisk (*). We pass a pointer to the handle.
esp_err_t spi_bus_init(spi_device_handle_t *handle_out);

// This matches your signature
esp_err_t dps368_read(spi_device_handle_t dev, uint8_t reg, uint8_t *out, size_t len);

//This gets the coefficients required for pressure calc. stores them in a provided array
void dps368_get_coeff(spi_device_handle_t dev, uint32_t *coeffs);

float dps368_get_raw_pressure(spi_device_handle_t dev);

esp_err_t dps368_write(spi_device_handle_t dev, uint8_t reg, uint8_t val);

#endif
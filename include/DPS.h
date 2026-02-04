#ifndef DPS_H
#define DPS_H

#include "driver/spi_master.h"

// Your code here
#define PIN_NUM_MISO 12
#define PIN_NUM_MOSI 13
#define PIN_NUM_CLK  14
#define PIN_NUM_CS1  15
#define PIN_NUM_CS2  16

void spi_bus_init();
esp_err_t dps368_read(spi_device_handle_t dev, uint8_t reg, uint8_t *out, size_t len);


#endif // DPS_H
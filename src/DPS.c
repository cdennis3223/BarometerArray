#include "driver/spi_master.h"
#include "driver/gpio.h"
#include <string.h>
#include "DPS.h"

// spi_bus_init now takes a pointer (handle_out) to fill the variable in main
esp_err_t spi_bus_init(spi_device_handle_t *handle_out) {
    spi_bus_config_t buscfg = {
        .miso_io_num = PIN_NUM_MISO,
        .mosi_io_num = PIN_NUM_MOSI,
        .sclk_io_num = PIN_NUM_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4094,
    };
    
    esp_err_t ret = spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) return ret;

    spi_device_interface_config_t devcfg1 = {
        .clock_speed_hz = 1*1000*1000,
        .mode = 3, // DPS368 standard
        .spics_io_num = PIN_NUM_CS1,
        .queue_size = 1,
    };

    // We pass handle_out directly because it is already a pointer
    return spi_bus_add_device(SPI2_HOST, &devcfg1, handle_out);
}


esp_err_t dps368_read(spi_device_handle_t dev, uint8_t reg, uint8_t *out, size_t len) {
    // Control byte: bit 7 = 1 for READ
    uint8_t tx_data[len + 1];
    uint8_t rx_data[len + 1];
    memset(tx_data, 0, sizeof(tx_data));
    tx_data[0] = 0x80 | reg; // Command byte

    spi_transaction_t t = {
        .length = 8 * (len + 1), // Total bits (Command + Data)
        .tx_buffer = tx_data,
        .rx_buffer = rx_data,
    };

    esp_err_t ret = spi_device_transmit(dev, &t);
    if (ret != ESP_OK) return ret;

    // The first byte received is garbage (while we sent the address)
    // The actual data starts at rx_data[1]
    memcpy(out, &rx_data[1], len);
    return ESP_OK;
}
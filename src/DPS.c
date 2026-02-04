#include "driver/spi_master.h"
#include "driver/gpio.h"
#include <string.h>
#include "DPS.h"

spi_device_handle_t dev_sensor1;
spi_device_handle_t dev_sensor2;

void spi_bus_init() {
    spi_bus_config_t buscfg = {
        .miso_io_num = PIN_NUM_MISO,
        .mosi_io_num = PIN_NUM_MOSI,
        .sclk_io_num = PIN_NUM_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4094,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO));

    spi_device_interface_config_t devcfg1 = {
        .clock_speed_hz = 1*1000*1000,
        .mode = 3,
        .spics_io_num = PIN_NUM_CS1,
        .queue_size = 1,
    };
    ESP_ERROR_CHECK(spi_bus_add_device(SPI2_HOST, &devcfg1, &dev_sensor1));

}

esp_err_t dps368_read(spi_device_handle_t dev, uint8_t reg, uint8_t *out, size_t len){
    //control byte: bit7 = 1 for read; bits 6-0 = reg address
    uint8_t ctrl = 0x00 | (reg & 0x7F);
    uint8_t txbuf[1] = {0x80|reg};
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length = 8 * (1 + len);
    t.tx_buffer = txbuf;
    t.rxlength = 8 * (1 + len);
    uint8_t rxbuf[8*(1+len)];
    t.rx_buffer = rxbuf;
    esp_err_t ret = spi_device_transmit(dev, &t);
    if (ret != ESP_OK) return ret;

    memcpy(out, rxbuf+1, len);
    return ESP_OK;
}

/*
void app_main() {
    spi_bus_init();

    while (1){
    uint8_t buf[3];
    //Example: read product ID register 0x0D (3 bytes not needed, just example)
    dps368_read(dev_sensor1, 0x0D, buf, 1);
    printf("Senso1 PROD/REV: 0x%02x/n", buf[0]);
    vTaskDelay(pdMS_TO_TICKS(500));
    }
    /*
    dps368_read(dev_sensor2, 0x0D, buf, 1);
    printf("Senso2 PROD/REV: 0x%02x/n", buf[0]);
    
}
*/
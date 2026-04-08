#include "driver/gpio.h"
#include "BatMGMT.h"
#include "esp_log.h"
#include "driver/i2c_master.h"
#include "driver/i2c_slave.h"
#include <string.h>

i2c_master_bus_handle_t bus_handle;
i2c_master_dev_handle_t dev_handle;


esp_err_t BatMGMT_init(void) {
    
    i2c_master_bus_config_t i2c_mst_config = {
    .clk_source = I2C_CLK_SRC_DEFAULT,
    .i2c_port = -1,//auto select
    .scl_io_num = I2C_MASTER_SCL,
    .sda_io_num = I2C_MASTER_SDA,
    .glitch_ignore_cnt = 7,
    .flags.enable_internal_pullup = true,
    };

    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_mst_config, &bus_handle));

    i2c_device_config_t dev_cfg = {
    .dev_addr_length = I2C_ADDR_BIT_LEN_7,
    .device_address = 0x6D, //0x6C is write for the battery management chip, 0x6D is read
    .scl_speed_hz = 400000,
    };

    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus_handle, &dev_cfg, &dev_handle));

    return ESP_OK;
}

static void send_cmd(uint8_t cmd, uint8_t addr)
{
    uint8_t buf[3] = {0x00, cmd, addr};
    i2c_master_transmit(dev_handle, buf, 3, 100);
}

static void send_data(uint8_t *data, size_t len)
{
    uint8_t buf[len + 1];
    buf[0] = 0x40;
    memcpy(buf + 1, data, len);
    i2c_master_transmit(dev_handle, buf, len + 1, 100);
}

static void BatMGMT_read(uint8_t reg, uint16_t *value){
    //read data from the battery management chip and print it to the console
    //SOC is read from REG 0x04, Direct voltage is from REG 0x02
    //Send 0x6D to read from device
    uint8_t buf[2];

    i2c_master_transmit_receive(dev_handle, &reg, 1, buf, 2, 100);
    *value = (buf[0] << 8) | buf[1];

}

void BatMGMT_readSOC(void){
    uint8_t buf[2];
    BatMGMT_read(0x04, (uint16_t *)buf);
    uint16_t raw_soc = (buf[0] << 8) | buf[1];
    float soc = raw_soc / 256.0;
    ESP_LOGI("BatMGMT", "State of Charge: %.2f%%", soc);
}



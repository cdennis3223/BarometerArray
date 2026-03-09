#include "driver/gpio.h"
#include "BatMGMT.h"
#include "esp_log.h"
#include "driver/i2c_master.h"
#include "driver/i2c_slave.h"

i2c_master_bus_config_t i2c_mst_config = {
    .clk_source = I2C_CLK_SRC_DEFAULT,
    .i2c_port = -1,//auto select
    .scl_io_num = I2C_MASTER_SCL,
    .sda_io_num = I2C_MASTER_SDA,
    .glitch_ignore_cnt = 7,
    .flags.enable_internal_pullup = true,
};

i2c_master_bus_handle_t bus_handle;


esp_err_t i2c_init() {
    esp_err_t ret = i2c_new_master_bus(&i2c_mst_config, &bus_handle);
    if (ret != ESP_OK) {
        ESP_LOGE("I2C", "Failed to initialize I2C master bus");
        return ret;
    }

    return ESP_OK;
}

i2c_device_config_t dev_cfg = {
    .dev_addr_length = I2C_ADDR_BIT_LEN_7,
    .device_address = 0x6C, //0x6C is write for the battery management chip, 0x6D is read
    .scl_speed_hz = 100000,
};


/*
Use these in main for initialization
i2c_new_master_bus(&i2c_mst_config, &bus_handle);
i2c_master_bus_add_device(bus_handle, &dev_cfg, &dev_handle);
*/


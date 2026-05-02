#include "driver/gpio.h"
#include "BatMGMT.h"
#include "driver/i2c_master.h"
#include <stdio.h>
#include <string.h>

#define MAX17043_VCELL_REG   0x02   // raw ADC voltage, MAX17043 datasheet Table 1
#define MAX17043_SOC_REG     0x04   // state of charge, MAX17043 datasheet Table 1
#define MAX17043_STATUS_REG  0x1A   // status register, MAX17043 datasheet Table 1
#define MAX17043_RI_MASK     0xFE00 // clears RI flag, upper byte only
#define MAX17043_VOLTAGE_LSB 78.125e-6f // 78.125µV per LSB, MAX17043 datasheet Table 1

static i2c_master_bus_handle_t bus_handle;
static i2c_master_dev_handle_t dev_handle;


esp_err_t BatMGMT_init(void)
{
    i2c_master_bus_config_t bus_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = I2C_NUM,
        .scl_io_num = I2C_MASTER_SCL,
        .sda_io_num = I2C_MASTER_SDA,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    esp_err_t err = i2c_new_master_bus(&bus_config, &bus_handle);
    if (err != ESP_OK) {
        printf("BatMGMT: i2c_new_master_bus failed: %s\n", esp_err_to_name(err));
        return err;
    }

    i2c_device_config_t dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = BATMGMT_ADDR,
        .scl_speed_hz = 100000,
    };

    err = i2c_master_bus_add_device(bus_handle, &dev_config, &dev_handle);
    if (err != ESP_OK) {
        printf("BatMGMT: i2c_master_bus_add_device failed: %s\n", esp_err_to_name(err));
        return err;
    }

    printf("BatMGMT: Battery management IC initialized\n");
    return ESP_OK;
}

static esp_err_t BatMGMT_read16(uint8_t reg, uint16_t *value)
{
    uint8_t buf[2] = {0};

    esp_err_t err = i2c_master_transmit_receive(dev_handle, &reg, 1, buf, 2, 100);
    if (err != ESP_OK) {
        printf("BatMGMT: Read reg 0x%02X failed: %s\n", reg, esp_err_to_name(err));
        return err;
    }

    *value = ((uint16_t)buf[0] << 8) | buf[1];
    return ESP_OK;
}

static esp_err_t BatMGMT_write16(uint8_t reg, uint16_t value)
{
    uint8_t buf[3];
    buf[0] = reg;
    buf[1] = (uint8_t)(value >> 8);
    buf[2] = (uint8_t)(value & 0xFF);

    esp_err_t err = i2c_master_transmit(dev_handle, buf, 3, 100);
    if (err != ESP_OK) {
        printf("BatMGMT: Write reg 0x%02X failed: %s\n", reg, esp_err_to_name(err));
    }
    return err;
}

void BatMGMT_clearRI(void)
{
    uint16_t status = 0;
    if (BatMGMT_read16(MAX17043_STATUS_REG, &status) != ESP_OK) {
        printf("BatMGMT: Failed to read STATUS before clearing RI\n");
        return;
    }

    printf("BatMGMT: STATUS before clear RI = 0x%04X\n", status);

    // Only clear RI in the upper byte. Do not trust or manipulate the low byte.
    uint16_t new_status = status & MAX17043_RI_MASK;

    if (BatMGMT_write16(MAX17043_STATUS_REG, new_status) != ESP_OK) {
        printf("BatMGMT: Failed to write STATUS to clear RI\n");
        return;
    }
    if (BatMGMT_read16(MAX17043_STATUS_REG, &status) != ESP_OK) {
        printf("BatMGMT: Failed to verify STATUS after clearing RI\n");
        return;
    }
    printf("BatMGMT: STATUS after clear RI  = 0x%04X\n", status);
}

float BatMGMT_readSOC(void)
{
    uint16_t raw_soc = 0;
    if (BatMGMT_read16(MAX17043_SOC_REG, &raw_soc) != ESP_OK) {
        return -1.0f;
    }
    return raw_soc / 256.0f;
}

float BatMGMT_readVoltage(void)
{
    uint16_t raw_vcell = 0;
    if (BatMGMT_read16(MAX17043_VCELL_REG, &raw_vcell) != ESP_OK) {
        return -1.0f;
    }
    return raw_vcell * MAX17043_VOLTAGE_LSB;
}
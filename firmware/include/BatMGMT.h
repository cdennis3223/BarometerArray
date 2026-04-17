#pragma once

#include "esp_log.h"
#include "driver/gpio.h"

#define I2C_MASTER_SCL 41
#define I2C_MASTER_SDA 42
#define MAX_ALERT 40
#define I2C_NUM I2C_NUM_0
#define BATMGMT_ADDR 0x36

void BatMGMT_init(void);
float BatMGMT_readVoltage(void);
float BatMGMT_readSOC(void);
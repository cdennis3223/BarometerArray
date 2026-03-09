#pragma once

#include "esp_log.h"
#include "driver/gpio.h"

//Define all GPIO pins for SD Card
#define SD_CLK_PIN 6
#define SD_CMD_PIN 7
#define SD_D0_PIN 4
#define SD_D1_PIN 5
#define SD_D2_PIN 8
#define SD_D3_PIN 9
#define DETECT_PIN 10

#define MOUNT_POINT "/sdcard"

esp_err_t sd_init(void);

esp_err_t sd_deinit(void);

esp_err_t sd_log_open(const char *name);

esp_err_t sd_log_sample(float p_pa, uint32_t t_c);

void sd_log_close(void);
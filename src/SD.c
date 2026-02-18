#include "SD.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/sdmmc_host.h"
#include "driver/sdmmc_defs.h"

esp_err_t sd_init() {
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    host.slot = SDMMC_HOST_SLOT_1; // Use SDMMC slot 1

    // GPIOs for SD card
    gpio_set_pull_mode(SD_CLK_PIN, GPIO_PULLUP_ONLY);
    gpio_set_pull_mode(SD_CMD_PIN, GPIO_PULLUP_ONLY);
    gpio_set_pull_mode(SD_D0_PIN, GPIO_PULLUP_ONLY);
    gpio_set_pull_mode(SD_D1_PIN, GPIO_PULLUP_ONLY);
    gpio_set_pull_mode(SD_D2_PIN, GPIO_PULLUP_ONLY);
    gpio_set_pull_mode(SD_D3_PIN, GPIO_PULLUP_ONLY);

    sdmmc_slot_config_t slot = SDMMC_SLOT_CONFIG_DEFAULT();
    slot.width = 4; // Use 4-bit mode
    slot.clk = SD_CLK_PIN;
    slot.cmd = SD_CMD_PIN;
    slot.d0 = SD_D0_PIN;
    slot.d1 = SD_D1_PIN;
    slot.d2 = SD_D2_PIN;
    slot.d3 = SD_D3_PIN;

    esp_err_t ret = sdmmc_host_init();
    if (ret != ESP_OK) {
        ESP_LOGE("SD", "Failed to initialize SDMMC host");
        return ret;
    }

    ret = sdmmc_host_init_slot(host.slot, &slot);
    if (ret != ESP_OK) {
        ESP_LOGE("SD", "Failed to initialize SDMMC slot");
        return ret;
    }

    esp_err_t err = sdmmc_card_init(&slot, &host);

    return ESP_OK;
}

esp_err_t sd_deinit() {
    return sdmmc_host_deinit();
}

esp_err_t sd_write(const char* path, const uint8_t* data, size_t size) {
    // Implement SD card write functionality here


    return ESP_OK;
}
#include "SD.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/sdmmc_host.h"
#include "sdmmc_cmd.h"
#include "esp_vfs_fat.h"
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include "GPS.h"
#include "collate.h"

static const char *TAG = "SD";
static FILE *g_f = NULL;
static uint32_t lines_since_flush = 0;

sdmmc_card_t *g_card = NULL; // store pointer for unmounting if needed

esp_err_t sd_init(void)
{
    esp_err_t ret;
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024
    };

    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    // If you want a specific freq: host.max_freq_khz = 20000; // 20 MHz

    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
    // If using custom pins (ESP32-S3 etc) set them here:
    slot_config.width = 4;
    slot_config.clk = SD_CLK_PIN;
    slot_config.cmd = SD_CMD_PIN;
    slot_config.d0  = SD_D0_PIN;
    slot_config.d1  = SD_D1_PIN;
    slot_config.d2  = SD_D2_PIN;
    slot_config.d3  = SD_D3_PIN;

    ESP_LOGI(TAG, "Mounting FAT filesystem at '%s'...", MOUNT_POINT);

    ret = esp_vfs_fat_sdmmc_mount(MOUNT_POINT, &host, &slot_config, &mount_config, &g_card);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_vfs_fat_sdmmc_mount failed: %s", esp_err_to_name(ret));
        // Possible actions: if format_if_mount_failed=true you may format here (be careful)
        return ret;
    }

    sdmmc_card_print_info(stdout, g_card);
    ESP_LOGI(TAG, "SD card mounted");
    return ESP_OK;
}

esp_err_t sd_deinit(void)
{
    if (g_card) {
        ESP_LOGI(TAG, "Unmounting FAT filesystem at '%s'...", MOUNT_POINT);
        esp_vfs_fat_sdcard_unmount(MOUNT_POINT, g_card);
        g_card = NULL;
    }
    // sdmmc_host_deinit() is called by unmount in examples; if needed:
    // return sdmmc_host_deinit();
    return ESP_OK;
}

// Opens a log file for writing. Caller should call sd_log_close() when done.
esp_err_t sd_log_open(const char *name, const char *mode)
{
    char path[128];
    snprintf(path, sizeof(path), "/sdcard/%s", name);

    if (strcmp(mode, "a") == 0){
    g_f = fopen(path, mode);
    if (!g_f) {
        ESP_LOGE("SD", "fopen failed %s (errno=%d: %s)", path, errno, strerror(errno));
        return ESP_FAIL;
    }
} else {
    g_f = fopen(path, mode);
    if(!g_f) {
        ESP_LOGE("SD", "fopen failed %s (errno=%d: %s)", path, errno, strerror(errno));
        return ESP_FAIL;
    }
    fprintf(g_f,"Year,Month,Day,Hour,Minute,Second,Millisecond,Pressure(Pa),Temperature(C),Time(ESP),Battery Voltage(V),SOC(%%),Valid Time \n");
    fflush(g_f);
}
    return ESP_OK;
}

esp_err_t sd_log_sample(const sample_t *s)
{
    if (!g_f) return ESP_ERR_INVALID_STATE;

    int n = fprintf(g_f, "%04d,%02d,%02d,%02d,%02d,%02d,%03d,%.10f,%.2f,%llu,%.2f,%.2f,%s\n",
                (unsigned short)s->year,
                (unsigned short)s->month,
                (unsigned short)s->day,
                (unsigned short)s->hour,
                (unsigned short)s->minute,
                (unsigned short)s->second,
                (unsigned short)s->millisecond,
                //(unsigned long)s.seq,
                (double)s->pressure,
                (double)s->temperature,
                (unsigned long long)s->esp_time_ms,
                (float) s->Voltage,
                (float) s->SOC,
                (char) s->valid_time ? "true" : "false");

    if (n <= 0) {
        ESP_LOGE("SD", "fprintf failed (errno=%d: %s)", errno, strerror(errno));
        return ESP_FAIL;
    }

    // Flush every 25 lines to ensure data is written to the SD card in a timely manner,
    // while avoiding excessive flushes. Each Flush is what writes the data to the SD card,
    // so we want to do it often enough to not lose much data if power is lost, 
    //but not so often that we degrade performance.
    lines_since_flush++;
    if (lines_since_flush >= 50) {
    if (fflush(g_f) != 0) {
        ESP_LOGE("SD", "fflush failed (errno=%d: %s)", errno, strerror(errno));
    }
    if (fsync(fileno(g_f)) != 0) {
        ESP_LOGE("SD", "fsync failed (errno=%d: %s)", errno, strerror(errno));
    }
    lines_since_flush = 0;
    }
    return ESP_OK;
}

// Closes the log file if open. Should be called when done logging. Part of shutdown procedure
void sd_log_close(void)
{
    if (g_f) {
        fflush(g_f);
        fclose(g_f);
        g_f = NULL;
    }
}

bool sd_is_mounted(void) { return g_card != NULL; }
bool sd_is_logging(void) { return g_f != NULL; }
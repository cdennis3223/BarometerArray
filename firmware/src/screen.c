#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include <string.h>
#include "screen.h"
#include "esp_timer.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include <stdio.h>
#include "GPS.h"
#include "GlobalWatch.h"
#include "collate.h"
#include "BatMGMT.h"
#include "SD.h"


static const char *TAG = "screen.c";

static i2c_master_bus_handle_t bus_handle;
static i2c_master_dev_handle_t dev_handle;

static void sh1107_clear(void);
static void sh1107_print_horizontal(uint8_t col, uint8_t page, const char *str);

static void format_utc_time(const char *raw, char *out, size_t out_size)
{
    if (!raw || raw[0] == '\0') {
        snprintf(out, out_size, "--:--:--");
        return;
    }

    if (strlen(raw) < 6) {
        snprintf(out, out_size, "%s", raw);
        return;
    }

    snprintf(out, out_size, "%.2s:%.2s:%.2s", raw, raw + 2, raw + 4);
}


//===screen rendering states========================================================
static void render_gps_screen(void)
{
    gps_display_data_t gps_data = {0};
    char line[24];
    char utc[16];

    gps_get_display_data(&gps_data);

    sh1107_clear();
    sh1107_print_horizontal(0, 15, "GPS DATA:");

    if (!gps_data.valid) {
        sh1107_print_horizontal(10, 15, "WAITING FOR FIX");
        sh1107_print_horizontal(20, 15, "LAT: --");
        sh1107_print_horizontal(30, 15, "LON: --");
        sh1107_print_horizontal(40, 15, "UTC: --:--:--");
        return;
    }

    snprintf(line, sizeof(line), "LAT: %.4f", gps_data.latitude);
    sh1107_print_horizontal(10, 15, line);

    snprintf(line, sizeof(line), "LON: %.4f", gps_data.longitude);
    sh1107_print_horizontal(20, 15, line);

    format_utc_time(gps_data.utc_time, utc, sizeof(utc));
    snprintf(line, sizeof(line), "UTC: %s", utc);
    sh1107_print_horizontal(30, 15, line);
}

static void render_pressure_screen(ring_buffer_t *pressure_buffer)
{
    sample_t latest_sample;
    char line[24];

    sh1107_clear();
    sh1107_print_horizontal(0, 15, "PRESSURE:");

    if (!ring_buffer_peek_latest(pressure_buffer, &latest_sample)) {
        sh1107_print_horizontal(10, 15, "NO DATA");
        return;
    }

    snprintf(line, sizeof(line), "%.2f hPa", latest_sample.pressure);
    sh1107_print_horizontal(10, 15, line);

    sh1107_print_horizontal(20, 15, "TEMPERATURE:");
    snprintf(line, sizeof(line), "%.2f C", latest_sample.temperature);
    sh1107_print_horizontal(30, 15, line);
}

static void render_battery_screen(){
    sh1107_print_horizontal(0, 15, "BATTERY STATUS");
    float voltage = BatMGMT_readVoltage();
    float SoC = BatMGMT_readSOC();
    char line[24];
    snprintf(line, sizeof(line), "VOLTAGE: %.4f", voltage);
    sh1107_print_horizontal(10, 15, line);

    snprintf(line, sizeof(line), "SOC: %.4f", SoC);
    sh1107_print_horizontal(20, 15, line);
}

static void render_sd_screen(void) {
    sh1107_clear();
    sh1107_print_horizontal(0, 15, "SD CARD:");
    sh1107_print_horizontal(10, 15, sd_is_mounted() ? "MOUNTED" : "NOT FOUND");
    sh1107_print_horizontal(20, 15, sd_is_logging() ? "LOGGING" : "NOT LOGGING");
}

static void button_init(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << BUTTON_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };

    gpio_config(&io_conf);
}


//=========================================================================

// Column-major 5x7 font for SH1107 horizontal printing
static const uint8_t font5x7_col[95][5] = {
    {0x00,0x00,0x00,0x00,0x00}, // ' '
    {0x00,0x00,0x5F,0x00,0x00}, // '!'
    {0x00,0x07,0x00,0x07,0x00}, // '"'
    {0x14,0x7F,0x14,0x7F,0x14}, // '#'
    {0x24,0x2A,0x7F,0x2A,0x12}, // '$'
    {0x23,0x13,0x08,0x64,0x62}, // '%'
    {0x36,0x49,0x55,0x22,0x50}, // '&'
    {0x00,0x05,0x03,0x00,0x00}, // '''
    {0x00,0x1C,0x22,0x41,0x00}, // '('
    {0x00,0x41,0x22,0x1C,0x00}, // ')'
    {0x08,0x2A,0x1C,0x2A,0x08}, // '*'
    {0x08,0x08,0x3E,0x08,0x08}, // '+'
    {0x00,0x50,0x30,0x00,0x00}, // ','
    {0x08,0x08,0x08,0x08,0x08}, // '-'
    {0x00,0x60,0x60,0x00,0x00}, // '.'
    {0x20,0x10,0x08,0x04,0x02}, // '/'
    {0x3E,0x51,0x49,0x45,0x3E}, // '0'
    {0x00,0x42,0x7F,0x40,0x00}, // '1'
    {0x42,0x61,0x51,0x49,0x46}, // '2'
    {0x21,0x41,0x45,0x4B,0x31}, // '3'
    {0x18,0x14,0x12,0x7F,0x10}, // '4'
    {0x27,0x45,0x45,0x45,0x39}, // '5'
    {0x3C,0x4A,0x49,0x49,0x30}, // '6'
    {0x01,0x71,0x09,0x05,0x03}, // '7'
    {0x36,0x49,0x49,0x49,0x36}, // '8'
    {0x06,0x49,0x49,0x29,0x1E}, // '9'
    {0x00,0x36,0x36,0x00,0x00}, // ':'
    {0x00,0x56,0x36,0x00,0x00}, // ';'
    {0x00,0x08,0x14,0x22,0x41}, // '<'
    {0x14,0x14,0x14,0x14,0x14}, // '='
    {0x41,0x22,0x14,0x08,0x00}, // '>'
    {0x02,0x01,0x51,0x09,0x06}, // '?'
    {0x32,0x49,0x79,0x41,0x3E}, // '@'
    {0x7E,0x11,0x11,0x11,0x7E}, // 'A'
    {0x7F,0x49,0x49,0x49,0x36}, // 'B'
    {0x3E,0x41,0x41,0x41,0x22}, // 'C'
    {0x7F,0x41,0x41,0x22,0x1C}, // 'D'
    {0x7F,0x49,0x49,0x49,0x41}, // 'E'
    {0x7F,0x09,0x09,0x09,0x01}, // 'F'
    {0x3E,0x41,0x49,0x49,0x7A}, // 'G'
    {0x7F,0x08,0x08,0x08,0x7F}, // 'H'
    {0x00,0x41,0x7F,0x41,0x00}, // 'I'
    {0x20,0x40,0x41,0x3F,0x01}, // 'J'
    {0x7F,0x08,0x14,0x22,0x41}, // 'K'
    {0x7F,0x40,0x40,0x40,0x40}, // 'L'
    {0x7F,0x02,0x04,0x02,0x7F}, // 'M'
    {0x7F,0x04,0x08,0x10,0x7F}, // 'N'
    {0x3E,0x41,0x41,0x41,0x3E}, // 'O'
    {0x7F,0x09,0x09,0x09,0x06}, // 'P'
    {0x3E,0x41,0x51,0x21,0x5E}, // 'Q'
    {0x7F,0x09,0x19,0x29,0x46}, // 'R'
    {0x46,0x49,0x49,0x49,0x31}, // 'S'
    {0x01,0x01,0x7F,0x01,0x01}, // 'T'
    {0x3F,0x40,0x40,0x40,0x3F}, // 'U'
    {0x1F,0x20,0x40,0x20,0x1F}, // 'V'
    {0x3F,0x40,0x38,0x40,0x3F}, // 'W'
    {0x63,0x14,0x08,0x14,0x63}, // 'X'
    {0x07,0x08,0x70,0x08,0x07}, // 'Y'
    {0x61,0x51,0x49,0x45,0x43}, // 'Z'
    {0x00,0x7F,0x41,0x41,0x00}, // '['
    {0x02,0x04,0x08,0x10,0x20}, // '\'
    {0x00,0x41,0x41,0x7F,0x00}, // ']'
    {0x04,0x02,0x01,0x02,0x04}, // '^'
    {0x40,0x40,0x40,0x40,0x40}, // '_'
    {0x00,0x01,0x02,0x04,0x00}, // '`'
    {0x20,0x54,0x54,0x54,0x78}, // 'a'
    {0x7F,0x48,0x44,0x44,0x38}, // 'b'
    {0x38,0x44,0x44,0x44,0x20}, // 'c'
    {0x38,0x44,0x44,0x48,0x7F}, // 'd'
    {0x38,0x54,0x54,0x54,0x18}, // 'e'
    {0x08,0x7E,0x09,0x01,0x02}, // 'f'
    {0x08,0x14,0x54,0x54,0x3C}, // 'g'
    {0x7F,0x08,0x04,0x04,0x78}, // 'h'
    {0x00,0x44,0x7D,0x40,0x00}, // 'i'
    {0x20,0x40,0x44,0x3D,0x00}, // 'j'
    {0x7F,0x10,0x28,0x44,0x00}, // 'k'
    {0x00,0x41,0x7F,0x40,0x00}, // 'l'
    {0x7C,0x04,0x18,0x04,0x78}, // 'm'
    {0x7C,0x08,0x04,0x04,0x78}, // 'n'
    {0x38,0x44,0x44,0x44,0x38}, // 'o'
    {0x7C,0x14,0x14,0x14,0x08}, // 'p'
    {0x08,0x14,0x14,0x18,0x7C}, // 'q'
    {0x7C,0x08,0x04,0x04,0x08}, // 'r'
    {0x48,0x54,0x54,0x54,0x20}, // 's'
    {0x04,0x3F,0x44,0x40,0x20}, // 't'
    {0x3C,0x40,0x40,0x20,0x7C}, // 'u'
    {0x1C,0x20,0x40,0x20,0x1C}, // 'v'
    {0x3C,0x40,0x30,0x40,0x3C}, // 'w'
    {0x44,0x28,0x10,0x28,0x44}, // 'x'
    {0x0C,0x50,0x50,0x50,0x3C}, // 'y'
    {0x44,0x64,0x54,0x4C,0x44}, // 'z'
    {0x00,0x08,0x36,0x41,0x00}, // '{'
    {0x00,0x00,0x7F,0x00,0x00}, // '|'
    {0x00,0x41,0x36,0x08,0x00}, // '}'
    {0x08,0x08,0x2A,0x1C,0x08}, // '~'
};

// --- Low level I2C ---
static void send_cmd(uint8_t cmd)
{
    uint8_t buf[2] = {0x00, cmd};
    i2c_master_transmit(dev_handle, buf, 2, 100);
}

static void send_data(uint8_t *data, size_t len)
{
    uint8_t buf[len + 1];
    buf[0] = 0x40;
    memcpy(buf + 1, data, len);
    i2c_master_transmit(dev_handle, buf, len + 1, 100);
}

// --- SH1107 driver ---
static void sh1107_init(void)
{
       // Init I2C bus
    i2c_master_bus_config_t bus_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = I2C_NUM1,
        .scl_io_num = I2C_SCL_PIN,
        .sda_io_num = I2C_SDA_PIN,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &bus_handle));

    // Add SH1107 device
    i2c_device_config_t dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = SH1107_ADDR,
        .scl_speed_hz = 400000,
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus_handle, &dev_config, &dev_handle));

    ESP_LOGI(TAG, "I2C initialized");
   
   //intialization routine
    send_cmd(0xAE); // display off
    send_cmd(0xA1); // segment remap
    send_cmd(0xC8); // COM flip
    send_cmd(0xA8);
    send_cmd(0x3F); // multiplex 64
    send_cmd(0xD3);
    send_cmd(0x20); // no offset
    // send_cmd(0x40); // start line 0
    send_cmd(0xDC);
    send_cmd(0x00);
    send_cmd(0x81);
    send_cmd(0xCF); // contrast
    send_cmd(0xA4); // display RAM
    send_cmd(0xA6); // normal colors
    send_cmd(0xD5);
    send_cmd(0x80); // clock
    send_cmd(0xD9);
    send_cmd(0xF1); // pre-charge
    send_cmd(0xDA);
    send_cmd(0x12); // COM pins
    send_cmd(0xDB);
    send_cmd(0x40); // VCOM
    send_cmd(0xAF); // display on
    vTaskDelay(pdMS_TO_TICKS(100));
}

static void sh1107_clear(void)
{
    uint8_t blank[128];
    memset(blank, 0x00, 128);
    for (uint8_t page = 0; page < 16; page++)
    {
        send_cmd(0xB0 + page);
        send_cmd(0x00);
        send_cmd(0x10);
        send_data(blank, 128);
    }
}

static void sh1107_print_horizontal(uint8_t col, uint8_t page, const char *str) {
    while (*str) {
        char c = *str++;
        if (c < 32 || c > 126) c = 32;
        const uint8_t *glyph = font5x7_col[c - 32];

        uint8_t rotated[8] = {0};
        for (int i = 0; i < 5; i++) {
            for (int j = 0; j < 7; j++) {
                if (glyph[i] & (1 << j)) {
                    rotated[j] |= (1 << (4 - i));
                }
            }
        }

        send_cmd(0xB0 + page);
        send_cmd(col & 0x0F);
        send_cmd(0x10 | (col >> 4));

        uint8_t buf[9];
        memcpy(buf, rotated, 8);
        buf[8] = 0x00;
        send_data(buf, 9);

        page -= 1;
        if (page >= 16) break;
    }
}

//==UI Interface Task=================================================================================================
//responds to botton task and cycles through readouts
void button_task(void *arg)
{
    ring_buffer_t *pressure_buffer = (ring_buffer_t *)arg;

    button_init();
    sh1107_init();

    int state = 0;

    const int debounce_ms = 20;
    const uint32_t LONG_PRESS_MS = 2000;

    bool raw_last = (gpio_get_level(BUTTON_GPIO) == 0);   // active low
    bool stable_state = raw_last;
    int64_t last_raw_change_us = esp_timer_get_time();

    bool button_pressed = false;
    int64_t press_start_ms = 0;

    sh1107_clear();
    sh1107_print_horizontal(0, 15, "...");

    while (1) {
        bool raw = (gpio_get_level(BUTTON_GPIO) == 0);
        int64_t now_us = esp_timer_get_time();
        int64_t now_ms = now_us / 1000;

        // Track raw changes
        if (raw != raw_last) {
            raw_last = raw;
            last_raw_change_us = now_us;
        }

        // Accept a new stable state only after debounce time
        if ((now_us - last_raw_change_us) >= (debounce_ms * 1000) && stable_state != raw) {
            stable_state = raw;

            if (stable_state) {
                // pressed
                button_pressed = true;
                press_start_ms = now_ms;
                ESP_LOGI(TAG, "Button pressed");
            } else {
                // released
                if (button_pressed) {
                    int64_t press_duration = now_ms - press_start_ms;
                    ESP_LOGI(TAG, "Button released, duration=%" PRId64 " ms", press_duration);

                    if (press_duration >= LONG_PRESS_MS) {
                        shutdown_requested = true;
                        ESP_LOGI(TAG, "Long press detected, initiating shutdown...");
                    } else {
                        state = (state + 1) % 4;
                        ESP_LOGI(TAG, "Short press detected, state=%d", state);

                        // Keep this light if possible
                        sh1107_clear();

                        if (state == 0) {
                            render_pressure_screen(pressure_buffer);
                        } else if (state == 1) {
                            render_gps_screen();
                        } else if (state == 3) {
                            render_battery_screen();
                        }else if (state == 2) {
                            render_sd_screen();
                    }
                }

                button_pressed = false;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
}

        //to refresh the GPS periodically 
        /*
        if (state == 1 && (now - last_gps_redraw_time) >= gps_redraw_ms * 1000) {
            render_gps_screen();
            last_gps_redraw_time = now;
        }
        */
        

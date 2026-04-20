#include "logger.h"
#include "SD.h"
#include "GlobalWatch.h"
#include "driver/gpio.h"

//checks if SD card is present
static bool card_inserted(void) {
    return gpio_get_level(DETECT_PIN) == 0; // active low
}

void logger_task(void *args) {
    ring_buffer_t *rb = (ring_buffer_t *)args; //casting back to ring buffer type
    uint32_t last_logged_seq = 0;
    bool have_logged_any = false;
    bool card_present = false;

    //configuring up the detection pin
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << DETECT_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);

    while (1) {
        bool detected = card_inserted();

        //checking if card is being inserted
        if (detected && !card_present) {
            vTaskDelay(pdMS_TO_TICKS(200)); // debounce
            if (card_inserted()) {
                if (sd_init() == ESP_OK && sd_log_open("log.csv") == ESP_OK) {
                    card_present = true;
                    printf("SD card inserted, logging resumed\n");
                }
            }
        //unexpected removal handling
        } else if (!detected && card_present) {
            sd_log_close();
            sd_deinit();
            card_present = false;
            printf("SD card removed\n");
        }
        //while card is present save data from ring buffer to it
        if (card_present) {
            sample_t s;
            if (ring_buffer_pop(rb, &s)) {
                sd_log_sample(s.pressure, s.temperature, s.esp_time_ms,
                              s.utc_time_ms, s.valid_time, s.Voltage, s.seq);
                last_logged_seq = s.seq;
                have_logged_any = true;
            }
        }
        //intentional shutdown and removal handling
        if (!card_inserted() && shutdown_requested) {
            sd_log_close();
            sd_deinit();
            printf("Logger task shutting down...\n");
            if (have_logged_any) {
                printf("Last logged sample seq: %" PRIu32 "\n", last_logged_seq);
            }
            logger_done = true;
            vTaskDelete(NULL);
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
  
#include "logger.h"
#include "SD.h"
#include "GlobalWatch.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "GPS.h"

volatile bool sampling_enabled = false;

// checks if SD card is present
static bool card_inserted(void)
{
    return gpio_get_level(DETECT_PIN) == 0; // active low
}

void logger_task(void *args)
{
    ring_buffer_t *rb = (ring_buffer_t *)args;
    uint32_t last_logged_seq = 0;
    bool have_logged_any = false;
    bool card_present = false;
    bool card_removed_confirmed = true;
    bool removal_pending = false;
    uint64_t removal_check_start_time = 0;

    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << DETECT_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);

    const uint32_t removed_debounce_ms = 400;

    while (1)
    {
        uint64_t now_ms = esp_timer_get_time() / 1000ULL;
        bool detected = card_inserted();

        // Confirmed insertion path
        if (detected && !card_present)
        {
            removal_pending = false;
            vTaskDelay(pdMS_TO_TICKS(200));   // insertion debounce
            if (card_inserted())
            {
                // Make a new file for the first insertion, or for any confirmed reinsertion
                if (card_removed_confirmed && sd_init() == ESP_OK)
                {
                    if (sd_log_open("log.csv", "w") == ESP_OK)
                    {
                        card_present = true;
                        card_removed_confirmed = false;
                        sampling_enabled = true;
                        printf("SD card inserted, logging to %s\n", "log.csv");
                    }
                    else
                    {
                        sd_deinit();
                    }
                }
            }
        }
        // Start removal debounce only if a card session is active
        else if (card_present && !detected && !removal_pending)
        {
            removal_pending = true;
            removal_check_start_time = now_ms;
        }
        // Confirmed removal==============================================================
        else if (card_present && removal_pending &&
                 (now_ms - removal_check_start_time) >= removed_debounce_ms)
        {
            sd_log_close();
            sd_deinit();
            card_present = false;
            removal_pending = false;
            card_removed_confirmed = true;
            sampling_enabled = false;
            printf("SD card removed\n");
        }
        // Logging samples from the ring buffer to the SD card happens here===========================
        if (card_present)
        {
            for (int i = 0; i < 5; i++) //Drain up to 5 samples each pass while card is present
            {
                sample_t s;
                if (!ring_buffer_pop(rb, &s)) {
                    break;
                }
                if (sd_log_sample(&s) == ESP_OK)
                {
                    last_logged_seq = s.seq;
                    have_logged_any = true;
                }
                else
                {
                    printf("sd_log_sample failed\n");
                    break;
                }
            }
        }
        // Shutdown handling==============================================================
        if (shutdown_requested)
        {
            if (card_present) {
                sd_log_close();
                sd_deinit();
            }

            printf("Logger task shutting down...\n");
            if (have_logged_any) {
                printf("Last logged sample seq: %" PRIu32 "\n", last_logged_seq);
            }

            logger_done = true;
            vTaskDelete(NULL);
        }
        vTaskDelay(pdMS_TO_TICKS(100));  //logs at a frequency of 10 Hz (period of 100ms)
    }
}
#include "logger.h"
#include "SD.h"
#include "GlobalWatch.h"

void logger_task(void *args) {
    ring_buffer_t *rb = (ring_buffer_t *)args;
    uint32_t last_logged_seq = 0;
    bool have_logged_any = false;

    while (1) {
        sample_t s;
        bool sample = ring_buffer_pop(rb, &s);

        if (sample) {
            sd_log_sample(s.pressure, s.temperature, s.esp_time_ms, s.utc_time_ms, s.valid_time, s.Voltage, s.seq);
            last_logged_seq = s.seq;
            have_logged_any = true; 
            }

        if (!sample && shutdown_requested) {
            sd_log_close();
            sd_deinit();
            printf("Logger task shutting down...\n");
            if (have_logged_any){
                printf("Logger Shutdown complete.\n");
                printf("Last logged sample seq: %" PRIu32 "\n", last_logged_seq);
            }
            logger_done = true;
            vTaskDelete(NULL);
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
} 
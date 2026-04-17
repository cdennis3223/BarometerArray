#include "logger.h"

void logger_task(void *args) {
    ring_buffer_t *rb = (ring_buffer_t *)args;

    while (1) {
        sample_t s;
        if (ring_buffer_pop(rb, &s)) {
            sd_log_sample(s.pressure, s.temperature, s.timestamp_ms, s.Voltage, s.SOC, s.seq);
        } else {
            vTaskDelay(pdMS_TO_TICKS(500)); // Sleep briefly if no data
        }
    }
}

void logger_start_task(ring_buffer_t *rb) {
    xTaskCreate(logger_task, "logger_task", 4096, rb, 4, NULL);
}
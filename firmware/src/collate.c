#include "collate.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include <stdlib.h>
#include "GPS.h"
#include "BatMGMT.h"

typedef struct {
    ring_buffer_t *rb;
    dps368_t *dev;
    uint32_t period_ms;
} collate_task_args_t;

static inline int next_index(int index) {
    return (index + 1) % BUFFER_SIZE;
}

void ring_buffer_init(ring_buffer_t *rb) {
    rb->head = 0;
    rb->tail = 0;
}

void ring_buffer_push(ring_buffer_t *rb, sample_t s) {
    int next = next_index(rb->head);

    if (next == rb->tail) {
        // buffer full, overwrite oldest
        rb->tail = next_index(rb->tail);
    }

    rb->buffer[rb->head] = s;
    rb->head = next;
}

bool ring_buffer_pop(ring_buffer_t *rb, sample_t *s) {
    if (rb->head == rb->tail) {
        return false;
    }

    *s = rb->buffer[rb->tail];
    rb->tail = next_index(rb->tail);
    return true;
}

bool ring_buffer_peek_latest(ring_buffer_t *rb, sample_t *s) {
    if (rb->head == rb->tail) {
        return false;
    }

    int latest = (rb->head - 1 + BUFFER_SIZE) % BUFFER_SIZE;
    *s = rb->buffer[latest];
    return true;
}

static void collate_task(void *arg) {
    collate_task_args_t *ctx = (collate_task_args_t *)arg;

    const TickType_t period_ticks = pdMS_TO_TICKS(ctx->period_ms);
    TickType_t last_wake = xTaskGetTickCount();

    int64_t last_print_us = 0;
    uint32_t seq = 0;

    while (1) {
        vTaskDelayUntil(&last_wake, period_ticks);

        sample_t s = {0};
        s.seq = seq++;
        s.pressure = corrected_pressure(ctx->dev);
        s.temperature = corrected_temperature(ctx->dev);
        s.voltage = BatMGMT_readVoltage();
        s.soc = BatMGMT_readSOC();

        int64_t now = esp_timer_get_time();


        if (gps_time.valid) {
            int64_t delta = now - gps_time.local_sync_us;
            uint64_t utc_us = gps_time.utc_sync_us + delta;
            s.timestamp_ms = (uint32_t)(utc_us / 1000ULL);

        } else {
            s.timestamp_ms = (uint32_t)(now / 1000ULL);
        }


        ring_buffer_push(ctx->rb, s);

        /*
        if ((now - last_print_us) >= 3000000) {
            sample_t latest;
        if (ring_buffer_peek_latest(ctx->rb, &latest)) {
            printf("latest: pressure=%.2f temp=%.2f timestamp_ms=%lu voltage=%.2f soc=%.2f\n",
                latest.pressure,
                latest.temperature,
                (unsigned long)latest.timestamp_ms,
                latest.voltage,
                latest.soc);
        }
        last_print_us = now;
        }

        vTaskDelay(pdMS_TO_TICKS(ctx->period_ms));
        */
    }
}

void collate_start_task(ring_buffer_t *rb, dps368_t *dev, uint32_t period_ms) {
    collate_task_args_t *args = malloc(sizeof(collate_task_args_t));
    if (args == NULL) {
        return;
    }

    args->rb = rb;
    args->dev = dev;
    args->period_ms = period_ms;

    xTaskCreate(collate_task, "collate_task", 4096, args, 5, NULL);
}
#include "collate.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include <stdlib.h>
#include "GPS.h"
#include "BatMGMT.h"
#include "logger.h"
#include "GlobalWatch.h"

static uint32_t sequence_start = 0;

typedef struct
{
    ring_buffer_t *rb;
    dps368_t *dev;
    uint32_t period_ms;
} collate_task_args_t;

static inline int next_index(int index)
{
    return (index + 1) % BUFFER_SIZE;
}

void ring_buffer_init(ring_buffer_t *rb)
{
    rb->head = 0;
    rb->tail = 0;
}

void ring_buffer_push(ring_buffer_t *rb, sample_t s)
{
    int next = next_index(rb->head);

    if (next == rb->tail)
    {
        // buffer full, overwrite oldest
        rb->tail = next_index(rb->tail);
        printf("Warning: Ring buffer overflow, overwriting oldest sample\n");
    }

    rb->buffer[rb->head] = s;
    rb->head = next;
}

bool ring_buffer_pop(ring_buffer_t *rb, sample_t *s)
{
    if (rb->head == rb->tail)
    {
        return false;
    }

    *s = rb->buffer[rb->tail];
    rb->tail = next_index(rb->tail);
    return true;
}

bool ring_buffer_peek_latest(ring_buffer_t *rb, sample_t *s)
{
    if (rb->head == rb->tail)
    {
        return false;
    }

    int latest = (rb->head - 1 + BUFFER_SIZE) % BUFFER_SIZE;
    *s = rb->buffer[latest];
    return true;
}

static void collate_task(void *arg)
{
    collate_task_args_t *ctx = (collate_task_args_t *)arg;

    int64_t last_print_us = 0;

    while (!shutdown_requested)
    {
        sample_t s = {0};

        if (!sampling_enabled) {
            s.seq = 0;
            vTaskDelay(pdMS_TO_TICKS(100));
            printf("Sampling paused, collate task waiting...\n");
            continue;
        }
        
        int64_t now_us = esp_timer_get_time();

        s.seq = sequence_start++;
        s.pressure = corrected_pressure(ctx->dev);
        s.temperature = corrected_temperature(ctx->dev);
        s.Voltage = BatMGMT_readVoltage();
        s.SOC = BatMGMT_readSOC();

        bool gps_valid;
        uint64_t utc_sync_us;
        int64_t local_sync_us;

        s.esp_time_ms = (uint64_t)(now_us / 1000ULL);

        if (gps_get_sync_snapshot(&gps_valid, &utc_sync_us, &local_sync_us) &&
            gps_valid && now_us >= local_sync_us)
        {
            uint64_t utc_now_us = utc_sync_us + (uint64_t)(now_us - local_sync_us);
            s.utc_time_ms = utc_now_us / 1000ULL;
            s.valid_time = true;
        }
        else
        {
            s.utc_time_ms = 0;
            s.valid_time = false;
        }

        if (s.valid_time){
            utc_ms_to_parts(s.utc_time_ms, &s);
        }

        ring_buffer_push(ctx->rb, s);
        vTaskDelay(pdMS_TO_TICKS(ctx->period_ms));
    }
    printf("Collate task shutting down...\n");
    vTaskDelete(NULL);
}

void collate_start_task(ring_buffer_t *rb, dps368_t *dev, uint32_t period_ms)
{
    collate_task_args_t *args = malloc(sizeof(collate_task_args_t));
    if (args == NULL)
    {
        return;
    }

    args->rb = rb;
    args->dev = dev;
    args->period_ms = period_ms;

    xTaskCreate(collate_task, "collate_task", 4096, args, 5, NULL);
}
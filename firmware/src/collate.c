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
    dps368_t *dev2;
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

//pushes sample_t to ring buffer array
void ring_buffer_push(ring_buffer_t *rb, sample_t s)
{
    int next = next_index(rb->head);

    if (next == rb->tail)// buffer full, overwrite oldest
    {
        rb->tail = next_index(rb->tail);
        printf("Warning: Ring buffer overflow, overwriting oldest sample\n");
    }
    rb->buffer[rb->head] = s;  //writes sample to the head of the ring buffer
    rb->head = next;           //increment the the buffer index for the next sample to write
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

//The collate task creates a sample struct, fills its fields by calling lower level drivers, and then pushes the sample to the ring buffer
static void collate_task(void *arg)
{
    collate_task_args_t *ctx = (collate_task_args_t *)arg; //casting args back to collate_task_args_t for later use

    while (!shutdown_requested) //shutdown happens when SD card is not present
    {
        sample_t s = {0}; //initializing measurement to save to ring buffer
        //filling fields for sample struct before pushing to the ring buffer
        s.seq = sequence_start++;   //for numbering each sample in the CSV log
        s.pressure_internal = corrected_pressure(ctx->dev);
        s.temperature_internal = corrected_temperature(ctx->dev);
        s.pressure_external = corrected_pressure(ctx->dev);
        s.temperature_external = corrected_temperature(ctx->dev);
        s.Voltage = BatMGMT_readVoltage();
        s.SOC = BatMGMT_readSOC();

        uint64_t now_us = (uint64_t)esp_timer_get_time();
        s.esp_time_ms = now_us / 1000ULL;

        //This block handles the timestamping of samples
        bool gps_valid;
        uint64_t utc_sync_us; //actual time
        uint64_t local_sync_us; //timer from ESP32 clock
        if (gps_get_sync_snapshot(&gps_valid, &utc_sync_us, &local_sync_us) &&
            gps_valid &&
            now_us >= local_sync_us)
        {
            uint64_t utc_now_us = utc_sync_us + (now_us - local_sync_us);
            s.utc_time_ms = utc_now_us / 1000ULL;
            s.valid_time = true;
        }
        else
        {
            s.utc_time_ms = 0;  //log file reads time as zero until the GPS receiver gets a fix and can provide UTC time
            s.valid_time = false;
        }
        //if GPS reciever is connected to satellite log the UTC time
        if (s.valid_time){
            utc_ms_to_parts(s.utc_time_ms, &s); //converting from millisecond time to YYYYMMDDHHSS.SSSS for human readability before logging to SD card in logger task
        }

        ring_buffer_push(ctx->rb, s);  //adding sample to ring buffer
        vTaskDelay(pdMS_TO_TICKS(ctx->period_ms)); //samples every period_ms(~33ms/32Hz)
    }
    printf("Collate task shutting down...\n");
    vTaskDelete(NULL); //ends the collate task, happens when the SD card is not present
}

void collate_start_task(ring_buffer_t *rb, dps368_t *dev, dps368_t *dev2, uint32_t period_ms)
{
    static collate_task_args_t args;
    args.rb = rb;
    args.dev = dev;
    args.dev2 = dev2;
    args.period_ms = period_ms;

    xTaskCreate(collate_task, "collate_task", 4096, &args, 5, NULL);
}
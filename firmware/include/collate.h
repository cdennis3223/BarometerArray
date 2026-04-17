#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "DPS.h"

#define BUFFER_SIZE 256

typedef struct {
    uint32_t seq;
    uint32_t dt_ms;
    float pressure;
    float temperature;
    uint32_t timestamp_ms;
    float Voltage;
    float SOC;
} sample_t;

typedef struct {
    sample_t buffer[BUFFER_SIZE];
    volatile int head;
    volatile int tail;
} ring_buffer_t;

void ring_buffer_init(ring_buffer_t *rb);
void ring_buffer_push(ring_buffer_t *rb, sample_t s);
bool ring_buffer_pop(ring_buffer_t *rb, sample_t *s);
bool ring_buffer_peek_latest(ring_buffer_t *rb, sample_t *s);

/* Starts a task that continuously samples the DPS368 and pushes into the buffer */
void collate_start_task(ring_buffer_t *rb, dps368_t *dev, uint32_t period_ms);

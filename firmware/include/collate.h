#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "DPS.h"

#define BUFFER_SIZE 256

//this struct is for storing a single measurement
typedef struct {
    uint32_t seq;
    double pressure;
    float temperature;
    float Voltage;
    float SOC;
    uint64_t esp_time_ms;
    uint64_t utc_time_ms;
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
    uint16_t millisecond;
    bool valid_time;
} sample_t;

//this struct is a ring buffer of 256 measurements long, it is periodically read by the SD logging task
typedef struct {
    sample_t buffer[BUFFER_SIZE];
    volatile int head; //write to head in the collate task
    volatile int tail;  //read from tail in the logger task
} ring_buffer_t;

void ring_buffer_init(ring_buffer_t *rb);
void ring_buffer_push(ring_buffer_t *rb, sample_t s);
bool ring_buffer_pop(ring_buffer_t *rb, sample_t *s);
bool ring_buffer_peek_latest(ring_buffer_t *rb, sample_t *s);

/* Starts a task that continuously samples the DPS368 and pushes into the buffer */
void collate_start_task(ring_buffer_t *rb, dps368_t *dev, uint32_t period_ms);

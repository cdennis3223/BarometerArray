#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "driver/uart.h"
#include "collate.h"

#define GPS_UART        UART_NUM_2
#define GPS_TX_PIN      1      //this is TX2 on the devkit
#define GPS_RX_PIN      2      //this is RX2 on the devkit
#define GPS_PPS_PIN     39
#define GPS_BUF_SIZE    1024


typedef struct {
    uint64_t utc_sync_us;      // UTC time at last GPS update
    uint64_t local_sync_us;    // esp_timer_get_time() at that moment
    bool valid;
} gps_time_sync_t;

typedef struct {
    double latitude;
    double longitude;
    char utc_time[10];
    bool valid;
} gps_display_data_t;

//needed by collate tasks in collate.c so I made it extern
extern gps_time_sync_t gps_time;
extern gps_display_data_t gps_display_data;

bool gps_get_sync_snapshot(bool *valid, uint64_t *utc_sync_us, uint64_t *local_sync_us);

bool utc_ms_to_parts(int64_t utc_ms, sample_t *out);

void gps_factory_reset();

void gps_task(void *arg);

void gps_get_display_data(gps_display_data_t *out);


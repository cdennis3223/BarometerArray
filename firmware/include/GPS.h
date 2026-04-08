#ifndef GPS_H
#define GPS_H

#define GPS_UART        UART_NUM_2
#define GPS_TX_PIN      1      //this is TX2 on the devkit
#define GPS_RX_PIN      2      //this is RX2 on the devkit
#define GPS_BUF_SIZE    1024


typedef struct {
    uint64_t utc_sync_us;      // UTC time at last GPS update
    int64_t local_sync_us;     // esp_timer_get_time() at that moment
    bool valid;
} gps_time_sync_t;

//needed by collate tasks in collate.c so I made it extern
extern gps_time_sync_t gps_time;


void gps_uart_init(void);

void gps_factory_reset();

void gps_task(void *arg);


#endif
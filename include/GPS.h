#ifndef GPS_H
#define GPS_H

#define GPS_UART        UART_NUM_2
#define GPS_TX_PIN      17      //this is TX2 on the devkit
#define GPS_RX_PIN      16      //this is RX2 on the devkit
#define GPS_BUF_SIZE    1024

void gps_uart_init(void);

void gps_factory_reset();

void gps_task(void *arg);


#endif
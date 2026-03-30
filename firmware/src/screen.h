#ifndef SCREEN_H
#define SCREEN_H

#include "driver/i2c.h"

#define I2C_MASTER_NUM  I2C_NUM_0
#define SH1107_ADDR     0x3C
#define SDA_GPIO        21
#define SCL_GPIO        22

void button_init(void);
void button_task(void *arg);
void sh1107_init(void);
void sh1107_clear(void);
void sh1107_print_horizontal(uint8_t col, uint8_t page, const char *str);


#endif // SCREEN_H

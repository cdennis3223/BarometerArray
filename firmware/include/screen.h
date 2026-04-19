#pragma once



#define I2C_SDA_PIN 12
#define I2C_SCL_PIN 11
#define BUTTON_GPIO 10
#define I2C_NUM1 I2C_NUM_1
#define SH1107_ADDR 0x3C

//Performs Initialization and then endlessly cycles through readout options
void button_task(void *arg);
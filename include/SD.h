#ifndef SD.h
#define SD.h

//Define all GPIO pins for SD Card
#define SD_CLK_PIN 6
#define SD_CMD_PIN 7
#define SD_D0_PIN 4
#define SD_D1_PIN 5
#define SD_D2_PIN 8
#define SD_D3_PIN 9

esp_err_t sd_init();




#endif
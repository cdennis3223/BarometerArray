#include "driver/spi_master.h"
#include "driver/gpio.h"
#include <string.h>
#include "DPS.h"
#include "esp_log.h"


// spi_bus_init now takes a pointer (handle_out) to fill the variable in main
esp_err_t spi_bus_init(spi_device_handle_t *handle_out) {
    spi_bus_config_t buscfg = {
        .miso_io_num = PIN_NUM_MISO,
        .mosi_io_num = PIN_NUM_MOSI,
        .sclk_io_num = PIN_NUM_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4094,
    };
    
    esp_err_t ret = spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) return ret;

    spi_device_interface_config_t devcfg1 = {
        .clock_speed_hz = 1 * 1000 * 1000,
        .mode = 3,  // DPS368 standard
        .spics_io_num = PIN_NUM_CS1,
        .queue_size = 1,
    };

    return spi_bus_add_device(SPI2_HOST, &devcfg1, handle_out);
}

esp_err_t dps368_read(spi_device_handle_t dev, uint8_t reg, uint8_t *out, size_t len) {
    // Control byte: bit 7 = 1 for READ
    uint8_t tx_data[len + 1];
    uint8_t rx_data[len + 1];
    memset(tx_data, 0, sizeof(tx_data));
    tx_data[0] = 0x80 | reg; // Command byte

    spi_transaction_t t = {
        .length = 8 * (len + 1), // Total bits (Command + Data)
        .tx_buffer = tx_data,
        .rx_buffer = rx_data,
    };

    esp_err_t ret = spi_device_transmit(dev, &t);
    if (ret != ESP_OK) return ret;

    // The first byte received is garbage (while we sent the address)
    // The actual data starts at rx_data[1]
    memcpy(out, &rx_data[1], len);
    ESP_LOGI("SPI_DEBUG", "Sent: 0x%02X | Recv[0]: 0x%02X, Recv[1]: 0x%02X", tx_data[0], rx_data[0], rx_data[1]);

    return ESP_OK;
}

esp_err_t dps368_write(spi_device_handle_t dev, uint8_t reg, uint8_t val) {
    uint8_t tx_data[2] = { reg & 0x7F, val }; // MSB = 0 for WRITE
    spi_transaction_t t = {
        .length = 16,
        .tx_buffer = tx_data,
    };
    return spi_device_transmit(dev, &t);
}

int32_t sign_extend(uint32_t val, int bits) {
    uint32_t m = 1u << (bits - 1);
    return (val ^ m) - m;        
}

void dps368_get_coeff(spi_device_handle_t dev, int32_t *coeffs){
    //All coefficients are stored in registers 0x10 to 0x21 coefficients are 12 bit numbers
    //Stores the coefficients in the provided array
    uint8_t tx_data[19];
    uint8_t rx_data[19];
    memset(tx_data, 0, sizeof(tx_data));
    tx_data[0] = 0x80 | 0x10; // Command byte to read from register 0x10
    spi_transaction_t t ={
        .length = 8 * 19, // Total bits (Command + 18 bytes of data)
        .tx_buffer = tx_data,
        .rx_buffer = rx_data,
    };
    spi_device_transmit(dev, &t);
    // The first byte received is garbage (while we sent the address)
    // The actual data starts at rx_data[1] coefficients c0,c1 are 12 bits,c00 and c10 are 20, 
    //the rest are 16 bits
    uint32_t dummy[9];
    dummy[0] = (rx_data[1]) | ((rx_data[2] & 0xF0) << 4);//c0
    dummy[1] = ((rx_data[2] & 0x0F) << 8) | rx_data[3];//c1
    dummy[2] = (rx_data[4]) << 12 | (rx_data[5] << 4) | ((rx_data[6] & 0xF0) >> 4);//c00
    dummy[3] = ((rx_data[6] & 0x0F) << 16) | (rx_data[7]<<12) | (rx_data[8]);//c10
    dummy[4] = (rx_data[9] << 8) | (rx_data[10]);//c01
    dummy[5] = (rx_data[11] << 8) | (rx_data[12]);//c11
    dummy[6] = (rx_data[13] << 8) | (rx_data[14]);//c20
    dummy[7] = (rx_data[15] << 8) | (rx_data[16]);//c21
    dummy[8] = (rx_data[17] << 8) | (rx_data[18]);//c30

    for (int i=0;i<9;i++){
        printf("dummy[%d]: 0x%08" PRIX32 "\n", i, dummy[i]);
    }

    //Handle the sign for the 12 and 20 bit values
    coeffs[0] = sign_extend(dummy[0], 12);
    coeffs[1] = sign_extend(dummy[1], 12);
    coeffs[2] = sign_extend(dummy[2], 20);
    coeffs[3] = sign_extend(dummy[3], 20);
    coeffs[4] = sign_extend(dummy[4], 16);
    coeffs[5] = sign_extend(dummy[5], 16);
    coeffs[6] = sign_extend(dummy[6], 16);
    coeffs[7] = sign_extend(dummy[7], 16);
    coeffs[8] = sign_extend(dummy[8], 16);
    return;
}

float dps368_get_raw_temp(spi_device_handle_t dev){
    uint8_t raw_data[3];
    // Temperature is stored in 3 registers: 0x03, 0x04, 0x05
    esp_err_t ret = dps368_read(dev, 0x03, raw_data, 3);
    
    if (ret != ESP_OK) return -1.0;

    // Combine 3 bytes into a single 24-bit integer
    // The DPS368 uses Two's Complement for its 24-bit values
    int32_t val = (raw_data[0] << 16) | (raw_data[1] << 8) | raw_data[2];
    
    // Handle the sign bit for 24-bit (if bit 23 is 1, it's negative)
    val = sign_extend(val, 24);

    return (float)val;
}

float dps368_get_raw_pressure(spi_device_handle_t dev) {
    uint8_t raw_data[3];
    // Pressure is stored in 3 registers: 0x00, 0x01, 0x02
    esp_err_t ret = dps368_read(dev, 0x00, raw_data, 3);
    
    if (ret != ESP_OK) return -1.0;

    // Combine 3 bytes into a single 24-bit integer
    // The DPS368 uses Two's Complement for its 24-bit values
    int32_t val = (raw_data[0] << 16) | (raw_data[1] << 8) | raw_data[2];
    
    // Handle the sign bit for 24-bit (if bit 23 is 1, it's negative)
    val = sign_extend(val, 24);

    return (float)val;
}

float corrected_pressure(float raw_pressure, float raw_temp, int32_t *coeffs){
    //This function takes the raw_pressure value and utilizes the coefficients and kP value
    //to correct it per dps368 datasheet, kP for our sample rate is 253952, kT is the same
    uint32_t corrected_pressure;
    uint16_t kP = 253952;
    uint16_t kT = 253952;
    float T_raw_sc;
    float P_raw_sc;
    T_raw_sc = raw_temp/kT;
    P_raw_sc = raw_pressure/kP;

    //Pcomp(Pa) = c00 + Praw_sc*(c10 + Praw_sc *(c20+ Praw_sc *c30)) + Traw_sc *c01 +
    //Traw_sc *Praw_sc *(c11+Praw_sc*c21), from datasheet
    float comp_pressure;
    comp_pressure = coeffs[2] + P_raw_sc*(coeffs[3] + P_raw_sc *(coeffs[6]+ P_raw_sc *coeffs[8])) + T_raw_sc *coeffs[4] +
    T_raw_sc *P_raw_sc *(coeffs[5]+P_raw_sc*coeffs[7]);
    return comp_pressure;
}

float corrected_temperature(float raw_temp, int32_t *coeffs){
    //This function takes the raw_temp value and utilizes the coefficients and kT value
    //to correct it per dps368 datasheet, kT for our sample rate is 253952
    uint16_t kT = 253952;
    float T_raw_sc;
    T_raw_sc = raw_temp/kT;

    //Tcomp(°C) = c0*0.5 + c1*Traw_sc, from datasheet
    float comp_temp;
    comp_temp = (coeffs[0]*0.5) + (coeffs[1]*T_raw_sc);
    return comp_temp;
}



#pragma once

#include <stdbool.h>

//These structs are for storing fields from parsed NMEA sentence in GPS task
typedef struct {
    double lat;             /* decimal degrees, negative = South */
    double lon;             /* decimal degrees, negative = West  */
    double altitude_m;
    double hdop;
    int    fix_quality;     /* 0=none,1=GPS,2=DGPS               */
    int    satellites;
    bool   valid;
} nmea_gga_t;

typedef struct {
    double lat;
    double lon;
    double speed_knots;
    double course_deg;
    bool   active;          /* A=active, V=void                  */
    char   date[7];         /* DDMMYY\0                          */
    char   time[10];        /* HHMMSS.ss\0                       */
    bool   valid;
} nmea_rmc_t;

typedef struct {
    nmea_gga_t gga;
    nmea_rmc_t rmc;
} nmea_data_t;


bool rmc_datetime_to_epoch_us(const char *date_str, const char *time_str, uint64_t *out_utc_us);

bool nmea_parse(char *sentence, nmea_data_t *data);

uint32_t rmc_time_to_seconds(const char *t);
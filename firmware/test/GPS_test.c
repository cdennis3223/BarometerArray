#include "GPS2.h"
#include <stdio.h>
#include <unity.h>



void test_GPS(void)
{
    nmea_data_t data = {0};

    char gga[] = "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47";
    char rmc[] = "$GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*6A";

    if (nmea_parse(gga, &data))
        printf("GGA: lat=%.6f  lon=%.6f  alt=%.1fm  sats=%d  hdop=%.1f\n",
               data.gga.lat, data.gga.lon, data.gga.altitude_m,
               data.gga.satellites, data.gga.hdop);

    if (nmea_parse(rmc, &data))
        printf("RMC: lat=%.6f  lon=%.6f  speed=%.1fkn  course=%.1f°  date=%s  time=%s\n",
               data.rmc.lat, data.rmc.lon, data.rmc.speed_knots,
               data.rmc.course_deg, data.rmc.date, data.rmc.time);

}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_GPS);
    return UNITY_END();
}
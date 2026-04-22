#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <stdbool.h>
#include "GPS_parser.h"

/* -----------------------------------------------------------------------
 * Config
 * -------------------------------------------------------------------- */
#define NMEA_MAX_FIELDS 25
#define NMEA_MAX_LEN 83 /* NMEA spec: 82 chars + null */

//tokenizer, breaks string into an array(fields) of smaller strings
static int parse_comma_delimited_str(char *string, char **fields, int max_fields)
{
    int i = 0;
    fields[i++] = string;

    while ((i < max_fields) && NULL != (string = strchr(string, ',')))
    {
        *string = '\0';
        fields[i++] = ++string;
    }

    return i; /* was --i; fixed */
}

/* -----------------------------------------------------------------------
 * Checksum
 *   XOR of all bytes between '$' and '*', exclusive.
 * -------------------------------------------------------------------- */
static bool nmea_validate_checksum(const char *sentence)
{
    const char *p = sentence;

    if (*p++ != '$')
        return false;

    uint8_t calculated = 0;
    while (*p && *p != '*')
        calculated ^= (uint8_t)*p++;

    if (*p++ != '*')
        return false;

    /* Parse the two hex digits after '*' */
    char hex[3] = {p[0], p[1], '\0'};
    uint8_t provided = (uint8_t)strtol(hex, NULL, 16);

    return calculated == provided;
}

/* -----------------------------------------------------------------------
 * Strip trailing *HH\r\n from the last field so strtod/atoi work cleanly.
 * Call once on the raw sentence buffer before tokenizing.
 * Returns false if the sentence is malformed.
 * -------------------------------------------------------------------- */
static bool nmea_strip_suffix(char *sentence)
{
    char *star = strchr(sentence, '*');
    if (!star)
        return false;
    *star = '\0'; /* null-terminate at '*'; checksum already validated */
    return true;
}

/* -----------------------------------------------------------------------
 * Coordinate helpers
 *   NMEA encodes as DDDMM.MMMM — convert to decimal degrees.
 * -------------------------------------------------------------------- */
static double nmea_coord_to_decimal(const char *coord, const char *hemi)
{
    if (!coord || *coord == '\0')
        return 0.0;

    double raw = atof(coord);
    int degrees = (int)(raw / 100);
    double minutes = raw - (degrees * 100.0);
    double decimal = degrees + (minutes / 60.0);

    if (hemi && (*hemi == 'S' || *hemi == 'W'))
        decimal = -decimal;

    return decimal;
}

/* -----------------------------------------------------------------------
 * Sentence parsers
 * -------------------------------------------------------------------- */

/*
 * GGA — Global Positioning System Fix Data
 *
 * $GPGGA,HHMMSS.ss,LLLL.LL,a,YYYYY.YY,a,q,nn,d.d,H.H,M,G.G,M,A.A,XXXX*hh
 * idx:   0         1        2 3        4 5 6  7  8   9   10 11  12 13   14
 */
static bool parse_gga(char **f, int n, nmea_gga_t *out)
{
    if (n < 10)
        return false;

    out->lat = nmea_coord_to_decimal(f[2], f[3]);
    out->lon = nmea_coord_to_decimal(f[4], f[5]);
    out->fix_quality = atoi(f[6]);
    out->satellites = atoi(f[7]);
    out->hdop = atof(f[8]);
    out->altitude_m = atof(f[9]);
    out->valid = (out->fix_quality > 0);
    return true;
}

/*
 * RMC — Recommended Minimum Navigation Information
 *
 * $GPRMC,HHMMSS.ss,A,LLLL.LL,a,YYYYY.YY,a,x.x,x.x,DDMMYY,x.x,a*hh
 * idx:   0         1 2 3      4 5        6 7   8   9      10  11
 */


static bool parse_rmc(char **f, int n, nmea_rmc_t *out)
{
    if (n < 10)
        return false;

    strncpy(out->time, f[1], sizeof(out->time) - 1);
    out->time[sizeof(out->time) - 1] = '\0';

    out->active = (f[2][0] == 'A');
    out->lat = nmea_coord_to_decimal(f[3], f[4]);
    out->lon = nmea_coord_to_decimal(f[5], f[6]);
    out->speed_knots = atof(f[7]);
    out->course_deg = atof(f[8]);

    strncpy(out->date, f[9], sizeof(out->date) - 1);
    out->date[sizeof(out->date) - 1] = '\0';

    out->valid = out->active;
    return true;
}

uint32_t rmc_time_to_seconds(const char *t)
{
    if (!t || strlen(t) < 6) return 0;

    int hh = (t[0] - '0') * 10 + (t[1] - '0');
    int mm = (t[2] - '0') * 10 + (t[3] - '0');
    int ss = (t[4] - '0') * 10 + (t[5] - '0');

    return hh * 3600 + mm * 60 + ss;
}


/* -----------------------------------------------------------------------
 * Top-level dispatch
 *   Pass a single raw NMEA sentence (null-terminated, mutable).
 * -------------------------------------------------------------------- */
bool nmea_parse(char *sentence, nmea_data_t *data)
{
    if (!sentence || !data)
        return false;

    if (!nmea_validate_checksum(sentence))
        return false;

    /* Work on a copy so we preserve the original if needed */
    char buf[NMEA_MAX_LEN];
    strncpy(buf, sentence, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    if (!nmea_strip_suffix(buf))
        return false;

    char *fields[NMEA_MAX_FIELDS] = {0};
    int n = parse_comma_delimited_str(buf, fields, NMEA_MAX_FIELDS);

    if (n < 1 || !fields[0])
        return false;

    /* Sentence ID is fields[0], e.g. "$GPGGA" or "$GNGGA" */
    const char *id = fields[0];

    /* Match on the last 3 chars to be talker-ID agnostic (GP, GN, GL, etc.) */
    size_t len = strlen(id);
    if (len < 3)
        return false;
    const char *type = id + len - 3;

    if (strcmp(type, "GGA") == 0)
        return parse_gga(fields, n, &data->gga);
    else if (strcmp(type, "RMC") == 0)
        return parse_rmc(fields, n, &data->rmc);

    return false; /* Sentence type not handled */
}

//Help cleanly convert RMC date/time to epoch microseconds for easier timestamp handling.
// Note that this does not handle time zones or leap seconds, but should be sufficient for 
//basic logging purposes.

static bool is_leap_year(int year)
{
    return ((year % 4 == 0) && (year % 100 != 0)) || (year % 400 == 0);
}

static int days_in_month(int year, int month)
{
    static const int dim[12] = {31,28,31,30,31,30,31,31,30,31,30,31};
    if (month == 2 && is_leap_year(year)) {
        return 29;
    }
    return dim[month - 1];
}

static uint64_t date_time_to_epoch_seconds(int year, int month, int day,
                                           int hour, int minute, int second)
{
    uint64_t days = 0;

    for (int y = 1970; y < year; y++) {
        days += is_leap_year(y) ? 366 : 365;
    }

    for (int m = 1; m < month; m++) {
        days += days_in_month(year, m);
    }

    days += (day - 1);

    return days * 86400ULL + (uint64_t)hour * 3600ULL +
           (uint64_t)minute * 60ULL + (uint64_t)second;
}

bool rmc_datetime_to_epoch_us(const char *date_str, const char *time_str, uint64_t *out_utc_us)
{
    if (!date_str || !time_str || !out_utc_us) {
        return false;
    }

    if (strlen(date_str) < 6 || strlen(time_str) < 6) {
        return false;
    }

    int day   = (date_str[0] - '0') * 10 + (date_str[1] - '0');
    int month = (date_str[2] - '0') * 10 + (date_str[3] - '0');
    int year2 = (date_str[4] - '0') * 10 + (date_str[5] - '0');

    int hour   = (time_str[0] - '0') * 10 + (time_str[1] - '0');
    int minute = (time_str[2] - '0') * 10 + (time_str[3] - '0');
    int second = (time_str[4] - '0') * 10 + (time_str[5] - '0');

    int year = 2000 + year2;

    if (month < 1 || month > 12 || day < 1 || day > 31 ||
        hour < 0 || hour > 23 || minute < 0 || minute > 59 ||
        second < 0 || second > 60) {
        return false;
    }

    uint64_t epoch_sec = date_time_to_epoch_seconds(year, month, day, hour, minute, second);
    *out_utc_us = epoch_sec * 1000000ULL;
    return true;
}

/* -----------------------------------------------------------------------
 * Example usage
 * -------------------------------------------------------------------- */

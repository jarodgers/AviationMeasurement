#ifndef GPS_H
#define GPS_H

#include <time.h>

uint8_t GPS_Initialize();

uint8_t GPS_CheckForNewData();

uint8_t GPS_GetLastNMEA(char *buf);

uint8_t GPS_GetCoords(float *latitude, float *longitude);

uint8_t GPS_GetHeading(float *true_heading, float *mag_heading);

uint8_t GPS_GetAltitude(float *altitude, char *altitude_units);

uint8_t GPS_GetGroundSpeedKnots(float *gs_knots);

uint8_t GPS_GetNumSats(int *num_sats);

uint8_t GPS_GetDateTime(int *year, int *month, int *day, int *hours, int *minutes, int *seconds);

uint8_t GPS_GetUNIXTimestamp(struct timespec *ts);

#endif

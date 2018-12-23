#ifndef GPS_H
#define GPS_H

uint8_t GPS_Initialize();

uint8_t GPS_CheckForNewData();

uint8_t GPS_GetLastNMEA(char *buf);

uint8_t GPS_GetCoords(float *latitude, float *longitude);

uint8_t GPS_GetHeading(float *true_heading, float *mag_heading);

uint8_t GPS_GetAltitude(float *altitude, char *altitude_units);

uint8_t GPS_GetGroundSpeedKnots(float *gs_knots);

uint8_t GPS_GetNumSats(int *num_sats);

#endif

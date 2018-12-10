/* tph.h
 * Temperature/Pressure/Humidity sensor */

#ifndef TPH_H
#define TPH_H

#include <stdint.h>

enum TPH_PressureUnits {TPH_PRESSURE_HPA, TPH_PRESSURE_INHG};

enum TPH_TemperatureUnits {TPH_TEMP_DEG_C, TPH_TEMP_DEG_F};

enum TPH_AltitudeUnits {TPH_ALTITUDE_FT, TPH_ALTITUDE_METERS};

int8_t TPH_Initialize();

int8_t TPH_StartMeasurement();

int8_t TPH_CheckForNewData();

uint8_t TPH_GetPressure(float *pressure, enum TPH_PressureUnits units);

uint8_t TPH_GetTemperature(float *temperature, enum TPH_TemperatureUnits units);

uint8_t TPH_GetAltitude(float *altitude, float altimeter, enum TPH_AltitudeUnits altitude_units, enum TPH_PressureUnits altimeter_units);

uint8_t TPH_GetHumidity(float *humidity);

uint16_t TPH_GetMeasTimeMs();

#endif

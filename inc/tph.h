/* tph.h
 * Temperature/Pressure/Humidity sensor */

#ifndef TPH_H
#define TPH_H

#include <stdint.h>

enum TPH_PressureUnits {TPH_PRESSURE_HPA, TPH_PRESSURE_INHG};

int8_t TPH_Initialize();

int8_t TPH_StartMeasurement();

int8_t TPH_CheckForNewData();

uint8_t TPH_GetPressure(float *pressure, enum TPH_PressureUnits units);

uint8_t TPH_GetTemperature(float *temperature);

uint8_t TPH_GetHumidity(float *humidity);

uint16_t TPH_GetMeasTimeMs();

#endif

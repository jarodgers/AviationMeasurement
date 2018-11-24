/* tph.h
 * Temperature/Pressure/Humidity sensor */

#ifndef TPH_H
#define TPH_H

#include <stdint.h>

#define TPH_INACTIVE        ((uint8_t)0x00)
#define TPH_OVERSAMPLING_1  ((uint8_t)0x01)
#define TPH_OVERSAMPLING_2  ((uint8_t)0x02)
#define TPH_OVERSAMPLING_4  ((uint8_t)0x03)
#define TPH_OVERSAMPLING_8  ((uint8_t)0x04)
#define TPH_OVERSAMPLING_16 ((uint8_t)0x05)

void TPH_Initialize();

void TPH_StartMeasurement(uint8_t temp_oversampling, uint8_t pressure_oversampling, uint8_t humidity_oversampling);

uint8_t TPH_CheckForNewData();

uint8_t TPH_GetPressure(float *pressure);

uint8_t TPH_GetTemperature(float *temperature);

uint8_t TPH_GetHumidity(float *humidity);

#endif

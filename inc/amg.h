/* amg.h
 * Accelerometer/Magnetometer/Gyroscope */

#ifndef AMG_H
#define AMG_H

uint8_t AMG_Initialize();

uint8_t AMG_GetAccelerometerValues(int16_t *acc_x, int16_t *acc_y, int16_t *acc_z);

uint8_t AMG_GetMagnetometerValues(int16_t *mag_x, int16_t *mag_y, int16_t *mag_z);

uint8_t AMG_GetGyroscopeValues(int16_t *gyro_x, int16_t *gyro_y, int16_t *gyro_z);

#endif

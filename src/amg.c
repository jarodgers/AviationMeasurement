#include <stdlib.h>

#include "stm32f4xx.h"

#include "i2c.h"
#include "amg.h"

#define ACCMAG_ADDRESS 0x3c
#define GYRO_ADDRESS 0x40

// FXOS8700CQ (accelerometer/magnetometer) internal register addresses
#define FXOS8700CQ_STATUS 0x00
#define FXOS8700CQ_WHOAMI 0x0D
#define FXOS8700CQ_XYZ_DATA_CFG 0x0E
#define FXOS8700CQ_CTRL_REG1 0x2A
#define FXOS8700CQ_M_CTRL_REG1 0x5B
#define FXOS8700CQ_M_CTRL_REG2 0x5C
#define FXOS8700CQ_WHOAMI_VAL 0xC7

// FXAS21002 (gyroscope) internal register addresses
#define FXAS21002_STATUS 0x00
#define FXAS21002_WHOAMI 0x0C
#define FXAS21002_CTRL_REG0 0x0D
#define FXAS21002_CTRL_REG1 0x13
#define FXAS21002_WHOAMI_VAL 0xD7

static uint8_t _is_init = 0;

uint8_t AMG_Initialize()
{
	if (!I2C_Initialize())
		return 0;

	// check that accelerometer/magnetometer is responding
	uint8_t databyte = 0;
	I2C_ReadRegs(ACCMAG_ADDRESS, FXOS8700CQ_WHOAMI, &databyte, 1);

	if (databyte != FXOS8700CQ_WHOAMI_VAL)
		return 0;

	// check that gyroscope is responding
	databyte = 0;
	I2C_ReadRegs(GYRO_ADDRESS, FXAS21002_WHOAMI, &databyte, 1);

	if (databyte != FXAS21002_WHOAMI_VAL)
		return 0;

	// initialize accelerometer/magnetometer
	// place device in standby
	databyte = 0x00;
	I2C_WriteRegs(ACCMAG_ADDRESS, FXOS8700CQ_CTRL_REG1, &databyte, 1);

	// configure magnetometer for 8x oversampling, hybrid mode (both accelerometer and magnetometer will be active)
	databyte = 0x1f;
	I2C_WriteRegs(ACCMAG_ADDRESS, FXOS8700CQ_M_CTRL_REG1, &databyte, 1);

	// configure magnetometer to use the hybrid mode register auto-increment
	databyte = 0x20;
	I2C_WriteRegs(ACCMAG_ADDRESS, FXOS8700CQ_M_CTRL_REG2, &databyte, 1);

	// use +/- 4g mode
	databyte = 0x01;
	I2C_WriteRegs(ACCMAG_ADDRESS, FXOS8700CQ_XYZ_DATA_CFG, &databyte, 1);

	// 200 Hz sampling (for hybrid mode), low noise mode, active
	databyte = 0x0d;
	I2C_WriteRegs(ACCMAG_ADDRESS, FXOS8700CQ_CTRL_REG1, &databyte, 1);

	// initialize gyroscope
	// reset device
	databyte = 0x40;
	I2C_WriteRegNoAck(GYRO_ADDRESS, FXAS21002_CTRL_REG1, &databyte);

	// 1000dps mode
	databyte = 0x01;
	I2C_WriteRegs(GYRO_ADDRESS, FXAS21002_CTRL_REG0, &databyte, 1);

	// 200 Hz sampling, ready and active
	databyte = 0x0b;
	I2C_WriteRegs(GYRO_ADDRESS, FXAS21002_CTRL_REG1, &databyte, 1);

	_is_init = 1;
	return 1;
}

uint8_t AMG_GetAccelerometerValues(int16_t *acc_x, int16_t *acc_y, int16_t *acc_z)
{
	if (acc_x == NULL || acc_y == NULL || acc_z == NULL)
		return 0;

	uint8_t buf[7];
	if (I2C_ReadRegs(ACCMAG_ADDRESS, FXOS8700CQ_STATUS, buf, 7) == 0)
	{
		uint8_t status = buf[0];
		*acc_x = (int16_t)(buf[1] << 8 | buf[2]) >> 2;
		*acc_y = (int16_t)(buf[3] << 8 | buf[4]) >> 2;
		*acc_z = (int16_t)(buf[5] << 8 | buf[6]) >> 2;
	}

	return 1;
}

uint8_t AMG_GetMagnetometerValues(int16_t *mag_x, int16_t *mag_y, int16_t *mag_z)
{
	if (mag_x == NULL || mag_y == NULL || mag_z == NULL)
		return 0;

	uint8_t buf[13];
	if (I2C_ReadRegs(ACCMAG_ADDRESS, FXOS8700CQ_STATUS, buf, 13) == 0)
	{
		uint8_t status = buf[0];
		*mag_x = (int16_t)(buf[7] << 8 | buf[8]);
		*mag_y = (int16_t)(buf[9] << 8 | buf[10]);
		*mag_z = (int16_t)(buf[11] << 8 | buf[12]);
	}

	return 1;
}

uint8_t AMG_GetGyroscopeValues(int16_t *gyro_x, int16_t *gyro_y, int16_t *gyro_z)
{
	if (gyro_x == NULL || gyro_y == NULL || gyro_z == NULL)
		return 0;

	uint8_t buf[7];
	if (I2C_ReadRegs(GYRO_ADDRESS, FXAS21002_STATUS, buf, 7) == 0)
	{
		uint8_t status = buf[0];
		*gyro_x = (int16_t)(buf[1] << 8 | buf[2]);
		*gyro_y = (int16_t)(buf[3] << 8 | buf[4]);
		*gyro_z = (int16_t)(buf[5] << 8 | buf[6]);
	}

	return 1;
}

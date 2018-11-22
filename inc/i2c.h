#ifndef I2C_H
#define I2C_H

#include "stm32f4xx.h"

void I2C_Initialize();

uint8_t I2C_IsInitialized();

uint8_t I2C_Transmit(uint8_t address, uint8_t *data, uint16_t len);

uint8_t I2C_Receive(uint8_t address, uint8_t *data, uint16_t *num_bytes);

#endif

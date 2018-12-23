#ifndef I2C_H
#define I2C_H

#include "stm32f4xx.h"

uint8_t I2C_Initialize();

uint8_t I2C_IsInitialized();

int8_t I2C_Transmit(uint8_t address, uint8_t *data, uint16_t len);

int8_t I2C_Receive(uint8_t address, uint8_t *data, uint16_t num_bytes);

int8_t I2C_ReadRegs(uint8_t address, uint8_t reg, uint8_t *data, uint16_t num_bytes);

int8_t I2C_WriteRegs(uint8_t address, uint8_t reg, uint8_t *data, uint16_t num_bytes);

int8_t I2C_WriteRegNoAck(uint8_t address, uint8_t reg, uint8_t *data);

#endif

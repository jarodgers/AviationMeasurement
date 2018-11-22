#include "stm32f4xx.h"
#include "i2c.h"

static uint8_t _is_init = 0;

void I2C_Initialize()
{
	// TODO: make sure this is protected by a mutex
	if (!_is_init)
	{
		RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);
		GPIO_InitTypeDef GPIOStruct;
		GPIOStruct.GPIO_Mode = GPIO_Mode_AF;
		GPIOStruct.GPIO_OType = GPIO_OType_OD;
		GPIOStruct.GPIO_PuPd = GPIO_PuPd_NOPULL;
		GPIOStruct.GPIO_Speed = GPIO_Fast_Speed;

		GPIOStruct.GPIO_Pin = GPIO_Pin_6;
		GPIO_Init(GPIOB, &GPIOStruct);

		GPIOStruct.GPIO_Pin = GPIO_Pin_7;
		GPIO_Init(GPIOB, &GPIOStruct);

		GPIO_PinAFConfig(GPIOB, GPIO_PinSource6, GPIO_AF_I2C1);
		GPIO_PinAFConfig(GPIOB, GPIO_PinSource7, GPIO_AF_I2C1);

		RCC_APB1PeriphClockCmd(RCC_APB1Periph_I2C1, ENABLE);
		I2C_InitTypeDef I2CStruct;
		I2C_StructInit(&I2CStruct);
		I2CStruct.I2C_ClockSpeed = 100000;
		I2CStruct.I2C_Ack = I2C_Ack_Enable;
		I2C_Init(I2C1, &I2CStruct);

		I2C_Cmd(I2C1, ENABLE);

		_is_init = 1;
	}
}

uint8_t I2C_IsInitialized()
{
	return _is_init;
}

uint8_t I2C_Transmit(uint8_t address, uint8_t *data, uint16_t len)
{
	if (!_is_init)
	{
		I2C_Initialize();
	}

	I2C_AcknowledgeConfig(I2C1, ENABLE);

	I2C_GenerateSTART(I2C1, ENABLE);
	while (!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_MODE_SELECT))
		; // wait for event

	I2C_Send7bitAddress(I2C1, address, I2C_Direction_Transmitter);
	while (!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED))
		; // wait for event

	uint16_t i;
	for (i = 0; i < len; ++i)
	{
		I2C_SendData(I2C1, data[i]);

		// if we are on the last byte, we wait for a slightly different event than intermediate bytes
		if (i == (len-1))
		{
			while (!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_BYTE_TRANSMITTED))
				; // wait for event
		}
		else
		{
			while (!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_BYTE_TRANSMITTING))
				; // wait for event
		}
	}

	I2C_GenerateSTOP(I2C1, ENABLE);

	return 0;
}

uint8_t I2C_Receive(uint8_t address, uint8_t *data, uint16_t num_bytes)
{
	if (!_is_init)
	{
		I2C_Initialize();
	}

	I2C_AcknowledgeConfig(I2C1, ENABLE);

	I2C_GenerateSTART(I2C1, ENABLE);
	while (!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_MODE_SELECT))
		; // wait for event

	I2C_Send7bitAddress(I2C1, address, I2C_Direction_Receiver);
	while (!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_RECEIVER_MODE_SELECTED))
		; // wait for event

	uint16_t i;
	for (i = 0; i < num_bytes; ++i)
	{
		if (i == (num_bytes-1))
		{
			I2C_AcknowledgeConfig(I2C1, DISABLE);
		}

		while (!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_BYTE_RECEIVED))
			; // wait for event

		data[i] = I2C_ReceiveData(I2C1);
	}

	return 0;
}

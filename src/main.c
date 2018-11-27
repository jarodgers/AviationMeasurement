/**
  ******************************************************************************
  * @file    main.c
  * @author  Ac6
  * @version V1.0
  * @date    01-December-2013
  * @brief   Default main function.
  ******************************************************************************
*/


#include "stm32f4xx.h"

#include "FreeRTOS.h"
#include "task.h"

#include "tph.h"
#include "i2c.h"
#include "util.h"

void TestTask(void *pvParameters)
{
	/*I2C_Initialize();
	uint8_t test = 0xd0;
	uint8_t result;
	I2C_Transmit(0xec, &test, 1);
	I2C_Receive(0xec, &result, 1);*/
	/*for (;;)
	{
		GPIO_ToggleBits(GPIOA, GPIO_Pin_5);
		Util_DelayMs(1);
	}*/
	float pressure = 0.0f;
	if(TPH_Initialize() == 0)
	{
		uint16_t meas_time;
		TickType_t xLastWakeTime = xTaskGetTickCount();
		int8_t result;
		for (;;)
		{
			Util_DelayMs(50);
			result = TPH_StartMeasurement();
			meas_time = TPH_GetMeasTimeMs();
			while (!TPH_CheckForNewData())
				;
			TPH_GetPressure(&pressure, TPH_PRESSURE_INHG);
			vTaskDelayUntil(&xLastWakeTime, meas_time);
		}
	}

	for (;;);
}

int main(void)
{
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);
	GPIO_InitTypeDef GPIOStruct;
	GPIO_StructInit(&GPIOStruct);
	GPIOStruct.GPIO_Pin = GPIO_Pin_5;
	GPIOStruct.GPIO_Mode = GPIO_Mode_OUT;
	GPIO_Init(GPIOA, &GPIOStruct);
	xTaskCreate( TestTask,
	"TestTask",
	256,
	NULL,
	1,
	NULL );

	vTaskStartScheduler();

	for(;;);
}

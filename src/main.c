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

#include "i2c.h"

void TestTask(void *pvParameters)
{
	//TickType_t xLastWakeTime = xTaskGetTickCount();
	I2C_Initialize();
	uint8_t test[2] = {0xe0, 0xb6};
	I2C_Transmit(0xec, test, 2);
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
	128,
	NULL,
	1,
	NULL );

	vTaskStartScheduler();

	for(;;);
}

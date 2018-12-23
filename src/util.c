#include "stm32f4xx.h"
#include "util.h"

static volatile uint8_t _is_init = 0;
static volatile uint8_t _timeout = 0;

void TIM2_IRQHandler()
{
	if (TIM_GetITStatus(TIM2, TIM_IT_Update) == SET)
	{
		_timeout = 1;
		TIM_Cmd(TIM2, DISABLE);

		TIM_ClearITPendingBit(TIM2, TIM_IT_Update);
	}
}

static void _init()
{
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);
	TIM_TimeBaseInitTypeDef TIMStruct;
	TIM_TimeBaseStructInit(&TIMStruct);
	TIMStruct.TIM_Prescaler = 44999; // 0.5 ms per period
	TIM_TimeBaseInit(TIM2, &TIMStruct);
	TIM_ITConfig(TIM2, TIM_IT_Update, ENABLE);

	NVIC_InitTypeDef NVICStruct;
	NVICStruct.NVIC_IRQChannel = TIM2_IRQn;
	NVICStruct.NVIC_IRQChannelCmd = ENABLE;
	NVICStruct.NVIC_IRQChannelPreemptionPriority = 2;
	NVIC_Init(&NVICStruct);

	_is_init = 1;
}

void Util_DelayMs(uint32_t msec)
{
	// TODO: protect this function with mutex
	if (!_is_init)
		_init();

	if (msec == 0)
		return;

	TIM_SetCounter(TIM2, 0);
	TIM_SetAutoreload(TIM2, msec * 2 - 1);

	_timeout = 0;
	TIM_Cmd(TIM2, ENABLE);
	while (!_timeout)
		;
}

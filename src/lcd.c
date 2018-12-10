#include "stm32f4xx.h"
#include "lcd.h"

#include "FreeRTOS.h"
#include "task.h"

#define SR_RCLK_PORT GPIOB
#define SR_RCLK_PIN GPIO_Pin_13
#define SR_SRCLK_PORT GPIOB
#define SR_SRCLK_PIN GPIO_Pin_14
#define SR_DATA_PORT GPIOB
#define SR_DATA_PIN GPIO_Pin_15

#define LCD_RS_PORT GPIOC
#define LCD_RS_PIN GPIO_Pin_5
#define LCD_EN_PORT GPIOC
#define LCD_EN_PIN GPIO_Pin_6

enum LCD_CommType {LCD_Instruction, LCD_Data};

static volatile uint8_t _is_init = 0;

void _shiftOut(uint8_t data)
{
	uint8_t i;
	for (i = 0; i < 8; ++i)
	{
		GPIO_WriteBit(SR_DATA_PORT, SR_DATA_PIN, data & (1 << i) ? Bit_SET : Bit_RESET);
		//__NOP(); __NOP(); __NOP();

		GPIO_WriteBit(SR_SRCLK_PORT, SR_SRCLK_PIN, Bit_SET);
		//__NOP(); __NOP(); __NOP();
		GPIO_WriteBit(SR_SRCLK_PORT, SR_SRCLK_PIN, Bit_RESET);
		//__NOP(); __NOP(); __NOP();
	}

	GPIO_WriteBit(SR_RCLK_PORT, SR_RCLK_PIN, Bit_SET);
	//__NOP(); __NOP(); __NOP();
	GPIO_WriteBit(SR_RCLK_PORT, SR_RCLK_PIN, Bit_RESET);
	//__NOP(); __NOP(); __NOP();
}

void _lcd_write(enum LCD_CommType comm_type, uint8_t data)
{
	if (comm_type == LCD_Instruction)
		GPIO_WriteBit(LCD_RS_PORT, LCD_RS_PIN, Bit_RESET);
	else
		GPIO_WriteBit(LCD_RS_PORT, LCD_RS_PIN, Bit_SET);

	_shiftOut(data);

	__NOP(); __NOP(); __NOP(); __NOP(); __NOP(); __NOP();
	GPIO_WriteBit(LCD_EN_PORT, LCD_EN_PIN, Bit_SET);
	__NOP(); __NOP(); __NOP(); __NOP(); __NOP(); __NOP();
	GPIO_WriteBit(LCD_EN_PORT, LCD_EN_PIN, Bit_RESET);

	vTaskDelay(pdMS_TO_TICKS(1));
}

void LCD_Init()
{
	// TODO: protect all of this with mutex
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC, ENABLE);
	GPIO_InitTypeDef GPIOStruct;
	GPIO_StructInit(&GPIOStruct);
	GPIOStruct.GPIO_Mode = GPIO_Mode_OUT;
	GPIOStruct.GPIO_Speed = GPIO_Fast_Speed;

	GPIOStruct.GPIO_Pin = SR_DATA_PIN;
	GPIO_Init(SR_DATA_PORT, &GPIOStruct);

	GPIOStruct.GPIO_Pin = SR_SRCLK_PIN;
	GPIO_Init(SR_SRCLK_PORT, &GPIOStruct);

	GPIOStruct.GPIO_Pin = SR_RCLK_PIN;
	GPIO_Init(SR_RCLK_PORT, &GPIOStruct);

	GPIOStruct.GPIO_Pin = LCD_RS_PIN;
	GPIO_Init(LCD_RS_PORT, &GPIOStruct);

	GPIO_WriteBit(LCD_EN_PORT, LCD_EN_PIN, Bit_RESET);
	GPIOStruct.GPIO_Pin = LCD_EN_PIN;
	GPIO_Init(LCD_EN_PORT, &GPIOStruct);

	vTaskDelay(pdMS_TO_TICKS(25));

	_lcd_write(LCD_Instruction, 0x38);
	_lcd_write(LCD_Instruction, 0x0c);

	_is_init = 1;
}

void LCD_Clear()
{
	if (!_is_init)
		return;

	_lcd_write(LCD_Instruction, 0x01); // clear screen
	vTaskDelay(pdMS_TO_TICKS(5));
	//_lcd_write(LCD_Instruction, 0x02); // return home (cursor to 0, unshift display)
}

void LCD_MoveCursor(uint8_t row, uint8_t col)
{
	if (!_is_init)
		return;

	if (row >= 2 || col >= 40)
		return;

	uint8_t pos = row*40 + col;
	_lcd_write(LCD_Instruction, 0x80 | pos);
}

void LCD_WriteString(char *str, uint8_t len)
{
	if (!_is_init)
		return;

	uint8_t i;
	for (i = 0; i < len; ++i)
	{
		_lcd_write(LCD_Data, str[i]);
	}
}

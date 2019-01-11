/**
  ******************************************************************************
  * @file    main.c
  * @author  Ac6
  * @version V1.0
  * @date    01-December-2013
  * @brief   Default main function.
  ******************************************************************************
*/

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#include "stm32f4xx.h"

#include "FreeRTOS.h"
#include "task.h"

#include "newlib.h"

#include "tph.h"
#include "lcd.h"
#include "gps.h"
#include "amg.h"
#include "util.h"
#include "ff.h"

static uint8_t update_display = 1;

static uint16_t altimeter_value = 2992;
#define ALTIMETER_INC_PORT GPIOB
#define ALTIMETER_INC_PIN GPIO_Pin_10
#define ALTIMETER_INC_EXTI_LINE EXTI_Line10
#define ALTIMETER_INC_EXTI_PORT_SOURCE EXTI_PortSourceGPIOB
#define ALTIMETER_INC_EXTI_PIN_SOURCE EXTI_PinSource10
#define ALTIMETER_DEC_PORT GPIOA
#define ALTIMETER_DEC_PIN GPIO_Pin_12
#define ALTIMETER_DEC_EXTI_LINE EXTI_Line12
#define ALTIMETER_DEC_EXTI_PORT_SOURCE EXTI_PortSourceGPIOA
#define ALTIMETER_DEC_EXTI_PIN_SOURCE EXTI_PinSource12

#define SCREEN_CHANGE_PORT GPIOA
#define SCREEN_CHANGE_PIN GPIO_Pin_11
#define SCREEN_CHANGE_EXTI_LINE EXTI_Line11
#define SCREEN_CHANGE_EXTI_PORT_SOURCE EXTI_PortSourceGPIOA
#define SCREEN_CHANGE_EXTI_PIN_SOURCE EXTI_PinSource11
#define NUM_SCREENS 3
#define SCREEN_COORDS 0
#define SCREEN_HEADING_SPEED 1
#define SCREEN_DATETIME 2

static uint8_t current_screen = 0;

static char printable_string[256] = {0};
static float latitude = 0.0f;
static float longitude = 0.0f;
static float mag_heading = 0.0f;
static float gs_knots = 0.0f;
static int num_sats = -1;
static float altitude = 0.0f;
static struct timespec ts = {0};

struct datetime
{
	int year;
	int month;
	int day;
	int hours;
	int minutes;
	int seconds;
};

static struct datetime starting_datetime = {-1, -1, -1, -1, -1, -1};
static struct datetime current_datetime = {-1, -1, -1, -1, -1, -1};

#define RAW_DATA_POINT_SIZE 24
#define TIMESTAMP_OFFSET 0
#define TIMESTAMP_LEN 4
#define LATITUDE_OFFSET 4
#define LATITUDE_LEN 4
#define LONGITUDE_OFFSET 8
#define LONGITUDE_LEN 4
#define ALTITUDE_OFFSET 12
#define ALTITUDE_LEN 4
#define HEADING_OFFSET 16
#define HEADING_LEN 4
#define SPEED_OFFSET 20
#define SPEED_LEN 4

void InitLED()
{
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);
	GPIO_InitTypeDef GPIOStruct;
	GPIO_StructInit(&GPIOStruct);
	GPIOStruct.GPIO_Pin = GPIO_Pin_5;
	GPIOStruct.GPIO_Mode = GPIO_Mode_OUT;
	GPIO_ResetBits(GPIOA, GPIO_Pin_5);
	GPIO_Init(GPIOA, &GPIOStruct);
}

void SetLED(uint8_t state)
{
	state ? GPIO_SetBits(GPIOA, GPIO_Pin_5) : GPIO_ResetBits(GPIOA, GPIO_Pin_5);
}

// thread-safe sprintf
int SPRINTF(char *s, const char *template, ...)
{
	int ret;
	vTaskSuspendAll();
	va_list args;
    va_start(args, template);
    ret = vsprintf(s, template, args);
    va_end(args);
	( void ) xTaskResumeAll();

	return ret;
}

uint8_t PrintableFloat(float f, char *str, uint8_t precision)
{
	int32_t whole_num;

	str[0] = '\0'; // empty string in case there is an error
	if (precision > 5 || precision == 0)
		return 0;
	whole_num = (int32_t)f;

	float remainder = f - (float)whole_num;
	if (remainder < 0.0f)
		remainder *= -1.0f;

	// pre-round the remainder
	float multiplier = powf(10.0f,(float)precision);
	remainder = roundf(remainder * multiplier);
	remainder /= multiplier;

	SPRINTF(str,"%d.",whole_num);

	// print leading zeroes for fractional part
	uint8_t i;
	for (i = 0; i < precision; ++i)
	{
		remainder *= 10.0f;
		if ((uint32_t)remainder == 0)
			SPRINTF(str+strlen(str),"0");
	}
	if ((uint32_t)remainder > 0)
	{
		SPRINTF(str+strlen(str),"%lu",(uint32_t)remainder);
	}

	return 1;
}

void DisplayTask(void *pvParameters)
{
	LCD_Init();
	LCD_Clear();

	char latitude_str[16] = {0};
	char longitude_str[16] = {0};

	for (;;)
	{
		// wait for an update request for the display
		while (!update_display)
			vTaskDelay(pdMS_TO_TICKS(4));
		update_display = 0;

		switch (current_screen)
		{
		case SCREEN_COORDS:
			PrintableFloat(latitude,latitude_str,2);
			PrintableFloat(longitude,longitude_str,2);
			SPRINTF(printable_string,"S%02d,%s,%s           ",num_sats,latitude_str,longitude_str);
			break;
		case SCREEN_HEADING_SPEED:
			SPRINTF(printable_string,"H%03u, %ukt           ",(uint32_t)mag_heading,(uint32_t)gs_knots);
			break;
		case SCREEN_DATETIME:
			SPRINTF(printable_string,"%02d/%02d/%02d %02d%02dZ      ",
				current_datetime.month,
				current_datetime.day,
				current_datetime.year,
				current_datetime.hours,
				current_datetime.minutes
			);
			break;
		default:
			printable_string[0] = '\0'; // display blank string
			break;
		}

		LCD_MoveCursor(0,0);
		LCD_WriteString(printable_string,strlen(printable_string));

		// second line always shows the altimeter
		SPRINTF(printable_string,"%02u.%02u -> %u'       ",altimeter_value/100,altimeter_value%100,(uint32_t)altitude);
		LCD_MoveCursor(1,0);
		LCD_WriteString(printable_string,strlen(printable_string));
	}
}

void GPSTask(void *pvParameters)
{
	if (!GPS_Initialize())
		for (;;) vTaskDelay(pdMS_TO_TICKS(100));

	for (;;)
	{
		if (GPS_CheckForNewData())
		{
			GPS_GetNumSats(&num_sats);
			GPS_GetCoords(&latitude,&longitude);
			GPS_GetHeading(NULL,&mag_heading);
			GPS_GetGroundSpeedKnots(&gs_knots);
			GPS_GetDateTime(
				&current_datetime.year,
				&current_datetime.month,
				&current_datetime.day,
				&current_datetime.hours,
				&current_datetime.minutes,
				&current_datetime.seconds);
			GPS_GetUNIXTimestamp(&ts);

			// the first time we get a valid datetime, set as the starting datetime
			if (current_datetime.year != -1 && starting_datetime.year == -1)
				starting_datetime = current_datetime;

			update_display = 1;
		}

		vTaskDelay(pdMS_TO_TICKS(200));
	}
}

void TPHTask(void *pvParameters)
{
	TPH_Initialize();

	for (;;)
	{
		TPH_StartMeasurement();
		while (!TPH_GetAltitude(&altitude, ((float)altimeter_value)/100.0f, TPH_ALTITUDE_FT, TPH_PRESSURE_INHG))
			vTaskDelay(pdMS_TO_TICKS(5));

		update_display = 1;

		vTaskDelay(pdMS_TO_TICKS(500));
	}
}

void SDTask(void *pvParameters)
{
	FRESULT res;
	FATFS fs;
	FIL file;
	UINT num_written = 0;
	char filename[32] = {0};
	uint8_t raw_data[RAW_DATA_POINT_SIZE] = {0};

	do
	{
		res = f_mount(&fs,"",1);
		if (res != FR_OK)
			vTaskDelay(pdMS_TO_TICKS(5000));
	} while (res != FR_OK);

	// enter the directory with the measurements
	res = f_chdir("/Meas");
	if (res != FR_OK)
		; // TODO: error handling

	// we are going to name the files by the starting datetime, get the date from the GPS, and do not create a file until we have a date
	while (starting_datetime.year == -1)
		vTaskDelay(pdMS_TO_TICKS(500));

	//SPRINTF(filename,"20%02d%02d%02d-%02d%02d%02d.dat",
	SPRINTF(filename,"%02d%02d%02d.dat",
		//starting_datetime.year,
		//starting_datetime.month,
		//starting_datetime.day,
		starting_datetime.hours,
		starting_datetime.minutes,
		starting_datetime.seconds
	);

	TickType_t xLastWakeTime = xTaskGetTickCount();

	for (;;)
	{
		vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(1000));

		// open the file
		res = f_open(&file,filename,FA_WRITE|FA_OPEN_APPEND);
		if (res != FR_OK)
			; // TODO:  error handling

		// write raw UNIX timestamp, coordinates, altitude, heading, and ground speed into file
		memcpy(raw_data+TIMESTAMP_OFFSET,&ts.tv_sec,TIMESTAMP_LEN);
		memcpy(raw_data+LATITUDE_OFFSET,&latitude,LATITUDE_LEN);
		memcpy(raw_data+LONGITUDE_OFFSET,&longitude,LONGITUDE_LEN);
		memcpy(raw_data+ALTITUDE_OFFSET,&altitude,ALTITUDE_LEN);
		memcpy(raw_data+HEADING_OFFSET,&mag_heading,HEADING_LEN);
		memcpy(raw_data+SPEED_OFFSET,&gs_knots,SPEED_LEN);
		SetLED(1);
		res = f_write(&file,raw_data,RAW_DATA_POINT_SIZE,&num_written);
		SetLED(0);
		if (res != FR_OK || num_written != RAW_DATA_POINT_SIZE)
			; // TODO: error handling

		// close file after we finish writing this data point
		res = f_close(&file);
	}
}

void TestTask(void *pvParameters)
{
	for (;;);
}

void EXTI15_10_IRQHandler(void)
{
	if (EXTI_GetITStatus(ALTIMETER_INC_EXTI_LINE) == SET)
	{
		++altimeter_value;
		update_display = 1;
		EXTI_ClearITPendingBit(ALTIMETER_INC_EXTI_LINE);
	}

	if (EXTI_GetITStatus(ALTIMETER_DEC_EXTI_LINE) == SET)
	{
		--altimeter_value;
		update_display = 1;
		EXTI_ClearITPendingBit(ALTIMETER_DEC_EXTI_LINE);
	}

	if (EXTI_GetITStatus(SCREEN_CHANGE_EXTI_LINE) == SET)
	{
		++current_screen;
		if (current_screen >= NUM_SCREENS)
			current_screen = 0;
		update_display = 1;
		EXTI_ClearITPendingBit(SCREEN_CHANGE_EXTI_LINE);
	}
}

void AltimeterGPIOInit()
{
	// init GPIO pins
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);
	GPIO_InitTypeDef GPIOStruct;
	GPIO_StructInit(&GPIOStruct);
	GPIOStruct.GPIO_Mode = GPIO_Mode_IN;

	GPIOStruct.GPIO_Pin = ALTIMETER_INC_PIN;
	GPIO_Init(ALTIMETER_INC_PORT, &GPIOStruct);

	GPIOStruct.GPIO_Pin = ALTIMETER_DEC_PIN;
	GPIO_Init(ALTIMETER_DEC_PORT, &GPIOStruct);

	GPIOStruct.GPIO_Pin = SCREEN_CHANGE_PIN;
	GPIO_Init(SCREEN_CHANGE_PORT, &GPIOStruct);

	// init interrupts
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_SYSCFG, ENABLE);
	NVIC_InitTypeDef NVICStruct;
	NVICStruct.NVIC_IRQChannel = EXTI15_10_IRQn;
	NVICStruct.NVIC_IRQChannelCmd = ENABLE;
	NVICStruct.NVIC_IRQChannelPreemptionPriority = 10;
	NVIC_Init(&NVICStruct);

	EXTI_InitTypeDef EXTIStruct;
	EXTIStruct.EXTI_LineCmd = ENABLE;
	EXTIStruct.EXTI_Mode = EXTI_Mode_Interrupt;
	EXTIStruct.EXTI_Trigger = EXTI_Trigger_Rising;
	EXTIStruct.EXTI_Line = ALTIMETER_INC_EXTI_LINE | ALTIMETER_DEC_EXTI_LINE | SCREEN_CHANGE_EXTI_LINE;
	EXTI_Init(&EXTIStruct);

	SYSCFG_EXTILineConfig(ALTIMETER_INC_EXTI_PORT_SOURCE, ALTIMETER_INC_EXTI_PIN_SOURCE);
	SYSCFG_EXTILineConfig(ALTIMETER_DEC_EXTI_PORT_SOURCE, ALTIMETER_DEC_EXTI_PIN_SOURCE);
	SYSCFG_EXTILineConfig(SCREEN_CHANGE_EXTI_PORT_SOURCE, SCREEN_CHANGE_EXTI_PIN_SOURCE);
}

int main(void)
{
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_4);

	InitLED();
	AltimeterGPIOInit();

	xTaskCreate(
		DisplayTask,
		"DisplayTask",
		256,
		NULL,
		2,
		NULL
	);

	xTaskCreate(
		GPSTask,
		"GPSTask",
		8192,
		NULL,
		3,
		NULL
	);

	xTaskCreate(
		TPHTask,
		"TPHTask",
		512,
		NULL,
		3,
		NULL
	);

	xTaskCreate(
		SDTask,
		"SDTask",
		4096,
		NULL,
		1,
		NULL
	);

	vTaskStartScheduler();

	for(;;);
}

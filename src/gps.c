#include <string.h>

#include "stm32f4xx.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#include "gps.h"
#include "minmea.h"

#define GPS_RX_PORT GPIOA
#define GPS_RX_PIN GPIO_Pin_10
#define GPS_RX_PINSOURCE GPIO_PinSource10
#define GPS_TX_PORT GPIOA
#define GPS_TX_PIN GPIO_Pin_9
#define GPS_TX_PINSOURCE GPIO_PinSource9

// Defines taken from Adafruit GPS library:

// different commands to set the update rate from once a second (1 Hz) to 10 times a second (10Hz)
// Note that these only control the rate at which the position is echoed, to actually speed up the
// position fix you must also send one of the position fix rate commands below too.
#define PMTK_SET_NMEA_UPDATE_100_MILLIHERTZ  "$PMTK220,10000*2F" // Once every 10 seconds, 100 millihertz.
#define PMTK_SET_NMEA_UPDATE_200_MILLIHERTZ  "$PMTK220,5000*1B"  // Once every 5 seconds, 200 millihertz.
#define PMTK_SET_NMEA_UPDATE_1HZ  "$PMTK220,1000*1F"
#define PMTK_SET_NMEA_UPDATE_2HZ  "$PMTK220,500*2B"
#define PMTK_SET_NMEA_UPDATE_5HZ  "$PMTK220,200*2C"
#define PMTK_SET_NMEA_UPDATE_10HZ "$PMTK220,100*2F"
// Position fix update rate commands.
#define PMTK_API_SET_FIX_CTL_100_MILLIHERTZ  "$PMTK300,10000,0,0,0,0*2C" // Once every 10 seconds, 100 millihertz.
#define PMTK_API_SET_FIX_CTL_200_MILLIHERTZ  "$PMTK300,5000,0,0,0,0*18"  // Once every 5 seconds, 200 millihertz.
#define PMTK_API_SET_FIX_CTL_1HZ  "$PMTK300,1000,0,0,0,0*1C"
#define PMTK_API_SET_FIX_CTL_5HZ  "$PMTK300,200,0,0,0,0*2F"
// Can't fix position faster than 5 times a second!


#define PMTK_SET_BAUD_57600 "$PMTK251,57600*2C"
#define PMTK_SET_BAUD_9600 "$PMTK251,9600*17"

// turn on only the second sentence (GPRMC)
#define PMTK_SET_NMEA_OUTPUT_RMCONLY "$PMTK314,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0*29"
// turn on GPRMC and GGA
#define PMTK_SET_NMEA_OUTPUT_RMCGGA "$PMTK314,0,1,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0*28"
// turn on ALL THE DATA
#define PMTK_SET_NMEA_OUTPUT_ALLDATA "$PMTK314,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0*28"
// turn off output
#define PMTK_SET_NMEA_OUTPUT_OFF "$PMTK314,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0*28"

// to generate your own sentences, check out the MTK command datasheet and use a checksum calculator
// such as the awesome http://www.hhhh.org/wiml/proj/nmeaxor.html

#define PMTK_LOCUS_STARTLOG  "$PMTK185,0*22"
#define PMTK_LOCUS_STOPLOG "$PMTK185,1*23"
#define PMTK_LOCUS_STARTSTOPACK "$PMTK001,185,3*3C"
#define PMTK_LOCUS_QUERY_STATUS "$PMTK183*38"
#define PMTK_LOCUS_ERASE_FLASH "$PMTK184,1*22"
#define LOCUS_OVERLAP 0
#define LOCUS_FULLSTOP 1

#define PMTK_ENABLE_SBAS "$PMTK313,1*2E"
#define PMTK_ENABLE_WAAS "$PMTK301,2*2E"

// standby command & boot successful message
#define PMTK_STANDBY "$PMTK161,0*28"
#define PMTK_STANDBY_SUCCESS "$PMTK001,161,3*36"  // Not needed currently
#define PMTK_AWAKE "$PMTK010,002*2D"

// ask for the release and version
#define PMTK_Q_RELEASE "$PMTK605*31"

// request for updates on antenna status
#define PGCMD_ANTENNA "$PGCMD,33,1*6C"
#define PGCMD_NOANTENNA "$PGCMD,33,0*6D"

// ------- end of Adafruit defines

#define BUF_LEN 128 // NMEA sentences evidently have a max length of 82 characters, but give some extra just in case
#define QUEUE_LEN 8
static char _sentence[BUF_LEN] = {0};
static QueueHandle_t _rx_queue = NULL;
static volatile uint8_t _rx_queue_full = 0;

static char _rx_buf[BUF_LEN] = {0};
static volatile uint8_t _rx_index = 0;
static volatile uint8_t _rx_len = 0;

static volatile uint8_t _is_transmitting = 0;
static uint8_t _tx_buf[BUF_LEN] = {0};

static struct minmea_float _latitude = {0,0};
static struct minmea_float _longitude = {0,0};
static struct minmea_float _track_true = {0,0};
static struct minmea_float _gs_knots = {0,0};
static struct minmea_float _mag_variation = {0,0};
static struct minmea_float _altitude = {0,0};
static int _num_sats = -1;

static char _altitude_units = '*';

static uint8_t _interpret_nmea(char *sentence)
{
	enum minmea_sentence_id sentence_id = minmea_sentence_id(sentence, 0);
	switch(sentence_id)
	{
	case MINMEA_INVALID:
	case MINMEA_UNKNOWN:
		return 0;
	case MINMEA_SENTENCE_RMC:
	{
		struct minmea_sentence_rmc rmc_sentence;
		if (!minmea_parse_rmc(&rmc_sentence, sentence))
			return 0;
		_latitude = rmc_sentence.latitude;
		_longitude = rmc_sentence.longitude;
		_gs_knots = rmc_sentence.speed;
		_mag_variation = rmc_sentence.variation;
		_track_true = rmc_sentence.course;
		// TODO: get date/time
		break;
	}
	case MINMEA_SENTENCE_GGA:
	{
		struct minmea_sentence_gga gga_sentence;
		if (!minmea_parse_gga(&gga_sentence, sentence))
			return 0;
		_latitude = gga_sentence.latitude;
		_longitude = gga_sentence.longitude;
		_altitude = gga_sentence.altitude;
		_altitude_units = gga_sentence.altitude_units;
		break;
	}
	case MINMEA_SENTENCE_GSA:
	{
		struct minmea_sentence_gsa gsa_sentence;
		if (!minmea_parse_gsa(&gsa_sentence, sentence))
			return 0;
		break;
	}
	case MINMEA_SENTENCE_GLL:
		return 0;
	case MINMEA_SENTENCE_GST:
		return 0;
	case MINMEA_SENTENCE_GSV:
	{
		struct minmea_sentence_gsv gsv_sentence;
		if (!minmea_parse_gsv(&gsv_sentence, sentence))
			return 0;
		_num_sats = gsv_sentence.total_sats;
		break;
	}
	case MINMEA_SENTENCE_VTG:
	{
		struct minmea_sentence_vtg vtg_sentence;
		if (!minmea_parse_vtg(&vtg_sentence, sentence))
			return 0;
		_track_true = vtg_sentence.true_track_degrees;
		_gs_knots = vtg_sentence.speed_knots;
		break;
	}
	case MINMEA_SENTENCE_ZDA:
		return 0;
	}

	return 1;
}

void USART1_IRQHandler(void)
{
	if (USART_GetITStatus(USART1, USART_IT_RXNE) == SET)
	{
		char c = (char)USART_ReceiveData(USART1);

		// zero the buffer if this is the first piece of data we are receiving
		if (_rx_index == 0)
		{
			_rx_len = 0;
			memset(_rx_buf,0,BUF_LEN);
		}

		// process the byte
		if (c == '\r')
		{
			// eat
		}
		else if (c == '\n') // this indicates the end of a sentence
		{
			_rx_len = _rx_index + 1;
			_rx_index = 0;

			if (xQueueSendToBackFromISR(_rx_queue, _rx_buf, NULL) == errQUEUE_FULL)
			{
				// TODO: give error response
				_rx_queue_full = 1;
			}
		}
		else // put the byte into the receive buffer
		{
			_rx_buf[_rx_index++] = c;
		}

		USART_ClearITPendingBit(USART1, USART_IT_RXNE);
	}

	if (USART_GetITStatus(USART1, USART_IT_TC) == SET)
	{
		_is_transmitting = 0;
		USART_ClearITPendingBit(USART1, USART_IT_TC);
	}
}

static uint8_t _gps_send_command(char *cmd, uint8_t len)
{
	if (_is_transmitting)
		return 0;

	memcpy(_tx_buf,cmd,len);
	DMA_SetCurrDataCounter(DMA2_Stream7, len);
	_is_transmitting = 1;
	USART_ClearFlag(USART1, USART_FLAG_TC);
	USART_DMACmd(USART1, USART_DMAReq_Tx, ENABLE);

	return 1;
}

uint8_t GPS_Initialize()
{
	_rx_queue = xQueueCreate(QUEUE_LEN, BUF_LEN);
	if (_rx_queue == NULL)
		return 0;

	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);
	GPIO_InitTypeDef GPIOStruct;
	GPIO_StructInit(&GPIOStruct);
	GPIOStruct.GPIO_Mode = GPIO_Mode_AF;
	GPIOStruct.GPIO_Speed = GPIO_Medium_Speed;

	GPIOStruct.GPIO_Pin = GPS_RX_PIN;
	GPIO_Init(GPS_RX_PORT, &GPIOStruct);

	GPIOStruct.GPIO_Pin = GPS_TX_PIN;
	GPIO_Init(GPS_TX_PORT, &GPIOStruct);

	GPIO_PinAFConfig(GPS_RX_PORT, GPS_RX_PINSOURCE, GPIO_AF_USART1);
	GPIO_PinAFConfig(GPS_TX_PORT, GPS_TX_PINSOURCE, GPIO_AF_USART1);

	RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, ENABLE);
	USART_InitTypeDef UARTStruct;
	USART_StructInit(&UARTStruct);
	USART_Init(USART1, &UARTStruct);

	USART_Cmd(USART1, ENABLE);

	NVIC_InitTypeDef NVICStruct;
	NVICStruct.NVIC_IRQChannelCmd = ENABLE;
	NVICStruct.NVIC_IRQChannel = USART1_IRQn;
	NVICStruct.NVIC_IRQChannelPreemptionPriority = 6;
	NVIC_Init(&NVICStruct);

	USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);

	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_DMA2, ENABLE);
	DMA_InitTypeDef DMAStruct;
	DMA_StructInit(&DMAStruct);
	DMAStruct.DMA_Channel = DMA_Channel_4;
	DMAStruct.DMA_PeripheralBaseAddr = (uint32_t) (&(USART1->DR));
	DMAStruct.DMA_Memory0BaseAddr = (uint32_t)_tx_buf;
	DMAStruct.DMA_DIR = DMA_DIR_MemoryToPeripheral;
	DMAStruct.DMA_BufferSize = BUF_LEN;
	DMAStruct.DMA_Priority = DMA_Priority_High;
	DMAStruct.DMA_MemoryInc = DMA_MemoryInc_Enable;
	DMA_Init(DMA2_Stream7, &DMAStruct);
	DMA_Cmd(DMA2_Stream7, ENABLE);

	USART_ClearFlag(USART1, USART_FLAG_TC);
	USART_ITConfig(USART1, USART_IT_TC, ENABLE);

	while(!_gps_send_command(PMTK_SET_NMEA_UPDATE_1HZ, strlen(PMTK_SET_NMEA_UPDATE_1HZ)));
	while(!_gps_send_command(PMTK_SET_NMEA_OUTPUT_RMCGGA, strlen(PMTK_SET_NMEA_OUTPUT_RMCGGA)));

	return 1;
}

uint8_t GPS_GetLastNMEA(char *buf)
{
	memcpy(buf,_sentence,strlen(_sentence));

	return 1;
}

uint8_t GPS_CheckForNewData()
{
	if (xQueueReceive(_rx_queue, _sentence, 0) == pdPASS)
	{
		_interpret_nmea(_sentence);
		return 1;
	}

	return 0;
}

uint8_t GPS_GetCoords(float *latitude, float *longitude)
{
	if (latitude == NULL || longitude == NULL)
		return 0;

	*latitude = minmea_tocoord(&_latitude);
	*longitude = minmea_tocoord(&_longitude);

	return 1;
}

uint8_t GPS_GetHeading(float *true_heading, float *mag_heading)
{
	if (true_heading == NULL || mag_heading == NULL)
		return 0;

	*true_heading = minmea_tofloat(&_track_true);
	*mag_heading = minmea_tofloat(&_track_true) + minmea_tofloat(&_mag_variation);

	return 1;
}

uint8_t GPS_GetAltitude(float *altitude, char *altitude_units)
{
	if (altitude == NULL || altitude_units == NULL)
		return 0;

	*altitude = minmea_tofloat(&_altitude);
	*altitude_units = _altitude_units;

	return 1;
}

uint8_t GPS_GetGroundSpeedKnots(float *gs_knots)
{
	if (gs_knots == NULL)
		return 0;

	*gs_knots = minmea_tofloat(&_gs_knots);

	return 1;
}

uint8_t GPS_GetNumSats(int *num_sats)
{
	if (num_sats == NULL)
		return 0;

	*num_sats = _num_sats;

	return 1;
}

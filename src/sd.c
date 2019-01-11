#include <stdlib.h>

#include "FreeRTOS.h"
#include "task.h"

#include "stm32f4xx.h"
#include "sd.h"

#define SDIO_CLK_DIV_INIT 0xe0
#define SDIO_CLK_DIV_NORMAL 0x00

#define SDIO_CK_PORT GPIOC
#define SDIO_CK_PIN GPIO_Pin_12
#define SDIO_CK_PINSOURCE GPIO_PinSource12
#define SDIO_D0_PORT GPIOC
#define SDIO_D0_PIN GPIO_Pin_8
#define SDIO_D0_PINSOURCE GPIO_PinSource8
#define SDIO_CMD_PORT GPIOD
#define SDIO_CMD_PIN GPIO_Pin_2
#define SDIO_CMD_PINSOURCE GPIO_PinSource2

#define SDIO_DMA_STREAM DMA2_Stream3
#define SDIO_DMA_TC_INT DMA_IT_TCIF3
#define DMA_DIR_MASK ((uint32_t)0x000000c0)

enum SDIO_Command
{
	SDIO_GO_IDLE_STATE = 0,
	SDIO_SEND_ALL_CID = 2,
	SDIO_SEND_REL_ADDR = 3,
	SDIO_SEL_DESEL_CARD = 7,
	SDIO_SEND_IF_COND = 8,
	SDIO_SEND_CSD = 9,
	SDIO_SEND_STATUS = 13,
	SDIO_SET_BLOCKLEN = 16,
	SDIO_READ_SINGLE_BLOCK = 17,
	SDIO_WRITE_SINGLE_BLOCK = 24,
	SDIO_APP_OP_COND = 41,
	SDIO_APP_CMD = 55
};

enum SDIO_Error
{
	SDIO_OK,
	SDIO_TIMEOUT,
	SDIO_CRCFAIL,
	SDIO_ILLEGAL_CMD,
	SDIO_RXOVERRUN,
	SDIO_TXUNDERRUN,
	SDIO_ERROR
};

enum SDIO_CardType
{
	SDIO_STD_CAPACITY_V1_1,
	SDIO_STD_CAPACITY_V2_0,
	SDIO_HIGH_CAPACITY
};
static enum SDIO_CardType _card_type = SDIO_STD_CAPACITY_V1_1;

#define SDIO_STATIC_FLAGS ((uint32_t)0x000005FF)
#define SDIO_OCR_ERRORBITS ((uint32_t)0xFDFFE008)
#define SDIO_R6_GENERAL_UNKNOWN_ERROR     ((uint32_t)0x00002000)
#define SDIO_R6_ILLEGAL_CMD               ((uint32_t)0x00004000)
#define SDIO_R6_COM_CRC_FAILED            ((uint32_t)0x00008000)
#define SDIO_CMD0_TIMEOUT 10000

static uint16_t _rca = 0;
static uint32_t _cid[4] = {0};
static uint32_t _csd[4] = {0};

static volatile enum SDIO_Error _transfer_error = SDIO_OK;
static volatile uint8_t _sdio_transfer_complete = 1;
static volatile uint8_t _dma_transfer_complete = 1;

void DMA2_Stream3_IRQHandler(void)
{
	if (DMA_GetITStatus(SDIO_DMA_STREAM, SDIO_DMA_TC_INT) == SET)
	{
		_dma_transfer_complete = 1;
		DMA_ClearITPendingBit(SDIO_DMA_STREAM, SDIO_DMA_TC_INT);
	}
}

void SDIO_IRQHandler(void)
{
	if (SDIO_GetITStatus(SDIO_IT_DCRCFAIL) != RESET)
	{
		_transfer_error = SDIO_CRCFAIL;
		SDIO_ClearITPendingBit(SDIO_IT_DCRCFAIL);
	}
	else if (SDIO_GetITStatus(SDIO_IT_DTIMEOUT) != RESET)
	{
		_transfer_error = SDIO_TIMEOUT;
		SDIO_ClearITPendingBit(SDIO_IT_DTIMEOUT);
	}
	else if (SDIO_GetITStatus(SDIO_IT_RXOVERR) != RESET)
	{
		_transfer_error = SDIO_RXOVERRUN;
		SDIO_ClearITPendingBit(SDIO_IT_RXOVERR);
	}
	else if (SDIO_GetITStatus(SDIO_IT_TXUNDERR) != RESET)
	{
		_transfer_error = SDIO_TXUNDERRUN;
		SDIO_ClearITPendingBit(SDIO_IT_TXUNDERR);
	}
	else if (SDIO_GetITStatus(SDIO_IT_DATAEND) != RESET)
	{
		_transfer_error = SDIO_OK;
		_sdio_transfer_complete = 1;
		SDIO_ClearITPendingBit(SDIO_IT_DATAEND);
	}

	SDIO_ITConfig(SDIO_IT_DCRCFAIL | SDIO_IT_DTIMEOUT | SDIO_IT_DATAEND |
		SDIO_IT_TXFIFOHE | SDIO_IT_RXFIFOHF | SDIO_IT_TXUNDERR |
		SDIO_IT_RXOVERR, DISABLE);
}

static enum SDIO_Error _sd_cmd_error()
{
	uint32_t timeout = SDIO_CMD0_TIMEOUT;

	while (SDIO_GetFlagStatus(SDIO_FLAG_CMDSENT) == RESET && timeout)
		--timeout;

	if (!timeout)
		return SDIO_TIMEOUT;

	SDIO_ClearFlag(SDIO_FLAG_CMDSENT);
	return SDIO_OK;
}

static enum SDIO_Error _sd_resp1_error(enum SDIO_Command cmd)
{
	uint32_t status;
	uint32_t response;

	do
	{
		status = SDIO->STA;
	} while (!(status & (SDIO_FLAG_CCRCFAIL | SDIO_FLAG_CMDREND | SDIO_FLAG_CTIMEOUT)));

	if (status & SDIO_FLAG_CTIMEOUT)
	{
		SDIO_ClearFlag(SDIO_FLAG_CTIMEOUT);
		return SDIO_TIMEOUT;
	}
	else if (status & SDIO_FLAG_CCRCFAIL)
	{
		SDIO_ClearFlag(SDIO_FLAG_CCRCFAIL);
		return SDIO_CRCFAIL;
	}

	if (SDIO_GetCommandResponse() != (uint8_t)cmd)
		return SDIO_ILLEGAL_CMD;

	SDIO_ClearFlag(SDIO_STATIC_FLAGS);

	response = SDIO_GetResponse(SDIO_RESP1);
	if (response & SDIO_OCR_ERRORBITS)
		return SDIO_ERROR;

	return SDIO_OK;
}

static enum SDIO_Error _sd_resp2_error()
{
	uint32_t status;

	do
	{
		status = SDIO->STA;
	} while (!(status & (SDIO_FLAG_CCRCFAIL | SDIO_FLAG_CMDREND | SDIO_FLAG_CTIMEOUT)));

	if (status & SDIO_FLAG_CTIMEOUT)
	{
		SDIO_ClearFlag(SDIO_FLAG_CTIMEOUT);
		return SDIO_TIMEOUT;
	}
	else if (status & SDIO_FLAG_CCRCFAIL)
	{
		SDIO_ClearFlag(SDIO_FLAG_CCRCFAIL);
		return SDIO_CRCFAIL;
	}

	SDIO_ClearFlag(SDIO_STATIC_FLAGS);

	return SDIO_OK;
}

static enum SDIO_Error _sd_resp3_error()
{
	uint32_t status;

	do
	{
		status = SDIO->STA;
	} while (!(status & (SDIO_FLAG_CCRCFAIL | SDIO_FLAG_CMDREND | SDIO_FLAG_CTIMEOUT)));

	if (status & SDIO_FLAG_CTIMEOUT)
	{
		SDIO_ClearFlag(SDIO_FLAG_CTIMEOUT);
		return SDIO_TIMEOUT;
	}

	SDIO_ClearFlag(SDIO_STATIC_FLAGS);
	return SDIO_OK;
}

static enum SDIO_Error _sd_resp6_error(enum SDIO_Command cmd)
{
	uint32_t status;
	uint32_t response;

	do
	{
		status = SDIO->STA;
	} while (!(status & (SDIO_FLAG_CCRCFAIL | SDIO_FLAG_CMDREND | SDIO_FLAG_CTIMEOUT)));

	if (status & SDIO_FLAG_CTIMEOUT)
	{
		SDIO_ClearFlag(SDIO_FLAG_CTIMEOUT);
		return SDIO_TIMEOUT;
	}
	else if (status & SDIO_FLAG_CCRCFAIL)
	{
		SDIO_ClearFlag(SDIO_FLAG_CCRCFAIL);
		return SDIO_CRCFAIL;
	}

	if (SDIO_GetCommandResponse() != (uint8_t)cmd)
		return SDIO_ILLEGAL_CMD;

	SDIO_ClearFlag(SDIO_STATIC_FLAGS);

	response = SDIO_GetResponse(SDIO_RESP1);
	if (response & (SDIO_R6_GENERAL_UNKNOWN_ERROR | SDIO_R6_ILLEGAL_CMD | SDIO_R6_COM_CRC_FAILED))
		return SDIO_ERROR;
	_rca = (uint16_t)(response >> 16);

	return SDIO_OK;
}

static enum SDIO_Error _sd_resp7_error()
{
	uint32_t status;

	do
	{
		status = SDIO->STA;
	} while (!(status & (SDIO_FLAG_CCRCFAIL | SDIO_FLAG_CMDREND | SDIO_FLAG_CTIMEOUT)));

	if (status & SDIO_FLAG_CTIMEOUT)
	{
		SDIO_ClearFlag(SDIO_FLAG_CTIMEOUT);
		return SDIO_TIMEOUT;
	}
	else if (status & SDIO_FLAG_CCRCFAIL)
	{
		SDIO_ClearFlag(SDIO_FLAG_CCRCFAIL);
		return SDIO_CRCFAIL;
	}

	SDIO_ClearFlag(SDIO_STATIC_FLAGS);

	return SDIO_OK;
}

static enum SDIO_Error _sd_send_command(enum SDIO_Command cmd, uint32_t arg)
{
	SDIO_CmdInitTypeDef SDIOCmdStruct;
	enum SDIO_Error errorstatus = SDIO_ERROR;

	switch (cmd)
	{
	case SDIO_GO_IDLE_STATE: // CMD0
		SDIOCmdStruct.SDIO_CmdIndex = (uint8_t)cmd;
		SDIOCmdStruct.SDIO_Argument = arg;
		SDIOCmdStruct.SDIO_Response = SDIO_Response_No;
		SDIOCmdStruct.SDIO_Wait = SDIO_Wait_No;
		SDIOCmdStruct.SDIO_CPSM = SDIO_CPSM_Enable;
		SDIO_SendCommand(&SDIOCmdStruct);

		errorstatus = _sd_cmd_error();
		break;
	case SDIO_SEND_ALL_CID: // CMD2
		SDIOCmdStruct.SDIO_CmdIndex = (uint8_t)cmd;
		SDIOCmdStruct.SDIO_Argument = arg;
		SDIOCmdStruct.SDIO_Response = SDIO_Response_Long;
		SDIOCmdStruct.SDIO_Wait = SDIO_Wait_No;
		SDIOCmdStruct.SDIO_CPSM = SDIO_CPSM_Enable;
		SDIO_SendCommand(&SDIOCmdStruct);

		errorstatus = _sd_resp2_error();
		break;
	case SDIO_SEND_REL_ADDR: // CMD3
		SDIOCmdStruct.SDIO_CmdIndex = (uint8_t)cmd;
		SDIOCmdStruct.SDIO_Argument = arg;
		SDIOCmdStruct.SDIO_Response = SDIO_Response_Short;
		SDIOCmdStruct.SDIO_Wait = SDIO_Wait_No;
		SDIOCmdStruct.SDIO_CPSM = SDIO_CPSM_Enable;
		SDIO_SendCommand(&SDIOCmdStruct);

		errorstatus = _sd_resp6_error(cmd);
		break;
	case SDIO_SEL_DESEL_CARD: // CMD7
		SDIOCmdStruct.SDIO_CmdIndex = (uint8_t)cmd;
		SDIOCmdStruct.SDIO_Argument = arg;
		SDIOCmdStruct.SDIO_Response = SDIO_Response_Short;
		SDIOCmdStruct.SDIO_Wait = SDIO_Wait_No;
		SDIOCmdStruct.SDIO_CPSM = SDIO_CPSM_Enable;
		SDIO_SendCommand(&SDIOCmdStruct);

		errorstatus = _sd_resp1_error(cmd);
		break;
	case SDIO_SEND_IF_COND: // CMD8
		SDIOCmdStruct.SDIO_CmdIndex = (uint8_t)cmd;
		SDIOCmdStruct.SDIO_Argument = arg;
		SDIOCmdStruct.SDIO_Response = SDIO_Response_Short;
		SDIOCmdStruct.SDIO_Wait = SDIO_Wait_No;
		SDIOCmdStruct.SDIO_CPSM = SDIO_CPSM_Enable;
		SDIO_SendCommand(&SDIOCmdStruct);

		errorstatus = _sd_resp7_error();

		break;
	case SDIO_SEND_CSD: // CMD9
		SDIOCmdStruct.SDIO_CmdIndex = (uint8_t)cmd;
		SDIOCmdStruct.SDIO_Argument = arg;
		SDIOCmdStruct.SDIO_Response = SDIO_Response_Long;
		SDIOCmdStruct.SDIO_Wait = SDIO_Wait_No;
		SDIOCmdStruct.SDIO_CPSM = SDIO_CPSM_Enable;
		SDIO_SendCommand(&SDIOCmdStruct);

		errorstatus = _sd_resp2_error();

		break;
	case SDIO_SEND_STATUS: // CMD13
		SDIOCmdStruct.SDIO_CmdIndex = (uint8_t)cmd;
		SDIOCmdStruct.SDIO_Argument = arg;
		SDIOCmdStruct.SDIO_Response = SDIO_Response_Short;
		SDIOCmdStruct.SDIO_Wait = SDIO_Wait_No;
		SDIOCmdStruct.SDIO_CPSM = SDIO_CPSM_Enable;
		SDIO_SendCommand(&SDIOCmdStruct);

		errorstatus = _sd_resp1_error(cmd);
		break;
	case SDIO_SET_BLOCKLEN: // CMD16
		SDIOCmdStruct.SDIO_CmdIndex = (uint8_t)cmd;
		SDIOCmdStruct.SDIO_Argument = arg;
		SDIOCmdStruct.SDIO_Response = SDIO_Response_Short;
		SDIOCmdStruct.SDIO_Wait = SDIO_Wait_No;
		SDIOCmdStruct.SDIO_CPSM = SDIO_CPSM_Enable;
		SDIO_SendCommand(&SDIOCmdStruct);

		errorstatus = _sd_resp1_error(cmd);
		break;
	case SDIO_READ_SINGLE_BLOCK: // CMD17
		SDIOCmdStruct.SDIO_CmdIndex = (uint8_t)cmd;
		SDIOCmdStruct.SDIO_Argument = arg;
		SDIOCmdStruct.SDIO_Response = SDIO_Response_Short;
		SDIOCmdStruct.SDIO_Wait = SDIO_Wait_No;
		SDIOCmdStruct.SDIO_CPSM = SDIO_CPSM_Enable;
		SDIO_SendCommand(&SDIOCmdStruct);

		errorstatus = _sd_resp1_error(cmd);
		break;
	case SDIO_WRITE_SINGLE_BLOCK: // CMD24
		SDIOCmdStruct.SDIO_CmdIndex = (uint8_t)cmd;
		SDIOCmdStruct.SDIO_Argument = arg;
		SDIOCmdStruct.SDIO_Response = SDIO_Response_Short;
		SDIOCmdStruct.SDIO_Wait = SDIO_Wait_No;
		SDIOCmdStruct.SDIO_CPSM = SDIO_CPSM_Enable;
		SDIO_SendCommand(&SDIOCmdStruct);

		errorstatus = _sd_resp1_error(cmd);
		break;
	case SDIO_APP_OP_COND: // CMD41
		SDIOCmdStruct.SDIO_CmdIndex = (uint8_t)cmd;
		SDIOCmdStruct.SDIO_Argument = 0xc0100000;
		SDIOCmdStruct.SDIO_Response = SDIO_Response_Short;
		SDIOCmdStruct.SDIO_Wait = SDIO_Wait_No;
		SDIOCmdStruct.SDIO_CPSM = SDIO_CPSM_Enable;
		SDIO_SendCommand(&SDIOCmdStruct);

		errorstatus = _sd_resp3_error();
		break;
	case SDIO_APP_CMD: // CMD55
		SDIOCmdStruct.SDIO_CmdIndex = (uint8_t)cmd;
		SDIOCmdStruct.SDIO_Argument = 0;
		SDIOCmdStruct.SDIO_Response = SDIO_Response_Short;
		SDIOCmdStruct.SDIO_Wait = SDIO_Wait_No;
		SDIOCmdStruct.SDIO_CPSM = SDIO_CPSM_Enable;
		SDIO_SendCommand(&SDIOCmdStruct);

		errorstatus = _sd_resp1_error(cmd);
		break;
	default:
		break;
	}

	return errorstatus;
}

uint8_t SD_Initialize()
{
	SDIO_DeInit();

	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC, ENABLE);
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOD, ENABLE);
	GPIO_InitTypeDef GPIOStruct;
	GPIOStruct.GPIO_Mode = GPIO_Mode_AF;
	GPIOStruct.GPIO_OType = GPIO_OType_PP;
	GPIOStruct.GPIO_PuPd = GPIO_PuPd_NOPULL;
	GPIOStruct.GPIO_Speed = GPIO_High_Speed;

	GPIOStruct.GPIO_Pin = SDIO_CK_PIN;
	GPIO_Init(SDIO_CK_PORT, &GPIOStruct);
	GPIOStruct.GPIO_Pin = SDIO_CMD_PIN;
	GPIO_Init(SDIO_CMD_PORT, &GPIOStruct);
	GPIOStruct.GPIO_Pin = SDIO_D0_PIN;
	GPIO_Init(SDIO_D0_PORT, &GPIOStruct);

	GPIO_PinAFConfig(SDIO_CK_PORT, SDIO_CK_PINSOURCE, GPIO_AF_SDIO);
	GPIO_PinAFConfig(SDIO_D0_PORT, SDIO_D0_PINSOURCE, GPIO_AF_SDIO);
	GPIO_PinAFConfig(SDIO_CMD_PORT, SDIO_CMD_PINSOURCE, GPIO_AF_SDIO);

	RCC_APB2PeriphClockCmd(RCC_APB2Periph_SDIO, ENABLE);
	SDIO_InitTypeDef SDIOStruct;
	SDIO_StructInit(&SDIOStruct);
	SDIOStruct.SDIO_ClockDiv = SDIO_CLK_DIV_INIT; // for initialization, clock should not exceed 400 kHz
	SDIO_Init(&SDIOStruct);

	SDIO_SetPowerState(SDIO_PowerState_ON);
	SDIO_ClockCmd(ENABLE);

	_sd_send_command(SDIO_GO_IDLE_STATE, 0);
	if (_sd_send_command(SDIO_SEND_IF_COND, 0x1aa) == SDIO_OK) // 2.7-3.6V range, and check pattern of 0xaa
	{
		_card_type = SDIO_STD_CAPACITY_V2_0;
	}
	else
	{
		_sd_send_command(SDIO_APP_CMD, 0);
	}

	if (_sd_send_command(SDIO_APP_CMD, 0) != SDIO_OK)
		return 0;

	uint8_t validvoltage = 0;
	uint16_t count = 0;
	while (!validvoltage && count < 40000)
	{
		if (_sd_send_command(SDIO_APP_CMD, 0) != SDIO_OK
			|| _sd_send_command(SDIO_APP_OP_COND, 0) != SDIO_OK)
		{
			return 0;
		}

		uint32_t response = SDIO_GetResponse(SDIO_RESP1);
		if (response & 0x80000000) // power-up status
		{
			validvoltage = 1;
			if (response & 0x40000000) // card capacity status
				_card_type = SDIO_HIGH_CAPACITY;
		}

		++count;
	}

	if (!validvoltage)
		return 0;

	if (_sd_send_command(SDIO_SEND_ALL_CID, 0) != SDIO_OK)
		return 0;
	_cid[0] = SDIO_GetResponse(SDIO_RESP1);
	_cid[1] = SDIO_GetResponse(SDIO_RESP2);
	_cid[2] = SDIO_GetResponse(SDIO_RESP3);
	_cid[3] = SDIO_GetResponse(SDIO_RESP4);

	if (_sd_send_command(SDIO_SEND_REL_ADDR, 0) != SDIO_OK)
		return 0;

	if (_sd_send_command(SDIO_SEND_CSD, ((uint32_t)_rca) << 16) != SDIO_OK)
		return 0;
	_csd[0] = SDIO_GetResponse(SDIO_RESP1);
	_csd[1] = SDIO_GetResponse(SDIO_RESP2);
	_csd[2] = SDIO_GetResponse(SDIO_RESP3);
	_csd[3] = SDIO_GetResponse(SDIO_RESP4);

	// set clock to normal rate
	SDIOStruct.SDIO_ClockDiv = SDIO_CLK_DIV_NORMAL;
	SDIO_Init(&SDIOStruct);

	if (_sd_send_command(SDIO_SEL_DESEL_CARD, ((uint32_t)_rca) << 16) != SDIO_OK)
		return 0;

	if (_sd_send_command(SDIO_SET_BLOCKLEN, 512) != SDIO_OK)
		return 0;

	// setup global DMA settings/interrupts
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_DMA2, ENABLE);
	DMA_ITConfig(SDIO_DMA_STREAM, DMA_IT_TC, ENABLE);

	NVIC_InitTypeDef NVICStruct;
	NVICStruct.NVIC_IRQChannel = DMA2_Stream3_IRQn;
	NVICStruct.NVIC_IRQChannelCmd = ENABLE;
	NVICStruct.NVIC_IRQChannelPreemptionPriority = 7;
	NVIC_Init(&NVICStruct);

	NVICStruct.NVIC_IRQChannel = SDIO_IRQn;
	NVIC_Init(&NVICStruct);

	return 1;
}

SDCardState SD_GetStatus()
{
	if (_rca == 0)
		return SD_CARD_ERROR;

	if (_sd_send_command(SDIO_SEND_STATUS, _rca << 16) != SDIO_OK)
		return SD_CARD_ERROR;

	uint32_t response = SDIO_GetResponse(SDIO_RESP1);
	return (SDCardState)((response >> 9) & 0x0F);
}

uint8_t SD_ReadBlock(uint32_t addr, uint8_t *buf)
{
	SDIO->DCTRL = 0x0;

	// set up DMA for receiving
	DMA_Cmd(SDIO_DMA_STREAM, DISABLE);
	DMA_DeInit(SDIO_DMA_STREAM);
	DMA_InitTypeDef DMAStruct;
	DMA_StructInit(&DMAStruct);
	DMAStruct.DMA_Channel = DMA_Channel_4;
	DMAStruct.DMA_PeripheralBaseAddr = (uint32_t)&SDIO->FIFO;
	DMAStruct.DMA_Memory0BaseAddr = (uint32_t)buf;
	DMAStruct.DMA_DIR = DMA_DIR_PeripheralToMemory;
	DMAStruct.DMA_BufferSize = 512;
	DMAStruct.DMA_MemoryInc = DMA_MemoryInc_Enable;
	DMAStruct.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Word;
	DMAStruct.DMA_MemoryDataSize = DMA_MemoryDataSize_Word;
	DMAStruct.DMA_Priority = DMA_Priority_VeryHigh;
	DMAStruct.DMA_FIFOMode = DMA_FIFOMode_Enable;
	DMAStruct.DMA_FIFOThreshold = DMA_FIFOThreshold_Full;
	DMAStruct.DMA_PeripheralBurst = DMA_PeripheralBurst_INC4;
	DMAStruct.DMA_MemoryBurst = DMA_MemoryBurst_INC4;
	DMA_Init(SDIO_DMA_STREAM, &DMAStruct);
	DMA_FlowControllerConfig(SDIO_DMA_STREAM, DMA_FlowCtrl_Peripheral);
	DMA_Cmd(SDIO_DMA_STREAM, ENABLE);

	// set up SDIO data config for receiving
	SDIO_DataInitTypeDef SDIODataStruct;
	SDIODataStruct.SDIO_DataTimeOut = 0xFFFFFFFF;
	SDIODataStruct.SDIO_DataBlockSize = SDIO_DataBlockSize_512b;
	SDIODataStruct.SDIO_DataLength = 512;
	SDIODataStruct.SDIO_TransferMode = SDIO_TransferMode_Block;
	SDIODataStruct.SDIO_TransferDir = SDIO_TransferDir_ToSDIO;
	SDIODataStruct.SDIO_DPSM = SDIO_DPSM_Enable;
	SDIO_DataConfig(&SDIODataStruct);

	_dma_transfer_complete = 0;
	_sdio_transfer_complete = 0;
	SDIO_ITConfig(SDIO_IT_DCRCFAIL | SDIO_IT_DTIMEOUT | SDIO_IT_DATAEND | SDIO_IT_RXOVERR, ENABLE);
	DMA_ClearFlag(SDIO_DMA_STREAM, DMA_FLAG_FEIF3 | DMA_FLAG_DMEIF3 | DMA_FLAG_TEIF3 | DMA_FLAG_HTIF3 | DMA_FLAG_TCIF3);
	SDIO_DMACmd(ENABLE);

	if (_sd_send_command(SDIO_SET_BLOCKLEN, 512) != SDIO_OK)
		return 0;

	if (_sd_send_command(SDIO_READ_SINGLE_BLOCK, addr << 9) != SDIO_OK)
		return 0;

	while (!_dma_transfer_complete && !_sdio_transfer_complete && _transfer_error == SDIO_OK)
	{
		vTaskDelay(pdMS_TO_TICKS(1));
	}

	SDCardState state;
	do
	{
		state = SD_GetStatus();
	} while (state != SD_CARD_TRANSFER && state != SD_CARD_ERROR);

	if (_transfer_error != SDIO_OK || state == SD_CARD_ERROR)
		return 0;

	return 1;
}

uint8_t SD_WriteBlock(uint32_t addr, const uint8_t *buf)
{
	SDIO->DCTRL = 0x0;

	// set up DMA for transmitting
	DMA_Cmd(SDIO_DMA_STREAM, DISABLE);
	DMA_DeInit(SDIO_DMA_STREAM);
	DMA_InitTypeDef DMAStruct;
	DMA_StructInit(&DMAStruct);
	DMAStruct.DMA_Channel = DMA_Channel_4;
	DMAStruct.DMA_PeripheralBaseAddr = (uint32_t)&SDIO->FIFO;
	DMAStruct.DMA_Memory0BaseAddr = (uint32_t)buf;
	DMAStruct.DMA_DIR = DMA_DIR_MemoryToPeripheral;
	DMAStruct.DMA_BufferSize = 512;
	DMAStruct.DMA_MemoryInc = DMA_MemoryInc_Enable;
	DMAStruct.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Word;
	DMAStruct.DMA_MemoryDataSize = DMA_MemoryDataSize_Word;
	DMAStruct.DMA_Priority = DMA_Priority_VeryHigh;
	DMAStruct.DMA_FIFOMode = DMA_FIFOMode_Enable;
	DMAStruct.DMA_FIFOThreshold = DMA_FIFOThreshold_Full;
	DMAStruct.DMA_PeripheralBurst = DMA_PeripheralBurst_INC4;
	DMAStruct.DMA_MemoryBurst = DMA_MemoryBurst_INC4;
	DMA_Init(SDIO_DMA_STREAM, &DMAStruct);
	DMA_FlowControllerConfig(SDIO_DMA_STREAM, DMA_FlowCtrl_Peripheral);
	DMA_Cmd(SDIO_DMA_STREAM, ENABLE);

	_dma_transfer_complete = 0;
	_sdio_transfer_complete = 0;
	SDIO_ITConfig(SDIO_IT_DCRCFAIL | SDIO_IT_DTIMEOUT | SDIO_IT_DATAEND | SDIO_IT_TXUNDERR, ENABLE);
	DMA_ClearFlag(SDIO_DMA_STREAM, DMA_FLAG_FEIF3 | DMA_FLAG_DMEIF3 | DMA_FLAG_TEIF3 | DMA_FLAG_HTIF3 | DMA_FLAG_TCIF3);
	SDIO_DMACmd(ENABLE);

	if (_sd_send_command(SDIO_SET_BLOCKLEN, 512) != SDIO_OK)
		return 0;

	if (_sd_send_command(SDIO_WRITE_SINGLE_BLOCK, addr << 9) != SDIO_OK)
		return 0;

	// set up SDIO data config for transmitting
	SDIO_DataInitTypeDef SDIODataStruct;
	SDIODataStruct.SDIO_DataTimeOut = 0xFFFFFFFF;
	SDIODataStruct.SDIO_DataBlockSize = SDIO_DataBlockSize_512b;
	SDIODataStruct.SDIO_DataLength = 512;
	SDIODataStruct.SDIO_TransferMode = SDIO_TransferMode_Block;
	SDIODataStruct.SDIO_TransferDir = SDIO_TransferDir_ToCard;
	SDIODataStruct.SDIO_DPSM = SDIO_DPSM_Enable;
	SDIO_DataConfig(&SDIODataStruct);

	while (!_dma_transfer_complete && !_sdio_transfer_complete && _transfer_error == SDIO_OK )
	{
		vTaskDelay(pdMS_TO_TICKS(1));
	}

	SDCardState state;
	do
	{
		state = SD_GetStatus();
	} while (state != SD_CARD_TRANSFER && state != SD_CARD_ERROR);

	if (_transfer_error != SDIO_OK || state == SD_CARD_ERROR)
		return 0;


	return 1;
}

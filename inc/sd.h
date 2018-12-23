/* sd.h
 * SD card */

#ifndef SD_H
#define SD_H

typedef enum
{
  SD_CARD_READY                  = ((uint32_t)0x00000001),
  SD_CARD_IDENTIFICATION         = ((uint32_t)0x00000002),
  SD_CARD_STANDBY                = ((uint32_t)0x00000003),
  SD_CARD_TRANSFER               = ((uint32_t)0x00000004),
  SD_CARD_SENDING                = ((uint32_t)0x00000005),
  SD_CARD_RECEIVING              = ((uint32_t)0x00000006),
  SD_CARD_PROGRAMMING            = ((uint32_t)0x00000007),
  SD_CARD_DISCONNECTED           = ((uint32_t)0x00000008),
  SD_CARD_ERROR                  = ((uint32_t)0x000000FF)
} SDCardState;

uint8_t SD_Initialize();

SDCardState SD_GetStatus();

uint8_t SD_ReadBlock(uint32_t addr, uint8_t *buf);

uint8_t SD_WriteBlock(uint32_t addr, uint8_t *buf);

#endif

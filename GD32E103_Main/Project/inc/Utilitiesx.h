#ifndef __UTILITIES_H__
#define __UTILITIES_H__

#include "DataDefine.h"

uint32_t GetNumberFromString(uint16_t BeginAddress, char* Buffer);
uint32_t GetHexNumberFromString(uint16_t BeginAddress, char* Buffer);
uint8_t CopyParameter(char* BufferSource, char* BufferDes, char FindCharBegin, char FindCharEnd);
uint16_t CalculateCheckSum(uint8_t* Buffer, uint16_t BeginAddress, uint16_t Length);
uint16_t CalculateCRC16(uint8_t *DataIn, uint8_t NbByte);
uint16_t CRC16Custom(uint8_t *Buff, uint16_t DataLen);
void toUpperCase(char* str);

#endif // __UTILITIES_H__

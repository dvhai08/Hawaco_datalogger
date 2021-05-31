#ifndef __UTILITIES_H__
#define __UTILITIES_H__

#include <stdint.h>
#include <stdbool.h>

uint16_t utilities_calculate_checksum(uint8_t* Buffer, uint16_t BeginAddress, uint16_t Length);
uint16_t utilities_calculate_crc16(uint8_t *DataIn, uint8_t NbByte);
uint16_t utilities_crc16_custom(uint8_t *Buff, uint16_t DataLen);
void utilities_to_upper_case(char* str);

#endif // __UTILITIES_H__

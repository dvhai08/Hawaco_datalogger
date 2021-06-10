#ifndef __UTILITIES_H__
#define __UTILITIES_H__

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief           Calculate checksum of a buffer
 * @param[in]       buffer Data
 * @param[in]       begin_addr  Starting point of checksum data
 * @param[in]       length Buffer length
 * @retval          Checksum of buffer
 */
uint16_t utilities_calculate_checksum(uint8_t* buffer, uint16_t begin_addr, uint16_t length);

/**
 * @brief           Calculate CRC16 of data
 * @param[in]       data Data
 * @param[in]       length Data length
 * @retval          CRC16 of buffer
 */
uint16_t utilities_calculate_crc16(uint8_t *data, uint32_t length);

/**
 * @brief           To upper case 
 * @param[in]       str String convert to upper case
 */
void utilities_to_upper_case(char* str);

#endif // __UTILITIES_H__

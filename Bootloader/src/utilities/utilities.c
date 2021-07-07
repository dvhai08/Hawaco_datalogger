#include <string.h>
#include "utilities.h"


void utilities_to_upper_case(char *buffer)
{
    uint16_t i = 0;
    while (buffer[i] && i < 256)
    {
        if (buffer[i] >= 97 && buffer[i] <= 122)
            buffer[i] = buffer[i] - 32;
        i++;
    }
}

uint16_t utilities_calculate_checksum(uint8_t *buffer, uint16_t start_addr, uint16_t length)
{
    uint32_t tmp_checksum = 0;
    uint16_t i = 0;

    for (i = start_addr; i < start_addr + length; i++)
        tmp_checksum += buffer[i];

    return (uint16_t)(tmp_checksum);
}

/***************************************************************************************************************************/
/*
 * 	Tinh check sum CRC 16 
 *
 */
#define ISO15693_PRELOADCRC16 0xFFFF
#define ISO15693_POLYCRC16 0x8408
#define ISO15693_MASKCRC16 0x0001
#define ISO15693_RESIDUECRC16 0xF0B8

uint16_t utilities_calculate_crc16(uint8_t *data_in, uint32_t size)
{
    int16_t i, j;
    int32_t res_crc = ISO15693_PRELOADCRC16;

    for (i = 0; i < size; i++)
    {
        res_crc = res_crc ^ data_in[i];
        for (j = 8; j > 0; j--)
        {
            res_crc = (res_crc & ISO15693_MASKCRC16) ? (res_crc >> 1) ^ ISO15693_POLYCRC16 : (res_crc >> 1);
        }
    }

    return ((~res_crc) & 0xFFFF);
}

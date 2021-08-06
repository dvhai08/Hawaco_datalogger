/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef MODBUS_MEMORY_H
#define MODBUS_MEMORY_H

#include <stdint.h>
#define MODBUS_MEMORY_MAX_RX_BUFFER_SIZE 64

uint8_t modbus_memory_ringbuffer_initialize(void);
void modbus_memory_flush_rx_buffer(void);
uint8_t modbus_memory_rx_availble(void);
void modbus_memory_serial_rx(uint8_t byte);
uint8_t modbus_memory_get_byte(void);
uint32_t modbus_master_get_milis(void);
#endif
/********END OF FILE****/

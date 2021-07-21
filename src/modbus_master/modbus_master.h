/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MODBUS_MASTER_H
#define __MODBUS_MASTER_H

/* USER CODE BEGIN Includes */

#include "crc16.h"
#include "word.h"

/* USER CODE END Includes */

#define MODBUS_MASTER_OK        0x00


#ifndef MODBUS_MASTER_MAX_BUFFER_SIZE
#define MODBUS_MASTER_MAX_BUFFER_SIZE 64
#endif /* MODBUS_MASTER_MAX_BUFFER_SIZE */


#ifndef MODBUS_MASTER_TIMEOUT_MS
#define MODBUS_MASTER_TIMEOUT_MS 1000
#endif /* MODBUS_MASTER_MAX_BUFFER_SIZE */

// Modbus function codes for bit access
#define MODBUS_MASTER_FUNCTION_READ_COILS   0x01
#define MODBUS_MASTER_FUNCTION_READ_DISCRETE_INPUT  0x02
#define MODBUS_MASTER_FUNCTION_READ_HOLDING_REGISTER 0x03
#define MODBUS_MASTER_FUNCTION_READ_INPUT_REGISTER 0x04

#define MODBUS_MASTER_FUNCTION_WRITE_SINGLE_COILS   0x05
#define MODBUS_MASTER_FUNCTION_WRITE_SINGLE_REGISTER   0x06
#define MODBUS_MASTER_FUNCTION_WRITE_MULTIPLE_COILS 0x0F
#define MODBUS_MASTER_FUNCTION_WRITE_MULTIPLES_REGISTER   0x10
#define MODBUS_MASTER_MASK_WRITE_REGISTER 0x16
#define MODBUS_MASTER_READ_WRITE_MULTIPLES_REGISTER 0x17

/* Modbus error code */
#define MODBUS_MASTER_ERROR_CODE_SUCCESS 0x00
#define MODBUS_MASTER_ERROR_CODE_ILLEGAL_FUNCTION 0x01
#define MODBUS_MASTER_ERROR_CODE_ILLEGAL_ADDR 0x02
#define MODBUS_MASTER_ERROR_CODE_ILLEGAL_DATA_VALUE 0x03
#define MODBUS_MASTER_ERROR_CODE_SLAVE_DEVICE_FAILED 0x04
#define MODBUS_MASTER_ERROR_CODE_INVALID_SLAVE_ID   0xE0
#define MODBUS_MASTER_ERROR_CODE_INVALID_FUNCTION   0xE1
#define MODBUS_MASTER_ERROR_CODE_RESPONSE_TIMEOUT   0xE2
#define MODBUS_MASTER_ERROR_CODE_INVALID_CRC        0xE3

void modbus_master_reset(uint16_t timeout_ms);
void modbus_master_transmission(uint16_t address);
uint8_t modbus_master_request_from(uint16_t address, uint16_t quantity);
void modbus_master_send_bit(uint8_t data);
void modbus_master_send_uint16(uint16_t data);
void modbus_master_send_uint32(uint32_t data);
void modbus_master_send_byte(uint8_t data);
uint8_t modbus_master_available(void);
uint16_t modbus_master_receive(void);
uint16_t modbus_master_get_response_buffer(uint8_t index);
uint8_t modbus_master_set_transmit_buffer(uint8_t index, uint16_t value);
void modbus_master_clear_transmit_buffer(void);
uint8_t modbus_master_read_coils(uint8_t slave_addr, uint16_t read_addr, uint16_t bit_qty);
uint8_t modbus_master_read_discrete_input(uint8_t slave_addr, uint16_t read_addr, uint16_t bit_qty);
uint8_t modbus_master_read_holding_register(uint8_t slave_addr, uint16_t read_addr, uint16_t read_qty);
uint8_t modbus_master_read_input_register(uint8_t slave_addr, uint16_t read_addr, uint8_t read_qty);
uint8_t modbus_master_write_single_coil(uint8_t slave_addr, uint16_t write_addr, uint8_t u8State);
uint8_t modbus_master_write_single_register(uint8_t slave_addr, uint16_t write_addr, uint16_t write_value);
uint8_t modbus_master_write_multiple_coils(uint8_t slave_addr, uint16_t write_addr, uint16_t bit_qty);
uint8_t modbus_master_write_multiple_registers(uint8_t slave_addr, uint16_t write_addr, uint16_t u16WriteQty);
uint8_t modbus_master_mask_write_registers(uint8_t slave_addr, uint16_t write_addr, uint16_t and_mask, uint16_t or_mask);
uint8_t modbus_master_read_write_multiple_registers(uint8_t slave_addr, uint16_t read_addr, uint16_t read_qty, uint16_t write_addr, uint16_t u16WriteQty);
extern uint32_t modbus_master_get_milis(void);
extern void modbus_master_serial_write(uint8_t *data, uint8_t length);
extern void modbus_master_sleep(void);

#endif
/********END OF FILE****/

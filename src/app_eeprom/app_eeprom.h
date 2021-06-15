#ifndef APP_EEPROM_H
#define APP_EEPROM_H

#include <stdint.h>

#define APP_EEPROM_VALID_FLAG		0x15
#define APP_EEPROM_SIZE				(6*1024)

typedef union
{
	struct
	{
		uint16_t output0 : 1;
		uint16_t output1 : 1;
		uint16_t output2 : 1;
		uint16_t output3 : 1;
		uint16_t input0 : 1;
		uint16_t input1 : 1;
		uint16_t input2 : 1;
		uint16_t input3 : 1;
		uint16_t rs485_en : 1;
		uint16_t warning : 1;
		uint16_t sos : 1;
		uint16_t reserve : 5;
	} __attribute__((packed)) name;
	uint16_t value;
} __attribute__((packed)) app_eeprom_io_enable_t;

typedef struct
{
	uint8_t measure_interval_s;
	uint8_t send_to_server_interval_sec;
	uint8_t phone[16];
	app_eeprom_io_enable_t io_enable;
	uint32_t k0;
	uint32_t offset0;
	uint32_t k1;
	uint32_t offset1;
    uint8_t meter_mode[2];
} app_eeprom_config_data_t;

#endif /* APP_EEPROM_H */

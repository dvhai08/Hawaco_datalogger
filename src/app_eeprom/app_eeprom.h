#ifndef APP_EEPROM_H
#define APP_EEPROM_H

#include <stdint.h>

#define APP_EEPROM_VALID_FLAG		0x15
#define APP_EEPROM_SIZE				(6*1024)

typedef struct
{
	uint16_t valid_flag;
	uint16_t wr_index;
	uint32_t input_0;
	uint32_t input_1;
} __attribute__((packed)) app_eeprom_measure_data_t;

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
} app_eeprom_config_data_t;

/**
 * @brief		Get measurement data in eeprom for read
 */
uint32_t app_eeprom_estimate_read_measurement_data_addr(void);

/**
 * @brief		Get measurement data in eeprom for write
 */
uint32_t app_eeprom_estimate_write_measurement_data_addr(void);

/**
 * @brief		Write measurement data into eeprom
 * @param[in]	data New measurement data
 */
void app_eeprom_store_measurement_data(app_eeprom_measure_data_t *data);


/**
 * @brief		Get measurement data from eeprom
 * @retval		Current measurement data
 */
app_eeprom_measure_data_t *app_eeprom_load_measurement_data(void);

#endif /* APP_EEPROM_H */

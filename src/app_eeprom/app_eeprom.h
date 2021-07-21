#ifndef APP_EEPROM_H
#define APP_EEPROM_H

#include <stdint.h>

#define APP_EEPROM_VALID_FLAG		0x15234519
#define APP_EEPROM_SIZE				(6*1024)

#define APP_EEPROM_METER_MODE_PWM_PLUS_DIR_MIN		0 // Meter mode 0 : PWM++, DIR--
#define APP_EEPROM_METER_MODE_ONLY_PWM				1 // Meter mode 1 : PWM++
#define APP_EEPROM_METER_MODE_PWM_F_PWM_R			2 // Meter mode 2 : PWM_F & PWM_R
#define APP_EEPROM_METER_MODE_MAX_ELEMENT           2
#define APP_EEPROM_MAX_PHONE_LENGTH                 16
#define APP_EEPROM_MAX_SERVER_ADDR_LENGTH           32
#ifdef DTG02
#define APP_EEPROM_NB_OF_INPUT_4_20MA               4
#else
#define APP_EEPROM_NB_OF_INPUT_4_20MA               1
#endif

typedef union
{
	struct
	{
		uint32_t output0 : 1;		// output0-3 enable
		uint32_t output1 : 1;
		uint32_t output2 : 1;
		uint32_t output3 : 1;
		uint32_t input0 : 1;
		uint32_t input1 : 1;
		uint32_t input2 : 1;
		uint32_t input3 : 1;
		
		uint32_t rs485_en : 1;
		uint32_t warning : 1;
		uint32_t sos : 1;
		uint32_t input_4_20ma_enable : 1;
		uint32_t output_4_20ma_enable : 1;
		
		uint32_t output_4_20ma_timeout_100ms : 8;
		uint32_t output_4_20ma_value : 5;
		
		uint32_t reserve : 6;
	} __attribute__((packed)) name;
	uint32_t value;
} __attribute__((packed)) app_eeprom_io_enable_t;

typedef struct
{
	uint32_t measure_interval_ms;
	uint32_t send_to_server_interval_ms;
	uint8_t phone[APP_EEPROM_MAX_PHONE_LENGTH];
	app_eeprom_io_enable_t io_enable;
	uint32_t k0;
	uint32_t offset0;
	uint32_t k1;
	uint32_t offset1;
    uint8_t meter_mode[APP_EEPROM_METER_MODE_MAX_ELEMENT];	
	uint32_t valid_flag;
    uint32_t send_to_server_delay_s;
    uint8_t server_addr[APP_EEPROM_MAX_SERVER_ADDR_LENGTH];
    uint8_t rs485_addr;
    uint8_t modbus_function_code;
    uint8_t register_offset;
    uint8_t register_length;
} __attribute__((packed))  app_eeprom_config_data_t;


/**
 * @brief       Init eeprom data 
 */
void app_eeprom_init(void);

/**
 * @brief		Save data to eeprom
 */
void app_eeprom_save_config(void);

/**
 * @brief		Read data from eeprom
 * @retval	 	Pointer to data, NULL on error
 */
app_eeprom_config_data_t *app_eeprom_read_config_data(void);

/**
 * @brief       Erase eeprom data
 */
void app_eeprom_erase(void);

#endif /* APP_EEPROM_H */

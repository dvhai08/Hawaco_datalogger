#ifndef APP_EEPROM_H
#define APP_EEPROM_H

#include <stdint.h>

#define APP_EEPROM_VALID_FLAG		0x15234519
#define APP_EEPROM_SIZE				(6*1024)

#define APP_EEPROM_METER_MODE_PWM_PLUS_DIR_MIN		0 // Meter mode 0 : PWM++, DIR--
#define APP_EEPROM_METER_MODE_ONLY_PWM				1 // Meter mode 1 : PWM++
#define APP_EEPROM_METER_MODE_PWM_F_PWM_R			2 // Meter mode 2 : PWM_F & PWM_R

typedef union
{
	struct
	{
		uint16_t output0 : 1;		// output0-3 enable
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
	uint32_t measure_interval_ms;
	uint32_t send_to_server_interval_ms;
	uint8_t phone[16];
	app_eeprom_io_enable_t io_enable;
	uint32_t k0;
	uint32_t offset0;
	uint32_t k1;
	uint32_t offset1;
    uint8_t meter_mode[2];	
	uint32_t valid_flag;
} app_eeprom_config_data_t;


/**
 * @brief       Init eeprom data 
 */
void app_eeprom_init(void);

/**
 * @brief		Save data to eeprom
 */
void app_eeprom_save_data(void);

/**
 * @brief		Read data from eeprom
 * @retval	 	Pointer to data, NULL on error
 */
app_eeprom_config_data_t *app_eeprom_read_config_data(void);

#endif /* APP_EEPROM_H */

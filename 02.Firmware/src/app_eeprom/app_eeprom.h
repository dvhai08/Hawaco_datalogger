#ifndef APP_EEPROM_H
#define APP_EEPROM_H

#include <stdint.h>
#include "hardware.h"

#define DEFAULT_SERVER_ADDR             "http://222.252.52.34:8080"


/**
 * \defgroup        App eeprom
 * \brief           Store device config into internal eeprom
 * \{
 */

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
		uint32_t input_4_20ma_0_enable : 1;
		uint32_t input_4_20ma_1_enable : 1;
		uint32_t input_4_20ma_2_enable : 1;
		uint32_t input_4_20ma_3_enable : 1;
		uint32_t output_4_20ma_enable : 1;
		
		uint32_t alarm_sensor_value_high : 1;
		uint32_t alarm_sensor_value_low : 1;
		
		uint32_t output_4_20ma_timeout_100ms : 8;  
	    uint32_t register_sim_status : 1;
		uint32_t reserve : 5;
				
	} __attribute__((packed)) name;
	uint32_t value;
} __attribute__((packed)) app_eeprom_io_enable_t;

typedef struct
{
	uint32_t measure_interval_ms;
	app_eeprom_io_enable_t io_enable;
	
	// Input offset and factor
	uint32_t k[APP_EEPROM_METER_MODE_MAX_ELEMENT];
	uint32_t offset[APP_EEPROM_METER_MODE_MAX_ELEMENT];
    uint8_t meter_mode[APP_EEPROM_METER_MODE_MAX_ELEMENT];	
	uint32_t valid_flag;
	
	// Server
    uint32_t send_to_server_delay_s;
    uint8_t server_addr[APP_EEPROM_MAX_NUMBER_OF_SERVER][APP_EEPROM_MAX_SERVER_ADDR_LENGTH];
	uint32_t send_to_server_interval_ms;
	uint8_t phone[APP_EEPROM_MAX_PHONE_LENGTH];
    
	// Modbus
	measure_input_modbus_register_t rs485[RS485_MAX_SLAVE_ON_BUS];
	
    float output_4_20ma;
	
	// Cai dat nguong canh bao cam bien xung
	uint32_t qmin;
	uint32_t qmax;
	
	uint32_t poll_config_interval_hour;		// Poll configuration from default link in second
	
	uint8_t battery_low_percent;
	uint8_t max_sms_1_day;
	uint8_t dir_level;
    uint32_t crc;
} __attribute__((packed))  app_eeprom_config_data_t;


typedef struct
{
    uint8_t server[APP_EEPROM_MAX_SERVER_ADDR_LENGTH];
    uint8_t reserve[32];
    uint32_t crc;
} __attribute__((packed)) app_eeprom_factory_data_t;


/*!
 * @brief       Init eeprom data 
 */
void app_eeprom_init(void);

/*!
 * @brief		Save data to eeprom
 */
void app_eeprom_save_config(void);

/*!
 * @brief		Read data from eeprom
 * @retval	 	Pointer to data, NULL on error
 */
app_eeprom_config_data_t *app_eeprom_read_config_data(void);

/*!
 * @brief       Erase eeprom data
 */
void app_eeprom_erase(void);

/*!
 * @brief       Save default factory data
 */
void app_eeprom_save_factory_data(app_eeprom_factory_data_t *factory_data);

/*!
 * @brief		Read factory reset data
 * @retval	 	Pointer to data
 */
app_eeprom_factory_data_t *app_eeprom_read_factory_data(void);

/**
 * \}
 */

#endif /* APP_EEPROM_H */

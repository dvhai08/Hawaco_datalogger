#include "app_eeprom.h"
#include "stm32l0xx_hal.h"
#include "stm32l0xx_hal_flash_ex.h"
#include "app_debug.h"
#include <string.h>
#include "flash_if.h"
#include "utilities.h"

#define EEPROM_STORE_DATA_ADDR	        0x08080000
#define EEPROM_STORE_FACTORY_ADDR       (0x08080000 + 2048)
#define EEPROM_EXT_FLASH_POINTER_ADDR
#define MEASURE_INTERVAL_S              (30*60*1000)
#define SEND_TO_SERVER_INTERVAL_S       (60*60*1000)

app_eeprom_config_data_t m_cfg;
static void app_eeprom_factory_data_initialize(void);
uint32_t sys_pulse_ms = EEPROM_RECHECK_PULSE_TIMEOUT_MS;
void app_eeprom_init(void)
{
    app_eeprom_config_data_t *tmp = (app_eeprom_config_data_t*)EEPROM_STORE_DATA_ADDR;
    uint32_t crc = utilities_calculate_crc32((uint8_t*)tmp, sizeof(app_eeprom_config_data_t) - CRC32_SIZE);        // last 2 bytes old is crc
	if (tmp->valid_flag != APP_EEPROM_VALID_FLAG
        || tmp->crc != crc)
	{
		memset(&m_cfg, 0, sizeof(m_cfg));
        // for low power application, set transistor output to 1
        m_cfg.io_enable.name.output0 = 1;
        m_cfg.io_enable.name.output1 = 1;
        m_cfg.io_enable.name.output2 = 1;
        m_cfg.io_enable.name.output3 = 1;
		m_cfg.poll_config_interval_hour = 24;
		m_cfg.battery_low_percent = 20;
		m_cfg.max_sms_1_day = 10;
		m_cfg.dir_level = 1;
		for (uint32_t i = 0; i < APP_EEPROM_METER_MODE_MAX_ELEMENT; i++)
		{
			m_cfg.meter_mode[i] = APP_EEPROM_METER_MODE_PWM_PLUS_DIR_MIN;
		}
       
		for (uint32_t i = 0; i < MEASURE_NUMBER_OF_WATER_METER_INPUT; i++)
		{
			m_cfg.k[i] = 1;
			m_cfg.meter_mode[i] = 0;
			m_cfg.offset[i] = 0;
		}
        m_cfg.measure_interval_ms = MEASURE_INTERVAL_S;
        
        m_cfg.io_enable.name.input0 = 1;
        m_cfg.io_enable.name.input1 = 1;
        m_cfg.io_enable.name.input2 = 1;
        m_cfg.io_enable.name.input3 = 1;
        
        m_cfg.io_enable.name.input_4_20ma_0_enable = 1;
        m_cfg.io_enable.name.input_4_20ma_1_enable = 1;
        m_cfg.io_enable.name.input_4_20ma_2_enable = 1;
        m_cfg.io_enable.name.input_4_20ma_3_enable = 1;

        memcpy(m_cfg.phone, "0", 1);
        m_cfg.send_to_server_interval_ms = SEND_TO_SERVER_INTERVAL_S;
        m_cfg.valid_flag = APP_EEPROM_VALID_FLAG;
        sprintf((char*)&m_cfg.server_addr[APP_EEPROM_MAIN_SERVER_ADDR_INDEX][0], "%s", DEFAULT_SERVER_ADDR);
		m_cfg.server_addr[1][0] = '\0'; 
		
       
        app_eeprom_save_config();
	}
    else
    {
        memcpy(&m_cfg, tmp, sizeof(app_eeprom_config_data_t));
        if (m_cfg.send_to_server_interval_ms == 0)
        {
            m_cfg.send_to_server_interval_ms = SEND_TO_SERVER_INTERVAL_S;
        }
        if (strlen((char*)m_cfg.server_addr[APP_EEPROM_MAIN_SERVER_ADDR_INDEX]) < 12
			&& strlen((char*)m_cfg.server_addr[APP_EEPROM_ALTERNATIVE_SERVER_ADDR_INDEX]) < 12)
        {
            sprintf((char*)&m_cfg.server_addr[APP_EEPROM_MAIN_SERVER_ADDR_INDEX], "%s", DEFAULT_SERVER_ADDR); 
        }
		
		if (m_cfg.poll_config_interval_hour == 0)
		{
			m_cfg.poll_config_interval_hour = 24;
		}
		
		if (m_cfg.battery_low_percent == 0)
		{
			m_cfg.battery_low_percent = 20;
		}
    }
    
    app_eeprom_factory_data_initialize();
}

void app_eeprom_erase(void)
{
    flash_if_init();
	uint32_t err;
	HAL_FLASHEx_DATAEEPROM_Unlock();
	for (uint32_t i = 0; (i < sizeof(app_eeprom_config_data_t)+3)/4; i++)
	{
		err = HAL_FLASHEx_DATAEEPROM_Erase(EEPROM_STORE_DATA_ADDR + i*4);
		if (HAL_OK != err)
		{
//			DEBUG_ERROR("Erase eeprom failed at addr 0x%08X, err code %08X\r\n", EEPROM_STORE_DATA_ADDR + i*4, err);
			break;
		}
	}
	HAL_FLASHEx_DATAEEPROM_Lock();
}

void app_eeprom_save_config(void)
{	
	uint32_t err;
	uint8_t *tmp = (uint8_t*)&m_cfg;
    m_cfg.crc = utilities_calculate_crc32((uint8_t*)&m_cfg, sizeof(app_eeprom_config_data_t) - CRC32_SIZE);        // last 4 bytes old is crc
    
	app_eeprom_erase();
    flash_if_init();
    
	
	HAL_FLASHEx_DATAEEPROM_Unlock();


	for (uint32_t i = 0; i < sizeof(app_eeprom_config_data_t); i++)
	{
		err = HAL_FLASHEx_DATAEEPROM_Program(FLASH_TYPEPROGRAMDATA_BYTE, EEPROM_STORE_DATA_ADDR + i, tmp[i]);
		if (HAL_OK != err)
		{
//			DEBUG_ERROR("Write eeprom failed at addr 0x%08X, err code %08X\r\n", EEPROM_STORE_DATA_ADDR + i, err);
			break;
		}
	}
	
//    if (err == HAL_OK)
//    {
//        DEBUG_VERBOSE("Store data success\r\n");
//    }
	HAL_FLASHEx_DATAEEPROM_Lock();
}

void app_eeprom_factory_data_initialize(void)
{
    app_eeprom_factory_data_t *factory_data = app_eeprom_read_factory_data(); 
    uint32_t crc = utilities_calculate_crc32((uint8_t*)factory_data, sizeof(app_eeprom_factory_data_t) - CRC32_SIZE);        // last 4 bytes old is crc
    if (crc != factory_data->crc || (factory_data->baudrate.baudrate_valid_key != EEPROM_BAUD_VALID)
        || (factory_data->pulse_valid_key != EEPROM_PULSE_VALID))
    {
        app_eeprom_factory_data_t new_data;
        memset(&new_data, 0, sizeof(app_eeprom_factory_data_t));
        new_data.baudrate.baudrate_valid_key = EEPROM_BAUD_VALID;
        new_data.baudrate.value = APP_EEPROM_DEFAULT_BAUD;
        new_data.byte_order = EEPROM_MODBUS_MSB_FIRST;
        memcpy(new_data.server, DEFAULT_SERVER_ADDR, strlen(DEFAULT_SERVER_ADDR));
        new_data.pulse_valid_key = EEPROM_PULSE_VALID;
        new_data.pulse_ms = EEPROM_RECHECK_PULSE_TIMEOUT_MS;
        app_eeprom_save_factory_data(&new_data);
    }
    else
    {
        sys_pulse_ms = factory_data->pulse_ms;
    }
}

void app_eeprom_save_factory_data(app_eeprom_factory_data_t *factory_data)
{
    uint32_t err;
	uint8_t *tmp = (uint8_t*)factory_data;
    factory_data->crc = utilities_calculate_crc32((uint8_t*)factory_data, sizeof(app_eeprom_factory_data_t) - CRC32_SIZE);        // last 4 bytes old is crc
    
    flash_if_init();
    
	HAL_FLASHEx_DATAEEPROM_Unlock();


	for (uint32_t i = 0; i < sizeof(app_eeprom_factory_data_t); i++)
	{
		err = HAL_FLASHEx_DATAEEPROM_Program(FLASH_TYPEPROGRAMDATA_BYTE, EEPROM_STORE_FACTORY_ADDR + i, tmp[i]);
		if (HAL_OK != err)
		{
//			DEBUG_ERROR("Write eeprom failed at addr 0x%08X, err code %08X\r\n", EEPROM_STORE_FACTORY_ADDR + i, err);
			break;
		}
	}

	HAL_FLASHEx_DATAEEPROM_Lock();
}

app_eeprom_factory_data_t *app_eeprom_read_factory_data(void)
{
    return (app_eeprom_factory_data_t*)(EEPROM_STORE_FACTORY_ADDR);
}

app_eeprom_config_data_t *app_eeprom_read_config_data(void)
{
	return &m_cfg;
} 

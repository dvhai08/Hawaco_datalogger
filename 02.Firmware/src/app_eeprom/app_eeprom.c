#include "app_eeprom.h"
#include "stm32l0xx_hal.h"
#include "stm32l0xx_hal_flash_ex.h"
#include "app_debug.h"
#include <string.h>
#include "flash_if.h"
#include "utilities.h"

#define EEPROM_STORE_DATA_ADDR	        0x08080000
#define MEASURE_INTERVAL_S              (30*60*1000)
#define SEND_TO_SERVER_INTERVAL_S       (60*60*1000)
#define DEFAULT_SERVER_ADDR             "https://iot.wilad.vn"

app_eeprom_config_data_t m_cfg;

void app_eeprom_init(void)
{
    app_eeprom_config_data_t *tmp = (app_eeprom_config_data_t*)EEPROM_STORE_DATA_ADDR;
    uint16_t crc = utilities_calculate_crc32((uint8_t*)tmp, sizeof(app_eeprom_config_data_t) - 2);        // last 2 bytes old is crc
	if (tmp->valid_flag != APP_EEPROM_VALID_FLAG
        || tmp->crc != crc)
	{
		memset(&m_cfg, 0, sizeof(m_cfg));
        // for low power application, set transoutput to 1
        m_cfg.io_enable.name.output0 = 1;
        m_cfg.io_enable.name.output1 = 1;
        m_cfg.io_enable.name.output2 = 1;
        m_cfg.io_enable.name.output3 = 1;
        
		for (uint32_t i = 0; i < MEASURE_NUMBER_OF_WATER_METER_INPUT; i++)
		{
			m_cfg.k[i] = 1;
			m_cfg.meter_mode[i] = 0;
			m_cfg.offset[i] = 0;
		}
        m_cfg.measure_interval_ms = MEASURE_INTERVAL_S;

        memcpy(m_cfg.phone, "0", 1);
        m_cfg.send_to_server_interval_ms = SEND_TO_SERVER_INTERVAL_S;
        m_cfg.valid_flag = APP_EEPROM_VALID_FLAG;
        sprintf((char*)&m_cfg.server_addr[0][0], "%s", DEFAULT_SERVER_ADDR);
		m_cfg.server_addr[1][0] = '\0'; 
        m_cfg.crc = utilities_calculate_crc32((uint8_t*)&m_cfg, sizeof(app_eeprom_config_data_t) - 4);        // last 4 bytes old is crc
        app_eeprom_save_config();
	}
    else
    {
        memcpy(&m_cfg, tmp, sizeof(app_eeprom_config_data_t));
        if (m_cfg.send_to_server_interval_ms == 0)
        {
            m_cfg.send_to_server_interval_ms = SEND_TO_SERVER_INTERVAL_S;
        }
        if (strlen((char*)m_cfg.server_addr) < 12)
        {
            sprintf((char*)&m_cfg.server_addr[0], "%s", DEFAULT_SERVER_ADDR); 
        }
    }
}

void app_eeprom_erase(void)
{
    memset(&m_cfg, 0, sizeof(m_cfg));
    app_eeprom_save_config();
}

void app_eeprom_save_config(void)
{	
	uint32_t err;
	uint8_t *tmp = (uint8_t*)&m_cfg;
    m_cfg.crc = utilities_calculate_crc32((uint8_t*)&m_cfg, sizeof(app_eeprom_config_data_t) - 4);        // last 4 bytes old is crc
    
    flash_if_init();
    
	HAL_FLASHEx_DATAEEPROM_Unlock();
	
//    for (uint32_t i = 0; i < sizeof(app_eeprom_config_data_t)/sizeof(uint32_t); i++)
////    {
////        err = HAL_FLASHEx_DATAEEPROM_Erase(EEPROM_STORE_DATA_ADDR + i);
////        if (HAL_OK != err)
////        {
////            DEBUG_PRINTF("Erase eeprom failed code %08X\r\n", err);
////        }
////        i += 4;
////    }

	for (uint32_t i = 0; i < sizeof(app_eeprom_config_data_t); i++)
	{
		err = HAL_FLASHEx_DATAEEPROM_Program(FLASH_TYPEPROGRAMDATA_BYTE, EEPROM_STORE_DATA_ADDR + i, tmp[i]);
		if (HAL_OK != err)
		{
			DEBUG_ERROR("Write eeprom failed at addr 0x%08X, err code %08X\r\n", EEPROM_STORE_DATA_ADDR + i, err);
			break;
		}
	}
	
    if (err == HAL_OK)
    {
        DEBUG_VERBOSE("Store data success\r\n");
    }
	HAL_FLASHEx_DATAEEPROM_Lock();
}


app_eeprom_config_data_t *app_eeprom_read_config_data(void)
{
	return &m_cfg;
} 

/******************************************************************************
 * @file    	Measurement.c
 * @author  	
 * @version 	V1.0.0
 * @date    	15/01/2014
 * @brief   	
 ******************************************************************************/

/******************************************************************************
                                   INCLUDES					    			 
 ******************************************************************************/
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "measure_input.h"
#include "DataDefine.h"
#include "hardware.h"
#include "gsm_utilities.h"
#include "hardware_manager.h"
#include "InternalFlash.h"
#include "main.h"
#include "control_output.h"
#include "app_bkup.h"
#include "app_eeprom.h"
#include "dac.h"
#include "app_sync.h"
#include "adc.h"
/******************************************************************************
                                   GLOBAL VARIABLES					    			 
 ******************************************************************************/
extern System_t xSystem;
static void dac_config(void);

static uint8_t m_dac_is_enabled = 0;
static uint32_t m_max_dac_output_ms = 0;

static bool is_dac_ouput_valid(void)
{	
	app_eeprom_config_data_t *cfg = app_eeprom_read_config_data();
	return (cfg->io_enable.name.output_4_20ma_enable 
		&& cfg->io_enable.name.output_4_20ma_timeout_100ms
	    && cfg->io_enable.name.output_4_20ma_value >= 4
		&& cfg->io_enable.name.output_4_20ma_value <= 20);
}

static void stop_dac_output(void *arg)
{
	DEBUG_PRINTF("Stop dac\r\n");
	m_max_dac_output_ms = 0;
	app_eeprom_config_data_t *cfg = app_eeprom_read_config_data();
	ENABLE_OUTPUT_4_20MA_POWER(0);
}
	
void control_output_dac_enable(uint32_t ms)
{
	if (m_dac_is_enabled)
	{
		/* Dieu khien dau ra output 4-20mA qua DAC
		* DAC: 0.6-3V () <=> 4-20mA output -> 1mA <=> 0.15V
		*/
		app_eeprom_config_data_t *cfg = app_eeprom_read_config_data();
		uint16_t dac_value = 0;
		uint16_t voltage = 600 + 150 * (cfg->io_enable.name.output_4_20ma_value - 4); //mV
		dac_value = voltage * 4095 / adc_get_input_result()->vdda_mv;

		dac_output_value(dac_value);
	}
	app_sync_remove_callback(stop_dac_output);
	m_max_dac_output_ms = ms;
	app_sync_register_callback(stop_dac_output, ms, SYNC_DRV_SINGLE_SHOT, SYNC_DRV_SCOPE_IN_LOOP);
}

void control_ouput_task(void)
{
	if (m_max_dac_output_ms == 0)
	{
		ENABLE_OUTPUT_4_20MA_POWER(0);
		return;
	}
	
	bool stop_dac = true;
	app_eeprom_config_data_t *cfg = app_eeprom_read_config_data();
	if (is_dac_ouput_valid())
	{
		//dieu khien dau ra output DAC on/off
		ENABLE_OUTPUT_4_20MA_POWER(1);

		if (!m_dac_is_enabled)
		{
			dac_config();
			m_dac_is_enabled = 1;
			/* Dieu khien dau ra output 4-20mA qua DAC
			* DAC: 0.6-3V () <=> 4-20mA output -> 1mA <=> 0.15V
			*/
			uint16_t dac_value = 0;
			uint16_t voltage = 600 + 150 * (cfg->io_enable.name.output_4_20ma_value - 4); //mV
			dac_value = voltage * 4096 / 3300;
			
			dac_output_value(dac_value);
		}
		stop_dac = false;
	}
	
	if (stop_dac)
	{
		if (m_dac_is_enabled)
		{
			m_dac_is_enabled = 0;
			ENABLE_OUTPUT_4_20MA_POWER(0);
			dac_stop();
		}
	}
}

/*!
    \brief      configure the DAC
    \param[in]  none
    \param[out] none
    \retval     none
*/
static void dac_config(void)
{
	app_eeprom_config_data_t *cfg = app_eeprom_read_config_data();
	if (cfg->io_enable.name.output_4_20ma_enable 
		&& cfg->io_enable.name.output_4_20ma_timeout_100ms
		&& cfg->io_enable.name.output_4_20ma_value >= 4
		&& cfg->io_enable.name.output_4_20ma_value <= 20)
	{
		dac_start();
		m_dac_is_enabled = 1;
		m_max_dac_output_ms = cfg->io_enable.name.output_4_20ma_timeout_100ms * 100;
	}
	else
	{
		ENABLE_OUTPUT_4_20MA_POWER(0);
		dac_stop();
	}
}


void control_ouput_init(void)
{
	dac_config();
}

void control_output_start_measure(void)
{
	if (is_dac_ouput_valid())
	{
		m_max_dac_output_ms = app_eeprom_read_config_data()->io_enable.name.output_4_20ma_timeout_100ms * 100;
	}
}

/********************************* END OF FILE *******************************/

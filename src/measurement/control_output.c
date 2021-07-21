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
#include "app_debug.h"
#include "hardware.h"
#include "gsm_utilities.h"
#include "hardware_manager.h"
#include "main.h"
#include "control_output.h"
#include "app_bkup.h"
#include "app_eeprom.h"
#include "dac.h"
#include "app_sync.h"
#include "adc.h"
#include "tim.h"
#define USE_LOOKUP_TABLE            0

typedef struct
{
    uint32_t current_ma_mil_100;		// 4ma =>> 400
    int32_t offset_ma;					// 4,08MA =>> * 100 = 408 =>> offset = 8
} output_4_20ma_lookup_t;

static const output_4_20ma_lookup_t m_4_20ma_out_lookup_table[] =
{
    {  400,   0   },
    {  500,   0  },  
    {  600,   0  },   
    {  700,   0  },     
    {  800,   0   },    
    {  900,   0   },    
    {  1000, 0   },  
    {  1100, 0  },    
    {  1200, 0  },     
    {  1300, 0  },     
    {  1400, 0  },     
    {  1500, 0    },     
    {  1600, -0   },    
    {  1700, -0   },     
    {  1800, -0  },    
    {  1900, -0  },    
    {  2000, -0  },
//    {  1800, 0  },    
//    {  1900, 0  },    
//    {  2000, 0  },
};

enum { NUM_CURRENT_LOOK_UP = sizeof(m_4_20ma_out_lookup_table) / sizeof(output_4_20ma_lookup_t) };

static int32_t get_offset_mv(uint32_t expect_ma)
{
#if USE_LOOKUP_TABLE
    uint32_t i;
	int32_t offset_mv = 0;
	expect_ma *= 100;		// 4mA convert to 400
    for (i = 0; i < NUM_CURRENT_LOOK_UP; i++)
    {
        if (expect_ma != m_4_20ma_out_lookup_table[i].current_ma_mil_100)
            continue;
		else
		{
			// 1ma = 150mV
			offset_mv = m_4_20ma_out_lookup_table[i].offset_ma*150/100;
			break;
		}
    }
    return offset_mv;
#else
	return 0;
#endif
}

static void output_4_20ma_config(void);
static uint8_t m_4_20ma_is_enabled = 0;
static uint32_t m_max_4_20ma_output_ms = 0;

static bool is_4_20ma_output_valid(void)
{	
	app_eeprom_config_data_t *cfg = app_eeprom_read_config_data();
	return (cfg->io_enable.name.output_4_20ma_enable 
		&& cfg->io_enable.name.output_4_20ma_timeout_100ms
	    && cfg->io_enable.name.output_4_20ma_value >= 4
		&& cfg->io_enable.name.output_4_20ma_value <= 20);
}

static void stop_dac_output(void *arg)
{
	DEBUG_VERBOSE("Stop dac\r\n");
	m_max_4_20ma_output_ms = 0;
	app_eeprom_config_data_t *cfg = app_eeprom_read_config_data();
	ENABLE_OUTPUT_4_20MA_POWER(0);
}
	
void control_output_dac_enable(uint32_t ms)
{
	if (m_4_20ma_is_enabled)
	{
		/* Dieu khien dau ra output 4-20mA qua DAC
		* DAC: 0.6-3V () <=> 4-20mA output -> 1mA <=> 0.15V
		*/
		app_eeprom_config_data_t *cfg = app_eeprom_read_config_data();
#ifdef DAC_4_20MA
		dac_output_value(dac_value);
		uint16_t dac_value = 0;
		uint16_t voltage = 600 + 150 * (cfg->io_enable.name.output_4_20ma_value - 4); //mV
		dac_value = voltage * 4095 / adc_get_input_result()->vdda_mv;
#else
		uint32_t thoughsand = 0;
		int32_t offset_mv = get_offset_mv(cfg->io_enable.name.output_4_20ma_value);
		uint32_t set_mv = 600 + 150 * (cfg->io_enable.name.output_4_20ma_value - 4) - offset_mv;
		thoughsand = set_mv * 1000 / 4890;		// output pwm voltage is 4890mv
		tim_pwm_output_percent(thoughsand);
#endif
	}
	app_sync_remove_callback(stop_dac_output);
	m_max_4_20ma_output_ms = ms;
	app_sync_register_callback(stop_dac_output, ms, SYNC_DRV_SINGLE_SHOT, SYNC_DRV_SCOPE_IN_LOOP);
}

void control_ouput_task(void)
{
	if (m_max_4_20ma_output_ms == 0)
	{
		ENABLE_OUTPUT_4_20MA_POWER(0);
		if (m_4_20ma_is_enabled)
		{
			m_4_20ma_is_enabled = 0;
#ifdef DAC_4_20MA
			dac_stop();
#else
			tim_pwm_stop();
#endif
		}
		return;
	}
	
	bool stop_dac = true;
	app_eeprom_config_data_t *cfg = app_eeprom_read_config_data();
	if (is_4_20ma_output_valid())
	{
		//dieu khien dau ra output DAC on/off
		ENABLE_OUTPUT_4_20MA_POWER(1);

		if (!m_4_20ma_is_enabled)
		{
			output_4_20ma_config();
			m_4_20ma_is_enabled = 1;
#ifdef DAC_4_20MA
			/* Dieu khien dau ra output 4-20mA qua DAC
			* DAC: 0.6-3V () <=> 4-20mA output -> 1mA <=> 0.15V
			*/
			uint16_t dac_value = 0;
			uint16_t voltage = 600 + 150 * (cfg->io_enable.name.output_4_20ma_value - 4); //mV
			dac_value = voltage * 4096 / 3300;
			dac_output_value(dac_value);
#else
			/* Dieu khien dau ra output 4-20mA qua DAC
			* DAC: 0.6-3V () <=> 4-20mA output -> 1mA <=> 0.15V
			*/
			uint32_t thoughsand = 0;
			int32_t offset_mv = get_offset_mv(cfg->io_enable.name.output_4_20ma_value);
			uint32_t set_mv = 600 + 150 * (cfg->io_enable.name.output_4_20ma_value - 4) - offset_mv;
			thoughsand = set_mv * 1000 / 4890;		// output pwm voltage is 4890mv
			tim_pwm_output_percent(thoughsand);
#endif
		}
		stop_dac = false;
	}
	else
	{
		
	}
	
	if (stop_dac)
	{
		if (m_4_20ma_is_enabled)
		{
			m_4_20ma_is_enabled = 0;
			ENABLE_OUTPUT_4_20MA_POWER(0);
#ifdef DAC_4_20MA
			dac_stop();
#else
			tim_pwm_stop();
#endif
		}
	}
}

/*!
    \brief      configure the DAC
    \param[in]  none
    \param[out] none
    \retval     none
*/
static void output_4_20ma_config(void)
{
	app_eeprom_config_data_t *cfg = app_eeprom_read_config_data();
	if (cfg->io_enable.name.output_4_20ma_enable 
		&& cfg->io_enable.name.output_4_20ma_timeout_100ms
		&& cfg->io_enable.name.output_4_20ma_value >= 4
		&& cfg->io_enable.name.output_4_20ma_value <= 20)
	{
#ifdef DAC_4_20MA
		dac_start();
#else
		tim_pwm_start();
#endif
		m_4_20ma_is_enabled = 1;
		m_max_4_20ma_output_ms = cfg->io_enable.name.output_4_20ma_timeout_100ms * 100;
	}
	else
	{
		ENABLE_OUTPUT_4_20MA_POWER(0);
#ifdef DAC_4_20MA
		dac_stop();
#else
		tim_pwm_stop();
#endif
	}
}


void control_ouput_init(void)
{
#ifdef DAC_4_20MA
	output_4_20ma_config();
#else
	tim_pwm_stop();
#endif
}

void control_output_start_measure(void)
{
	if (is_4_20ma_output_valid())
	{
		m_max_4_20ma_output_ms = app_eeprom_read_config_data()->io_enable.name.output_4_20ma_timeout_100ms * 100;
	}
}

/********************************* END OF FILE *******************************/

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
#include "hardware.h"
#include "gsm_utilities.h"
#include "hardware_manager.h"
#include "main.h"
#include "app_bkup.h"
#include "gsm.h"
#include "lwrb.h"
#include "app_eeprom.h"
#include "adc.h"
#include "usart.h"
#include "modbus_master.h"
#include "sys_ctx.h"
#include "trans_recieve_buff_control.h"
#include "app_debug.h"

#define STORE_MEASURE_INVERVAL_SEC              30
#define ADC_MEASURE_INTERVAL_MS			        30000
#define PULSE_STATE_INVALID                     -1
#define PULSE_DIR_FORWARD_LOGICAL_LEVEL          1
//#define DELAY_TIMEOUT_ENTER_SLEEP       2000
typedef struct
{
    uint32_t forward;
    uint32_t reserve;
} backup_pulse_data_t;

uint8_t m_is_pulse_trigger = 0;
#ifdef GD32E10X
uint32_t m_begin_pulse_timestamp, m_end_pulse_timestamp;
int8_t m_pull_state = -1;
static uint32_t m_pull_diff;
#else
uint32_t m_begin_pulse_timestamp[MEASURE_NUMBER_OF_WATER_METER_INPUT], m_end_pulse_timestamp[MEASURE_NUMBER_OF_WATER_METER_INPUT];
int8_t m_pull_state[MEASURE_NUMBER_OF_WATER_METER_INPUT];
static uint32_t m_pull_diff[MEASURE_NUMBER_OF_WATER_METER_INPUT];
#endif

static measure_input_water_meter_input_t m_water_meter_input[2];
static backup_pulse_data_t m_pulse_counter_in_backup[MEASURE_NUMBER_OF_WATER_METER_INPUT];
static measure_input_perpheral_data_t m_measure_data;
volatile uint32_t store_measure_result_timeout = 0;
bool m_this_is_the_first_time = true;

static void measure_input_pulse_counter_poll(void)
{
    if (m_is_pulse_trigger)
    {
#ifdef DTG01
        //Store to BKP register
        app_bkup_write_pulse_counter(m_pulse_counter_in_backup[0].forward,
									m_pulse_counter_in_backup[0].reserve,
									0,
									0);
#else
        //Store to BKP register
        app_bkup_write_pulse_counter(m_pulse_counter_in_backup[0].forward,
									m_pulse_counter_in_backup[0].reserve,
									m_pulse_counter_in_backup[1].forward,
									m_pulse_counter_in_backup[1].forward);
#endif
        m_is_pulse_trigger = 0;
    }
}

static uint32_t m_last_time_store_data = 0;
static uint32_t m_last_time_measure_data = 0;
uint32_t m_adc_convert_count = 0;
app_eeprom_config_data_t *eeprom_cfg;
void measure_input_task(void)
{
	eeprom_cfg = app_eeprom_read_config_data();
    measure_input_pulse_counter_poll();
    adc_input_value_t *input_adc = adc_get_input_result();
#ifdef DTG02
	m_measure_data.input_on_off[0] = LL_GPIO_IsInputPinSet(OPTOIN1_GPIO_Port, OPTOIN1_Pin) ? 1 : 0;
	m_measure_data.input_on_off[1] = LL_GPIO_IsInputPinSet(OPTOIN2_GPIO_Port, OPTOIN2_Pin) ? 1 : 0;
	m_measure_data.input_on_off[2] = LL_GPIO_IsInputPinSet(OPTOIN3_GPIO_Port, OPTOIN3_Pin) ? 1 : 0;
	m_measure_data.input_on_off[3] = LL_GPIO_IsInputPinSet(OPTOIN4_GPIO_Port, OPTOIN4_Pin) ? 1 : 0;
	
	m_measure_data.water_pulse_counter[MEASURE_INPUT_PORT_0].line_break_detect = LL_GPIO_IsInputPinSet(CIRIN0_GPIO_Port, CIRIN0_Pin) ? 0 : 1;
	m_measure_data.water_pulse_counter[MEASURE_INPUT_PORT_1].line_break_detect = LL_GPIO_IsInputPinSet(CIRIN1_GPIO_Port, CIRIN1_Pin) ? 0 : 1;
#else	// DTG01	
	m_measure_data.water_pulse_counter[MEASURE_INPUT_PORT_0].line_break_detect = LL_GPIO_IsInputPinSet(CIRIN0_GPIO_Port, CIRIN0_Pin) ? 0 : 1;
#endif	
    m_measure_data.vbat_percent = input_adc->bat_percent;
    m_measure_data.vbat_raw = input_adc->bat_mv;
    for (uint32_t i = 0; i < NUMBER_OF_INPUT_4_20MA; i++)
    {
        m_measure_data.input_4_20mA[i] = input_adc->i_4_20ma_in[i];
    }
    
	if (m_this_is_the_first_time ||
		((sys_get_ms() - m_last_time_measure_data) >= (uint32_t)5000))
		
//	if (m_this_is_the_first_time ||
//		((sys_get_ms() - m_last_time_measure_data) >= ADC_MEASURE_INTERVAL_MS))
	{
		bool start_adc = true;
		if (adc_conversion_cplt(true))
		{
			adc_convert();
			if (m_adc_convert_count++ > 2)
			{
				m_last_time_measure_data = sys_get_ms();
				m_this_is_the_first_time = false;
				m_adc_convert_count = 0;
				adc_stop();
				start_adc = false;
//				DEBUG_INFO("Measurement data finished\r\n");
			}
			else
			{
				start_adc = true;
			}
		}
		if (start_adc)
		{
			adc_start();
		}
	}
	
    if ((sys_get_ms() - m_last_time_store_data) >= eeprom_cfg->measure_interval_ms)
    {
		m_last_time_store_data = sys_get_ms();
		gsm_build_http_post_msg();
    }	
	#warning "Please implement save data to flash cmd"
//    /* Save pulse counter to flash every 30s */
//    if (store_measure_result_timeout >= STORE_MEASURE_INVERVAL_SEC)
//    {
//        store_measure_result_timeout = 0;

//        // Neu counter in BKP != in flash -> luu flash
//        if (xSystem.MeasureStatus.PulseCounterInBkup != xSystem.MeasureStatus.PulseCounterInFlash)
//        {
//            xSystem.MeasureStatus.PulseCounterInFlash = xSystem.MeasureStatus.PulseCounterInBkup;
//            InternalFlash_WriteMeasures();
//            uint8_t res = InternalFlash_WriteConfig();
//            DEBUG_INFO("Save pulse counter %u to flash: %s\r\n", xSystem.MeasureStatus.PulseCounterInFlash, res ? "FAIL" : "OK");
//        }
//    }
}

void measure_input_initialize(void)
{	
	for (uint32_t i = 0; i < MEASURE_NUMBER_OF_WATER_METER_INPUT; i++)
	{
		m_pull_state[i] = PULSE_STATE_INVALID;
	}
    /* Doc gia tri do tu bo nho backup, neu gia tri tu BKP < flash -> lay theo gia tri flash
    * -> Case: Mat dien nguon -> mat du lieu trong RTC backup register
    */
	uint32_t counter0_f, counter1_f, counter0_r, counter1_r;
	app_bkup_read_pulse_counter(&counter0_f, &counter1_f, &counter0_r, &counter1_r);
    DEBUG_INFO("Pulse counter in BKP: %u-%u, %u-%u\r\n", counter0_f, counter0_r, counter1_f, counter1_r);
}


extern uint8_t Modbus_Master_Rece_Handler(uint8_t byte);
void measure_input_rs485_uart_handler(uint8_t data)
{
    sys_ctx_t *system = sys_ctx();
    if (!system->status.is_enter_test_mode)
    {
     	Modbus_Master_Rece_Handler(data);   
    }
    else
    {
        Modbus_Master_Write(&data, 1);
    }
}

void measure_input_rs485_idle_detect(void)
{
    
}

measure_input_water_meter_input_t *measure_input_get_backup_counter(void)
{
	return &m_water_meter_input[0];
}

void measure_input_pulse_irq(measure_input_water_meter_input_t *input)
{
	  __disable_irq();
	if (input->pwm_level)
	{
		m_begin_pulse_timestamp[input->port] = sys_get_ms();
		m_pull_state[input->port] = 0;
	}
	else if (m_pull_state[input->port] != PULSE_STATE_INVALID)
	{
		m_pull_state[input->port] = PULSE_STATE_INVALID;
		m_end_pulse_timestamp[input->port] = sys_get_ms();
		if (m_end_pulse_timestamp[input->port] > m_begin_pulse_timestamp[input->port])
		{
			m_pull_diff[input->port] = m_end_pulse_timestamp[input->port] - m_begin_pulse_timestamp[input->port];
		}
		else
		{
			m_pull_diff[input->port] = 0xFFFFFFFF - m_begin_pulse_timestamp[input->port] + m_end_pulse_timestamp[input->port];
		}

        if (eeprom_cfg->meter_mode[input->port] == APP_EEPROM_METER_MODE_PWM_PLUS_DIR_MIN)
		{
            if (m_pull_diff[input->port] > 50)
            {
                DEBUG_PRINTF("+++++++++ in %ums\r\n", m_pull_diff[input->port]);
                m_is_pulse_trigger = 1;
                if (input->dir_level == 0)
                {
                    DEBUG_INFO("Reserve\r\n");
                }
                if (PULSE_DIR_FORWARD_LOGICAL_LEVEL == input->dir_level)
                {
                    m_pulse_counter_in_backup[input->port].forward++;
                }
                else
                {
                    if (m_pulse_counter_in_backup[input->port].forward > 0)
                    {
                        m_pulse_counter_in_backup[input->port].forward--;
                    }
                }
                m_pulse_counter_in_backup[input->port].reserve = 0;
            }
		}
        else if (eeprom_cfg->meter_mode[input->port] == APP_EEPROM_METER_MODE_ONLY_PWM)
        {
            m_pulse_counter_in_backup[input->port].forward++;
            m_pulse_counter_in_backup[input->port].reserve = 0;
        }
        else
        {
            if (input->new_data_type == MEASURE_INPUT_NEW_DATA_TYPE_PWM_PIN)
            {
                m_pulse_counter_in_backup[input->port].forward++;
            }
            else
            {
                m_pulse_counter_in_backup[input->port].reserve++;
            }
        }
		sys_set_delay_time_before_deep_sleep(2000);
	}
    __enable_irq();
}


void Modbus_Master_Write(uint8_t *buf, uint8_t length)
{
    volatile uint32_t i;
    if (!RS485_DIR_IS_TX())
    {
        RS485_DIR_TX();
        i = 32;     // clock = 16Mhz =>> 1us = 16, delay at least 1.3us
        while (i--);        
    }
#if 1
    for (i = 0; i < length; i++)
    {
		LL_LPUART_TransmitData8(LPUART1, buf[i]);
        while (0 == LL_LPUART_IsActiveFlag_TXE(LPUART1));
    }
#else
	HAL_UART_Transmit(&hlpuart1, buf, length, 10);
#endif
    while (0 == LL_LPUART_IsActiveFlag_TC(LPUART1))
    {
        
    }
	RS485_DIR_RX();
}

uint32_t Modbus_Master_Millis(void)
{
	return sys_get_ms();
}


measure_input_perpheral_data_t *measure_input_current_data(void)
{
	#warning "Please implement measurement data"
	return &m_measure_data;
}

void Modbus_Master_Sleep(void)
{
	__WFI();
}

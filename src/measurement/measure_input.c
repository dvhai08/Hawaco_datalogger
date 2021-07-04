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
#include "app_rtc.h"
#include "rtc.h"
#include "stm32l0xx_ll_rtc.h"
#include "app_spi_flash.h"

#define STORE_MEASURE_INVERVAL_SEC              30
#define ADC_MEASURE_INTERVAL_MS			        30000
#define PULSE_STATE_WAIT_FOR_FALLING_EDGE       0
#define PULSE_STATE_WAIT_FOR_RISING_EDGE        1
#define PULSE_DIR_FORWARD_LOGICAL_LEVEL         1

typedef struct
{
    uint32_t forward;
    uint32_t reserve;
} backup_pulse_data_t;

typedef struct
{
    uint32_t counter;
    int32_t subsecond;
} measure_input_timestamp_t;


volatile uint8_t m_is_pulse_trigger = 0;

measure_input_timestamp_t m_begin_pulse_timestamp[MEASURE_NUMBER_OF_WATER_METER_INPUT], m_end_pulse_timestamp[MEASURE_NUMBER_OF_WATER_METER_INPUT];
int8_t m_pull_state[MEASURE_NUMBER_OF_WATER_METER_INPUT];
static uint32_t m_pull_diff[MEASURE_NUMBER_OF_WATER_METER_INPUT];

static measure_input_water_meter_input_t m_water_meter_input[2];
static backup_pulse_data_t m_pulse_counter_in_backup[MEASURE_NUMBER_OF_WATER_METER_INPUT];
static measure_input_perpheral_data_t m_measure_data;
volatile uint32_t store_measure_result_timeout = 0;
bool m_this_is_the_first_time = true;
measurement_msg_queue_t m_sensor_msq[MEASUREMENT_MAX_MSQ_IN_RAM];

void test_pulse_counter(void)
{
    m_is_pulse_trigger = 1;
    m_pulse_counter_in_backup[0].forward++;
}

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
        DEBUG_INFO("Save counter %u-%u to backup\r\n", m_pulse_counter_in_backup[0].forward, m_pulse_counter_in_backup[0].reserve);
#else
        //Store to BKP register
        app_bkup_write_pulse_counter(m_pulse_counter_in_backup[0].forward,
									m_pulse_counter_in_backup[0].reserve,
									m_pulse_counter_in_backup[1].forward,
									m_pulse_counter_in_backup[1].forward);
        DEBUG_PRINTF("Save counter (%u-%u), (%u-%u) to backup\r\n", 
                                    m_pulse_counter_in_backup[0].forward, m_pulse_counter_in_backup[0].reserve,
                                    m_pulse_counter_in_backup[1].forward, m_pulse_counter_in_backup[1].reserve);
#endif
        m_is_pulse_trigger = 0;
    }
    sys_ctx()->peripheral_running.name.measure_input_pwm_running = 0;
}

static uint32_t m_last_time_measure_data = 0;
uint32_t m_number_of_adc_conversion = 0;
app_eeprom_config_data_t *eeprom_cfg;
static bool m_force_sensor_build_msq = false;

void measure_input_measure_wakeup_to_get_data()
{
    m_this_is_the_first_time = true;
    sys_ctx()->peripheral_running.name.adc = 1;
}

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
        m_measure_data.input_4_20mA[i] = input_adc->in_4_20ma_in[i];
    }
    
	if (m_this_is_the_first_time ||
		((sys_get_ms() - m_last_time_measure_data) >= eeprom_cfg->measure_interval_ms))
	{
        adc_start();
        m_last_time_measure_data = sys_get_ms();
        if (m_this_is_the_first_time)
        {
            m_this_is_the_first_time = false;
            #warning "Please check rs485 then force build msg"
            m_force_sensor_build_msq = true;
        }
        m_this_is_the_first_time = false;
        m_number_of_adc_conversion = 0;
        adc_stop();
        
        // Put data to msq
        adc_input_value_t *adc_retval = adc_get_input_result();
        measurement_msg_queue_t queue;
        m_force_sensor_build_msq = false;

        queue.measure_timestamp = app_rtc_get_counter();
        queue.vbat_mv = adc_retval->bat_mv;
        queue.vbat_percent = adc_retval->bat_percent;     

        app_bkup_read_pulse_counter(&queue.counter0_f, 
               &queue.counter1_f,
               &queue.counter0_r,
               &queue.counter1_r);

        queue.csq_percent = gsm_get_csq_in_percent();
        for (uint32_t i = 0; i < NUMBER_OF_INPUT_4_20MA; i++)
        {
            queue.input_4_20ma[i] = adc_retval->in_4_20ma_in[i]/10;
        }

        queue.temperature = adc_retval->temp;

        queue.state = MEASUREMENT_QUEUE_STATE_PENDING;

        // Scan for empty buffer
        bool queue_full = true;
        for (uint32_t i = 0; i < MEASUREMENT_MAX_MSQ_IN_RAM; i++)
        {
            if (m_sensor_msq[i].state == MEASUREMENT_QUEUE_STATE_IDLE)
            {
                memcpy(&m_sensor_msq[i], &queue, sizeof(measurement_msg_queue_t));
                queue_full = false;
                DEBUG_PRINTF("Puts new msg to sensor queue\r\n");
                break;
            }
        }        

        if (queue_full)
        {
            DEBUG_ERROR("Message queue full\r\n");
        }

        m_last_time_measure_data = sys_get_ms();
        
        sys_ctx()->peripheral_running.name.adc = 0;
	}
	#warning "Please implement save data to flash"
}

void measure_input_initialize(void)
{	
	for (uint32_t i = 0; i < MEASURE_NUMBER_OF_WATER_METER_INPUT; i++)
	{
		m_pull_state[i] = PULSE_STATE_WAIT_FOR_FALLING_EDGE;
	}
    /* Doc gia tri do tu bo nho backup, neu gia tri tu BKP < flash -> lay theo gia tri flash
    * -> Case: Mat dien nguon -> mat du lieu trong RTC backup register
    */
    
	app_bkup_read_pulse_counter(&m_pulse_counter_in_backup[0].forward, 
                                &m_pulse_counter_in_backup[0].reserve, 
                                &m_pulse_counter_in_backup[1].forward, 
                                &m_pulse_counter_in_backup[1].reserve);
    
    for (uint32_t i = 0; i < MEASUREMENT_MAX_MSQ_IN_RAM; i++)
    {
        m_sensor_msq[i].state = MEASUREMENT_QUEUE_STATE_IDLE;
    }
    
    app_spi_flash_data_t last_data;
    if (app_spi_flash_get_lastest_data(&last_data))
    {
        bool save = false;
        if (last_data.meter_input[0].pwm_f > m_pulse_counter_in_backup[0].forward)
        {
            m_pulse_counter_in_backup[0].forward = last_data.meter_input[0].pwm_f;
            save = true;
        }
        
        if (last_data.meter_input[0].dir_r > m_pulse_counter_in_backup[0].reserve)
        {
            m_pulse_counter_in_backup[0].reserve = last_data.meter_input[0].dir_r;
            save = true;
        }
#ifdef DTG02
        if (last_data.meter_input[1].pwm_f > m_pulse_counter_in_backup[1].forward)
        {
            m_pulse_counter_in_backup[1].forward = last_data.meter_input[1].pwm_f;
            save = true; 
        }
        
        if (last_data.meter_input[1].dir_r > m_pulse_counter_in_backup[1].reserve)
        {
            m_pulse_counter_in_backup[1].reserve = last_data.meter_input[1].dir_r;
            save = true;    
        }
#endif
    }
    DEBUG_INFO("Pulse counter in BKP: %u-%u, %u-%u\r\n", 
                    m_pulse_counter_in_backup[0].forward, m_pulse_counter_in_backup[0].reserve, 
                    m_pulse_counter_in_backup[1].forward, m_pulse_counter_in_backup[1].reserve);
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


static inline uint32_t get_diff_ms(measure_input_timestamp_t *begin, measure_input_timestamp_t *end)
{
    uint32_t ms = (end->counter - begin->counter)*((uint32_t)1000);
    uint32_t prescaler = LL_RTC_GetSynchPrescaler(RTC);
    end->subsecond = 1000 *(prescaler - end->subsecond) / (prescaler + 1);
    begin->subsecond = 1000 *(prescaler - begin->subsecond) / (prescaler + 1);
    
    ms += end->subsecond;
    ms -= begin->subsecond;
    return ms;
}


void measure_input_pulse_irq(measure_input_water_meter_input_t *input)
{
	  __disable_irq();
	if (input->pwm_level)
	{
		m_begin_pulse_timestamp[input->port].counter = app_rtc_get_counter();
        m_begin_pulse_timestamp[input->port].subsecond = app_rtc_get_subsecond_counter();
		m_pull_state[input->port] = PULSE_STATE_WAIT_FOR_RISING_EDGE;
	}
	else if (m_pull_state[input->port] == PULSE_STATE_WAIT_FOR_RISING_EDGE)
	{
		m_pull_state[input->port] = PULSE_STATE_WAIT_FOR_FALLING_EDGE;
        
		m_end_pulse_timestamp[input->port].counter = app_rtc_get_counter();
        m_end_pulse_timestamp[input->port].subsecond = app_rtc_get_subsecond_counter();
        m_pull_diff[input->port] = get_diff_ms(&m_begin_pulse_timestamp[input->port], &m_end_pulse_timestamp[input->port]);
        if (m_pull_diff[input->port] > 50)
        {
            m_is_pulse_trigger = 1;
            DEBUG_INFO("+++++++ in %ums\r\n", m_pull_diff[input->port]);
            if (eeprom_cfg->meter_mode[input->port] == APP_EEPROM_METER_MODE_PWM_PLUS_DIR_MIN)
            {
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
        }
        else
        {
            DEBUG_WARN("Noise, diff time %ums\r\n", m_pull_diff[input->port]);
        }
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
	return &m_measure_data;
}

void Modbus_Master_Sleep(void)
{
	__WFI();
}

bool measure_input_sensor_data_availble(void)
{
    bool retval = false;
    
    for (uint32_t i = 0; i < MEASUREMENT_MAX_MSQ_IN_RAM; i++)
    {
        if (m_sensor_msq[i].state == MEASUREMENT_QUEUE_STATE_PENDING)
        {
            return true;
        }
    }
    return retval;
}

measurement_msg_queue_t *measure_input_get_data_in_queue(void)
{
    measurement_msg_queue_t *ptr = NULL;
    
    for (uint32_t i = 0; i < MEASUREMENT_MAX_MSQ_IN_RAM; i++)
    {
        if (m_sensor_msq[i].state == MEASUREMENT_QUEUE_STATE_PENDING)
        {
            m_sensor_msq[i].state = MEASUREMENT_QUEUE_STATE_PROCESSING;
            return &m_sensor_msq[i];
        }
    }
    
    return ptr;
}

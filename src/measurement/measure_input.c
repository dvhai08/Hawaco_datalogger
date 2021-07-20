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
#include "modbus_memory.h"
#include "app_debug.h"
#include "app_rtc.h"
#include "rtc.h"
#include "stm32l0xx_ll_rtc.h"
#include "app_spi_flash.h"
#include "spi.h"
#include "modbus_master.h"

#define VBAT_DETECT_HIGH_MV                     9000
#define STORE_MEASURE_INVERVAL_SEC              30
#define ADC_MEASURE_INTERVAL_MS			        30000
#define PULSE_STATE_WAIT_FOR_FALLING_EDGE       0
#define PULSE_STATE_WAIT_FOR_RISING_EDGE        1
#define PULSE_DIR_FORWARD_LOGICAL_LEVEL         1
#define PULSE_MINMUM_WITDH_MS                   50
#define ADC_OFFSET_MA                           0      // 0.6mA, mul by 10
#define DEFAULT_INPUT_4_20MA_ENABLE_TIMEOUT     7000
typedef struct
{
    uint32_t forward;
    uint32_t reserve;
} backup_pulse_data_t;

typedef struct
{
    uint32_t counter_pwm;
    int32_t  subsecond_pwm;
    uint32_t counter_dir;
    int32_t  subsecond_dir;
} measure_input_timestamp_t;

typedef struct
{
    uint8_t pwm;
    uint8_t dir;
} measure_input_pull_state_t;

volatile uint8_t m_is_pulse_trigger = 0;

measure_input_timestamp_t m_begin_pulse_timestamp[MEASURE_NUMBER_OF_WATER_METER_INPUT], m_end_pulse_timestamp[MEASURE_NUMBER_OF_WATER_METER_INPUT];
measure_input_pull_state_t m_pull_state[MEASURE_NUMBER_OF_WATER_METER_INPUT];
static uint32_t m_pull_diff[MEASURE_NUMBER_OF_WATER_METER_INPUT];

static measure_input_water_meter_input_t m_water_meter_input[2];
static backup_pulse_data_t m_pulse_counter_in_backup[MEASURE_NUMBER_OF_WATER_METER_INPUT];
static measure_input_perpheral_data_t m_measure_data;
volatile uint32_t store_measure_result_timeout = 0;
static bool m_this_is_the_first_time = true;
measurement_msg_queue_t m_sensor_msq[MEASUREMENT_MAX_MSQ_IN_RAM];
volatile uint32_t measure_input_turn_on_in_4_20ma_power = 0;

static void process_rs485(void)
{
    app_eeprom_config_data_t *eeprom_cfg = app_eeprom_read_config_data();
    sys_ctx_t *ctx = sys_ctx();
    bool do_stop = true;
    if (eeprom_cfg->io_enable.name.rs485_en)
    {
        ctx->peripheral_running.name.rs485_running = 1;
        usart_lpusart_485_control(1);
        
        if (eeprom_cfg->modbus_function_code == MODBUS_MASTER_FUNCTION_READ_HOLDING_REGISTER)
        {
            DEBUG_PRINTF("Read holding register\r\n");
        }
        else if (eeprom_cfg->modbus_function_code == MODBUS_MASTER_FUNCTION_READ_INPUT_REGISTER)
        {
            DEBUG_PRINTF("Read input register\r\n");
        }
        
    }
    else if (ctx->status.is_enter_test_mode == 0)
    {
        do_stop = false;
    }
    
    if (do_stop)
    {
        ctx->peripheral_running.name.rs485_running = 0;
        usart_lpusart_485_control(0);
    }
}

static void measure_input_pulse_counter_poll(void)
{
    if (m_is_pulse_trigger)
    {
#ifdef DTG01
        //Store to BKP register
        app_bkup_write_pulse_counter(m_pulse_counter_in_backup[0].forward,
									0,
                                    m_pulse_counter_in_backup[0].reserve,
									0);
        DEBUG_INFO("Save counter %u-%u to backup\r\n", m_pulse_counter_in_backup[0].forward, m_pulse_counter_in_backup[0].reserve);
#else
        //Store to BKP register
        app_bkup_write_pulse_counter(m_pulse_counter_in_backup[0].forward,
                                    m_pulse_counter_in_backup[1].forward,
									m_pulse_counter_in_backup[0].reserve,
									m_pulse_counter_in_backup[1].reserve);
        DEBUG_INFO("Save counter (%u-%u), (%u-%u) to backup\r\n", 
                                    m_pulse_counter_in_backup[0].forward, m_pulse_counter_in_backup[0].reserve,
                                    m_pulse_counter_in_backup[1].forward, m_pulse_counter_in_backup[1].reserve);
#endif
        m_is_pulse_trigger = 0;
    }
    sys_ctx()->peripheral_running.name.measure_input_pwm_running = 0;
}

//static uint32_t m_last_time_measure_data = 0;
uint32_t m_number_of_adc_conversion = 0;
app_eeprom_config_data_t *eeprom_cfg;
void measure_input_measure_wakeup_to_get_data()
{
    m_this_is_the_first_time = true;
    sys_ctx()->peripheral_running.name.adc = 1;
}

void measure_input_reset_counter(uint8_t index)
{
    if (index == 0)
    {
        m_pulse_counter_in_backup[0].forward = 0;
        m_pulse_counter_in_backup[0].reserve = 0;
    }
#ifdef DTG02
    else
    {
        m_pulse_counter_in_backup[1].forward = 0;
        m_pulse_counter_in_backup[1].forward = 0;
    }
#endif
    app_bkup_write_pulse_counter(m_pulse_counter_in_backup[0].forward,
                                m_pulse_counter_in_backup[1].forward,
                                m_pulse_counter_in_backup[0].reserve,
                                m_pulse_counter_in_backup[1].forward);
}

void measure_input_save_all_data_to_flash(void)
{
    app_spi_flash_data_t wr_data;
    sys_ctx_t *ctx = sys_ctx();
    wr_data.header_overlap_detect = APP_FLASH_DATA_HEADER_KEY;
    wr_data.resend_to_server_flag = 0;

    for (uint32_t j = 0; j < MEASUREMENT_MAX_MSQ_IN_RAM; j++)
    {
        for (uint32_t i = 0; i < APP_FLASH_NB_OFF_4_20MA_INPUT; i++)
        {
            wr_data.input_4_20mA[i] = m_sensor_msq[i].input_4_20ma[i];
        }
        wr_data.meter_input[0].pwm_f = m_sensor_msq[j].counter0_f;
        wr_data.meter_input[0].dir_r = m_sensor_msq[j].counter0_r;
        
#ifdef DTG02
        wr_data.meter_input[1].pwm_f = m_sensor_msq[j].counter1_f;
        wr_data.meter_input[1].dir_r = m_sensor_msq[j].counter1_r;
#endif        
        wr_data.timestamp = m_sensor_msq[j].measure_timestamp;
        wr_data.valid_flag = APP_FLASH_VALID_DATA_KEY;
        wr_data.vbat_mv = m_sensor_msq[j].vbat_mv;
        wr_data.vbat_precent = m_sensor_msq[j].vbat_percent;
        wr_data.temp = m_sensor_msq[j].temperature;
        
        if (!ctx->peripheral_running.name.flash_running)
        {
            DEBUG_VERBOSE("Wakup flash\r\n");
            spi_init();
            app_spi_flash_wakeup();
            ctx->peripheral_running.name.flash_running = 1;
        }
        app_spi_flash_write_data(&wr_data);
        m_sensor_msq[j].state = MEASUREMENT_QUEUE_STATE_IDLE;
    }   
}

uint32_t estimate_measure_timestamp = 0;
void measure_input_task(void)
{
	eeprom_cfg = app_eeprom_read_config_data();
    measure_input_pulse_counter_poll();
    adc_input_value_t *input_adc = adc_get_input_result();
    sys_ctx_t *ctx = sys_ctx();
    uint32_t measure_interval_sec = eeprom_cfg->measure_interval_ms/1000;
    adc_input_value_t *adc_retval = adc_get_input_result();
    uint32_t current_sec = app_rtc_get_counter();
    
    if (estimate_measure_timestamp == 0)
    {
        estimate_measure_timestamp = (measure_interval_sec*(current_sec/measure_interval_sec + 1));
    }
    
    if (ctx->status.is_enter_test_mode)
    {
        if (eeprom_cfg->io_enable.name.input_4_20ma_enable)
        {
            ENABLE_INPUT_4_20MA_POWER(1);
        }
        else
        {
            measure_input_turn_on_in_4_20ma_power = 0;
        }
        estimate_measure_timestamp = current_sec + 2;
    }
    
    if (adc_retval->bat_mv > VBAT_DETECT_HIGH_MV)
    {
        ctx->peripheral_running.name.high_bat_detect = 1;
    }
    else
    {
        ctx->peripheral_running.name.high_bat_detect = 0;
    }
        
#ifdef DTG02
    TRANS_1_OUTPUT(eeprom_cfg->io_enable.name.output0);
    TRANS_2_OUTPUT(eeprom_cfg->io_enable.name.output1);
    TRANS_3_OUTPUT(eeprom_cfg->io_enable.name.output2);
    TRANS_4_OUTPUT(eeprom_cfg->io_enable.name.output3);
    
	m_measure_data.input_on_off[0] = LL_GPIO_IsInputPinSet(OPTOIN1_GPIO_Port, OPTOIN1_Pin) ? 1 : 0;
	m_measure_data.input_on_off[1] = LL_GPIO_IsInputPinSet(OPTOIN2_GPIO_Port, OPTOIN2_Pin) ? 1 : 0;
	m_measure_data.input_on_off[2] = LL_GPIO_IsInputPinSet(OPTOIN3_GPIO_Port, OPTOIN3_Pin) ? 1 : 0;
	m_measure_data.input_on_off[3] = LL_GPIO_IsInputPinSet(OPTOIN4_GPIO_Port, OPTOIN4_Pin) ? 1 : 0;
	
	m_measure_data.water_pulse_counter[MEASURE_INPUT_PORT_1].line_break_detect = LL_GPIO_IsInputPinSet(CIRIN1_GPIO_Port, CIRIN1_Pin);    
#endif	
    m_measure_data.water_pulse_counter[MEASURE_INPUT_PORT_0].line_break_detect = LL_GPIO_IsInputPinSet(CIRIN0_GPIO_Port, CIRIN0_Pin);
    
    m_measure_data.vbat_percent = input_adc->bat_percent;
    m_measure_data.vbat_raw = input_adc->bat_mv;
    for (uint32_t i = 0; i < NUMBER_OF_INPUT_4_20MA; i++)
    {
        m_measure_data.input_4_20mA[i] = input_adc->in_4_20ma_in[i];
    }
    
    // Vtemp
    if (input_adc->temp_is_valid)
    {
        m_measure_data.temperature_error = 0;
        m_measure_data.temperature = input_adc->temp;
    }
    else
    {
        m_measure_data.temperature_error = 1;
    }

	if ((m_this_is_the_first_time || (current_sec >= estimate_measure_timestamp))
        && (measure_input_turn_on_in_4_20ma_power == 0))
	{
        if (eeprom_cfg->io_enable.name.input_4_20ma_enable)
        {
            if (!INPUT_POWER_4_20_MA_IS_ENABLE())
            {
                measure_input_turn_on_in_4_20ma_power = DEFAULT_INPUT_4_20MA_ENABLE_TIMEOUT;
                ctx->peripheral_running.name.wait_for_input_4_20ma_power_on = 1;
            }
        }
        if (measure_input_turn_on_in_4_20ma_power == 0)
        {
            // Process rs485
            process_rs485();
            
            // ADC conversion
            adc_start();
//            m_last_time_measure_data = sys_get_ms();
            if (m_this_is_the_first_time)
            {
                m_this_is_the_first_time = false;
            }
            m_this_is_the_first_time = false;
            m_number_of_adc_conversion = 0;
            
            // Stop adc
            adc_stop();
    #ifdef DTG01
            sys_enable_power_plug_detect();
    #endif
            
            // Put data to msq
            adc_retval = adc_get_input_result();
            
            if (ctx->status.is_enter_test_mode == 0)
            {
                measurement_msg_queue_t queue;
                
                queue.measure_timestamp = app_rtc_get_counter();
                queue.vbat_mv = adc_retval->bat_mv;
                queue.vbat_percent = adc_retval->bat_percent;
#ifdef DTG02                
                queue.vin_mv = adc_retval->vin_24;
#endif
                app_bkup_read_pulse_counter(&queue.counter0_f, 
                       &queue.counter1_f,
                       &queue.counter0_r,
                       &queue.counter1_r);

                queue.csq_percent = gsm_get_csq_in_percent();
                for (uint32_t i = 0; i < NUMBER_OF_INPUT_4_20MA; i++)
                {
                    queue.input_4_20ma[i] = adc_retval->in_4_20ma_in[i]/10;
                }
                
                DEBUG_INFO("PWM0 %u, PWM1 %u\r\n", queue.counter0_f, queue.counter1_f);
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
                        DEBUG_INFO("Puts new msg to sensor queue\r\n");
                        break;
                    }
                }        

                if (queue_full)
                {
                    DEBUG_ERROR("Message queue full\r\n");
                    measure_input_save_all_data_to_flash();
                }
            }        
            
            estimate_measure_timestamp = (measure_interval_sec*(current_sec/measure_interval_sec + 1));
//            m_last_time_measure_data = sys_get_ms();
            ctx->peripheral_running.name.adc = 0;
            ctx->peripheral_running.name.wait_for_input_4_20ma_power_on = 0;
        }
	}
}

void measure_input_initialize(void)
{	
	for (uint32_t i = 0; i < MEASURE_NUMBER_OF_WATER_METER_INPUT; i++)
	{
		m_pull_state[i].pwm = PULSE_STATE_WAIT_FOR_FALLING_EDGE;
        m_pull_state[i].dir = PULSE_STATE_WAIT_FOR_FALLING_EDGE;
	}
    /* Doc gia tri do tu bo nho backup, neu gia tri tu BKP < flash -> lay theo gia tri flash
    * -> Case: Mat dien nguon -> mat du lieu trong RTC backup register
    */
    
	app_bkup_read_pulse_counter(&m_pulse_counter_in_backup[0].forward, 
                                &m_pulse_counter_in_backup[1].reserve, 
                                &m_pulse_counter_in_backup[0].forward, 
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
        
        if (save)
        {
            DEBUG_WARN("Save new data from flash\r\n");
            app_bkup_write_pulse_counter(m_pulse_counter_in_backup[0].forward, 
                                        m_pulse_counter_in_backup[1].forward, 
                                        m_pulse_counter_in_backup[0].reserve, 
                                        m_pulse_counter_in_backup[1].reserve);
        }
#else
        
        if (save)
        {
            app_bkup_write_pulse_counter(m_pulse_counter_in_backup[0].forward, 
                                        m_pulse_counter_in_backup[0].reserve, 
                                        0, 
                                        0);
        }
#endif
    }
    DEBUG_INFO("Pulse counter in BKP: %u-%u, %u-%u\r\n", 
                    m_pulse_counter_in_backup[0].forward, m_pulse_counter_in_backup[0].reserve, 
                    m_pulse_counter_in_backup[1].forward, m_pulse_counter_in_backup[1].reserve);
    m_this_is_the_first_time = true;
}


void measure_input_rs485_uart_handler(uint8_t data)
{
    modbus_memory_serial_rx(data); 
}

void measure_input_rs485_idle_detect(void)
{
    
}

measure_input_water_meter_input_t *measure_input_get_backup_counter(void)
{
	return &m_water_meter_input[0];
}


static inline uint32_t get_diff_ms(measure_input_timestamp_t *begin, measure_input_timestamp_t *end, uint8_t isr_type)
{
    uint32_t ms;
    uint32_t prescaler = LL_RTC_GetSynchPrescaler(RTC);
    if (isr_type == MEASURE_INPUT_NEW_DATA_TYPE_PWM_PIN)
    {
        ms = (end->counter_pwm - begin->counter_pwm)*((uint32_t)1000);
        
        end->subsecond_pwm = 1000 *(prescaler - end->subsecond_pwm) / (prescaler + 1);
        begin->subsecond_pwm = 1000 *(prescaler - begin->subsecond_pwm) / (prescaler + 1);
        
        ms += end->subsecond_pwm;
        ms -= begin->subsecond_pwm;
    }
    else
    {
        ms = (end->counter_dir - begin->counter_dir)*((uint32_t)1000);
        
        end->subsecond_dir = 1000 *(prescaler - end->subsecond_dir) / (prescaler + 1);
        begin->subsecond_dir = 1000 *(prescaler - begin->subsecond_dir) / (prescaler + 1);
        
        ms += end->subsecond_dir;
        ms -= begin->subsecond_dir;
    }
    return ms;
}


void measure_input_pulse_irq(measure_input_water_meter_input_t *input)
{
	__disable_irq();
    if (input->new_data_type == MEASURE_INPUT_NEW_DATA_TYPE_PWM_PIN)
    {
        if (input->pwm_level)
        {
            m_begin_pulse_timestamp[input->port].counter_pwm = app_rtc_get_counter();
            m_begin_pulse_timestamp[input->port].subsecond_pwm = app_rtc_get_subsecond_counter();
            m_pull_state[input->port].pwm = PULSE_STATE_WAIT_FOR_RISING_EDGE;
            m_pull_state[input->port].dir = PULSE_STATE_WAIT_FOR_FALLING_EDGE;
        }
        else if (m_pull_state[input->port].pwm == PULSE_STATE_WAIT_FOR_RISING_EDGE)
        {
            m_pull_state[input->port].pwm = PULSE_STATE_WAIT_FOR_FALLING_EDGE;
            m_pull_state[input->port].dir = PULSE_STATE_WAIT_FOR_FALLING_EDGE;
            
            m_end_pulse_timestamp[input->port].counter_pwm = app_rtc_get_counter();
            m_end_pulse_timestamp[input->port].subsecond_pwm = app_rtc_get_subsecond_counter();
            
            m_pull_diff[input->port] = get_diff_ms(&m_begin_pulse_timestamp[input->port], 
                                                &m_end_pulse_timestamp[input->port],
                                                MEASURE_INPUT_NEW_DATA_TYPE_PWM_PIN);
            if (m_pull_diff[input->port] > PULSE_MINMUM_WITDH_MS)
            {
                m_is_pulse_trigger = 1;
                if (eeprom_cfg->meter_mode[input->port] == APP_EEPROM_METER_MODE_PWM_PLUS_DIR_MIN)
                {
                    if (input->dir_level == 0)
                    {
                        DEBUG_VERBOSE("Reserve\r\n");
                    }
                    if (PULSE_DIR_FORWARD_LOGICAL_LEVEL == input->dir_level)
                    {
                        DEBUG_VERBOSE("[PWM%u] +++++++ in %ums\r\n", input->port, m_pull_diff[input->port]);
                        m_pulse_counter_in_backup[input->port].forward++;
                    }
                    else
                    {
                        if (m_pulse_counter_in_backup[input->port].forward > 0)
                        {
                            DEBUG_VERBOSE("[PWM] ----- in %ums\r\n", m_pull_diff[input->port]);
                            m_pulse_counter_in_backup[input->port].forward--;
                        }
                    }
                    m_pulse_counter_in_backup[input->port].reserve = 0;
                }
                else if (eeprom_cfg->meter_mode[input->port] == APP_EEPROM_METER_MODE_ONLY_PWM)
                {
                    DEBUG_VERBOSE("[PWM] +++++++ in %ums\r\n", m_pull_diff[input->port]);
                    m_pulse_counter_in_backup[input->port].forward++;
                    m_pulse_counter_in_backup[input->port].reserve = 0;
                }
                else
                {
                    m_pulse_counter_in_backup[input->port].forward++;
                }
            }
            else
            {
                DEBUG_WARN("PWM Noise, diff time %ums\r\n", m_pull_diff[input->port]);
            }
        }
    }
    else if (input->new_data_type == MEASURE_INPUT_NEW_DATA_TYPE_DIR_PIN)
    {
        if (eeprom_cfg->meter_mode[input->port] == APP_EEPROM_METER_MODE_PWM_F_PWM_R)
        {
            if (input->dir_level)
            {
                m_begin_pulse_timestamp[input->port].counter_dir = app_rtc_get_counter();
                m_begin_pulse_timestamp[input->port].subsecond_dir = app_rtc_get_subsecond_counter();
                m_pull_state[input->port].pwm = PULSE_STATE_WAIT_FOR_RISING_EDGE;
                m_pull_state[input->port].dir = PULSE_STATE_WAIT_FOR_FALLING_EDGE;
            }
            else if (m_pull_state[input->port].dir == PULSE_STATE_WAIT_FOR_RISING_EDGE)
            {
                m_is_pulse_trigger = 1;
                // reset state
                m_pull_state[input->port].pwm = PULSE_STATE_WAIT_FOR_FALLING_EDGE;
                m_pull_state[input->port].dir = PULSE_STATE_WAIT_FOR_FALLING_EDGE;
                
                m_end_pulse_timestamp[input->port].counter_dir = app_rtc_get_counter();
                m_end_pulse_timestamp[input->port].subsecond_dir = app_rtc_get_subsecond_counter();
                
                m_pull_diff[input->port] = get_diff_ms(&m_begin_pulse_timestamp[input->port], 
                                                    &m_end_pulse_timestamp[input->port],
                                                    MEASURE_INPUT_NEW_DATA_TYPE_DIR_PIN);
                
                if (m_pull_diff[input->port] > PULSE_MINMUM_WITDH_MS)
                {
                    DEBUG_VERBOSE("[DIR] +++++++ in %ums\r\n", m_pull_diff[input->port]);
                    m_pulse_counter_in_backup[input->port].reserve++;
                }
            }
            else
            {
                DEBUG_VERBOSE("DIR Noise, diff time %ums\r\n", m_pull_diff[input->port]);
            }
        }
    }
    __enable_irq();
}


void modbus_master_serial_write(uint8_t *buf, uint8_t length)
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

uint32_t modbus_master_get_milis(void)
{
	return sys_get_ms();
}


measure_input_perpheral_data_t *measure_input_current_data(void)
{
	return &m_measure_data;
}

void modbus_master_sleep(void)
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

void measure_input_delay_delay_measure_input_4_20ma(uint32_t ms)
{
    measure_input_turn_on_in_4_20ma_power = ms;
}

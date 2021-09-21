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
#include "umm_malloc.h"

#define VBAT_DETECT_HIGH_MV                     9000
#define STORE_MEASURE_INVERVAL_SEC              30
#define ADC_MEASURE_INTERVAL_MS			        30000
#define PULSE_STATE_WAIT_FOR_FALLING_EDGE       0
#define PULSE_STATE_WAIT_FOR_RISING_EDGE        1
//#define PULSE_DIR_FORWARD_LOGICAL_LEVEL         1
#define ADC_OFFSET_MA                           0      // 0.6mA, mul by 10
#define DEFAULT_INPUT_4_20MA_ENABLE_TIMEOUT     2000
#define CHECK_BOTH_INPUT_EDGE					0

#if CHECK_BOTH_INPUT_EDGE
#define PULSE_MINMUM_WITDH_MS                   30
#else
#define PULSE_MINMUM_WITDH_MS                   100
#endif


#define ALWAYS_SAVE_DATA_TO_FLASH               1
#define CALCULATE_FLOW_SPEED                    1

typedef measure_input_counter_t backup_pulse_data_t;


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

// PWM, DIR interrupt timestamp between 2 times interrupt called
static uint32_t m_pull_diff[MEASURE_NUMBER_OF_WATER_METER_INPUT];

static backup_pulse_data_t m_pulse_counter_in_backup[MEASURE_NUMBER_OF_WATER_METER_INPUT], m_pre_pulse_counter_in_backup[MEASURE_NUMBER_OF_WATER_METER_INPUT];
static measure_input_perpheral_data_t m_measure_data;
volatile uint32_t store_measure_result_timeout = 0;
static bool m_this_is_the_first_time = true;
measure_input_perpheral_data_t m_sensor_msq[MEASUREMENT_MAX_MSQ_IN_RAM];
volatile uint32_t measure_input_turn_on_in_4_20ma_power = 0;

// Min-max value of water flow speed between 2 times send data to web
static float fw_flow_min_in_cycle_send_web[MEASURE_NUMBER_OF_WATER_METER_INPUT];
static float fw_flow_max_in_cycle_send_web[MEASURE_NUMBER_OF_WATER_METER_INPUT];
static float reserve_flow_min_in_cycle_send_web[MEASURE_NUMBER_OF_WATER_METER_INPUT];
static float reserve_flow_max_in_cycle_send_web[MEASURE_NUMBER_OF_WATER_METER_INPUT];

// Input4-20mA min max
static float input_4_20ma_min_value[NUMBER_OF_INPUT_4_20MA];
static float input_4_20ma_max_value[NUMBER_OF_INPUT_4_20MA];

// 485
static measure_input_rs485_min_max_t m_485_min_max[RS485_MAX_SLAVE_ON_BUS];


static void process_rs485(measure_input_modbus_register_t *register_value)
{	
    app_eeprom_config_data_t *eeprom_cfg = app_eeprom_read_config_data();
    sys_ctx_t *ctx = sys_ctx();
    bool do_stop = true;
    
	// If rs485 enable =>> Start measure data
    if (eeprom_cfg->io_enable.name.rs485_en && ctx->status.is_enter_test_mode == 0)
    {
        ctx->peripheral_running.name.rs485_running = 1;
		RS485_POWER_EN(1);
        usart_lpusart_485_control(1);
		sys_delay_ms(50);   // ensure rs485 transmister ic power on
        
		for (uint32_t slave_count = 0; slave_count < RS485_MAX_SLAVE_ON_BUS; slave_count++)
		{
            uint16_t delay_modbus = 1000;
			for (uint32_t sub_reg_idx = 0; sub_reg_idx < RS485_MAX_SUB_REGISTER; sub_reg_idx++)
			{
				if (eeprom_cfg->rs485[slave_count].sub_register[sub_reg_idx].data_type.name.valid == 0)
				{
					uint32_t slave_addr = eeprom_cfg->rs485[slave_count].sub_register[sub_reg_idx].read_ok = 0;
					continue;
				}
				sys_delay_ms(15);   // delay between 2 transaction
				// convert register to function code and offset
				// ex : 30001 =>> function code = 04, offset = 01
				uint32_t function_code = eeprom_cfg->rs485[slave_count].sub_register[sub_reg_idx].register_addr / 10000;
				uint32_t register_addr = eeprom_cfg->rs485[slave_count].sub_register[sub_reg_idx].register_addr - function_code * 10000;
				function_code += 1;     // 30001 =>> function code = 04 read input register
				uint32_t slave_addr = eeprom_cfg->rs485[slave_count].slave_addr;
				register_value[slave_count].sub_register[sub_reg_idx].data_type.name.valid = 1;
				if (register_addr)		// For example, we want to read data at addr 30100 =>> register_addr = 99
				{
					register_addr--;
				}
				switch (function_code)
				{
					case MODBUS_MASTER_FUNCTION_READ_COILS:
						break;
					
					case MODBUS_MASTER_FUNCTION_READ_DISCRETE_INPUT:
						break;

					case MODBUS_MASTER_FUNCTION_READ_HOLDING_REGISTER:
						break;
					
					case MODBUS_MASTER_FUNCTION_READ_INPUT_REGISTER:
					{
						uint8_t result;
						
						modbus_master_reset(delay_modbus);
						uint32_t halfword_quality = 1;
						if (eeprom_cfg->rs485[slave_count].sub_register[sub_reg_idx].data_type.name.type == RS485_DATA_TYPE_FLOAT
							|| eeprom_cfg->rs485[slave_count].sub_register[sub_reg_idx].data_type.name.type == RS485_DATA_TYPE_INT32)
						{
							halfword_quality = 2;
						}
						
						DEBUG_INFO("MB id %u, offset %u, size %u\r\n", slave_addr, register_addr, halfword_quality);
						result = modbus_master_read_input_register(slave_addr, 
																	register_addr, 
																	halfword_quality);
						register_value[slave_count].slave_addr = slave_addr;
						
						if (result != MODBUS_MASTER_OK)		// Read data error
						{
							DEBUG_ERROR("Modbus read input register failed\r\n");
                            delay_modbus = 200;     // if 1 register failed =>> maybe other register will be fail =>> Reduce delay time 
							register_value[slave_count].sub_register[sub_reg_idx].read_ok = 0;
							modbus_master_clear_response_buffer();
                            m_485_min_max[slave_count].max_forward_flow.type_int = INPUT_485_INVALID_INT_VALUE;
                            m_485_min_max[slave_count].min_forward_flow.type_int = INPUT_485_INVALID_INT_VALUE;
                            m_485_min_max[slave_count].min_reserve_flow.type_int = INPUT_485_INVALID_INT_VALUE;
                            m_485_min_max[slave_count].max_reserve_flow.type_int = INPUT_485_INVALID_INT_VALUE;
							ctx->error_not_critical.detail.rs485_err = 1;
						}
						else		// Read data ok
						{
							// Read ok, copy result to buffer
							register_value[slave_count].sub_register[sub_reg_idx].read_ok = 1;
							register_value[slave_count].sub_register[sub_reg_idx].value = modbus_master_get_response_buffer(0);
							
							if (halfword_quality == 2)
							{	
								// Byte order : Float, int32 (2-1,4-3)
								// int16 2 - 1
								register_value[slave_count].sub_register[sub_reg_idx].value |= (modbus_master_get_response_buffer(1) << 16);
							}
//							DEBUG_RAW("%u-0x%08X\r\n", eeprom_cfg->rs485[slave_count].sub_register[sub_reg_idx].register_addr, 
//													register_value[slave_count].sub_register[sub_reg_idx].value);
							register_value[slave_count].sub_register[sub_reg_idx].data_type.name.type = eeprom_cfg->rs485[slave_count].sub_register[sub_reg_idx].data_type.name.type;
							
                            for (uint32_t slave_on_bus = 0; slave_on_bus < RS485_MAX_SLAVE_ON_BUS; slave_on_bus++)
                            {
                                if ((register_addr+1) == eeprom_cfg->rs485[slave_on_bus].fw_flow_reg)      // If register == forward flow register
                                {
                                    if (register_value[slave_count].sub_register[sub_reg_idx].data_type.name.type == RS485_DATA_TYPE_FLOAT) // If data type is float
                                    {
                                        // Neu chua dc khoi tao min max =>> khoi tao gia tri min max
                                        if (m_485_min_max[slave_on_bus].min_forward_flow.type_int == INPUT_485_INVALID_INT_VALUE
                                            || m_485_min_max[slave_on_bus].min_forward_flow.type_float == INPUT_485_INVALID_FLOAT_VALUE)
                                        {
                                            m_485_min_max[slave_on_bus].min_forward_flow.type_float = (float)register_value[slave_count].sub_register[sub_reg_idx].value;
                                        }
                                        
                                        if (m_485_min_max[slave_on_bus].min_forward_flow.type_float > (float)register_value[slave_count].sub_register[sub_reg_idx].value)
                                        {
                                            m_485_min_max[slave_on_bus].min_forward_flow.type_float = (float)register_value[slave_count].sub_register[sub_reg_idx].value;
                                        }
                                    }
                                    else        // Data type is int
                                    {
                                        if (m_485_min_max[slave_on_bus].max_forward_flow.type_int == INPUT_485_INVALID_INT_VALUE
                                            || m_485_min_max[slave_on_bus].max_forward_flow.type_float == INPUT_485_INVALID_FLOAT_VALUE)
                                        {
                                            m_485_min_max[slave_on_bus].max_forward_flow.type_int = (float)register_value[slave_count].sub_register[sub_reg_idx].value;
                                        }
                                        
                                        if (m_485_min_max[slave_on_bus].max_forward_flow.type_int < (int32_t)register_value[slave_count].sub_register[sub_reg_idx].value)
                                        {
                                            m_485_min_max[slave_on_bus].max_forward_flow.type_int = (int32_t)register_value[slave_count].sub_register[sub_reg_idx].value;
                                        }
                                    }
                                        
                                }
                                else if ((register_addr+1) == eeprom_cfg->rs485[slave_on_bus].reserve_flow_reg) // If register == reserve flow register
                                {
                                    if (register_value[slave_count].sub_register[sub_reg_idx].data_type.name.type == RS485_DATA_TYPE_FLOAT) // If data type is float
                                    {
                                        // Neu chua dc khoi tao min max =>> khoi tao gia tri min max
                                        if (m_485_min_max[slave_on_bus].min_reserve_flow.type_int == INPUT_485_INVALID_INT_VALUE
                                            || m_485_min_max[slave_on_bus].min_reserve_flow.type_float == INPUT_485_INVALID_FLOAT_VALUE)
                                        {
                                            m_485_min_max[slave_on_bus].min_reserve_flow.type_float = (float)register_value[slave_count].sub_register[sub_reg_idx].value;
                                        }
                                        
                                        if (m_485_min_max[slave_on_bus].min_reserve_flow.type_float > (float)register_value[slave_count].sub_register[sub_reg_idx].value)
                                        {
                                            m_485_min_max[slave_on_bus].min_reserve_flow.type_float = (float)register_value[slave_count].sub_register[sub_reg_idx].value;
                                        }
                                    }
                                    else    // Data type is int
                                    {
                                        if (m_485_min_max[slave_on_bus].max_reserve_flow.type_int == INPUT_485_INVALID_INT_VALUE
                                            || m_485_min_max[slave_on_bus].max_reserve_flow.type_float == INPUT_485_INVALID_FLOAT_VALUE)
                                        {
                                            m_485_min_max[slave_on_bus].max_reserve_flow.type_int = (float)register_value[slave_count].sub_register[sub_reg_idx].value;
                                        }
                                        
                                        if (m_485_min_max[slave_on_bus].max_reserve_flow.type_int < (int32_t)register_value[slave_count].sub_register[sub_reg_idx].value)
                                        {
                                            m_485_min_max[slave_on_bus].max_reserve_flow.type_int = (int32_t)register_value[slave_count].sub_register[sub_reg_idx].value;
                                        }
                                    }
                                }
                            }
                            
                            register_value[slave_count].sub_register[sub_reg_idx].register_addr = eeprom_cfg->rs485[slave_count].sub_register[sub_reg_idx].register_addr;
							ctx->error_not_critical.detail.rs485_err = 0;
						}
						strncpy((char*)register_value[slave_count].sub_register[sub_reg_idx].unit,
								(char*)eeprom_cfg->rs485[slave_count].sub_register[sub_reg_idx].unit,
								6);

					}
						break;
					
					default:
//						DEBUG_WARN("Unsupported function code %u\r\n", function_code);
						break;
				}
			}
		}
        RS485_POWER_EN(0);  
        sys_delay_ms(1000);
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

void measure_input_pulse_counter_poll(void)
{
    if (m_is_pulse_trigger)
    {
		char ptr[48];
		uint8_t total_length = 0;
		uint32_t temp_counter;
		app_eeprom_config_data_t *eeprom_cfg = app_eeprom_read_config_data();
		
		// Build debug counter message
		for (uint32_t i = 0; i < MEASURE_NUMBER_OF_WATER_METER_INPUT; i++)
		{
			temp_counter = m_pulse_counter_in_backup[i].real_counter / eeprom_cfg->k[i] + eeprom_cfg->offset[i];
			total_length += sprintf((char *)(ptr + total_length), "(%u-",
									  temp_counter);
			
			temp_counter = m_pulse_counter_in_backup[i].reverse_counter/eeprom_cfg->k[i] /* + eeprom_cfg->offset[i] */;
			total_length += sprintf((char *)(ptr + total_length), "%u),",
										temp_counter);
			
			m_pulse_counter_in_backup[i].k = eeprom_cfg->k[i];
			m_pulse_counter_in_backup[i].indicator = eeprom_cfg->offset[i];
		}
	
        app_bkup_write_pulse_counter(&m_pulse_counter_in_backup[0]);
#ifdef DTG01
        //Store to BKP register

		
		DEBUG_INFO("Counter (%u-%u), real value %s\r\n", 
				m_pulse_counter_in_backup[0].real_counter, m_pulse_counter_in_backup[0].reverse_counter,
				ptr);
#else
								
        DEBUG_INFO("Counter(0-1) : (%u-%u), (%u-%u), real value %s\r\n", 
				m_pulse_counter_in_backup[0].real_counter, m_pulse_counter_in_backup[0].reverse_counter,
				m_pulse_counter_in_backup[1].real_counter, m_pulse_counter_in_backup[1].reverse_counter,
				ptr);
#endif			
        m_is_pulse_trigger = 0;
    }
    sys_ctx()->peripheral_running.name.measure_input_pwm_running = 0;
}

static uint32_t m_last_time_measure_data = 0;
uint32_t m_number_of_adc_conversion = 0;
app_eeprom_config_data_t *eeprom_cfg;
void measure_input_measure_wakeup_to_get_data()
{
    m_this_is_the_first_time = true;
    sys_ctx()->peripheral_running.name.adc = 1;
}



void measure_input_reset_indicator(uint8_t index, uint32_t new_indicator)
{
	if (index == 0)
	{
        memset(&m_measure_data.counter[0], 0, sizeof(measure_input_counter_t));
		m_measure_data.counter[0].indicator = new_indicator;
        
		m_pulse_counter_in_backup[0].indicator = new_indicator;
        m_pulse_counter_in_backup[0].total_reserve = 0;
        m_pulse_counter_in_backup[0].total_reserve_index = 0;
		m_pulse_counter_in_backup[0].total_forward_index = new_indicator;
        m_pulse_counter_in_backup[0].total_forward = 0;
	}
#ifndef DTG01
    else
    {
        memset(&m_measure_data.counter[1], 0, sizeof(measure_input_counter_t));
		m_measure_data.counter[1].indicator = new_indicator;
        
		m_pulse_counter_in_backup[1].indicator = new_indicator;
        m_pulse_counter_in_backup[1].total_reserve = 0;
        m_pulse_counter_in_backup[1].total_reserve_index = 0;
		m_pulse_counter_in_backup[1].total_forward_index = new_indicator;
        m_pulse_counter_in_backup[1].total_forward = 0;
    }
#endif
    memcpy(m_pre_pulse_counter_in_backup, m_pulse_counter_in_backup, sizeof(m_pulse_counter_in_backup));
    app_bkup_write_pulse_counter(&m_pulse_counter_in_backup[0]);
}

void measure_input_reset_k(uint8_t index, uint32_t new_k)
{
	if (index == 0)
	{
		m_measure_data.counter[0].k = new_k;
		m_pulse_counter_in_backup[0].k = new_k;
	}
#ifndef DTG01
    else
    {
		m_measure_data.counter[1].k = new_k;
		m_pulse_counter_in_backup[1].k = new_k;
    }
#endif
    
    memcpy(m_pre_pulse_counter_in_backup, m_pulse_counter_in_backup, sizeof(m_pulse_counter_in_backup));
}

void measure_input_reset_counter(uint8_t index)
{
    if (index == 0)
    {
        m_pulse_counter_in_backup[0].real_counter = 0;
        m_pulse_counter_in_backup[0].reverse_counter = 0;
    }
#if defined(DTG02) || defined(DTG02V2)
    else
    {
        m_pulse_counter_in_backup[1].real_counter = 0;
        m_pulse_counter_in_backup[1].reverse_counter = 0;
    }
#endif
    app_bkup_write_pulse_counter(&m_pulse_counter_in_backup[0]);
    memcpy(m_pre_pulse_counter_in_backup, m_pulse_counter_in_backup, sizeof(m_pulse_counter_in_backup));
}

void measure_input_save_all_data_to_flash(void)
{
    app_spi_flash_data_t *spi_flash_store_data = umm_malloc(sizeof(app_spi_flash_data_t));
    if (spi_flash_store_data == NULL)
    {
        DEBUG_WARN("Malloc spi data failed\r\n");
        NVIC_SystemReset();
    }
    sys_ctx_t *ctx = sys_ctx();
    spi_flash_store_data->resend_to_server_flag = 0;

    for (uint32_t j = 0; j < MEASUREMENT_MAX_MSQ_IN_RAM; j++)
    {
		if (m_sensor_msq[j].state != MEASUREMENT_QUEUE_STATE_IDLE)
		{
            DEBUG_INFO("Store data in queue index %u to flash\r\n", j);
			// 4-20mA input
			for (uint32_t i = 0; i < APP_FLASH_NB_OFF_4_20MA_INPUT; i++)
			{
				spi_flash_store_data->input_4_20mA[i] = m_sensor_msq[j].input_4_20mA[i];
                memcpy(&spi_flash_store_data->input_4_20ma_cycle_send_web[i], 
                        &m_sensor_msq[j].input_4_20ma_cycle_send_web[i], 
                        sizeof(input_4_20ma_min_max_hour_t));
			}
			
			// Meter input
			for (uint32_t i = 0; i < APP_FLASH_NB_OF_METER_INPUT; i++)
			{
				memcpy(&spi_flash_store_data->counter[i], &m_sensor_msq[j].counter[i], sizeof(measure_input_counter_t));
			}			
			
#ifndef DTG01
			// On/off
			spi_flash_store_data->on_off.name.input_on_off_0 = m_sensor_msq[j].input_on_off[0];
			spi_flash_store_data->on_off.name.input_on_off_0 = m_sensor_msq[j].input_on_off[1];
			spi_flash_store_data->on_off.name.input_on_off_1 = m_sensor_msq[j].input_on_off[2];
			spi_flash_store_data->on_off.name.input_on_off_2 = m_sensor_msq[j].input_on_off[3];
			spi_flash_store_data->on_off.name.output_on_off_0 = m_sensor_msq[j].output_on_off[0];
			spi_flash_store_data->on_off.name.output_on_off_1 = m_sensor_msq[j].output_on_off[1];
			spi_flash_store_data->on_off.name.output_on_off_2 = m_sensor_msq[j].output_on_off[2];
			spi_flash_store_data->on_off.name.output_on_off_3 = m_sensor_msq[j].output_on_off[3];
#endif
			spi_flash_store_data->output_4_20mA[0] = m_sensor_msq[j].output_4_20mA[0];
			
			spi_flash_store_data->timestamp = m_sensor_msq[j].measure_timestamp;
			spi_flash_store_data->valid_flag = APP_FLASH_VALID_DATA_KEY;
			spi_flash_store_data->resend_to_server_flag = 0;
			spi_flash_store_data->vbat_mv = m_sensor_msq[j].vbat_mv;
			spi_flash_store_data->vbat_precent = m_sensor_msq[j].vbat_percent;
			spi_flash_store_data->temp = m_sensor_msq[j].temperature;
            spi_flash_store_data->csq_percent = m_sensor_msq[j].csq_percent;
			
			// 485
			for (uint32_t nb_485_device = 0; nb_485_device < RS485_MAX_SLAVE_ON_BUS; nb_485_device++)
			{
				memcpy(&spi_flash_store_data->rs485[nb_485_device], 
                        &m_sensor_msq[j].rs485[nb_485_device], 
                        sizeof(measure_input_modbus_register_t));
			}
			
			
			if (!ctx->peripheral_running.name.flash_running)
			{
				spi_init();
				app_spi_flash_wakeup();
				ctx->peripheral_running.name.flash_running = 1;
			}
			app_spi_flash_write_data(spi_flash_store_data);
			m_sensor_msq[j].state = MEASUREMENT_QUEUE_STATE_IDLE;
		}
    }   
    
    umm_free(spi_flash_store_data);
}

uint32_t estimate_measure_timestamp = 0;
void measure_input_task(void)
{
	eeprom_cfg = app_eeprom_read_config_data();
    // Poll input counter state machine
    measure_input_pulse_counter_poll();
    
    // Get ADC result pointer
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
        ENABLE_INPUT_4_20MA_POWER(1);
        estimate_measure_timestamp = current_sec;
    }
    
    if (adc_retval->bat_mv > VBAT_DETECT_HIGH_MV)
    {
        ctx->peripheral_running.name.high_bat_detect = 1;
    }
    else
    {
        ctx->peripheral_running.name.high_bat_detect = 0;
    }
        
#ifndef DTG01
    if (sys_ctx()->status.is_enter_test_mode == 0)
    {
        TRANS_1_OUTPUT(eeprom_cfg->io_enable.name.output0);
        TRANS_2_OUTPUT(eeprom_cfg->io_enable.name.output1);
        TRANS_3_OUTPUT(eeprom_cfg->io_enable.name.output2);
        TRANS_4_OUTPUT(eeprom_cfg->io_enable.name.output3);
    }
    
    // Get input and output on/off value
	m_measure_data.input_on_off[0] = LL_GPIO_IsInputPinSet(OPTOIN1_GPIO_Port, OPTOIN1_Pin) ? 1 : 0;
	m_measure_data.input_on_off[1] = LL_GPIO_IsInputPinSet(OPTOIN2_GPIO_Port, OPTOIN2_Pin) ? 1 : 0;
	m_measure_data.input_on_off[2] = LL_GPIO_IsInputPinSet(OPTOIN3_GPIO_Port, OPTOIN3_Pin) ? 1 : 0;
	m_measure_data.input_on_off[3] = LL_GPIO_IsInputPinSet(OPTOIN4_GPIO_Port, OPTOIN4_Pin) ? 1 : 0;
	m_measure_data.output_on_off[0] = TRANS_1_IS_OUTPUT_HIGH();
	m_measure_data.output_on_off[1] = TRANS_2_IS_OUTPUT_HIGH();
	m_measure_data.output_on_off[2] = TRANS_3_IS_OUTPUT_HIGH();
	m_measure_data.output_on_off[3] = TRANS_4_IS_OUTPUT_HIGH();
	
    // Check meter input circuit status
	m_measure_data.counter[MEASURE_INPUT_PORT_2].cir_break = LL_GPIO_IsInputPinSet(CIRIN2_GPIO_Port, CIRIN2_Pin)
																				&& (eeprom_cfg->meter_mode[1] != APP_EEPROM_METER_MODE_DISABLE);   
    m_measure_data.counter[MEASURE_INPUT_PORT_1].cir_break = LL_GPIO_IsInputPinSet(CIRIN1_GPIO_Port, CIRIN1_Pin)
																				 && (eeprom_cfg->meter_mode[0] != APP_EEPROM_METER_MODE_DISABLE);	
#else
    TRANS_OUTPUT(eeprom_cfg->io_enable.name.output0);
	m_measure_data.output_on_off[0] = TRANS_IS_OUTPUT_HIGH();
	m_measure_data.counter[MEASURE_INPUT_PORT_1].cir_break = LL_GPIO_IsInputPinSet(CIRIN0_GPIO_Port, CIRIN0_Pin)
																				 && (eeprom_cfg->meter_mode[0] != APP_EEPROM_METER_MODE_DISABLE);
#endif	
    
    // Get Vbat
    m_measure_data.vbat_percent = adc_retval->bat_percent;
    m_measure_data.vbat_mv = adc_retval->bat_mv;
    
    // Vtemp
    if (adc_retval->temp_is_valid)
    {
        m_measure_data.temperature_error = 0;
        m_measure_data.temperature = adc_retval->temp;
    }
    else
    {
        m_measure_data.temperature_error = 1;
    }

	if ((m_this_is_the_first_time || (current_sec >= estimate_measure_timestamp))
        && (measure_input_turn_on_in_4_20ma_power == 0))
	{
#ifdef DTG01
        if (eeprom_cfg->io_enable.name.input_4_20ma_0_enable)
#else
		if (eeprom_cfg->io_enable.name.input_4_20ma_0_enable
			|| eeprom_cfg->io_enable.name.input_4_20ma_1_enable
			|| eeprom_cfg->io_enable.name.input_4_20ma_2_enable
			|| eeprom_cfg->io_enable.name.input_4_20ma_3_enable)
#endif
        {
            if (!INPUT_POWER_4_20_MA_IS_ENABLE())
            {
				ENABLE_INPUT_4_20MA_POWER(1);
                measure_input_turn_on_in_4_20ma_power = DEFAULT_INPUT_4_20MA_ENABLE_TIMEOUT;
                ctx->peripheral_running.name.wait_for_input_4_20ma_power_on = 1;
            }
        }

        if (measure_input_turn_on_in_4_20ma_power == 0)
        {
            rtc_date_time_t now;
            app_rtc_get_time(&now);
            estimate_measure_timestamp = (measure_interval_sec*(current_sec/measure_interval_sec + 1));
            uint32_t next_hour = estimate_measure_timestamp/3600;
            next_hour %= 24;
            uint32_t next_min = (estimate_measure_timestamp/60);
            next_min %= 60;
            if (sys_ctx()->status.is_enter_test_mode == 0)
            {
                DEBUG_WARN("[%02u:%02u] Next measurement tick is %02u:%02u\r\n", now.hour, now.minute, next_hour, next_min);
            }
            
            // Process rs485
            process_rs485(&m_measure_data.rs485[0]);
            
//            DEBUG_INFO("ADC start\r\n");
            // ADC conversion
            adc_start();

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
                m_measure_data.measure_timestamp = app_rtc_get_counter();
                m_measure_data.vbat_mv = adc_retval->bat_mv;
                m_measure_data.vbat_percent = adc_retval->bat_percent;
                
#ifndef DTG01             
                m_measure_data.vin_mv = adc_retval->vin_24;
#endif
//                app_bkup_read_pulse_counter(&m_measure_data.counter[0]);

				// CSQ
                m_measure_data.csq_percent = gsm_get_csq_in_percent();
                
				// Input 4-20mA
                // If input is disable, we set value of input 4-20mA to zero
                if (eeprom_cfg->io_enable.name.input_4_20ma_0_enable)
                {
#if FAKE_MIN_MAX_DATA     
                    #warning "Fake data"
                    adc_retval->in_4_20ma_in[0] = adc_retval->bat_mv;
#endif
                    m_measure_data.input_4_20mA[0] = adc_retval->in_4_20ma_in[0];                            
                    // Set init min-max value
                    if (input_4_20ma_min_value[0] == INPUT_4_20MA_INVALID_VALUE)
                    {
                        input_4_20ma_min_value[0] = m_measure_data.input_4_20mA[0];
                    }
                    if (input_4_20ma_max_value[0] == INPUT_4_20MA_INVALID_VALUE)
                    {
                        input_4_20ma_max_value[0] = m_measure_data.input_4_20mA[0];
                    }
                    
                    if (input_4_20ma_min_value[0] > adc_retval->in_4_20ma_in[0])
                    {
                        input_4_20ma_min_value[0] = adc_retval->in_4_20ma_in[0];
                    }                        
                    if (input_4_20ma_max_value[0] < adc_retval->in_4_20ma_in[0])
                    {
                        input_4_20ma_max_value[0] = adc_retval->in_4_20ma_in[0];
                    }  
                }
                else
                {
                    m_measure_data.input_4_20mA[0] = 0;
                    input_4_20ma_min_value[0] = 0;
                }
#ifndef DTG01         

#if FAKE_MIN_MAX_DATA     
                adc_retval->in_4_20ma_in[1] = (float)adc_retval->bat_mv;
                adc_retval->in_4_20ma_in[2] = (float)adc_retval->bat_mv;
                adc_retval->in_4_20ma_in[3] = (float)adc_retval->bat_mv;
#endif
                
                if (eeprom_cfg->io_enable.name.input_4_20ma_1_enable)
                {
                    m_measure_data.input_4_20mA[1] = adc_retval->in_4_20ma_in[1];
                }
                else
                {
                    m_measure_data.input_4_20mA[1] = 0;
                    input_4_20ma_min_value[1] = 0;
                }
                if (input_4_20ma_min_value[1] > adc_retval->in_4_20ma_in[1])
                {
                    input_4_20ma_min_value[1] = adc_retval->in_4_20ma_in[1];
                }                        
                if (input_4_20ma_max_value[1] < adc_retval->in_4_20ma_in[1])
                {
                    input_4_20ma_max_value[1] = adc_retval->in_4_20ma_in[1];
                } 
                
                
                if (eeprom_cfg->io_enable.name.input_4_20ma_2_enable)
                {
                    m_measure_data.input_4_20mA[2] = adc_retval->in_4_20ma_in[2];
                }
                else
                {
                    m_measure_data.input_4_20mA[2] = 0;
                    input_4_20ma_min_value[2] = 0;
                }
                if (input_4_20ma_min_value[2] > adc_retval->in_4_20ma_in[2])
                {
                    input_4_20ma_min_value[2] = adc_retval->in_4_20ma_in[2];
                }                        
                if (input_4_20ma_max_value[2] < adc_retval->in_4_20ma_in[2])
                {
                    input_4_20ma_max_value[2] = adc_retval->in_4_20ma_in[2];
                }
                
                
                if (eeprom_cfg->io_enable.name.input_4_20ma_3_enable)
                {
                    m_measure_data.input_4_20mA[3] = adc_retval->in_4_20ma_in[3];
                }
                else
                {
                    m_measure_data.input_4_20mA[3] = 0;
                    input_4_20ma_min_value[3] = 0;
                }
                
                if (input_4_20ma_min_value[3] > adc_retval->in_4_20ma_in[3])
                {
                    input_4_20ma_min_value[3] = adc_retval->in_4_20ma_in[3];
                }                        
                if (input_4_20ma_max_value[3] < adc_retval->in_4_20ma_in[3])
                {
                    input_4_20ma_max_value[3] = adc_retval->in_4_20ma_in[3];
                }
#endif
				// Output 4-20mA
				if (eeprom_cfg->io_enable.name.output_4_20ma_enable)
				{
					m_measure_data.output_4_20mA[0] = eeprom_cfg->output_4_20ma;
				}
				else
				{
					m_measure_data.output_4_20mA[0] = 0;
				}
                
                uint32_t now = sys_get_ms();
                // Calculate avg flow speed between 2 times measurement data
                uint32_t diff = (uint32_t)(now - m_last_time_measure_data);
                
                // Copy backup data into current measure data
                bool estimate_time_save_min_max_value = false;
                uint32_t current_counter = app_rtc_get_counter();
                uint32_t wakeup_counter = gsm_data_layer_get_estimate_wakeup_time_stamp();
                
                for (uint32_t counter_index = 0; counter_index < MEASURE_NUMBER_OF_WATER_METER_INPUT; counter_index++)
                {
                    if (diff)
                    {
                        // Forward direction
                        // speed = ((counter  forward - pre_counter forward)/k)/diff (hour)
                        m_pulse_counter_in_backup[counter_index].flow_speed_forward_agv_cycle_wakeup = (float)(m_pulse_counter_in_backup[counter_index].fw_flow 
                                                                                                        - m_pre_pulse_counter_in_backup[counter_index].fw_flow);
                        m_pulse_counter_in_backup[counter_index].flow_speed_forward_agv_cycle_wakeup /= m_pulse_counter_in_backup[counter_index].k; 
                        
                        DEBUG_VERBOSE("Diff forward%u - %.2f\r\n", counter_index, m_pulse_counter_in_backup[counter_index].flow_speed_forward_agv_cycle_wakeup);
                        if (eeprom_cfg->io_enable.name.calculate_flow_speed)
                        {
                            m_pulse_counter_in_backup[counter_index].flow_speed_forward_agv_cycle_wakeup *= 3600000.0f/(float)diff;
                        }            
                        m_pulse_counter_in_backup[counter_index].flow_avg_cycle_send_web.fw_flow_sum += m_pulse_counter_in_backup[counter_index].flow_speed_forward_agv_cycle_wakeup;
                        
                        // Reverse direction
                        m_pulse_counter_in_backup[counter_index].flow_speed_reserve_agv_cycle_wakeup = (float)(m_pulse_counter_in_backup[counter_index].reverse_flow 
                                                                                                        - m_pre_pulse_counter_in_backup[counter_index].reverse_flow);
                        m_pulse_counter_in_backup[counter_index].flow_speed_reserve_agv_cycle_wakeup /= m_pulse_counter_in_backup[counter_index].k;
                        m_pulse_counter_in_backup[counter_index].flow_avg_cycle_send_web.reverse_flow_sum += m_pulse_counter_in_backup[counter_index].flow_speed_reserve_agv_cycle_wakeup;
                        
                        DEBUG_VERBOSE("Diff reserve%u - %.2f\r\n", counter_index, m_pulse_counter_in_backup[counter_index].flow_speed_reserve_agv_cycle_wakeup);
                        if (eeprom_cfg->io_enable.name.calculate_flow_speed)
                        {
                            m_pulse_counter_in_backup[counter_index].flow_speed_reserve_agv_cycle_wakeup *= 3600000.0f/(float)diff;
                        }
                        
                        // min-max of forward/reserve direction flow speed
                        // If value is invalid =>> Initialize new value
                        // If min max value is invalid =>> Copy the first data
                        if (fw_flow_min_in_cycle_send_web[counter_index] == FLOW_INVALID_VALUE
                            || fw_flow_max_in_cycle_send_web[counter_index] == FLOW_INVALID_VALUE)        
                        {
                            fw_flow_min_in_cycle_send_web[counter_index] = m_pulse_counter_in_backup[counter_index].flow_speed_forward_agv_cycle_wakeup;
                            fw_flow_max_in_cycle_send_web[counter_index] = m_pulse_counter_in_backup[counter_index].flow_speed_forward_agv_cycle_wakeup;
                            DEBUG_WARN("Set MinFwFlow%u - %.2f\r\n", counter_index, fw_flow_max_in_cycle_send_web[counter_index]);
                            DEBUG_WARN("Set MaxFwFlow%u - %.2f\r\n", counter_index, fw_flow_min_in_cycle_send_web[counter_index]);
                        }
                        
                        if (reserve_flow_min_in_cycle_send_web[counter_index] == FLOW_INVALID_VALUE
                            || reserve_flow_max_in_cycle_send_web[counter_index] == FLOW_INVALID_VALUE)        
                        {
                            reserve_flow_min_in_cycle_send_web[counter_index] = m_pulse_counter_in_backup[counter_index].flow_speed_reserve_agv_cycle_wakeup;
                            reserve_flow_max_in_cycle_send_web[counter_index] = m_pulse_counter_in_backup[counter_index].flow_speed_reserve_agv_cycle_wakeup;
                            DEBUG_WARN("Set MaxRsvFlow%u - %.2f\r\n", counter_index, reserve_flow_max_in_cycle_send_web[counter_index]);
                            DEBUG_WARN("Set MixRsvFlow%u - %.2f\r\n", counter_index, reserve_flow_min_in_cycle_send_web[counter_index]);
                        }
                        
                        // Compare old value and new value to determite new min-max value
                        // 1.1 Forward direction min value
                        if (fw_flow_min_in_cycle_send_web[counter_index] > m_pulse_counter_in_backup[counter_index].flow_speed_forward_agv_cycle_wakeup)        
                        {
                            fw_flow_min_in_cycle_send_web[counter_index] = m_pulse_counter_in_backup[counter_index].flow_speed_forward_agv_cycle_wakeup;
                        }
                        DEBUG_INFO("[Temporaty] MinFlow%u - %.2f\r\n", counter_index, fw_flow_min_in_cycle_send_web[counter_index]);
                        
                        // 1.2 Forward direction max value
                        if (fw_flow_max_in_cycle_send_web[counter_index] < m_pulse_counter_in_backup[counter_index].flow_speed_forward_agv_cycle_wakeup)        
                        {
                            fw_flow_max_in_cycle_send_web[counter_index] = m_pulse_counter_in_backup[counter_index].flow_speed_forward_agv_cycle_wakeup;
                        }
                        DEBUG_INFO("[Temporaty] MaxFlow%u - %.2f\r\n", counter_index, fw_flow_max_in_cycle_send_web[counter_index]);
                        
                        // 2.1 reserve direction min value
                        if (reserve_flow_min_in_cycle_send_web[counter_index] > m_pulse_counter_in_backup[counter_index].flow_speed_reserve_agv_cycle_wakeup)        
                        {
                            reserve_flow_min_in_cycle_send_web[counter_index] = m_pulse_counter_in_backup[counter_index].flow_speed_reserve_agv_cycle_wakeup;
                        }
                        
                        // 2.2 reserve direction max value
                        if (reserve_flow_max_in_cycle_send_web[counter_index] < m_pulse_counter_in_backup[counter_index].flow_speed_reserve_agv_cycle_wakeup)        
                        {
                            reserve_flow_max_in_cycle_send_web[counter_index] = m_pulse_counter_in_backup[counter_index].flow_speed_reserve_agv_cycle_wakeup;
                        }
                        
                        
                        estimate_time_save_min_max_value = true;
                    }
                    else        // invalid value, should never get there
                    {
                        DEBUG_ERROR("Shoud never get there\r\n");
                        m_pulse_counter_in_backup[counter_index].flow_speed_forward_agv_cycle_wakeup = 0;
                        m_pulse_counter_in_backup[counter_index].flow_speed_reserve_agv_cycle_wakeup = 0;
                    }
                    
                    if (estimate_time_save_min_max_value)
                    {
                        // If current counter >= wakeup counter =>> Send min max to server
                        // Else we dont need to send min max data to server
                        if (current_counter >= wakeup_counter)
                        {
                            // Pulse counter
                            m_pulse_counter_in_backup[counter_index].flow_avg_cycle_send_web.fw_flow_max = fw_flow_max_in_cycle_send_web[counter_index];
                            m_pulse_counter_in_backup[counter_index].flow_avg_cycle_send_web.fw_flow_min = fw_flow_min_in_cycle_send_web[counter_index];
                            m_pulse_counter_in_backup[counter_index].flow_avg_cycle_send_web.reserve_flow_max = reserve_flow_max_in_cycle_send_web[counter_index];
                            m_pulse_counter_in_backup[counter_index].flow_avg_cycle_send_web.reserve_flow_min = reserve_flow_min_in_cycle_send_web[counter_index];
                            
                            m_pulse_counter_in_backup[counter_index].flow_avg_cycle_send_web.valid = 1; 
                            
                            DEBUG_WARN("Cycle send web counter %u: Min max fw flow %.2f %.2f\r\n", counter_index, 
                                                        m_pulse_counter_in_backup[counter_index].flow_avg_cycle_send_web.fw_flow_max,
                                                        m_pulse_counter_in_backup[counter_index].flow_avg_cycle_send_web.fw_flow_min);
                
                            DEBUG_WARN("Cycle send web counter %u: Min max resv flow %.2f %.2f\r\n", counter_index,
                                                        m_pulse_counter_in_backup[counter_index].flow_avg_cycle_send_web.reserve_flow_max,
                                                        m_pulse_counter_in_backup[counter_index].flow_avg_cycle_send_web.reserve_flow_min);
                            
                            // Clean min max value
                            fw_flow_max_in_cycle_send_web[counter_index] = FLOW_INVALID_VALUE;
                            fw_flow_min_in_cycle_send_web[counter_index] = FLOW_INVALID_VALUE;
                            reserve_flow_max_in_cycle_send_web[counter_index] = FLOW_INVALID_VALUE;
                            reserve_flow_min_in_cycle_send_web[counter_index] = FLOW_INVALID_VALUE;
                        }
                        else        // Mark invalid, we dont need to send data to server
                        {
                            m_pulse_counter_in_backup[counter_index].flow_avg_cycle_send_web.valid = 0;
                        }
                    }

                                               
                    // Copy current value to prev value
                    memcpy(&m_pre_pulse_counter_in_backup[counter_index], &m_pulse_counter_in_backup[counter_index], sizeof(backup_pulse_data_t));
                    
                    // Copy current measure counter to message queue variable
                    memcpy(&m_measure_data.counter[counter_index], &m_pulse_counter_in_backup[counter_index], sizeof(measure_input_counter_t));
                    
                    __disable_irq(); // counter ext interrupt maybe happen, disable for isr safe
                    m_pulse_counter_in_backup[counter_index].fw_flow = 0;
                    m_pre_pulse_counter_in_backup[counter_index].fw_flow = 0;
                    m_pulse_counter_in_backup[counter_index].reverse_flow  = 0;
                    m_pre_pulse_counter_in_backup[counter_index].reverse_flow = 0;
                    
                    if (m_pulse_counter_in_backup[counter_index].flow_avg_cycle_send_web.valid)
                    {
                        // Clean all data after 1 cycle send web, next time we will measure again
                        m_pulse_counter_in_backup[counter_index].flow_avg_cycle_send_web.fw_flow_max = FLOW_INVALID_VALUE;
                        m_pulse_counter_in_backup[counter_index].flow_avg_cycle_send_web.fw_flow_min = FLOW_INVALID_VALUE;
                        m_pulse_counter_in_backup[counter_index].flow_avg_cycle_send_web.reserve_flow_max = FLOW_INVALID_VALUE;
                        m_pulse_counter_in_backup[counter_index].flow_avg_cycle_send_web.reserve_flow_min = FLOW_INVALID_VALUE;
                        m_pulse_counter_in_backup[counter_index].flow_avg_cycle_send_web.fw_flow_sum = 0.0f;
                        m_pulse_counter_in_backup[counter_index].flow_avg_cycle_send_web.reverse_flow_sum = 0.0f;
                    }
                    __enable_irq();
                    // Mark as invalid data
                    m_pulse_counter_in_backup[counter_index].flow_avg_cycle_send_web.valid = 0;
                    
                    DEBUG_VERBOSE("PWM%u %u\r\n", counter_index, m_measure_data.counter[counter_index].real_counter);
                }
                
                // If current counter >= wakeup counter =>> Send min max to server
                // Else we dont need to send min max data to server
                if (current_counter >= wakeup_counter)
                {
                    // Store 485 min - max value
                                            
                    // 485
                    for (uint32_t slave_index = 0; slave_index < RS485_MAX_SLAVE_ON_BUS; slave_index++)
                    {
                        m_measure_data.rs485[slave_index].min_max.min_forward_flow = m_485_min_max[slave_index].min_forward_flow;
                        m_measure_data.rs485[slave_index].min_max.max_forward_flow = m_485_min_max[slave_index].max_forward_flow;
                        
                        m_measure_data.rs485[slave_index].min_max.min_reserve_flow = m_485_min_max[slave_index].min_reserve_flow;
                        m_measure_data.rs485[slave_index].min_max.max_reserve_flow = m_485_min_max[slave_index].max_reserve_flow;
                        
                        m_measure_data.rs485[slave_index].min_max.valid = 1;
                    }
                    
                    // 4-20mA
                    // Input 4-20mA
                    for (uint32_t i = 0; i < NUMBER_OF_INPUT_4_20MA; i++)
                    {
                        m_measure_data.input_4_20ma_cycle_send_web[i].input4_20ma_min = input_4_20ma_min_value[i];
                        m_measure_data.input_4_20ma_cycle_send_web[i].input4_20ma_max = input_4_20ma_max_value[i];
                        m_measure_data.input_4_20ma_cycle_send_web[i].valid = 1;
                    }
                }
                else
                {
                     for (uint32_t slave_index = 0; slave_index < RS485_MAX_SLAVE_ON_BUS; slave_index++)
                     {
                        m_measure_data.rs485[slave_index].min_max.min_forward_flow.type_int = INPUT_485_INVALID_INT_VALUE;
                        m_measure_data.rs485[slave_index].min_max.max_forward_flow.type_int = INPUT_485_INVALID_INT_VALUE;
                        
                        m_measure_data.rs485[slave_index].min_max.min_reserve_flow.type_int = INPUT_485_INVALID_INT_VALUE;
                        m_measure_data.rs485[slave_index].min_max.max_reserve_flow.type_int = INPUT_485_INVALID_INT_VALUE;
                        m_measure_data.rs485[slave_index].min_max.valid = 0;
                     }
             
                     for (uint32_t i = 0; i < NUMBER_OF_INPUT_4_20MA; i++)
                     { 
                         m_measure_data.input_4_20ma_cycle_send_web[i].valid = 0;
                     }
                }
                    
#ifdef DTG01        
				m_measure_data.output_on_off[0] = TRANS_IS_OUTPUT_HIGH();
#endif
                m_measure_data.temperature = adc_retval->temp;
                m_measure_data.state = MEASUREMENT_QUEUE_STATE_PENDING;
#if ALWAYS_SAVE_DATA_TO_FLASH == 0                
                bool scan = true;
                while (scan)
                {
                    // Scan for empty buffer
                    bool queue_full = true;
                    for (uint32_t i = 0; i < MEASUREMENT_MAX_MSQ_IN_RAM; i++)
                    {
                        if (m_sensor_msq[i].state == MEASUREMENT_QUEUE_STATE_IDLE)
                        {
                            memcpy(&m_sensor_msq[i], &m_measure_data, sizeof(measure_input_perpheral_data_t));
                            queue_full = false;
                            DEBUG_INFO("Puts new msg to sensor queue\r\n");
                            scan = false;
                            break;
                        }
                    }        

                    if (queue_full)
                    {
                        DEBUG_ERROR("Message queue full\r\n");
                        measure_input_save_all_data_to_flash();
                    }
                }
#else
                    for (uint32_t i = 0; i < MEASUREMENT_MAX_MSQ_IN_RAM; i++)
                    {
                        if (m_sensor_msq[i].state == MEASUREMENT_QUEUE_STATE_IDLE)
                        {
                            DEBUG_VERBOSE("Store data to flash with reserve index %u %u\r\n", 
                                        m_sensor_msq[i].counter[0].total_reserve_index,
                                        m_sensor_msq[i].counter[1].total_reserve_index);
                            
                            DEBUG_INFO("Pulse counter in BKP: %u-%u, %u-%u\r\n", 
                                        m_pulse_counter_in_backup[0].real_counter, m_pulse_counter_in_backup[0].reverse_counter, 
                                        m_pulse_counter_in_backup[1].real_counter, m_pulse_counter_in_backup[1].reverse_counter);
                            

                            memcpy(&m_sensor_msq[i], &m_measure_data, sizeof(measure_input_perpheral_data_t));
                            measure_input_save_all_data_to_flash();
                            break;
                        }
                    }        
#endif
            }        
            
            m_last_time_measure_data = sys_get_ms();
            ctx->peripheral_running.name.adc = 0;
            ctx->peripheral_running.name.wait_for_input_4_20ma_power_on = 0;
        }
	}
}

void measure_input_initialize(void)
{	
    m_last_time_measure_data = sys_get_ms();
	app_eeprom_config_data_t *eeprom_cfg = app_eeprom_read_config_data();
    memset(m_pulse_counter_in_backup, 0, sizeof(m_pulse_counter_in_backup));
	m_pulse_counter_in_backup[0].k = eeprom_cfg->k[0];
	m_pulse_counter_in_backup[0].indicator = eeprom_cfg->offset[0];
	m_measure_data.counter[0].k = eeprom_cfg->k[0];
	m_measure_data.counter[0].indicator = eeprom_cfg->offset[0];
    
    // Mark forward-reserve value is invalid
    fw_flow_min_in_cycle_send_web[0] = FLOW_INVALID_VALUE;
    fw_flow_max_in_cycle_send_web[0] = FLOW_INVALID_VALUE;
    reserve_flow_min_in_cycle_send_web[0] = FLOW_INVALID_VALUE;
    reserve_flow_max_in_cycle_send_web[0] = FLOW_INVALID_VALUE;
    
#ifndef DTG01
	m_pulse_counter_in_backup[1].k = eeprom_cfg->k[1];
	m_pulse_counter_in_backup[1].indicator = eeprom_cfg->offset[1];
	m_measure_data.counter[1].k = eeprom_cfg->k[1];
	m_measure_data.counter[1].indicator = eeprom_cfg->offset[1];
    
    // Mark forward-reserve value is invalid
    fw_flow_min_in_cycle_send_web[1] = FLOW_INVALID_VALUE;
    fw_flow_max_in_cycle_send_web[1] = FLOW_INVALID_VALUE;
    reserve_flow_min_in_cycle_send_web[1] = FLOW_INVALID_VALUE;
    reserve_flow_max_in_cycle_send_web[1] = FLOW_INVALID_VALUE;
#endif	
    
    // Reset input 4-20ma to invalid value
    for (uint32_t i = 0; i < NUMBER_OF_INPUT_4_20MA; i++)
    {
        input_4_20ma_min_value[i] = INPUT_4_20MA_INVALID_VALUE;
        input_4_20ma_max_value[i] = INPUT_4_20MA_INVALID_VALUE;
    }
    
    // Reset 485 register min - max
    for (uint32_t i = 0; i < RS485_MAX_SLAVE_ON_BUS; i++)
    {
        m_485_min_max[i].min_forward_flow.type_int = INPUT_485_INVALID_INT_VALUE;
        m_485_min_max[i].max_forward_flow.type_int = INPUT_485_INVALID_INT_VALUE;
        m_485_min_max[i].min_reserve_flow.type_int = INPUT_485_INVALID_INT_VALUE;
        m_485_min_max[i].max_reserve_flow.type_int = INPUT_485_INVALID_INT_VALUE;
        
    }
	
    /* Doc gia tri do tu bo nho backup, neu gia tri tu BKP < flash -> lay theo gia tri flash
    * -> Case: Mat dien nguon -> mat du lieu trong RTC backup register
    */
    
	app_bkup_read_pulse_counter(&m_pulse_counter_in_backup[0]);
    
    for (uint32_t i = 0; i < MEASUREMENT_MAX_MSQ_IN_RAM; i++)
    {
        m_sensor_msq[i].state = MEASUREMENT_QUEUE_STATE_IDLE;
    }
    
    app_spi_flash_data_t last_data;
    if (app_spi_flash_get_lastest_data(&last_data))
    {
        bool save = false;
        if (last_data.counter[0].real_counter > m_pulse_counter_in_backup[0].real_counter)
        {
            m_pulse_counter_in_backup[0].real_counter = last_data.counter[0].real_counter;
            save = true;
        }
        
        if (last_data.counter[0].reverse_counter > m_pulse_counter_in_backup[0].reverse_counter)
        {
            m_pulse_counter_in_backup[0].reverse_counter = last_data.counter[0].reverse_counter;
            save = true;
        }
#ifndef DTG01
        if (last_data.counter[1].real_counter > m_pulse_counter_in_backup[1].real_counter)
        {
            m_pulse_counter_in_backup[1].real_counter = last_data.counter[1].real_counter;
            save = true; 
        }
        
        if (last_data.counter[1].reverse_counter > m_pulse_counter_in_backup[1].reverse_counter)
        {
            m_pulse_counter_in_backup[1].reverse_counter = last_data.counter[1].reverse_counter;
            save = true;    
        }
        
        if (save)
        {
//            DEBUG_VERBOSE("Save new data from flash\r\n");
            app_bkup_write_pulse_counter(&m_pulse_counter_in_backup[0]);
        }
		DEBUG_INFO("Pulse counter in BKP: %u-%u, %u-%u\r\n", 
                    m_pulse_counter_in_backup[0].real_counter, m_pulse_counter_in_backup[0].reverse_counter, 
                    m_pulse_counter_in_backup[1].real_counter, m_pulse_counter_in_backup[1].reverse_counter);
#else
        
        if (save)
        {
            app_bkup_write_pulse_counter(&m_pulse_counter_in_backup[0]);
        }
		DEBUG_INFO("Pulse counter in BKP: %u-%u\r\n", 
                    m_pulse_counter_in_backup[0].real_counter, m_pulse_counter_in_backup[0].reverse_counter);
#endif
    }
    memcpy(m_pre_pulse_counter_in_backup, m_pulse_counter_in_backup, sizeof(m_pulse_counter_in_backup));
    m_this_is_the_first_time = true;
}


void measure_input_rs485_uart_handler(uint8_t data)
{
    modbus_memory_serial_rx(data); 
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

//volatile uint32_t m_last_direction_is_forward = 1;
void measure_input_pulse_irq(measure_input_water_meter_input_t *input)
{
	__disable_irq();
    if (input->new_data_type == MEASURE_INPUT_NEW_DATA_TYPE_PWM_PIN)
    {        
        m_end_pulse_timestamp[input->port].counter_pwm = app_rtc_get_counter();
        m_end_pulse_timestamp[input->port].subsecond_pwm = app_rtc_get_subsecond_counter();
        
        m_pull_diff[input->port] = get_diff_ms(&m_begin_pulse_timestamp[input->port], 
                                            &m_end_pulse_timestamp[input->port],
                                            MEASURE_INPUT_NEW_DATA_TYPE_PWM_PIN);
        m_begin_pulse_timestamp[input->port].counter_dir = m_end_pulse_timestamp[input->port].counter_dir;
        m_begin_pulse_timestamp[input->port].subsecond_dir = m_end_pulse_timestamp[input->port].subsecond_dir;
        if (m_pull_diff[input->port] > PULSE_MINMUM_WITDH_MS)
        {
                    // Increase total forward counter
            
            m_is_pulse_trigger = 1;
            if (eeprom_cfg->meter_mode[input->port] == APP_EEPROM_METER_MODE_PWM_PLUS_DIR_MIN)
            {
                // If direction current level = direction on forward level
                if (eeprom_cfg->dir_level == input->dir_level)
                {
                    DEBUG_VERBOSE("[PWM%u]+++ \r\n", input->port);
                    m_pulse_counter_in_backup[input->port].total_forward++;
                    m_pulse_counter_in_backup[input->port].fw_flow++;
                    m_pulse_counter_in_backup[input->port].total_forward_index = m_pulse_counter_in_backup[input->port].total_forward/m_pulse_counter_in_backup[input->port].k
                                                                                + m_pulse_counter_in_backup[input->port].indicator;
                    m_pulse_counter_in_backup[input->port].real_counter++;
                }
                else    // Reverser counter
                {
                    DEBUG_WARN("[PWM]---\r\n");
                    m_pulse_counter_in_backup[input->port].reverse_flow++;
                    m_pulse_counter_in_backup[input->port].real_counter--;
                    m_pulse_counter_in_backup[input->port].total_reserve++;
                    m_pulse_counter_in_backup[input->port].total_reserve_index = m_pulse_counter_in_backup[input->port].total_reserve
                                                                                /m_pulse_counter_in_backup[input->port].k;
                }
                m_pulse_counter_in_backup[input->port].reverse_counter = 0;
            }
            else if (eeprom_cfg->meter_mode[input->port] == APP_EEPROM_METER_MODE_ONLY_PWM)
            {
                DEBUG_VERBOSE("[PWM] +++++++\r\n");
                m_pulse_counter_in_backup[input->port].total_forward++;
                m_pulse_counter_in_backup[input->port].total_forward++;
                            // Calculate total forward index
                m_pulse_counter_in_backup[input->port].total_forward_index = m_pulse_counter_in_backup[input->port].total_forward/m_pulse_counter_in_backup[input->port].k
                                                                                + m_pulse_counter_in_backup[input->port].indicator;
                
                m_pulse_counter_in_backup[input->port].real_counter++;
                m_pulse_counter_in_backup[input->port].reverse_counter = 0;
            }
            else if (eeprom_cfg->meter_mode[input->port] == APP_EEPROM_METER_MODE_DISABLE)
            {
                m_pulse_counter_in_backup[input->port].real_counter = 0;
                m_pulse_counter_in_backup[input->port].reverse_counter = 0;
                m_pulse_counter_in_backup[input->port].total_forward_index = 0;
                m_pulse_counter_in_backup[input->port].total_forward = 0;
            }
        }
        else
        {
            DEBUG_WARN("PWM Noise\r\n");
        }
    }
    else if (input->new_data_type == MEASURE_INPUT_NEW_DATA_TYPE_DIR_PIN)
    {
        if (eeprom_cfg->meter_mode[input->port] == APP_EEPROM_METER_MODE_PWM_F_PWM_R)
        {
            m_is_pulse_trigger = 1;
              
            m_end_pulse_timestamp[input->port].counter_dir = app_rtc_get_counter();
            m_end_pulse_timestamp[input->port].subsecond_dir = app_rtc_get_subsecond_counter();
            
            m_pull_diff[input->port] = get_diff_ms(&m_begin_pulse_timestamp[input->port], 
                                                &m_end_pulse_timestamp[input->port],
                                                MEASURE_INPUT_NEW_DATA_TYPE_DIR_PIN);
            
            m_begin_pulse_timestamp[input->port].counter_dir = m_end_pulse_timestamp[input->port].counter_dir;
            m_begin_pulse_timestamp[input->port].subsecond_dir = m_end_pulse_timestamp[input->port].subsecond_dir;
            if (m_pull_diff[input->port] > PULSE_MINMUM_WITDH_MS)
            {   
                DEBUG_VERBOSE("[DIR]++++\r\n");
                if (eeprom_cfg->meter_mode[input->port] == APP_EEPROM_METER_MODE_DISABLE
                    || eeprom_cfg->meter_mode[input->port] == APP_EEPROM_METER_MODE_ONLY_PWM)
                {
                    m_pulse_counter_in_backup[input->port].reverse_counter = 0;
                    m_pulse_counter_in_backup[input->port].total_reserve_index = 0;
                    m_pulse_counter_in_backup[input->port].total_reserve = 0;
                }
                else
                {
                    m_pulse_counter_in_backup[input->port].total_reserve++;    
                    m_pulse_counter_in_backup[input->port].reverse_counter++;
                    m_pulse_counter_in_backup[input->port].total_reserve_index = m_pulse_counter_in_backup[input->port].total_reserve
                                                                                    /m_pulse_counter_in_backup[input->port].k;                    
                }
            }
            else
            {
                DEBUG_WARN("DIR Noise\r\n");
            }
        }
    }
    __enable_irq();
}


void modbus_master_serial_write(uint8_t *buf, uint8_t length)
{
    volatile uint32_t i;
    if (!RS485_GET_DIRECTION())
    {
        RS485_DIR_TX();     // Set TX mode
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
//    __WFI();
    sys_delay_ms(1);
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

measure_input_perpheral_data_t *measure_input_get_data_in_queue(void)
{    
    for (uint32_t i = 0; i < MEASUREMENT_MAX_MSQ_IN_RAM; i++)
    {
        if (m_sensor_msq[i].state == MEASUREMENT_QUEUE_STATE_PENDING)
        {
            m_sensor_msq[i].state = MEASUREMENT_QUEUE_STATE_PROCESSING;
            return &m_sensor_msq[i];
        }
    }
    
    return NULL;
}

void measure_input_delay_delay_measure_input_4_20ma(uint32_t ms)
{
    measure_input_turn_on_in_4_20ma_power = ms;
}


uint32_t measure_input_get_next_time_wakeup(void)
{
    uint32_t current_sec = app_rtc_get_counter();
    return (current_sec - estimate_measure_timestamp);
}

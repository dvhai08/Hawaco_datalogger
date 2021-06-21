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
#include "app_bkup.h"
#include "gsm.h"
#include "lwrb.h"
#include "app_eeprom.h"
#include "adc.h"


#define STORE_MEASURE_INVERVAL_SEC      30

extern System_t xSystem;

/******************************************************************************
                                   GLOBAL FUNCTIONS					    			 
 ******************************************************************************/

/******************************************************************************
                                   DATA TYPE DEFINE					    			 
 ******************************************************************************/
/* constant for adc resolution is 12 bit = 4096 */
#define ADC_12BIT_FACTOR 4096

#define RS485_DIR_OUT() GPIO_SetBits(RS485_DR_PORT, RS485_DR_PIN)
#define RS485_DIR_IN() GPIO_ResetBits(RS485_DR_PORT, RS485_DR_PIN)

/******************************************************************************
                                   PRIVATE VARIABLES					    			 
 ******************************************************************************/
__IO uint16_t ADC_RegularConvertedValueTab[MEASURE_INPUT_ADC_DMA_UNIT];

static SmallBuffer_t m_sensor_uart_buffer;

//static uint8_t m_last_pulse_state = 0;
//static uint8_t m_cur_pulse_state = 0;
//static uint32_t m_pulse_length_in_ms = 0;
uint8_t m_is_pulse_trigger = 0;
#ifdef GD32E10X
uint32_t m_begin_pulse_timestamp, m_end_pulse_timestamp;
int8_t m_pull_state = -1;
static uint32_t m_pull_diff;
#else
uint32_t m_begin_pulse_timestamp[MEASURE_NUMBER_OF_WATER_METER_INPUT], m_end_pulse_timestamp[MEASURE_NUMBER_OF_WATER_METER_INPUT];
int8_t m_pull_state[MEASURE_NUMBER_OF_WATER_METER_INPUT] = {-1, -1};
static uint32_t m_pull_diff[MEASURE_NUMBER_OF_WATER_METER_INPUT];
#endif
/******************************************************************************
                                   LOCAL FUNCTIONS					    			 
 ******************************************************************************/
static void process_rs485_uart(void);
static measure_input_water_meter_input_t m_water_meter_input[2];
static uint32_t m_pulse_counter_in_backup[4];
static measure_input_perpheral_data_t m_measure_data;
volatile uint32_t store_measure_result_timeout = 0;
volatile uint32_t Measure420mATick = 0;

static void measure_input_pulse_counter_poll(void)
{
    if (m_is_pulse_trigger)
    {
        //Store to BKP register
        app_bkup_write_pulse_counter(m_pulse_counter_in_backup[0],
									m_pulse_counter_in_backup[1],
									m_pulse_counter_in_backup[2],
									m_pulse_counter_in_backup[3]);
        m_is_pulse_trigger = 0;
        DEBUG_PRINTF("+++++++++ in %ums\r\n", m_pull_diff);
    }
}

static uint32_t m_last_measure_time = 0;
static uint8_t m_measure_timeout = 0;
void measure_input_task(void)
{
	app_eeprom_config_data_t *cfg = app_eeprom_read_config_data();
    measure_input_pulse_counter_poll();

	m_measure_data.input_on_off[0] = LL_GPIO_IsInputPinSet(OPTOIN1_GPIO_Port, OPTOIN1_Pin) ? 1 : 0;
	m_measure_data.input_on_off[1] = LL_GPIO_IsInputPinSet(OPTOIN2_GPIO_Port, OPTOIN2_Pin) ? 1 : 0;
	m_measure_data.input_on_off[2] = LL_GPIO_IsInputPinSet(OPTOIN3_GPIO_Port, OPTOIN3_Pin) ? 1 : 0;
	m_measure_data.input_on_off[3] = LL_GPIO_IsInputPinSet(OPTOIN4_GPIO_Port, OPTOIN4_Pin) ? 1 : 0;
	m_measure_data.water_pulse_counter[0].line_break_detect = LL_GPIO_IsInputPinSet(CIRIN1_GPIO_Port, CIRIN1_Pin) ? 0 : 1;
	m_measure_data.water_pulse_counter[1].line_break_detect = LL_GPIO_IsInputPinSet(CIRIN2_GPIO_Port, CIRIN2_Pin) ? 0 : 1;
	
    if (m_sensor_uart_buffer.State)
    {
        m_sensor_uart_buffer.State--;
        if (m_sensor_uart_buffer.State == 0)
        {
			
        }
    }
	
    if ((sys_get_ms() - m_last_measure_time) >= cfg->measure_interval_ms)
    {
		if (adc_conversion_cplt(true))
		{
			adc_convert();
			m_last_measure_time = sys_get_ms();
			gsm_build_http_post_msg();
			DEBUG_INFO("Measurement data finished\r\n");
		}
		else
		{
			adc_start();
		}
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
//            DEBUG_PRINTF("Save pulse counter %u to flash: %s\r\n", xSystem.MeasureStatus.PulseCounterInFlash, res ? "FAIL" : "OK");
//        }
//    }
}


/*****************************************************************************/
/**
 * @brief	:  	InitMeasurement
 * @param	:  
 * @retval	:
 * @author	:	
 * @created	:	15/01/2014
 * @version	:
 * @reviewer:	
 */
void measure_input_initialize(void)
{
#ifdef GD32E10X
    //Magnet switch
    gpio_init(SWITCH_IN_PORT, GPIO_MODE_IN_FLOATING, GPIO_OSPEED_10MHZ, SWITCH_IN_PIN);
	    //Pulse input
    gpio_init(SENS_PULSE_PORT, GPIO_MODE_IPU, GPIO_OSPEED_10MHZ, SENS_PULSE_PIN);

    //Direction
    gpio_init(SENS_DIR_PORT, GPIO_MODE_IPD, GPIO_OSPEED_10MHZ, SENS_DIR_PIN);

    /* enable and set key user EXTI interrupt to the lowest priority */
    nvic_irq_enable(EXTI0_IRQn, 2U, 1U);

    /* connect key user EXTI line to key GPIO pin */
    gpio_exti_source_select(SWITCH_PORT_SOURCE, SWITCH_PIN_SOURCE);

    /* configure key user EXTI line */
    exti_init(SWITCH_EXTI_LINE, EXTI_INTERRUPT, EXTI_TRIG_FALLING);
    exti_interrupt_flag_clear(SWITCH_EXTI_LINE);
    exti_interrupt_enable(SWITCH_EXTI_LINE);

    /* enable and set key user EXTI interrupt to the lowest priority */
    nvic_irq_enable(EXTI2_IRQn, 2U, 1U);

    /* connect key user EXTI line to key GPIO pin */
    gpio_exti_source_select(SENS_PULSE_PORT_SOURCE, SENS_PULSE_PIN_SOURCE);

    /* configure key user EXTI line */
    exti_init(SENS_PULSE_EXTI_LINE, EXTI_INTERRUPT, EXTI_TRIG_BOTH);
    exti_interrupt_flag_clear(SENS_PULSE_EXTI_LINE);
    exti_interrupt_enable(SWITCH_EXTI_LINE);
#else
	#ifdef DG_G1
		#warning "Please implement DTG1 magnet"
	#endif
#endif

    /* Doc gia tri do tu bo nho backup, neu gia tri tu BKP < flash -> lay theo gia tri flash
    * -> Case: Mat dien nguon -> mat du lieu trong RTC backup register
    */
	uint32_t counter0_f, counter1_f, counter0_r, counter1_r;
	app_bkup_read_pulse_counter(&counter0_f, &counter1_f, &counter0_r, &counter1_r);
    DEBUG_PRINTF("Pulse counter in BKP: %u-%u, %u-%u\r\n", counter0_f, counter0_r, counter1_f, counter1_r);
}

uint8_t measure_input_is_rs485_power_on(void)
{
#ifdef GD32E10X
    return GPIO_ReadOutputDataBit(RS485_PWR_EN_PORT, RS485_PWR_EN_PIN);
#else
	return (LL_GPIO_IsOutputPinSet(RS485_EN_GPIO_Port, RS485_EN_Pin) ? 0 : 1);
#endif
}


///*****************************************************************************/
///**
// * @brief	:  	Do dien ap nguon Solar va Battery. Tick every 1s
// * @param	:  
// * @retval	:
// * @author	:	
// * @created	:	15/03/2016
// * @version	:
// * @reviewer:	
// */
//static void measure_adc_input(void)
//{
//    //Dau vao 4-20mA: chi tinh toan khi thuc hien do
//    if (m_measure_timeout > 0)
//    {
//        float V20mA = ADC_RegularConvertedValueTab[ADCMEM_V20mV] * ADC_VREF / ADC_12BIT_FACTOR;
//        xSystem.MeasureStatus.Input420mA = V20mA / 124; //R = 124
//    }
//}
//uint16_t measure_input_get_Vin(void)
//{
//    return xSystem.MeasureStatus.Vin;
//}

/*******************************************************************************
 * Function Name  	: USART1_IRQHandler 
 * Return         	: None
 * Parameters 		: None
 * Created by		: Phinht
 * Date created	: 28/02/2016
 * Description		: This function handles USART1 global interrupt request. 
 * Notes			: 
 *******************************************************************************/
void measure_input_rs485_uart_handler(uint8_t data)
{
    m_sensor_uart_buffer.Buffer[m_sensor_uart_buffer.BufferIndex++] = data;
    if (m_sensor_uart_buffer.BufferIndex >= SMALL_BUFFER_SIZE)
        m_sensor_uart_buffer.BufferIndex = 0;
    m_sensor_uart_buffer.Buffer[m_sensor_uart_buffer.BufferIndex] = 0;

    m_sensor_uart_buffer.State = 7;
}

void measure_input_rs485_idle_detect(void)
{
	if (m_sensor_uart_buffer.State > 2)
	{
		m_sensor_uart_buffer.State = 2;
	}
}

/*!
    \brief      this function handles external lines 2 interrupt request
    \param[in]  none
    \param[out] none
    \retval     none
*/
#ifdef GD32E10X
void EXTI2_IRQHandler(void)
{
    /* check the Pulse input pin */
    if (RESET != exti_interrupt_flag_get(EXTI_2))
    {
#if 1
        if (getPulseState())
        {
            m_begin_pulse_timestamp = sys_get_ms();
            m_pull_state = 0;
        }
        else if (m_pull_state == 0)
        {
            m_pull_state = -1;
            m_end_pulse_timestamp = sys_get_ms();
            if (m_end_pulse_timestamp > m_begin_pulse_timestamp)
            {
                m_pull_diff = m_end_pulse_timestamp - m_begin_pulse_timestamp;
            }
            else
            {
                m_pull_diff = 0xFFFFFFFF - m_begin_pulse_timestamp + m_end_pulse_timestamp;
            }

            if (m_pull_diff > 200)
            {
                m_is_pulse_trigger = 1;
                DEBUG_PRINTF("Dir %u\r\n", GPIO_ReadInputDataBit(SENS_DIR_PORT, SENS_DIR_PIN));
                if (0 == GPIO_ReadInputDataBit(SENS_DIR_PORT, SENS_DIR_PIN))
                {
                    xSystem.MeasureStatus.PulseCounterInBkup++;
                }
                else
                {
                    if (xSystem.MeasureStatus.PulseCounterInBkup)
                    {
                        xSystem.MeasureStatus.PulseCounterInBkup--;
                    }
                }
            }
            delay_sleeping_for_exit_wakeup = 2;
        }
#else
        m_is_pulse_trigger = 1;
#endif
        exti_interrupt_flag_clear(EXTI_2);
    }
}
#else



measure_input_water_meter_input_t *measure_input_get_backup_counter(void)
{
	return &m_water_meter_input[0];
}

void measure_input_pulse_irq(measure_input_water_meter_input_t *input)
{
	#warning "Please call the measure input in isr context"
	if (input->pwm_level)
	{
		m_begin_pulse_timestamp[input->port] = sys_get_ms();
		m_pull_state[input->port] = 0;
	}
	else if (m_pull_state[input->port] == 0)
	{
		m_pull_state[input->port] = -1;
		m_end_pulse_timestamp[input->port] = sys_get_ms();
		if (m_end_pulse_timestamp[input->port] > m_begin_pulse_timestamp[input->port])
		{
			m_pull_diff[input->port] = m_end_pulse_timestamp[input->port] - m_begin_pulse_timestamp[input->port];
		}
		else
		{
			m_pull_diff[input->port] = 0xFFFFFFFF - m_begin_pulse_timestamp[input->port] + m_end_pulse_timestamp[input->port];
		}

		if (m_pull_diff[input->port] > 200)
		{
			m_is_pulse_trigger = 1;
			DEBUG_PRINTF("Dir %u\r\n", input->dir_level);
			if (0 == input->dir_level)
			{
				m_pulse_counter_in_backup[input->port]++;
			}
			else
			{
				if (m_pulse_counter_in_backup[input->port])
				{
					m_pulse_counter_in_backup[input->port]--;
				}
			}
		}
		sys_set_delay_time_before_deep_sleep(2000);
	}
}
#endif

uint8_t Modbus_Master_GetByte(uint8_t *getbyte)
{
	if (m_sensor_uart_buffer.BufferIndex)
	{
		*getbyte = m_sensor_uart_buffer.Buffer[m_sensor_uart_buffer.BufferIndex];
		return 0;
	}
	return 1;
}

uint8_t Modbus_Master_Write(uint8_t *buf, uint8_t length)
{
    for (uint32_t i = 0; i < length; i++)
    {
		LL_LPUART_TransmitData8(LPUART1, buf[i]);
        while (0 == LL_LPUART_IsActiveFlag_TXE(LPUART1));
    }
	return 0;
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

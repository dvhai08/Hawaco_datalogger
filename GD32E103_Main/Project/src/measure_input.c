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
#include "HardwareManager.h"
#include "InternalFlash.h"
#include "main.h"
#include "app_bkup.h"
#include "MQTTUser.h"
#include "gsm.h"

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

static uint8_t m_last_pulse_state = 0;
static uint8_t m_cur_pulse_state = 0;
static uint32_t m_pulse_length_in_ms = 0;
uint8_t m_is_pulse_trigger = 0;
uint32_t m_begin_pulse_timestamp, m_end_pulse_timestamp;
int8_t m_pull_state = -1;
static uint32_t m_pull_diff;
/******************************************************************************
                                   LOCAL FUNCTIONS					    			 
 ******************************************************************************/
static void measure_adc_input(void);
static void process_rs485_uart(void);

static void measure_input_pulse_counter_poll(void)
{
    if (m_is_pulse_trigger)
    {
        //Store to BKP register
        app_bkup_write_pulse_counter(xSystem.MeasureStatus.PulseCounterInBkup);
        m_is_pulse_trigger = 0;
        DEBUG_PRINTF("+++++++++ in %ums\r\n", m_pull_diff);
    }
}


static uint8_t m_measure_timeout = 0;
void measure_input_task(void)
{
    measure_input_pulse_counter_poll();

    if (xSystem.Status.InitSystemDone == 0)
        return;

    if (m_sensor_uart_buffer.State)
    {
        m_sensor_uart_buffer.State--;
        if (m_sensor_uart_buffer.State == 0)
        {
            process_rs485_uart();
            m_sensor_uart_buffer.BufferIndex = 0;
        }
    }
}

volatile uint32_t store_measure_result_timeout = 0;
volatile uint32_t Measure420mATick = 0;
void MeasureTick1000ms(void)
{
    measure_adc_input();

    if (xSystem.Status.ADCOut)
    {
        DEBUG_PRINTF("ADC: Vin: %d, Vsens: %d\r\n", ADC_RegularConvertedValueTab[ADCMEM_VSYS],
              ADC_RegularConvertedValueTab[ADCMEM_V20mV]);
        DEBUG_PRINTF("Vin: %d mV, Vsens: %d mV\r\n", xSystem.MeasureStatus.Vin, xSystem.MeasureStatus.Vsens);
    }

    /* === DO DAU VAO 4-20mA DINH KY === //
	*/
    if (Measure420mATick >= xSystem.Parameters.period_measure_peripheral * 60)
    {
        Measure420mATick = 0;

        //Cho phep do thi moi do
        if (xSystem.Parameters.input.name.ma420)
        {
            SENS_420mA_PWR_ON();
            m_measure_timeout = 10;
        }
        else
        {
            SENS_420mA_PWR_OFF();
 #if (__USE_MQTT__)
            if (MQTT_PublishDataMsg() == 0) // No memory
            {
                MQTT_DiscardOldestDataMsg();
                MQTT_PublishDataMsg();
            }
 #else
        gsm_build_http_post_msg();
 #endif
        }
    }
    if (m_measure_timeout > 0)
    {
        m_measure_timeout--;
        if (m_measure_timeout == 0)
        {
            DEBUG_PRINTF("--- Timeout measure ---r\n");
#if (__USE_MQTT__)
            if (MQTT_PublishDataMsg() == 0) // No memory
            {
                MQTT_DiscardOldestDataMsg();
                MQTT_PublishDataMsg();
            }
#else
        gsm_build_http_post_msg();
 #endif
            SENS_420mA_PWR_OFF();
        }
    }

    /* Save pulse counter to flash every 30s */
    if (store_measure_result_timeout >= STORE_MEASURE_INVERVAL_SEC)
    {
        store_measure_result_timeout = 0;

        // Neu counter in BKP != in flash -> luu flash
        if (xSystem.MeasureStatus.PulseCounterInBkup != xSystem.MeasureStatus.PulseCounterInFlash)
        {
            xSystem.MeasureStatus.PulseCounterInFlash = xSystem.MeasureStatus.PulseCounterInBkup;
            InternalFlash_WriteMeasures();
            uint8_t res = InternalFlash_WriteConfig();
            DEBUG_PRINTF("Save pulse counter %u to flash: %s\r\n", xSystem.MeasureStatus.PulseCounterInFlash, res ? "FAIL" : "OK");
        }
    }
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

    /* Doc gia tri do tu bo nho backup, neu gia tri tu BKP < flash -> lay theo gia tri flash
    * -> Case: Mat dien nguon -> mat du lieu trong RTC backup register
    */
    xSystem.MeasureStatus.PulseCounterInBkup = app_bkup_read_pulse_counter();
    if (xSystem.MeasureStatus.PulseCounterInBkup < xSystem.MeasureStatus.PulseCounterInFlash)
    {
        xSystem.MeasureStatus.PulseCounterInBkup = xSystem.MeasureStatus.PulseCounterInFlash;
    }
    DEBUG_PRINTF("Pulse counter in BKP: %d\r\n", xSystem.MeasureStatus.PulseCounterInBkup);
}

uint8_t measure_input_is_rs485_power_on(void)
{
    return GPIO_ReadOutputDataBit(RS485_PWR_EN_PORT, RS485_PWR_EN_PIN);
}


/*****************************************************************************/
/**
 * @brief	:  	Do dien ap nguon Solar va Battery. Tick every 1s
 * @param	:  
 * @retval	:
 * @author	:	
 * @created	:	15/03/2016
 * @version	:
 * @reviewer:	
 */
static void measure_adc_input(void)
{
    //Dau vao 4-20mA: chi tinh toan khi thuc hien do
    if (m_measure_timeout > 0)
    {
        float V20mA = ADC_RegularConvertedValueTab[ADCMEM_V20mV] * ADC_VREF / ADC_12BIT_FACTOR;
        xSystem.MeasureStatus.Input420mA = V20mA / 124; //R = 124
    }
}

/*****************************************************************************/
/**
 * @brief	:  	Xu ly ban tin UART sensor laser
 * @param	:  
 * @retval	:
 * @author	:	
 * @created	:	15/03/2016
 * @version	:
 * @reviewer:	
 */
static void process_rs485_uart(void)
{

}

uint16_t measure_input_get_Vin(void)
{
    return xSystem.MeasureStatus.Vin;
}

static void rs485_add_uart_data(uint8_t data)
{
    m_sensor_uart_buffer.Buffer[m_sensor_uart_buffer.BufferIndex++] = data;
    if (m_sensor_uart_buffer.BufferIndex >= SMALL_BUFFER_SIZE)
        m_sensor_uart_buffer.BufferIndex = 0;
    m_sensor_uart_buffer.Buffer[m_sensor_uart_buffer.BufferIndex] = 0;

    m_sensor_uart_buffer.State = 7;
}

/*******************************************************************************
 * Function Name  	: USART1_IRQHandler 
 * Return         	: None
 * Parameters 		: None
 * Created by		: Phinht
 * Date created	: 28/02/2016
 * Description		: This function handles USART1 global interrupt request. 
 * Notes			: 
 *******************************************************************************/
void RS485_UART_Handler(void)
{
    if (usart_interrupt_flag_get(RS485_UART, USART_INT_FLAG_RBNE) != RESET)
    {
        usart_interrupt_flag_clear(RS485_UART, USART_INT_FLAG_RBNE);
        rs485_add_uart_data(USART_ReceiveData(RS485_UART));
    }
}

/*!
    \brief      this function handles external lines 2 interrupt request
    \param[in]  none
    \param[out] none
    \retval     none
*/
extern volatile uint32_t delay_sleeping_for_exit_wakeup;
#if 1
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
#endif


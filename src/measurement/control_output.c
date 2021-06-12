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
#include "ControlOutput.h"
#include "app_bkup.h"

/******************************************************************************
                                   GLOBAL VARIABLES					    			 
 ******************************************************************************/
extern System_t xSystem;
static void dac_config(void);

static uint8_t m_dac_is_enabled = 0;
void control_ouput_task(void)
{
    //dieu khien dau ra output on/off
    TRAN_OUTPUT(xSystem.Parameters.outputOnOff);

    if (xSystem.Parameters.outputOnOff
        && (xSystem.Parameters.output420ma >= 4 && xSystem.Parameters.output420ma <= 20))
    {
        if (!m_dac_is_enabled)
        {
            dac_config();
            m_dac_is_enabled = 1;
        }
        /* Dieu khien dau ra output 4-20mA qua DAC
		* DAC: 0.6-3V () <=> 4-20mA output -> 1mA <=> 0.15V
		*/
        uint16_t dac_value = 0;
        uint16_t voltage = 600 + 150 * (xSystem.Parameters.output420ma - 4); //mV
        dac_value = voltage * 4096 / 3300;
        dac_data_set(DAC1, DAC_ALIGN_12B_R, dac_value);
        dac_software_trigger_enable(DAC1);
    }
    else
    {
        if (m_dac_is_enabled)
        {
            dac_data_set(DAC1, DAC_ALIGN_12B_R, 0);
            dac_software_trigger_enable(DAC1);
            Delayms(2);
            dac_software_trigger_disable(DAC1);
            dac_concurrent_disable();
            dac_concurrent_output_buffer_disable();
            dac_disable(DAC1);
            rcu_periph_clock_disable(RCU_DAC);
            m_dac_is_enabled = 0;
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
    /* once enabled the DAC, the corresponding GPIO pin is connected to the DAC converter automatically */
    gpio_init(DAC_OUT_PORT, GPIO_MODE_AIN, GPIO_OSPEED_10MHZ, DAC_OUT_PIN);
    if (xSystem.Parameters.outputOnOff)
    {
        rcu_periph_clock_enable(RCU_DAC);

        dac_deinit();
        /* configure the DAC1 */
        dac_trigger_disable(DAC1);
        dac_wave_mode_config(DAC1, DAC_WAVE_DISABLE);
        dac_output_buffer_enable(DAC1);

        /* enable DAC concurrent mode and set data */
        dac_enable(DAC1);
        m_dac_is_enabled = 1;
    }
    else
    {
        rcu_periph_clock_disable(RCU_DAC);
    }
}


void control_ouput_init(void)
{
    //output on/off
    gpio_init(TRAN_OUT_PORT, GPIO_MODE_OUT_PP, GPIO_OSPEED_10MHZ, TRAN_OUT_PIN);

    //output 4-20mA
    dac_config();
}

/********************************* END OF FILE *******************************/

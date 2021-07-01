/**
  ******************************************************************************
  * @file    adc.c
  * @brief   This file provides code for the configuration
  *          of the ADC instances.
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2021 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under BSD 3-Clause license,
  * the "License"; You may not use this file except in compliance with the
  * License. You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "adc.h"

/* USER CODE BEGIN 0 */
#include "app_debug.h"
#include <stdbool.h>
#include "lpf.h"
#include "hardware.h"
#include "app_eeprom.h"

#define ADC_NUMBER_OF_CONVERSION_TIMES		10
#ifdef DTG02
#define ADC_CHANNEL_DMA_COUNT				7
#define ADC_VBAT_RESISTOR_DIV				7911
#define ADC_VIN_RESISTOR_DIV				7911
#define ADC_VREF							3300

#define V_INPUT_3_4_20MA_CHANNEL_INDEX		0
#define VIN_24V_CHANNEL_INDEX				1
#define V_INPUT_2_4_20MA_CHANNEL_INDEX		2
#define V_INPUT_1_4_20MA_CHANNEL_INDEX		3
#define V_INPUT_0_4_20MA_CHANNEL_INDEX		4
#define VBAT_CHANNEL_INDEX					5
#define V_TEMP_CHANNEL_INDEX				6
#else
#define ADC_CHANNEL_DMA_COUNT				5
#define ADC_VBAT_RESISTOR_DIV				2
#define ADC_VIN_RESISTOR_DIV				7911
#define ADC_VREF							3300
#define VBAT_CHANNEL_INDEX					0
#define V_INPUT_0_4_20MA_CHANNEL_INDEX		1
#define VIN_24V_CHANNEL_INDEX				2	
#define V_TEMP_CHANNEL_INDEX				3	
#define V_REF_CHANNEL_INDEX					4
#endif
#define GAIN_INPUT_4_20MA_IN				143
#define VREF_OFFSET_MV						80

static volatile bool m_adc_started = false;
volatile uint16_t m_adc_raw_data[ADC_CHANNEL_DMA_COUNT];
lpf_data_t m_adc_filterd_data[ADC_CHANNEL_DMA_COUNT];
/* USER CODE END 0 */

/* ADC init function */
void MX_ADC_Init(void)
{

  /* USER CODE BEGIN ADC_Init 0 */
	
  /* USER CODE END ADC_Init 0 */

  LL_ADC_REG_InitTypeDef ADC_REG_InitStruct = {0};
  LL_ADC_InitTypeDef ADC_InitStruct = {0};

  LL_GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* Peripheral clock enable */
  LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_ADC1);

  LL_IOP_GRP1_EnableClock(LL_IOP_GRP1_PERIPH_GPIOC);
  LL_IOP_GRP1_EnableClock(LL_IOP_GRP1_PERIPH_GPIOA);
  /**ADC GPIO Configuration
  PC1   ------> ADC_IN11
  PA1   ------> ADC_IN1
  PA4   ------> ADC_IN4
  PA6   ------> ADC_IN6
  */
  GPIO_InitStruct.Pin = VTEMP_Pin;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_ANALOG;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
  LL_GPIO_Init(VTEMP_GPIO_Port, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = ADC_VBAT_Pin;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_ANALOG;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
  LL_GPIO_Init(ADC_VBAT_GPIO_Port, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = ADC_4_20MA_IN_Pin;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_ANALOG;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
  LL_GPIO_Init(ADC_4_20MA_IN_GPIO_Port, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = ADC_24V_Pin;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_ANALOG;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
  LL_GPIO_Init(ADC_24V_GPIO_Port, &GPIO_InitStruct);

  /* ADC interrupt Init */
  NVIC_SetPriority(ADC1_COMP_IRQn, 0);
  NVIC_EnableIRQ(ADC1_COMP_IRQn);

  /* USER CODE BEGIN ADC_Init 1 */

  /* USER CODE END ADC_Init 1 */
  /** Configure Regular Channel
  */
  LL_ADC_REG_SetSequencerChAdd(ADC1, LL_ADC_CHANNEL_1);
  /** Configure Regular Channel
  */
  LL_ADC_REG_SetSequencerChAdd(ADC1, LL_ADC_CHANNEL_4);
  /** Configure Regular Channel
  */
  LL_ADC_REG_SetSequencerChAdd(ADC1, LL_ADC_CHANNEL_6);
  /** Configure Regular Channel
  */
  LL_ADC_REG_SetSequencerChAdd(ADC1, LL_ADC_CHANNEL_11);
  /** Configure Regular Channel
  */
  LL_ADC_REG_SetSequencerChAdd(ADC1, LL_ADC_CHANNEL_VREFINT);
  LL_ADC_SetCommonPathInternalCh(__LL_ADC_COMMON_INSTANCE(ADC1), LL_ADC_PATH_INTERNAL_VREFINT);
  /** Common config
  */
  ADC_REG_InitStruct.TriggerSource = LL_ADC_REG_TRIG_SOFTWARE;
  ADC_REG_InitStruct.SequencerDiscont = LL_ADC_REG_SEQ_DISCONT_DISABLE;
  ADC_REG_InitStruct.ContinuousMode = LL_ADC_REG_CONV_SINGLE;
  ADC_REG_InitStruct.DMATransfer = LL_ADC_REG_DMA_TRANSFER_NONE;
  ADC_REG_InitStruct.Overrun = LL_ADC_REG_OVR_DATA_PRESERVED;
  LL_ADC_REG_Init(ADC1, &ADC_REG_InitStruct);
  LL_ADC_SetSamplingTimeCommonChannels(ADC1, LL_ADC_SAMPLINGTIME_160CYCLES_5);
  LL_ADC_SetOverSamplingScope(ADC1, LL_ADC_OVS_DISABLE);
  LL_ADC_REG_SetSequencerScanDirection(ADC1, LL_ADC_REG_SEQ_SCAN_DIR_FORWARD);
  LL_ADC_SetCommonFrequencyMode(__LL_ADC_COMMON_INSTANCE(ADC1), LL_ADC_CLOCK_FREQ_MODE_HIGH);
  LL_ADC_DisableIT_EOC(ADC1);
  LL_ADC_DisableIT_EOS(ADC1);
  ADC_InitStruct.Clock = LL_ADC_CLOCK_SYNC_PCLK_DIV2;
  ADC_InitStruct.Resolution = LL_ADC_RESOLUTION_12B;
  ADC_InitStruct.DataAlignment = LL_ADC_DATA_ALIGN_RIGHT;
  ADC_InitStruct.LowPowerMode = LL_ADC_LP_MODE_NONE;
  LL_ADC_Init(ADC1, &ADC_InitStruct);

  /* Enable ADC internal voltage regulator */
  LL_ADC_EnableInternalRegulator(ADC1);
  /* Delay for ADC internal voltage regulator stabilization. */
  /* Compute number of CPU cycles to wait for, from delay in us. */
  /* Note: Variable divided by 2 to compensate partially */
  /* CPU processing cycles (depends on compilation optimization). */
  /* Note: If system core clock frequency is below 200kHz, wait time */
  /* is only a few CPU processing cycles. */
  uint32_t wait_loop_index;
  wait_loop_index = ((LL_ADC_DELAY_INTERNAL_REGUL_STAB_US * (SystemCoreClock / (100000 * 2))) / 10);
  while(wait_loop_index != 0)
  {
    wait_loop_index--;
  }
  /* USER CODE BEGIN ADC_Init 2 */
  
    if (LL_ADC_IsEnabled(ADC1) == 0)
    {
        DEBUG_PRINTF("Start calib AD \r\n");
        /* Run ADC self calibration */
        LL_ADC_StartCalibration(ADC1);

        /* Poll for ADC effectively calibrated */
        #if (USE_TIMEOUT == 1)
        Timeout = ADC_CALIBRATION_TIMEOUT_MS;
        #endif /* USE_TIMEOUT */
        
        DEBUG_PRINTF("Wait for adc calib\r\n");
        while (LL_ADC_IsCalibrationOnGoing(ADC1) != 0)
        {
        #if (USE_TIMEOUT == 1)
          /* Check Systick counter flag to decrement the time-out value */
          if (LL_SYSTICK_IsActiveCounterFlag())
          {
            if(Timeout-- == 0)
            {
            /* Time-out occurred. Set LED to blinking mode */
            LED_Blinking(LED_BLINK_ERROR);
            }
          }
        #endif /* USE_TIMEOUT */
        }
    }
    
    LL_ADC_EnableIT_EOC(ADC1);
    LL_ADC_Enable(ADC1);
    DEBUG_PRINTF("Wait for adc ready\r\n");
    while (LL_ADC_IsActiveFlag_ADRDY(ADC1) == 0);
  /* USER CODE END ADC_Init 2 */

}

/* USER CODE BEGIN 1 */

void ADC1_COMP_IRQHandler(void)
{
    /* Check whether ADC group regular end of unitary conversion caused         */
    /* the ADC interruption.                                                    */
    if(LL_ADC_IsActiveFlag_EOC(ADC1) != 0)
    {
        /* Clear flag ADC group regular end of unitary conversion */
        LL_ADC_ClearFlag_EOC(ADC1);

    }

    /* Check whether ADC group regular overrun caused the ADC interruption */
    if(LL_ADC_IsActiveFlag_OVR(ADC1) != 0)
    {
        DEBUG_ERROR("ADC overrun\r\n");
        LL_ADC_ClearFlag_OVR(ADC1);
    }
}

static bool m_is_the_first_time_convert = true;
//static bool m_adc_new_data = false;
//void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *AdcHandle)
//{
//	/* Get the converted value of regular channel */
//	m_adc_started = false;
//	m_adc_new_data = true;
//}

void adc_start(void)
{
    app_eeprom_config_data_t * cfg = app_eeprom_read_config_data();
    if (cfg->io_enable.name.input_4_20ma_enable)
    {
        ENABLE_INOUT_4_20MA_POWER(1);
    }
    
    if (!NTC_IS_POWERED())
    {
        ENABLE_NTC_POWER(1);
    }
    
    if (LL_ADC_IsEnabled(ADC1) == 0)
    {
        MX_ADC_Init();
    }
    for (uint32_t i = 0; i < ADC_CHANNEL_DMA_COUNT; i++)
    {
#ifdef DTG01
        if (i == 0)
        {
            LL_ADC_REG_SetSequencerChannels(ADC1, LL_ADC_CHANNEL_1);
        }
        else if (i == 1)
        {
            LL_ADC_REG_SetSequencerChannels(ADC1, LL_ADC_CHANNEL_4);
        }
        else if (i == 2)
        {
            LL_ADC_REG_SetSequencerChannels(ADC1, LL_ADC_CHANNEL_6);
        }
        else if (i == 3)
        {
           LL_ADC_REG_SetSequencerChannels(ADC1, LL_ADC_CHANNEL_11); 
        }
        else if (i == 4)
        {
            LL_ADC_REG_SetSequencerChannels(ADC1, LL_ADC_CHANNEL_VREFINT); 
        }
#else
        #warning "Please implemte adc channel seq"
#endif
        if (LL_ADC_REG_IsConversionOngoing(ADC1) == 0)
        {
            LL_ADC_REG_StartConversion(ADC1);
        }
//        __WFI();
        while (LL_ADC_REG_IsConversionOngoing(ADC1));
        m_adc_raw_data[i] = LL_ADC_REG_ReadConversionData12(ADC1);
    }
    DEBUG_PRINTF("Convert complete\r\n");
    adc_convert();
}

void adc_stop(void)
{
    DEBUG_PRINTF("ADC stop\r\n");
	ENABLE_NTC_POWER(0);
    NVIC_DisableIRQ(ADC1_COMP_IRQn);
    LL_ADC_Disable(ADC1);
    LL_ADC_DisableInternalRegulator(ADC1);
    LL_APB2_GRP1_DisableClock(LL_APB2_GRP1_PERIPH_ADC1);
}

static uint8_t convert_vin_to_percent(uint32_t vin)
{
#define VIN_MAX 4200
#define VIN_MIN 3700

    if (vin >= VIN_MAX)
        return 100;
    if (vin <= VIN_MIN)
        return 0;
    return ((vin - VIN_MIN) * 100) / (VIN_MAX - VIN_MIN);
}


adc_input_value_t m_adc_input;

adc_input_value_t *adc_get_input_result(void)
{
	return &m_adc_input;
}

void adc_convert(void)
{
	for (uint32_t i = 0; i < ADC_CHANNEL_DMA_COUNT; i++)
	{
		if (!m_is_the_first_time_convert)
		{
//            if (i != V_INPUT_0_4_20MA_CHANNEL_INDEX)
            {
                m_adc_filterd_data[i].estimate_value *= 100;
                int32_t tmp = m_adc_raw_data[i] * 100;
                lpf_update_estimate(&m_adc_filterd_data[i], &tmp);
                m_adc_filterd_data[i].estimate_value /= 100;	
            }
//            else
//            {
//                m_adc_filterd_data[i].estimate_value += m_adc_raw_data[i];
//                m_adc_filterd_data[i].estimate_value /= 2;
//            }
		}
		else
		{
			m_adc_filterd_data[i].estimate_value = m_adc_raw_data[i];
			m_adc_filterd_data[i].gain = 1;		/* 1% */
		}
	}
	m_is_the_first_time_convert = false;
	m_adc_input.vref_int = *((uint16_t*)0x1FF80078);
	m_adc_input.vdda_mv = 3000 * m_adc_input.vref_int/m_adc_filterd_data[V_REF_CHANNEL_INDEX].estimate_value + VREF_OFFSET_MV;
	/* ADC Vbat */
	m_adc_input.bat_mv = (ADC_VBAT_RESISTOR_DIV*m_adc_filterd_data[VBAT_CHANNEL_INDEX].estimate_value*m_adc_input.vdda_mv/4095);
	m_adc_input.bat_percent = convert_vin_to_percent(m_adc_input.bat_mv);
	
	/* ADC Vin 24V */
	m_adc_input.vin_24 = ((uint32_t)ADC_VIN_RESISTOR_DIV*m_adc_filterd_data[VIN_24V_CHANNEL_INDEX].estimate_value/(uint32_t)1000)*m_adc_input.vdda_mv/4095;
	
	/* ADC input 4-20mA */
	m_adc_input.in_4_20ma_in[0] = m_adc_filterd_data[V_INPUT_0_4_20MA_CHANNEL_INDEX].estimate_value*m_adc_input.vdda_mv*10/(GAIN_INPUT_4_20MA_IN*4095);
#ifdef DTG02
	m_adc_input.i_4_20ma_in[1] = m_adc_filterd_data[V_INPUT_1_4_20MA_CHANNEL_INDEX].estimate_value*m_adc_input.vdda_mv*10/(GAIN_INPUT_4_20MA_IN*4095);
	m_adc_input.i_4_20ma_in[2] = m_adc_filterd_data[V_INPUT_2_4_20MA_CHANNEL_INDEX].estimate_value*m_adc_input.vdda_mv*10/(GAIN_INPUT_4_20MA_IN*4095);
	m_adc_input.i_4_20ma_in[3] = m_adc_filterd_data[V_INPUT_3_4_20MA_CHANNEL_INDEX].estimate_value*m_adc_input.vdda_mv*10/(GAIN_INPUT_4_20MA_IN*4095);
#endif
	/* v_temp */
	m_adc_input.temp = m_adc_filterd_data[V_TEMP_CHANNEL_INDEX].estimate_value*m_adc_input.vdda_mv/4095;
}

//bool adc_conversion_cplt(bool clear_on_exit)
//{
//	bool retval = m_adc_new_data;
//	
//	if (clear_on_exit)
//	{
//		m_adc_new_data = false;
//	}
//	return retval;
//}



/* USER CODE END 1 */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/

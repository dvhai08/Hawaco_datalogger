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
#include "math.h"

#define ADC_NUMBER_OF_CONVERSION_TIMES		10
#ifdef DTG02
#define ADC_CHANNEL_DMA_COUNT				8
#define ADC_VBAT_RESISTOR_DIV				7911
#define ADC_VIN_RESISTOR_DIV				7911
#define ADC_VREF							3300

#define V_INPUT_3_4_20MA_CHANNEL_INDEX		0
#define VIN_24V_CHANNEL_INDEX				1
#define V_INPUT_2_4_20MA_CHANNEL_INDEX		2
#define V_INPUT_1_4_20MA_CHANNEL_INDEX		3
#define V_INPUT_0_4_20MA_CHANNEL_INDEX		4
#define VBAT_CHANNEL_INDEX					5
#define V_INTERNAL_CHIP_TEMP_CHANNEL_INDEX  6
#define V_TEMP_CHANNEL_INDEX				7
#else
#define ADC_CHANNEL_DMA_COUNT				6
#define ADC_VBAT_RESISTOR_DIV				2
#define ADC_VIN_RESISTOR_DIV				7911
#define ADC_VREF							3300

#define VBAT_CHANNEL_INDEX					0
#define V_INPUT_0_4_20MA_CHANNEL_INDEX		1
#define VIN_24V_CHANNEL_INDEX				2	
#define V_TEMP_CHANNEL_INDEX				3
#define V_INTERNAL_CHIP_TEMP_CHANNEL_INDEX  4
#define V_REF_CHANNEL_INDEX					5
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
	DEBUG_PRINTF("ADC inititlize\r\n");
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
//  NVIC_SetPriority(ADC1_COMP_IRQn, 0);
//  NVIC_EnableIRQ(ADC1_COMP_IRQn);

  /* USER CODE BEGIN ADC_Init 1 */
  if (SystemCoreClock > 8000000)
  {
        DEBUG_PRINTF("ADC main clk is high\r\n");
        LL_ADC_SetCommonClock(__LL_ADC_COMMON_INSTANCE(ADC1), LL_ADC_CLOCK_ASYNC_DIV12);
  }
  else
  {
      DEBUG_PRINTF("ADC main clk is low\r\n");
      LL_ADC_SetCommonClock(__LL_ADC_COMMON_INSTANCE(ADC1), LL_ADC_CLOCK_ASYNC_DIV1);
  }
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
  LL_ADC_REG_SetSequencerChAdd(ADC1, LL_ADC_CHANNEL_TEMPSENSOR);
  LL_ADC_SetCommonPathInternalCh(__LL_ADC_COMMON_INSTANCE(ADC1), LL_ADC_PATH_INTERNAL_TEMPSENSOR);
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
  LL_ADC_SetCommonFrequencyMode(__LL_ADC_COMMON_INSTANCE(ADC1), LL_ADC_CLOCK_FREQ_MODE_LOW);
  LL_ADC_DisableIT_EOC(ADC1);
  LL_ADC_DisableIT_EOS(ADC1);
  ADC_InitStruct.Resolution = LL_ADC_RESOLUTION_12B;
  ADC_InitStruct.DataAlignment = LL_ADC_DATA_ALIGN_RIGHT;
  ADC_InitStruct.LowPowerMode = LL_ADC_LP_MODE_NONE;
  LL_ADC_Init(ADC1, &ADC_InitStruct);
  LL_ADC_SetClock(ADC1, LL_ADC_CLOCK_ASYNC);

  /* Enable ADC internal voltage regulator */
  LL_ADC_EnableInternalRegulator(ADC1);
  /* Delay for ADC internal voltage regulator stabilization. */
  /* Compute number of CPU cycles to wait for, from delay in us. */
  /* Note: Variable divided by 2 to compensate partially */
  /* CPU processing cycles (depends on compilation optimization). */
  /* Note: If system core clock frequency is below 200kHz, wait time */
  /* is only a few CPU processing cycles. */
  volatile uint32_t wait_loop_index;
  wait_loop_index = ((2*LL_ADC_DELAY_INTERNAL_REGUL_STAB_US * (SystemCoreClock / (100000 * 2))) / 10);
  while(wait_loop_index != 0)
  {
    wait_loop_index--;
  }
  /* USER CODE BEGIN ADC_Init 2 */
  
//    if (LL_ADC_IsEnabled(ADC1) == 0)
    {
        DEBUG_VERBOSE("Start calib AD \r\n");
        /* Run ADC self calibration */
        LL_ADC_StartCalibration(ADC1);

        /* Poll for ADC effectively calibrated */
        #if (USE_TIMEOUT == 1)
        Timeout = ADC_CALIBRATION_TIMEOUT_MS;
        #endif /* USE_TIMEOUT */
        
        DEBUG_VERBOSE("Wait for adc calib\r\n");
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
    
//    LL_ADC_EnableIT_EOC(ADC1);
    // Clear the ADRDY bit in ADC_ISR register by programming this bit to 1.
	SET_BIT(ADC1->ISR, LL_ADC_FLAG_ADRDY);
    LL_ADC_Enable(ADC1);
    DEBUG_VERBOSE("Wait for adc ready\r\n");
    while (LL_ADC_IsActiveFlag_ADRDY(ADC1) == 0);
    DEBUG_VERBOSE("ADC ready\r\n");
  /* USER CODE END ADC_Init 2 */

}

/* USER CODE BEGIN 1 */

void adc_isr_cb(void)
{
    DEBUG_VERBOSE("ADC ISR cb\r\n");
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


void adc_start(void)
{
    app_eeprom_config_data_t * cfg = app_eeprom_read_config_data();
    if (cfg->io_enable.name.input_4_20ma_enable)
    {
        ENABLE_INOUT_4_20MA_POWER(1);
    }
    else
    {
        ENABLE_INOUT_4_20MA_POWER(0);
    }
    
#ifndef USE_INTERNAL_VREF    
    if (!NTC_IS_POWERED())
    {
        ENABLE_NTC_POWER(1);
    }
#else
    ENABLE_NTC_POWER(0);
#endif
    
    if (LL_ADC_IsEnabled(ADC1) == 0)
    {
        MX_ADC_Init();
    }
    for (uint32_t j = 0; j < 3; j++)
    {
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
                LL_ADC_REG_SetSequencerChannels(ADC1, LL_ADC_CHANNEL_TEMPSENSOR);      
                LL_ADC_SetCommonPathInternalCh(__LL_ADC_COMMON_INSTANCE(ADC1), LL_ADC_PATH_INTERNAL_TEMPSENSOR);               
            }
            else if (i == 5)
            {
                LL_ADC_REG_SetSequencerChannels(ADC1, LL_ADC_CHANNEL_VREFINT);
                LL_ADC_SetCommonPathInternalCh(__LL_ADC_COMMON_INSTANCE(ADC1), LL_ADC_PATH_INTERNAL_VREFINT); 
            }
#else
            #warning "Please implemte adc channel seq"
#endif
            if (LL_ADC_REG_IsConversionOngoing(ADC1) == 0)
            {
                LL_ADC_REG_StartConversion(ADC1);
            }
            while (LL_ADC_REG_IsConversionOngoing(ADC1))
            {
//                __WFI();
            }
            m_adc_raw_data[i] += LL_ADC_REG_ReadConversionData12(ADC1);
        }
    }
    for (uint32_t i = 0; i < ADC_CHANNEL_DMA_COUNT; i++)
    {
        m_adc_raw_data[i] /= 3;
    }
    ENABLE_INOUT_4_20MA_POWER(0);
    DEBUG_VERBOSE("Convert complete\r\n");
    adc_convert();
}

void adc_stop(void)
{
    DEBUG_VERBOSE("ADC stop\r\n");
	ENABLE_NTC_POWER(0);
//    NVIC_DisableIRQ(ADC1_COMP_IRQn);
    LL_ADC_DeInit(ADC1);
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
#ifndef USE_INTERNAL_VREF
static bool convert_temperature(uint32_t vtemp_mv, uint32_t vbat_mv, int32_t *result)
{
    #define HW_RESISTOR_SERIES_NTC 300000 //300K Ohm
    /* This is thermistor dependent and it should be in the datasheet, or refer to the
    article for how to calculate it using the Beta equation.
    I had to do this, but I would try to get a thermistor with a known
    beta if you want to avoid empirical calculations. */
    #define BETA 4570.0f //B25/B75

    /* This is also needed for the conversion equation as "typical" room temperature
    is needed as an input. */
    #define ROOM_TEMP 298.15f // room temperature in Kelvin

    /* Thermistors will have a typical resistance at room temperature so write this 
    down here. Again, needed for conversion equations. */
    #define RESISTOR_ROOM_TEMP 470000.0f //Resistance in Ohms @ 25oC	330k/470k

    bool retval = true;
    float vtemp_float;
    float vbat_float;
    float r_ntc;
    float temp ;
    
    if(vtemp_mv == vbat_mv
        || vtemp_mv == 0)
    {
        retval = false;
        goto end;
    }

    //NTC ADC: Vntc1 = Vntc2 = Vin
    //Tinh dien ap NTC
    vtemp_float = vtemp_mv / 1000.0f;
    vbat_float = vbat_mv / 1000.0f;

    //Caculate NTC resistor : Vntc = Vin*Rs/(Rs + Rntc) -> Rntc = Rs(Vin/Vntc - 1)
    r_ntc = HW_RESISTOR_SERIES_NTC*(vbat_float/vtemp_float - 1);
    DEBUG_PRINTF("Resistant NTC = %d\r\n", (int)r_ntc);


    if (r_ntc <= 0)
    {
        retval = false;
        goto end;
    }

    temp = (BETA * ROOM_TEMP) / (BETA + (ROOM_TEMP * log(r_ntc / RESISTOR_ROOM_TEMP))) - 273.15f;

    if (temp < 0)
    {
        DEBUG_ERROR("Invalid temperature %.1f\r\n", temp);
        retval = false;
        goto end;
    }

    *result = (int32_t)temp;
end:
    return retval;
}
#endif /* USE_INTERNAL_VREF */

void adc_convert(void)
{
#if 0
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
#else
	
    // VREF and VDDA
    m_adc_input.vref_int = *((uint16_t*)0x1FF80078);
	// m_adc_input.vdda_mv = 3000 * m_adc_input.vref_int/m_adc_raw_data[V_REF_CHANNEL_INDEX] + VREF_OFFSET_MV;
    m_adc_input.vdda_mv = __LL_ADC_CALC_VREFANALOG_VOLTAGE(m_adc_raw_data[V_REF_CHANNEL_INDEX], LL_ADC_RESOLUTION_12B);
	/* ADC Vbat */
	m_adc_input.bat_mv = (ADC_VBAT_RESISTOR_DIV*m_adc_raw_data[VBAT_CHANNEL_INDEX]*m_adc_input.vdda_mv/4095);
	m_adc_input.bat_percent = convert_vin_to_percent(m_adc_input.bat_mv);
	
	/* ADC Vin 24V */
	m_adc_input.vin_24 = ((uint32_t)ADC_VIN_RESISTOR_DIV*m_adc_raw_data[VIN_24V_CHANNEL_INDEX]/(uint32_t)1000)*m_adc_input.vdda_mv/4095;
	
	/* ADC input 4-20mA */
	m_adc_input.in_4_20ma_in[0] = m_adc_raw_data[V_INPUT_0_4_20MA_CHANNEL_INDEX]*m_adc_input.vdda_mv*10/(GAIN_INPUT_4_20MA_IN*4095);
#endif

#ifdef DTG02
	m_adc_input.in_4_20ma_in[1] = m_adc_raw_data[V_INPUT_1_4_20MA_CHANNEL_INDEX]*m_adc_input.vdda_mv*10/(GAIN_INPUT_4_20MA_IN*4095);
	m_adc_input.in_4_20ma_in[2] = m_adc_raw_data[V_INPUT_2_4_20MA_CHANNEL_INDEX]*m_adc_input.vdda_mv*10/(GAIN_INPUT_4_20MA_IN*4095);
	m_adc_input.in_4_20ma_in[3] = m_adc_raw_data[V_INPUT_3_4_20MA_CHANNEL_INDEX]*m_adc_input.vdda_mv*10/(GAIN_INPUT_4_20MA_IN*4095);
#endif

#ifndef USE_INTERNAL_VREF
	/* v_temp */
    if (m_adc_raw_data[V_TEMP_CHANNEL_INDEX])
    {
        uint32_t vtemp_mv = m_adc_raw_data[V_TEMP_CHANNEL_INDEX]*m_adc_input.vdda_mv/4095;
        if (convert_temperature(vtemp_mv, m_adc_input.vdda_mv, &m_adc_input.temp))
        {
            m_adc_input.temp_is_valid = 1;
        }
        else
        {
            m_adc_input.temp_is_valid = 0;
        }
    }
#else
    m_adc_input.temp_is_valid = 1;
    m_adc_input.temp = __LL_ADC_CALC_TEMPERATURE(m_adc_input.vdda_mv, m_adc_raw_data[V_INTERNAL_CHIP_TEMP_CHANNEL_INDEX], LL_ADC_RESOLUTION_12B);
#endif
}


/* USER CODE END 1 */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/

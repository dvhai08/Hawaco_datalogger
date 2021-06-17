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

static volatile bool m_adc_started = false;
static uint32_t m_adc_conversion_count = 0;
static uint16_t m_adc_raw_data[ADC_CHANNEL_DMA_COUNT];
static lpf_data_t m_adc_filterd_data[ADC_CHANNEL_DMA_COUNT];
/* USER CODE END 0 */

ADC_HandleTypeDef hadc;
DMA_HandleTypeDef hdma_adc;

/* ADC init function */
void MX_ADC_Init(void)
{

  /* USER CODE BEGIN ADC_Init 0 */
	
  /* USER CODE END ADC_Init 0 */

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC_Init 1 */

  /* USER CODE END ADC_Init 1 */
  /** Configure the global features of the ADC (Clock, Resolution, Data Alignment and number of conversion)
  */
  hadc.Instance = ADC1;
  hadc.Init.OversamplingMode = DISABLE;
  hadc.Init.ClockPrescaler = ADC_CLOCK_ASYNC_DIV64;
  hadc.Init.Resolution = ADC_RESOLUTION_12B;
  hadc.Init.SamplingTime = ADC_SAMPLETIME_160CYCLES_5;
  hadc.Init.ScanConvMode = ADC_SCAN_DIRECTION_FORWARD;
  hadc.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc.Init.ContinuousConvMode = DISABLE;
  hadc.Init.DiscontinuousConvMode = DISABLE;
  hadc.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc.Init.DMAContinuousRequests = DISABLE;
  hadc.Init.EOCSelection = ADC_EOC_SEQ_CONV;
  hadc.Init.Overrun = ADC_OVR_DATA_OVERWRITTEN;
  hadc.Init.LowPowerAutoWait = ENABLE;
  hadc.Init.LowPowerFrequencyMode = ENABLE;
  hadc.Init.LowPowerAutoPowerOff = ENABLE;
  if (HAL_ADC_Init(&hadc) != HAL_OK)
  {
    Error_Handler();
  }
  /** Configure for the selected ADC regular channel to be converted.
  */
  sConfig.Channel = ADC_CHANNEL_0;
  sConfig.Rank = ADC_RANK_CHANNEL_NUMBER;
  if (HAL_ADC_ConfigChannel(&hadc, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /** Configure for the selected ADC regular channel to be converted.
  */
  sConfig.Channel = ADC_CHANNEL_1;
  if (HAL_ADC_ConfigChannel(&hadc, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /** Configure for the selected ADC regular channel to be converted.
  */
  sConfig.Channel = ADC_CHANNEL_5;
  if (HAL_ADC_ConfigChannel(&hadc, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /** Configure for the selected ADC regular channel to be converted.
  */
  sConfig.Channel = ADC_CHANNEL_6;
  if (HAL_ADC_ConfigChannel(&hadc, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /** Configure for the selected ADC regular channel to be converted.
  */
  sConfig.Channel = ADC_CHANNEL_7;
  if (HAL_ADC_ConfigChannel(&hadc, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /** Configure for the selected ADC regular channel to be converted.
  */
  sConfig.Channel = ADC_CHANNEL_11;
  if (HAL_ADC_ConfigChannel(&hadc, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /** Configure for the selected ADC regular channel to be converted.
  */
  sConfig.Channel = ADC_CHANNEL_15;
  if (HAL_ADC_ConfigChannel(&hadc, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC_Init 2 */

  /* USER CODE END ADC_Init 2 */

}

void HAL_ADC_MspInit(ADC_HandleTypeDef* adcHandle)
{

  GPIO_InitTypeDef GPIO_InitStruct = {0};
  if(adcHandle->Instance==ADC1)
  {
  /* USER CODE BEGIN ADC1_MspInit 0 */

  /* USER CODE END ADC1_MspInit 0 */
    /* ADC1 clock enable */
    __HAL_RCC_ADC1_CLK_ENABLE();

    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    /**ADC GPIO Configuration
    PC1     ------> ADC_IN11
    PA0     ------> ADC_IN0
    PA1     ------> ADC_IN1
    PA5     ------> ADC_IN5
    PA6     ------> ADC_IN6
    PA7     ------> ADC_IN7
    PC5     ------> ADC_IN15
    */
    GPIO_InitStruct.Pin = GPIO_PIN_1|GPIO_PIN_5;
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_5|GPIO_PIN_6
                          |GPIO_PIN_7;
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /* ADC1 DMA Init */
    /* ADC Init */
    hdma_adc.Instance = DMA1_Channel1;
    hdma_adc.Init.Request = DMA_REQUEST_0;
    hdma_adc.Init.Direction = DMA_PERIPH_TO_MEMORY;
    hdma_adc.Init.PeriphInc = DMA_PINC_DISABLE;
    hdma_adc.Init.MemInc = DMA_MINC_ENABLE;
    hdma_adc.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
    hdma_adc.Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;
    hdma_adc.Init.Mode = DMA_NORMAL;
    hdma_adc.Init.Priority = DMA_PRIORITY_LOW;
    if (HAL_DMA_Init(&hdma_adc) != HAL_OK)
    {
      Error_Handler();
    }

    __HAL_LINKDMA(adcHandle,DMA_Handle,hdma_adc);

    /* ADC1 interrupt Init */
    HAL_NVIC_SetPriority(ADC1_COMP_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(ADC1_COMP_IRQn);
  /* USER CODE BEGIN ADC1_MspInit 1 */

  /* USER CODE END ADC1_MspInit 1 */
  }
}

void HAL_ADC_MspDeInit(ADC_HandleTypeDef* adcHandle)
{

  if(adcHandle->Instance==ADC1)
  {
  /* USER CODE BEGIN ADC1_MspDeInit 0 */

  /* USER CODE END ADC1_MspDeInit 0 */
    /* Peripheral clock disable */
    __HAL_RCC_ADC1_CLK_DISABLE();

    /**ADC GPIO Configuration
    PC1     ------> ADC_IN11
    PA0     ------> ADC_IN0
    PA1     ------> ADC_IN1
    PA5     ------> ADC_IN5
    PA6     ------> ADC_IN6
    PA7     ------> ADC_IN7
    PC5     ------> ADC_IN15
    */
    HAL_GPIO_DeInit(GPIOC, GPIO_PIN_1|GPIO_PIN_5);

    HAL_GPIO_DeInit(GPIOA, GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_5|GPIO_PIN_6
                          |GPIO_PIN_7);

    /* ADC1 DMA DeInit */
    HAL_DMA_DeInit(adcHandle->DMA_Handle);

    /* ADC1 interrupt Deinit */
    HAL_NVIC_DisableIRQ(ADC1_COMP_IRQn);
  /* USER CODE BEGIN ADC1_MspDeInit 1 */

  /* USER CODE END ADC1_MspDeInit 1 */
  }
}

/* USER CODE BEGIN 1 */
static bool m_is_the_first_time_convert = true;
static bool m_adc_new_data = false;
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *AdcHandle)
{
	/* Get the converted value of regular channel */
	DEBUG_PRINTF("DMA DAC complete callback\r\n");
	if (m_adc_conversion_count++ >= ADC_NUMBER_OF_CONVERSION_TIMES)
	{
		m_adc_conversion_count = 0;
		HAL_ADC_MspDeInit(&hadc);
		m_adc_started = false;
		m_adc_new_data = true;
		ENABLE_NTC_POWER(0);
		ENABLE_INOUT_2_20MA_POWER(0);
	}
	else
	{
		for (uint32_t i = 0; i < ADC_CHANNEL_DMA_COUNT; i++)
		{
			if (!m_is_the_first_time_convert)
			{
				m_adc_filterd_data[i].estimate_value *= 100;
				m_adc_raw_data[i] *= 100;
				lpf_update_estimate(&m_adc_filterd_data[i], (int32_t*)&m_adc_raw_data[i]);
				m_adc_filterd_data[i].estimate_value /= 100;				
			}
			else
			{
				m_adc_filterd_data[i].estimate_value = m_adc_raw_data[i];
				m_adc_filterd_data[i].gain = 1;		/* 1% */
				m_is_the_first_time_convert = false;
			}
		}
		HAL_ADC_Start_DMA(&hadc, (uint32_t*)m_adc_raw_data, sizeof(m_adc_raw_data));
	}
}

void adc_start(void)
{
	if (m_adc_started == false)
	{
		app_eeprom_config_data_t * cfg = app_eeprom_read_config_data();
		if (cfg->io_enable.name.input_4_20ma_enable)
		{
			ENABLE_INOUT_2_20MA_POWER(1);
		}
		
		ENABLE_NTC_POWER(1);
		sys_delay_ms(3);		// 3ms for NTC resistor power on
		m_adc_started = true;
		MX_ADC_Init();
		HAL_ADC_Start_DMA(&hadc, (uint32_t*)m_adc_raw_data, sizeof(m_adc_raw_data));
	}
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


static adc_input_value_t m_adc_input;

adc_input_value_t *adc_get_input_result(void)
{
	return &m_adc_input;
}

void adc_convert(void)
{
	/* ADC Vbat */
	m_adc_input.bat_mv = (ADC_VBAT_RESISTOR_DIV*m_adc_filterd_data[VBAT_CHANNEL_INDEX].estimate_value*ADC_VREF/4095);
	m_adc_input.bat_percent = convert_vin_to_percent(m_adc_input.bat_mv);
	
	/* ADC Vin 24V */
	m_adc_input.vin_24 = (ADC_VIN_RESISTOR_DIV*m_adc_filterd_data[VIN_24V_CHANNEL_INDEX].estimate_value*ADC_VREF/4095);
	
	/* ADC input 4-20mA */
	m_adc_input.i_4_20ma_in[0] = m_adc_filterd_data[V_INPUT_0_4_20MA_CHANNEL_INDEX].estimate_value*ADC_VREF/4095;
	m_adc_input.i_4_20ma_in[1] = m_adc_filterd_data[V_INPUT_1_4_20MA_CHANNEL_INDEX].estimate_value*ADC_VREF/4095;
	m_adc_input.i_4_20ma_in[2] = m_adc_filterd_data[V_INPUT_2_4_20MA_CHANNEL_INDEX].estimate_value*ADC_VREF/4095;
	m_adc_input.i_4_20ma_in[3] = m_adc_filterd_data[V_INPUT_3_4_20MA_CHANNEL_INDEX].estimate_value*ADC_VREF/4095;
	
	/* v_temp */
	m_adc_input.temp = m_adc_filterd_data[V_TEMP_CHANNEL_INDEX].estimate_value*ADC_VREF/4095;
}

bool adc_conversion_cplt(bool clear_on_exit)
{
	bool retval = m_adc_new_data;
	
	if (clear_on_exit)
	{
		m_adc_new_data = false;
	}
	return retval;
}



/* USER CODE END 1 */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/

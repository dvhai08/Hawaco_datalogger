/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    tim.c
  * @brief   This file provides code for the configuration
  *          of the TIM instances.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2021 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "tim.h"

/* USER CODE BEGIN 0 */
#include "app_debug.h"
#include <stdbool.h>
#include "gsm.h"

#define TIME_FREQ       8000000
bool m_dac_started = false;
volatile uint32_t m_reload_value = 2999;     // 5khz

/* USER CODE END 0 */

/* TIM2 init function */
void MX_TIM2_Init(void)
{

  /* USER CODE BEGIN TIM2_Init 0 */

  /* USER CODE END TIM2_Init 0 */

  LL_TIM_InitTypeDef TIM_InitStruct = {0};
  LL_TIM_OC_InitTypeDef TIM_OC_InitStruct = {0};

  LL_GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* Peripheral clock enable */
  LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_TIM2);

  /* USER CODE BEGIN TIM2_Init 1 */

  /* USER CODE END TIM2_Init 1 */
  TIM_InitStruct.Prescaler = 0;
  TIM_InitStruct.CounterMode = LL_TIM_COUNTERMODE_UP;
  TIM_InitStruct.Autoreload = m_reload_value;
  TIM_InitStruct.ClockDivision = LL_TIM_CLOCKDIVISION_DIV1;
  LL_TIM_Init(TIM2, &TIM_InitStruct);
  LL_TIM_DisableARRPreload(TIM2);
  LL_TIM_OC_EnablePreload(TIM2, LL_TIM_CHANNEL_CH4);
  TIM_OC_InitStruct.OCMode = LL_TIM_OCMODE_PWM1;
  TIM_OC_InitStruct.OCState = LL_TIM_OCSTATE_DISABLE;
  TIM_OC_InitStruct.CompareValue = 0;
  TIM_OC_InitStruct.OCPolarity = LL_TIM_OCPOLARITY_HIGH;
  LL_TIM_OC_Init(TIM2, LL_TIM_CHANNEL_CH4, &TIM_OC_InitStruct);
  LL_TIM_OC_DisableFast(TIM2, LL_TIM_CHANNEL_CH4);
  LL_TIM_SetTriggerOutput(TIM2, LL_TIM_TRGO_RESET);
  LL_TIM_DisableMasterSlaveMode(TIM2);
  /* USER CODE BEGIN TIM2_Init 2 */

  /* USER CODE END TIM2_Init 2 */
  LL_IOP_GRP1_EnableClock(LL_IOP_GRP1_PERIPH_GPIOA);
    /**TIM2 GPIO Configuration
    PA3     ------> TIM2_CH4
    */
  GPIO_InitStruct.Pin = OUTPUT_4_20MA_PWM_Pin;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_ALTERNATE;
  GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
  GPIO_InitStruct.Alternate = LL_GPIO_AF_2;
  LL_GPIO_Init(OUTPUT_4_20MA_PWM_GPIO_Port, &GPIO_InitStruct);

}

/* USER CODE BEGIN 1 */
void tim_pwm_change_freq(uint32_t freq)
{
    if (freq)
    {
        m_reload_value = TIME_FREQ/freq - 1;      // 8mhz clock
//        DEBUG_INFO("Change reload value to %u\r\n", m_reload_value);
        tim_pwm_stop();
        tim_pwm_start();
    }
}

void tim_pwm_stop(void)
{
//	DEBUG_PRINTF("Stop pwm\r\n");
	m_dac_started = false;
    LL_APB1_GRP1_ForceReset(LL_APB1_GRP1_PERIPH_TIM2);
    LL_APB1_GRP1_ReleaseReset(LL_APB1_GRP1_PERIPH_TIM2);
	
	LL_APB1_GRP1_DisableClock(LL_APB1_GRP1_PERIPH_TIM2);
	LL_GPIO_InitTypeDef GPIO_InitStruct = {0};
	GPIO_InitStruct.Pin = OUTPUT_4_20MA_PWM_Pin;
	GPIO_InitStruct.Mode = LL_GPIO_MODE_ANALOG;
	GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
	LL_GPIO_Init(OUTPUT_4_20MA_PWM_GPIO_Port, &GPIO_InitStruct);
}

void tim_pwm_start(void)
{
	MX_TIM2_Init();
}

static uint32_t m_pre_pwm = 0;
void tim_pwm_output_percent(uint32_t thoughsand)
{
    if (m_pre_pwm != thoughsand)
    {
        m_pre_pwm = thoughsand;
        DEBUG_INFO("Set PWM percent %u%%o\r\n", thoughsand);
    }
	if (m_dac_started == false)
	{
		m_dac_started = true;
		tim_pwm_start();
	}
	LL_TIM_CC_EnableChannel(TIM2, LL_TIM_CHANNEL_CH4);
	TIM2->CCR4 = thoughsand * (m_reload_value+1) / 1000;
	LL_TIM_EnableCounter(TIM2);
}

bool tim_is_pwm_active(void)
{
    return m_dac_started;
}

/* USER CODE END 1 */

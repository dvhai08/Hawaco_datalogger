/**
  ******************************************************************************
  * @file    adc.h
  * @brief   This file contains all the function prototypes for
  *          the adc.c file
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
/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __ADC_H__
#define __ADC_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* USER CODE BEGIN Includes */
#include <stdbool.h>
typedef struct
{
	uint32_t bat_mv;			// battery voltage in MV
	uint32_t bat_percent;		// battery voltage in percent
	uint32_t vin;			// 24V input
	uint32_t vdda_mv;			// VDDA in mv
	uint32_t vref_int;
#ifdef DTG02
	float in_4_20ma_in[4];		// 4.5mA =>> convert to 45
#else
	float in_4_20ma_in[1];		// 4.5mA =>> convert to 45
#endif
	int32_t temp;
    uint32_t temp_is_valid;
} adc_input_value_t;

/**
 * @brief		Get adc input result of serveral sersor
 */
adc_input_value_t *adc_get_input_result(void);

/**
 * @brief		Convert adc value
 */
void adc_convert(void);

/**
 * @brief		Check if adc conversion is complete
 * @retval		TRUE ADC convert is completed
 *				FALSE ADC convert is inprogess
 */
bool adc_conversion_cplt(bool clear_on_exit);

/* USER CODE END Includes */

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

void MX_ADC_Init(void);

/* USER CODE BEGIN Prototypes */

/**
 * @brief		Start adc task
 */
void adc_start(void);

/**
 * @brief		Stop adc task
 */
void adc_stop(void);

/**
 * @brief		ADC ISR callback
 */
void adc_isr_cb(void);

/* USER CODE END Prototypes */

#ifdef __cplusplus
}
#endif

#endif /* __ADC_H__ */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/

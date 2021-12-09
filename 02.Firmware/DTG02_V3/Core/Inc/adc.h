/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    adc.h
  * @brief   This file contains all the function prototypes for
  *          the adc.c file
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
	uint32_t vin;			    // 12-24V input
	uint32_t vdda_mv;			// VDDA in mv
	uint32_t vref_int;
#ifndef DTG01
	float in_4_20ma_in[4];		
#else
	float in_4_20ma_in[1];		
#endif

#ifdef DTG02V3
    uint16_t analog_input_io[2];
#endif
    
	int32_t temp;
    uint32_t temp_is_valid;
} adc_input_value_t;

/* USER CODE END Includes */

/* USER CODE BEGIN Private defines */
#define VREFINT_CAL                     ((uint16_t*) VREFINT_CAL_ADDR)

/* USER CODE END Private defines */

void MX_ADC_Init(void);

/* USER CODE BEGIN Prototypes */

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


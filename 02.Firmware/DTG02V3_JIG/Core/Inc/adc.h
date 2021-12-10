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
  * This software component is licensed by ST under Ultimate Liberty license
  * SLA0044, the "License"; You may not use this file except in compliance with
  * the License. You may obtain a copy of the License at:
  *                             www.st.com/SLA0044
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
#ifndef VREFINT_CAL_ADDR
#define VREFINT_CAL_ADDR                0x1FFFF7BA  /* datasheet p. 19 */
#endif
#define VREFINT_CAL ((uint16_t*)        VREFINT_CAL_ADDR)


#define ADC_MAX_CHANNEL         9
#define ADC_CHANNEL_3V3         0
#define ADC_CHANNEL_VPS_TX      1
#define ADC_CHANNEL_VPS         2
#define ADC_CHANNEL_VGSM        3
#define ADC_CHANNEL_3V3_RTC     4
#define ADC_CHANNEL_VCC_RF      5
#define ADC_CHANNEL_1V8         6
#define ADC_CHANNEL_4V2         7
#define ADC_CHANNEL_VREF        8
#define ADC_RESISTOR_DIV        2
/* USER CODE END Includes */

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

void MX_ADC_Init(void);

/* USER CODE BEGIN Prototypes */
uint16_t *adc_get_result(void);

void adc_start(void);

/* USER CODE END Prototypes */

#ifdef __cplusplus
}
#endif

#endif /* __ADC_H__ */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/

/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
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
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32l0xx_hal.h"

#include "stm32l0xx_ll_iwdg.h"
#include "stm32l0xx_ll_lpuart.h"
#include "stm32l0xx_ll_rcc.h"
#include "stm32l0xx_ll_usart.h"
#include "stm32l0xx_ll_system.h"
#include "stm32l0xx_ll_gpio.h"
#include "stm32l0xx_ll_exti.h"
#include "stm32l0xx_ll_bus.h"
#include "stm32l0xx_ll_cortex.h"
#include "stm32l0xx_ll_utils.h"
#include "stm32l0xx_ll_pwr.h"
#include "stm32l0xx_ll_dma.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define CIRIN1_Pin LL_GPIO_PIN_13
#define CIRIN1_GPIO_Port GPIOC
#define VTEMP_Pin LL_GPIO_PIN_1
#define VTEMP_GPIO_Port GPIOC
#define SW1_Pin LL_GPIO_PIN_0
#define SW1_GPIO_Port GPIOA
#define SW1_EXTI_IRQn EXTI0_1_IRQn
#define ADC_VBAT_Pin LL_GPIO_PIN_1
#define ADC_VBAT_GPIO_Port GPIOA
#define TRANS_OUTPUT_Pin LL_GPIO_PIN_2
#define TRANS_OUTPUT_GPIO_Port GPIOA
#define DIR_Pin LL_GPIO_PIN_3
#define DIR_GPIO_Port GPIOA
#define ADC_4_20MA_IN_Pin LL_GPIO_PIN_4
#define ADC_4_20MA_IN_GPIO_Port GPIOA
#define DAC_4_20MA_OUT_Pin LL_GPIO_PIN_5
#define DAC_4_20MA_OUT_GPIO_Port GPIOA
#define ADC_24V_Pin LL_GPIO_PIN_6
#define ADC_24V_GPIO_Port GPIOA
#define EXT_FLASH_CS_Pin LL_GPIO_PIN_4
#define EXT_FLASH_CS_GPIO_Port GPIOC
#define CHARGE_EN_Pin LL_GPIO_PIN_5
#define CHARGE_EN_GPIO_Port GPIOC
#define RS485_EN_Pin LL_GPIO_PIN_0
#define RS485_EN_GPIO_Port GPIOB
#define PWM_Pin LL_GPIO_PIN_1
#define PWM_GPIO_Port GPIOB
#define PWM_EXTI_IRQn EXTI0_1_IRQn
#define ENABLE_OUTPUT_4_20MA_Pin LL_GPIO_PIN_2
#define ENABLE_OUTPUT_4_20MA_GPIO_Port GPIOB
#define RS485_TX_Pin LL_GPIO_PIN_10
#define RS485_TX_GPIO_Port GPIOB
#define RS485_RX_Pin LL_GPIO_PIN_11
#define RS485_RX_GPIO_Port GPIOB
#define GSM_PWR_KEY_Pin LL_GPIO_PIN_12
#define GSM_PWR_KEY_GPIO_Port GPIOB
#define GSM_RESET_Pin LL_GPIO_PIN_14
#define GSM_RESET_GPIO_Port GPIOB
#define DT485_B_Pin LL_GPIO_PIN_15
#define DT485_B_GPIO_Port GPIOB
#define EN_4_20MA_IN_Pin LL_GPIO_PIN_8
#define EN_4_20MA_IN_GPIO_Port GPIOA
#define GSM_RX_Pin LL_GPIO_PIN_9
#define GSM_RX_GPIO_Port GPIOA
#define GSM_TX_Pin LL_GPIO_PIN_10
#define GSM_TX_GPIO_Port GPIOA
#define GSM_RI_Pin LL_GPIO_PIN_11
#define GSM_RI_GPIO_Port GPIOA
#define GSM_STATUS_Pin LL_GPIO_PIN_12
#define GSM_STATUS_GPIO_Port GPIOA
#define GSM_EN_Pin LL_GPIO_PIN_15
#define GSM_EN_GPIO_Port GPIOA
#define LED2_Pin LL_GPIO_PIN_4
#define LED2_GPIO_Port GPIOB
#define LED1_Pin LL_GPIO_PIN_5
#define LED1_GPIO_Port GPIOB
#define RS485_DIR_Pin LL_GPIO_PIN_7
#define RS485_DIR_GPIO_Port GPIOB
#define VNTC_Pin LL_GPIO_PIN_9
#define VNTC_GPIO_Port GPIOB
/* USER CODE BEGIN Private defines */

/**
 * @brief	Get systick in ms
 * @retval	System tick
 */
uint32_t sys_get_ms(void);


/**
 * @brief		Delay ms
 * @param[in] 	ms Delay in ms
 */
void sys_delay_ms(uint32_t ms);

/**
 * @brief		Set system timeout before enabling deepsleep
 * @param[in] 	ms Delay in ms
 */
void sys_set_delay_time_before_deep_sleep(uint32_t ms);

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/

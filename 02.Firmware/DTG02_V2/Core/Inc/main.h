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

#include "stm32l0xx_ll_adc.h"
#include "stm32l0xx_ll_crc.h"
#include "stm32l0xx_ll_iwdg.h"
#include "stm32l0xx_ll_lpuart.h"
#include "stm32l0xx_ll_rcc.h"
#include "stm32l0xx_ll_spi.h"
#include "stm32l0xx_ll_tim.h"
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
#define CIRIN0_Pin LL_GPIO_PIN_0
#define CIRIN0_GPIO_Port GPIOH
#define CIRIN1_Pin LL_GPIO_PIN_1
#define CIRIN1_GPIO_Port GPIOH
#define OPTOIN1_Pin LL_GPIO_PIN_0
#define OPTOIN1_GPIO_Port GPIOC
#define ADC_VBAT_Pin LL_GPIO_PIN_1
#define ADC_VBAT_GPIO_Port GPIOC
#define EXT_FLASH_MISO_Pin LL_GPIO_PIN_2
#define EXT_FLASH_MISO_GPIO_Port GPIOC
#define ADC_4_20MA_IN4_Pin LL_GPIO_PIN_3
#define ADC_4_20MA_IN4_GPIO_Port GPIOC
#define WKUP1_Pin LL_GPIO_PIN_0
#define WKUP1_GPIO_Port GPIOA
#define WKUP1_EXTI_IRQn EXTI0_1_IRQn
#define ADC_24V_Pin LL_GPIO_PIN_1
#define ADC_24V_GPIO_Port GPIOA
#define EN_4_20MA_IN_Pin LL_GPIO_PIN_2
#define EN_4_20MA_IN_GPIO_Port GPIOA
#define OUTPUT_4_20MA_PWM_Pin LL_GPIO_PIN_3
#define OUTPUT_4_20MA_PWM_GPIO_Port GPIOA
#define OPTOIN2_Pin LL_GPIO_PIN_4
#define OPTOIN2_GPIO_Port GPIOA
#define ADC_4_20MA_IN3_Pin LL_GPIO_PIN_5
#define ADC_4_20MA_IN3_GPIO_Port GPIOA
#define ADC_4_20MA_IN3A6_Pin LL_GPIO_PIN_6
#define ADC_4_20MA_IN3A6_GPIO_Port GPIOA
#define ADC_4_20MA_IN1_Pin LL_GPIO_PIN_7
#define ADC_4_20MA_IN1_GPIO_Port GPIOA
#define CHARGE_EN_Pin LL_GPIO_PIN_4
#define CHARGE_EN_GPIO_Port GPIOC
#define ADC_VTEMP_Pin LL_GPIO_PIN_5
#define ADC_VTEMP_GPIO_Port GPIOC
#define OPTOIN3_Pin LL_GPIO_PIN_2
#define OPTOIN3_GPIO_Port GPIOB
#define RS485_RX_Pin LL_GPIO_PIN_10
#define RS485_RX_GPIO_Port GPIOB
#define RS485_TX_Pin LL_GPIO_PIN_11
#define RS485_TX_GPIO_Port GPIOB
#define EXT_FLASH_CS_Pin LL_GPIO_PIN_12
#define EXT_FLASH_CS_GPIO_Port GPIOB
#define EXT_FLASH_SCK_Pin LL_GPIO_PIN_13
#define EXT_FLASH_SCK_GPIO_Port GPIOB
#define PULSE_PWM1_Pin LL_GPIO_PIN_14
#define PULSE_PWM1_GPIO_Port GPIOB
#define PULSE_PWM1_EXTI_IRQn EXTI4_15_IRQn
#define EXT_FLASH_MOSI_Pin LL_GPIO_PIN_15
#define EXT_FLASH_MOSI_GPIO_Port GPIOB
#define GSM_EN_Pin LL_GPIO_PIN_6
#define GSM_EN_GPIO_Port GPIOC
#define PULSE_DIR1_Pin LL_GPIO_PIN_7
#define PULSE_DIR1_GPIO_Port GPIOC
#define PULSE_DIR1_EXTI_IRQn EXTI4_15_IRQn
#define PULSE_PWM2_Pin LL_GPIO_PIN_8
#define PULSE_PWM2_GPIO_Port GPIOC
#define PULSE_PWM2_EXTI_IRQn EXTI4_15_IRQn
#define PULSE_DIR2_Pin LL_GPIO_PIN_9
#define PULSE_DIR2_GPIO_Port GPIOC
#define PULSE_DIR2_EXTI_IRQn EXTI4_15_IRQn
#define GSM_PWR_KEY_Pin LL_GPIO_PIN_8
#define GSM_PWR_KEY_GPIO_Port GPIOA
#define GSM_RX_Pin LL_GPIO_PIN_9
#define GSM_RX_GPIO_Port GPIOA
#define GSM_TX_Pin LL_GPIO_PIN_10
#define GSM_TX_GPIO_Port GPIOA
#define SYS_4V2_EN_Pin LL_GPIO_PIN_15
#define SYS_4V2_EN_GPIO_Port GPIOA
#define TRANS_OUT2_Pin LL_GPIO_PIN_10
#define TRANS_OUT2_GPIO_Port GPIOC
#define TRANS_OUT3_Pin LL_GPIO_PIN_11
#define TRANS_OUT3_GPIO_Port GPIOC
#define TRANS_OUT4_Pin LL_GPIO_PIN_12
#define TRANS_OUT4_GPIO_Port GPIOC
#define RS485_EN_Pin LL_GPIO_PIN_2
#define RS485_EN_GPIO_Port GPIOD
#define OPTOIN4_Pin LL_GPIO_PIN_4
#define OPTOIN4_GPIO_Port GPIOB
#define LED1_Pin LL_GPIO_PIN_5
#define LED1_GPIO_Port GPIOB
#define VNTC_Pin LL_GPIO_PIN_6
#define VNTC_GPIO_Port GPIOB
#define RS485_DIR_Pin LL_GPIO_PIN_7
#define RS485_DIR_GPIO_Port GPIOB
#define TRANS_OUT1_Pin LL_GPIO_PIN_8
#define TRANS_OUT1_GPIO_Port GPIOB
#define GSM_RESET_Pin LL_GPIO_PIN_9
#define GSM_RESET_GPIO_Port GPIOB
/* USER CODE BEGIN Private defines */
#define PWMIN1_Pin					PULSE_PWM1_Pin
#define PWMIN1_GPIO_Port			PULSE_PWM1_GPIO_Port
#define DIRIN1_Pin					PULSE_DIR1_Pin
#define DIRIN1_GPIO_Port			PULSE_DIR1_GPIO_Port

#define PWMIN2_Pin					PULSE_PWM2_Pin
#define PWMIN2_GPIO_Port			PULSE_PWM2_GPIO_Port
#define DIRIN2_Pin					PULSE_DIR2_Pin
#define DIRIN2_GPIO_Port			PULSE_DIR2_GPIO_Port

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

/**
 * @brief		Turn on led with in ms
 * @param[in] 	Led on in ms
 */
void sys_turn_on_led(uint32_t delay_ms);

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/

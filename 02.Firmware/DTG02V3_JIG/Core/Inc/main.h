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
  * This software component is licensed by ST under Ultimate Liberty license
  * SLA0044, the "License"; You may not use this file except in compliance with
  * the License. You may obtain a copy of the License at:
  *                             www.st.com/SLA0044
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
#include "stm32f0xx_hal.h"

#include "stm32f0xx_ll_adc.h"
#include "stm32f0xx_ll_dma.h"
#include "stm32f0xx_ll_iwdg.h"
#include "stm32f0xx_ll_crs.h"
#include "stm32f0xx_ll_rcc.h"
#include "stm32f0xx_ll_bus.h"
#include "stm32f0xx_ll_system.h"
#include "stm32f0xx_ll_exti.h"
#include "stm32f0xx_ll_cortex.h"
#include "stm32f0xx_ll_utils.h"
#include "stm32f0xx_ll_pwr.h"
#include "stm32f0xx_ll_usart.h"
#include "stm32f0xx_ll_gpio.h"

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
#define VGSM_Pin LL_GPIO_PIN_0
#define VGSM_GPIO_Port GPIOA
#define VREF_Pin LL_GPIO_PIN_1
#define VREF_GPIO_Port GPIOA
#define VCC_485_Pin LL_GPIO_PIN_3
#define VCC_485_GPIO_Port GPIOA
#define V_SYS_Pin LL_GPIO_PIN_4
#define V_SYS_GPIO_Port GPIOA
#define V_5V_Pin LL_GPIO_PIN_5
#define V_5V_GPIO_Port GPIOA
#define VBAT_Pin LL_GPIO_PIN_6
#define VBAT_GPIO_Port GPIOA
#define VCC_AIN_Pin LL_GPIO_PIN_7
#define VCC_AIN_GPIO_Port GPIOA
#define VCC_LORA_Pin LL_GPIO_PIN_0
#define VCC_LORA_GPIO_Port GPIOB
#define RS485_DIR_Pin LL_GPIO_PIN_2
#define RS485_DIR_GPIO_Port GPIOB
#define MCU_OUT2_Pin LL_GPIO_PIN_12
#define MCU_OUT2_GPIO_Port GPIOB
#define MCU_OUT1_Pin LL_GPIO_PIN_13
#define MCU_OUT1_GPIO_Port GPIOB
/* USER CODE BEGIN Private defines */

void hid_tx_cplt_callback(uint8_t epnum);
void on_rs485_uart_cb(uint8_t data);

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */

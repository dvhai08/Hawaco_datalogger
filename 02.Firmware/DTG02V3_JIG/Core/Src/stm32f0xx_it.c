/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    stm32f0xx_it.c
  * @brief   Interrupt Service Routines.
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

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "stm32f0xx_it.h"
/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "app_debug.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN TD */

/* USER CODE END TD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/* External variables --------------------------------------------------------*/
extern PCD_HandleTypeDef hpcd_USB_FS;
/* USER CODE BEGIN EV */

/* USER CODE END EV */

/******************************************************************************/
/*           Cortex-M0 Processor Interruption and Exception Handlers          */
/******************************************************************************/
/**
  * @brief This function handles Non maskable interrupt.
  */
void NMI_Handler(void)
{
  /* USER CODE BEGIN NonMaskableInt_IRQn 0 */

  /* USER CODE END NonMaskableInt_IRQn 0 */
  /* USER CODE BEGIN NonMaskableInt_IRQn 1 */
  while (1)
  {
  }
  /* USER CODE END NonMaskableInt_IRQn 1 */
}

/**
  * @brief This function handles Hard fault interrupt.
  */
void HardFault_Handler(void)
{
  /* USER CODE BEGIN HardFault_IRQn 0 */
    DEBUG_ERROR("Hardfault\r\n");
    NVIC_SystemReset();
  /* USER CODE END HardFault_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_HardFault_IRQn 0 */
    /* USER CODE END W1_HardFault_IRQn 0 */
  }
}

/**
  * @brief This function handles System service call via SWI instruction.
  */
void SVC_Handler(void)
{
  /* USER CODE BEGIN SVC_IRQn 0 */

  /* USER CODE END SVC_IRQn 0 */
  /* USER CODE BEGIN SVC_IRQn 1 */

  /* USER CODE END SVC_IRQn 1 */
}

/**
  * @brief This function handles Pendable request for system service.
  */
void PendSV_Handler(void)
{
  /* USER CODE BEGIN PendSV_IRQn 0 */

  /* USER CODE END PendSV_IRQn 0 */
  /* USER CODE BEGIN PendSV_IRQn 1 */

  /* USER CODE END PendSV_IRQn 1 */
}

/**
  * @brief This function handles System tick timer.
  */
void SysTick_Handler(void)
{
  /* USER CODE BEGIN SysTick_IRQn 0 */

  /* USER CODE END SysTick_IRQn 0 */
  HAL_IncTick();
  /* USER CODE BEGIN SysTick_IRQn 1 */

  /* USER CODE END SysTick_IRQn 1 */
}

/******************************************************************************/
/* STM32F0xx Peripheral Interrupt Handlers                                    */
/* Add here the Interrupt Handlers for the used peripherals.                  */
/* For the available peripheral interrupt handler names,                      */
/* please refer to the startup file (startup_stm32f0xx.s).                    */
/******************************************************************************/

/**
  * @brief This function handles DMA1 channel 1 global interrupt.
  */
void DMA1_Channel1_IRQHandler(void)
{
  /* USER CODE BEGIN DMA1_Channel1_IRQn 0 */
  /* Check whether DMA transfer complete caused the DMA interruption */
  if(LL_DMA_IsActiveFlag_TC1(DMA1) == 1)
  {
    /* Clear flag DMA transfer complete */
    LL_DMA_ClearFlag_TC1(DMA1);
  }
  
  /* Check whether DMA half transfer caused the DMA interruption */
  if(LL_DMA_IsActiveFlag_HT1(DMA1) == 1)
  {
    /* Clear flag DMA half transfer */
    LL_DMA_ClearFlag_HT1(DMA1);
  }
  
  /* Note: If DMA half transfer is not used, possibility to replace        */
  /*       management of DMA half transfer and transfer complete flags by  */
  /*       DMA global interrupt flag:                                      */
  /* Clear flag DMA global interrupt */
  /* (global interrupt flag: half transfer and transfer complete flags) */
  // LL_DMA_ClearFlag_GI1(DMA1);
  
  /* Check whether DMA transfer error caused the DMA interruption */
  if(LL_DMA_IsActiveFlag_TE1(DMA1) == 1)
  {
    /* Clear flag DMA transfer error */
    LL_DMA_ClearFlag_TE1(DMA1);
  }
  /* USER CODE END DMA1_Channel1_IRQn 0 */

  /* USER CODE BEGIN DMA1_Channel1_IRQn 1 */

  /* USER CODE END DMA1_Channel1_IRQn 1 */
}

/**
  * @brief This function handles USART3 and USART4 global interrupts.
  */
void USART3_4_IRQHandler(void)
{
  /* USER CODE BEGIN USART3_4_IRQn 0 */
//    DEBUG_INFO("ISR\r\n");
    if (LL_USART_IsActiveFlag_ORE(USART3))
    {
        DEBUG_WARN("USART3 OVR\r\n");
        uint32_t tmp = USART3->RDR;
        LL_USART_ClearFlag_ORE(USART3);
    }
    
    if (LL_USART_IsActiveFlag_FE(USART3))
    {
        DEBUG_WARN("USART3 FE\r\n");
        LL_USART_ClearFlag_FE(USART3);
    }
    
    if (LL_USART_IsActiveFlag_NE(USART3))
    {
        DEBUG_WARN("USART3 NE\r\n");
        LL_USART_ClearFlag_NE(USART3);
    }
	
	if (LL_USART_IsActiveFlag_RXNE(USART3))
	{
		uint32_t data = USART3->RDR;
        on_rs485_uart_cb(data & 0xFF);
	}
	
	if (LL_USART_IsEnabledIT_IDLE(USART3) && LL_USART_IsActiveFlag_IDLE(USART3)) 
    {
        LL_USART_ClearFlag_IDLE(USART3);        /* Clear IDLE line flag */
    }
  /* USER CODE END USART3_4_IRQn 0 */

  /* USER CODE BEGIN USART3_4_IRQn 1 */
#if 0
    if (LL_USART_IsActiveFlag_ORE(USART4))
    {
        DEBUG_WARN("USART4 OVR\r\n");
        uint32_t tmp = USART4->RDR;
        LL_USART_ClearFlag_ORE(USART4);
    }
    
    if (LL_USART_IsActiveFlag_FE(USART4))
    {
        DEBUG_WARN("USART4 FE\r\n");
        LL_USART_ClearFlag_FE(USART4);
    }
    
    if (LL_USART_IsActiveFlag_NE(USART4))
    {
        DEBUG_WARN("USART4 NE\r\n");
        LL_USART_ClearFlag_NE(USART4);
    }
	
	if (LL_USART_IsActiveFlag_RXNE(USART4))
	{
		uint32_t data = USART4->RDR;
	}
	
	if (LL_USART_IsEnabledIT_IDLE(USART4) && LL_USART_IsActiveFlag_IDLE(USART4)) 
    {
        LL_USART_ClearFlag_IDLE(USART4);        /* Clear IDLE line flag */
    }
#endif
  /* USER CODE END USART3_4_IRQn 1 */
}

/**
  * @brief This function handles USB global interrupt / USB wake-up interrupt through EXTI line 18.
  */
void USB_IRQHandler(void)
{
  /* USER CODE BEGIN USB_IRQn 0 */

  /* USER CODE END USB_IRQn 0 */
  HAL_PCD_IRQHandler(&hpcd_USB_FS);
  /* USER CODE BEGIN USB_IRQn 1 */

  /* USER CODE END USB_IRQn 1 */
}

/* USER CODE BEGIN 1 */

/* USER CODE END 1 */


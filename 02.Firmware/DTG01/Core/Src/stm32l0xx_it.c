/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    stm32l0xx_it.c
  * @brief   Interrupt Service Routines.
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

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "stm32l0xx_it.h"
/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "gsm.h"
#include "measure_input.h"
#include "usart.h"
#include "control_output.h"
#include "gsm.h"
#include "app_debug.h"
#include "adc.h"
#include "sys_ctx.h"
#include "rtc.h"
#include "app_eeprom.h"
#include "jig.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN TD */
typedef struct
{
    uint32_t last_exti;
} input_ext_isr_t;
/* USER CODE END TD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define EXIT_INPUT0_TIMESTAMP_INDEX 0
#ifdef DTG2
#define EXIT_INPUT1_TIMESTAMP_INDEX 1
#endif
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
extern void uart1_rx_complete_callback(bool status);
extern volatile uint32_t led_blink_delay;
//volatile uint32_t m_last_exti0_timestamp;
extern volatile uint32_t measure_input_turn_on_in_4_20ma_power;
extern volatile uint32_t m_delay_consider_wakeup;
extern volatile uint32_t jig_timeout_ms;
/* USER CODE END 0 */

/* External variables --------------------------------------------------------*/
extern RTC_HandleTypeDef hrtc;
/* USER CODE BEGIN EV */
input_ext_isr_t m_last_exti[MEASURE_NUMBER_OF_WATER_METER_INPUT];
extern volatile int32_t ota_update_timeout_ms;
/* USER CODE END EV */

/******************************************************************************/
/*           Cortex-M0+ Processor Interruption and Exception Handlers          */
/******************************************************************************/
/**
  * @brief This function handles Non maskable Interrupt.
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
	DEBUG_PRINTF("Hardfault\r\n");
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
	if (led_blink_delay > 0)
	{
        LED1(1);
		led_blink_delay--;
	}
    
    if (ota_update_timeout_ms != -1)
    {
        if (ota_update_timeout_ms-- == 0)
        {
            DEBUG_VERBOSE("OTA update timeout\r\n");
            NVIC_SystemReset();
        }
    }
    
    if (measure_input_turn_on_in_4_20ma_power)
    {
        measure_input_turn_on_in_4_20ma_power--;
    }
    
    if (m_delay_consider_wakeup)
    {
        m_delay_consider_wakeup--;
        if (m_delay_consider_wakeup == 0)
        {
            if (LL_GPIO_IsInputPinSet(ADC_24V_GPIO_Port, ADC_24V_Pin))
            {
                if (gsm_data_layer_is_module_sleeping())
                {
                    measure_input_measure_wakeup_to_get_data();
                    gsm_set_wakeup_now();
                    sys_ctx_t *ctx = sys_ctx();
                    ctx->peripheral_running.name.gsm_running = 1;
                    if (app_eeprom_read_config_data()->io_enable.name.input_4_20ma_0_enable)
                    {
                        DEBUG_VERBOSE("Enable input 4-20ma\r\n");
                        ENABLE_INPUT_4_20MA_POWER(1);
                        measure_input_delay_delay_measure_input_4_20ma(7000);
                    }
                }
            }
        }
    }
	if (jig_timeout_ms)
	{
		jig_timeout_ms--;
	}
	
	if (sys_ctx()->status.timeout_wait_message_sync_data)
	{
		sys_ctx()->status.timeout_wait_message_sync_data--;
	}
  /* USER CODE END SysTick_IRQn 1 */
}

/******************************************************************************/
/* STM32L0xx Peripheral Interrupt Handlers                                    */
/* Add here the Interrupt Handlers for the used peripherals.                  */
/* For the available peripheral interrupt handler names,                      */
/* please refer to the startup file (startup_stm32l0xx.s).                    */
/******************************************************************************/

/**
  * @brief This function handles RTC global interrupt through EXTI lines 17, 19 and 20 and LSE CSS interrupt through EXTI line 19.
  */
void RTC_IRQHandler(void)
{
  /* USER CODE BEGIN RTC_IRQn 0 */
//    DEBUG_VERBOSE("RTC IRQ\r\n");
  /* USER CODE END RTC_IRQn 0 */
  HAL_RTCEx_WakeUpTimerIRQHandler(&hrtc);
  /* USER CODE BEGIN RTC_IRQn 1 */

  /* USER CODE END RTC_IRQn 1 */
}

/**
  * @brief This function handles EXTI line 0 and line 1 interrupts.
  */
void EXTI0_1_IRQHandler(void)
{
  /* USER CODE BEGIN EXTI0_1_IRQn 0 */
  /* USER CODE END EXTI0_1_IRQn 0 */
  if (LL_EXTI_IsActiveFlag_0_31(LL_EXTI_LINE_0) != RESET)
  {
		LL_EXTI_ClearFlag_0_31(LL_EXTI_LINE_0);
		sys_ctx()->status.timeout_wait_message_sync_data = 10000;		// max wait time is 1000
    /* USER CODE BEGIN LL_EXTI_LINE_0 */
        LED1(1);
        led_blink_delay = 5;
		if (gsm_data_layer_is_module_sleeping())
        {
            measure_input_measure_wakeup_to_get_data();
            gsm_set_wakeup_now();
            sys_ctx_t *ctx = sys_ctx();
            ctx->peripheral_running.name.gsm_running = 1;
        }
    /* USER CODE END LL_EXTI_LINE_0 */
  }
  if (LL_EXTI_IsActiveFlag_0_31(LL_EXTI_LINE_1) != RESET)
  {
    LL_EXTI_ClearFlag_0_31(LL_EXTI_LINE_1);
    /* USER CODE BEGIN LL_EXTI_LINE_1 */
        uint32_t current_tick = sys_get_ms();
//        if (current_tick - m_last_exti0_timestamp > (uint32_t)10)
        {
//            m_last_exti0_timestamp = current_tick;
#ifdef DTG01
            measure_input_water_meter_input_t input;
            input.port = MEASURE_INPUT_PORT_1;
            input.pwm_level = LL_GPIO_IsInputPinSet(PWM_GPIO_Port, PWM_Pin) ? 1 : 0;
            input.dir_level = LL_GPIO_IsInputPinSet(DIR0_GPIO_Port, DIR0_Pin) ? 1 : 0;
            input.line_break_detect = LL_GPIO_IsInputPinSet(CIRIN0_GPIO_Port, CIRIN0_Pin);
            input.new_data_type = MEASURE_INPUT_NEW_DATA_TYPE_PWM_PIN;
            measure_input_pulse_irq(&input);
#endif
            sys_ctx()->peripheral_running.name.measure_input_pwm_running = 1;
        }
      LL_EXTI_ClearFlag_0_31(LL_EXTI_LINE_1);
    /* USER CODE END LL_EXTI_LINE_1 */
  }
  /* USER CODE BEGIN EXTI0_1_IRQn 1 */

  /* USER CODE END EXTI0_1_IRQn 1 */
}

/**
  * @brief This function handles EXTI line 2 and line 3 interrupts.
  */
void EXTI2_3_IRQHandler(void)
{
  /* USER CODE BEGIN EXTI2_3_IRQn 0 */
//    DEBUG_VERBOSE("EXT2-3 irq\r\n");
  /* USER CODE END EXTI2_3_IRQn 0 */
  if (LL_EXTI_IsActiveFlag_0_31(LL_EXTI_LINE_3) != RESET)
  {
    LL_EXTI_ClearFlag_0_31(LL_EXTI_LINE_3);
    /* USER CODE BEGIN LL_EXTI_LINE_3 */
#ifdef DTG01
        measure_input_water_meter_input_t input;
        input.port = MEASURE_INPUT_PORT_1;
        input.pwm_level = LL_GPIO_IsInputPinSet(PWM_GPIO_Port, PWM_Pin) ? 1 : 0;
        input.dir_level = LL_GPIO_IsInputPinSet(DIR0_GPIO_Port, DIR0_Pin) ? 1 : 0;
        input.line_break_detect = LL_GPIO_IsInputPinSet(CIRIN0_GPIO_Port, CIRIN0_Pin);
        input.new_data_type = MEASURE_INPUT_NEW_DATA_TYPE_DIR_PIN;
        measure_input_pulse_irq(&input);
#endif
    /* USER CODE END LL_EXTI_LINE_3 */
  }
  /* USER CODE BEGIN EXTI2_3_IRQn 1 */

  /* USER CODE END EXTI2_3_IRQn 1 */
}

/**
  * @brief This function handles DMA1 channel 2 and channel 3 interrupts.
  */
void DMA1_Channel2_3_IRQHandler(void)
{
  /* USER CODE BEGIN DMA1_Channel2_3_IRQn 0 */
	if(LL_DMA_IsActiveFlag_TC2(DMA1))
	{
		LL_DMA_ClearFlag_TC2(DMA1);
		/* Call function Transmission complete Callback */
		usart1_tx_complete_callback(true);
		usart1_start_dma_rx();
	}
	else if(LL_DMA_IsActiveFlag_TE2(DMA1))
	{
		DEBUG_ERROR("USART1 TE2 error\r\n");
		/* Call Error function */
		usart1_tx_complete_callback(false);
	}
  /* USER CODE END DMA1_Channel2_3_IRQn 0 */

  /* USER CODE BEGIN DMA1_Channel2_3_IRQn 1 */
    if (LL_DMA_IsActiveFlag_HT3(DMA1))
    {
//        DEBUG_PRINTF("USART1 HT\r\n");
        LL_DMA_ClearFlag_HT3(DMA1);
		usart1_rx_complete_callback(true);
    }
	if(LL_DMA_IsActiveFlag_TC3(DMA1))
	{
		LL_DMA_ClearFlag_GI3(DMA1);
		/* Call function Reception complete Callback */
		usart1_rx_complete_callback(true);
	}
	else if(LL_DMA_IsActiveFlag_TE3(DMA1))
	{
		/* Call Error function */
        DEBUG_ERROR("USART1 Error\r\n");
		usart1_rx_complete_callback(false);
		// USART_TransferError_Callback();
	}
  /* USER CODE END DMA1_Channel2_3_IRQn 1 */
}

/**
  * @brief This function handles SPI2 global interrupt.
  */
void SPI2_IRQHandler(void)
{
  /* USER CODE BEGIN SPI2_IRQn 0 */

  /* USER CODE END SPI2_IRQn 0 */
  /* USER CODE BEGIN SPI2_IRQn 1 */

  /* USER CODE END SPI2_IRQn 1 */
}

/**
  * @brief This function handles USART1 global interrupt / USART1 wake-up interrupt through EXTI line 25.
  */
void USART1_IRQHandler(void)
{
  /* USER CODE BEGIN USART1_IRQn 0 */
    if (LL_USART_IsEnabledIT_IDLE(USART1) && LL_USART_IsActiveFlag_IDLE(USART1)) 
    {
        LL_USART_ClearFlag_IDLE(USART1);        /* Clear IDLE line flag */
        usart1_rx_complete_callback(true);
    }
    
    if (LL_USART_IsActiveFlag_ORE(USART1))
    {
        DEBUG_PRINTF("GSM UART : Over run\r\n");
        uint32_t tmp = USART1->RDR;
        LL_USART_ClearFlag_ORE(USART1);
    }
    
    if (LL_USART_IsActiveFlag_ORE(USART1))
    {
        DEBUG_PRINTF("Frame error\r\n");
        LL_USART_ClearFlag_FE(USART1);
    }
    
    if (LL_USART_IsActiveFlag_NE(USART1))
    {
        DEBUG_PRINTF("Noise error\r\n");
        LL_USART_ClearFlag_NE(USART1);
    }
  /* USER CODE END USART1_IRQn 0 */
  /* USER CODE BEGIN USART1_IRQn 1 */

  /* USER CODE END USART1_IRQn 1 */
}

/**
  * @brief This function handles AES, RNG and LPUART1 interrupts / LPUART1 wake-up interrupt through EXTI line 28.
  */
void AES_RNG_LPUART1_IRQHandler(void)
{
  /* USER CODE BEGIN AES_RNG_LPUART1_IRQn 0 */
    if (LL_USART_IsActiveFlag_ORE(LPUART1))
    {
        DEBUG_PRINTF("LPUART1 Over run\r\n");
        uint32_t tmp = LPUART1->RDR;
        LL_USART_ClearFlag_ORE(USART1);
    }
    
    if (LL_USART_IsActiveFlag_FE(LPUART1))
    {
        DEBUG_PRINTF("Frame error\r\n");
        LL_USART_ClearFlag_FE(LPUART1);
    }
    
    if (LL_USART_IsActiveFlag_NE(LPUART1))
    {
        DEBUG_PRINTF("Noise error\r\n");
        LL_USART_ClearFlag_NE(LPUART1);
    }
	
	if (LL_USART_IsActiveFlag_RXNE(LPUART1))
	{
		uint32_t data = LPUART1->RDR;
		if (jig_timeout_ms == 0 && sys_ctx()->status.timeout_wait_message_sync_data == 0)
		{
			measure_input_rs485_uart_handler(data);
		}
		else
		{
			jig_uart_insert(data);
		}
	}
	
	if (LL_USART_IsEnabledIT_IDLE(LPUART1) && LL_USART_IsActiveFlag_IDLE(LPUART1)) 
    {
        LL_USART_ClearFlag_IDLE(LPUART1);        /* Clear IDLE line flag */
		measure_input_rs485_idle_detect();
    }
	
  /* USER CODE END AES_RNG_LPUART1_IRQn 0 */

  /* USER CODE BEGIN AES_RNG_LPUART1_IRQn 1 */

  /* USER CODE END AES_RNG_LPUART1_IRQn 1 */
}

/* USER CODE BEGIN 1 */
void EXTI4_15_IRQHandler(void)
{
    if (LL_EXTI_IsActiveFlag_0_31(LL_EXTI_LINE_6) != RESET)
    {
        LL_EXTI_ClearFlag_0_31(LL_EXTI_LINE_6);
        if (LL_GPIO_IsInputPinSet(ADC_24V_GPIO_Port, ADC_24V_Pin))
        {
            DEBUG_INFO("Power up\r\n");
            m_delay_consider_wakeup = 2000;		// When power off from 24V =>> 1 interrupt generated from EXT6 =>> We add delay and read 24V in again
        }
        else
        {
            DEBUG_INFO("Power down\r\n");
            if (measure_input_turn_on_in_4_20ma_power)
            {
                measure_input_turn_on_in_4_20ma_power = 0;
            }
        }
    }
}
/* USER CODE END 1 */
/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/

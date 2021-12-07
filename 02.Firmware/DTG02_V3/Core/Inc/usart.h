/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    usart.h
  * @brief   This file contains all the function prototypes for
  *          the usart.c file
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
#ifndef __USART_H__
#define __USART_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* USER CODE BEGIN Includes */
#include <stdbool.h>
/* USER CODE END Includes */

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

void MX_LPUART1_UART_Init(void);
void MX_USART1_UART_Init(void);

/* USER CODE BEGIN Prototypes */
/**
 * @brief       Send raw data to usart1 port
 * @param[in]   raw Raw data to send
 * @param[in]   length Data length
 */
void uart1_hw_uart_send_raw(uint8_t* raw, uint32_t length);

/**
 * @brief       Control uart1 port
 * @param[in]   enable TRUE - enable uart1 port
                        FALSE - disable uart1 port
 */
void usart1_control(bool enable);

/**
 * @brief       New uart TX callback
 * @param[in]   enable TRUE - Data is valid
                        FALSE - Error orcurs
 */
void usart1_tx_complete_callback(bool status);


/**
 * @brief       New uart RX callback
 * @param[in]   enable TRUE - Data is valid
                        FALSE - Error orcurs
 */
void usart1_rx_complete_callback(bool status);


/**
 * @brief       Start DMA RX
 */
void usart1_start_dma_rx(void);

/**
 * @brief       Enable/Disable lpuart1 for rs485
 * @param[in]   enable TRUE - Enable RS485
                       DISABLE RS485
 */
void usart_lpusart_485_control(bool enable);

/**
 * @brief       Send data to rs485 port
 * @param[in]   data RS485 tx data
 * @param[in]   length RS485 data size
 */
void usart_lpusart_485_send(uint8_t *data, uint32_t length);
/* USER CODE END Prototypes */

#ifdef __cplusplus
}
#endif

#endif /* __USART_H__ */


/**
  ******************************************************************************
  * @file    usart.c
  * @brief   This file provides code for the configuration
  *          of the USART instances.
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

/* Includes ------------------------------------------------------------------*/
#include "usart.h"

/* USER CODE BEGIN 0 */
#include <stdbool.h>
#include "lwrb.h"
#include "app_debug.h"

#define UART1_RX_BUFFER_SIZE    256

static lwrb_t m_ringbuffer_uart1_tx = 
{
    .buff = NULL,
};
static uint8_t m_uart1_tx_buffer[256];
static inline void uart1_hw_uart_rx_raw(uint8_t *data, uint32_t length);
static uint8_t m_uart1_rx_buffer[UART1_RX_BUFFER_SIZE];
/* USER CODE END 0 */

/* LPUART1 init function */

void MX_LPUART1_UART_Init(void)
{

  /* USER CODE BEGIN LPUART1_Init 0 */

  /* USER CODE END LPUART1_Init 0 */

  LL_LPUART_InitTypeDef LPUART_InitStruct = {0};

  LL_GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* Peripheral clock enable */
  LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_LPUART1);

  LL_IOP_GRP1_EnableClock(LL_IOP_GRP1_PERIPH_GPIOB);
  /**LPUART1 GPIO Configuration
  PB10   ------> LPUART1_TX
  PB11   ------> LPUART1_RX
  */
  GPIO_InitStruct.Pin = LL_GPIO_PIN_10;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_ALTERNATE;
  GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
  GPIO_InitStruct.Alternate = LL_GPIO_AF_4;
  LL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = LL_GPIO_PIN_11;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_ALTERNATE;
  GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
  GPIO_InitStruct.Alternate = LL_GPIO_AF_4;
  LL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* LPUART1 interrupt Init */
  NVIC_SetPriority(AES_RNG_LPUART1_IRQn, 0);
  NVIC_EnableIRQ(AES_RNG_LPUART1_IRQn);

  /* USER CODE BEGIN LPUART1_Init 1 */

  /* USER CODE END LPUART1_Init 1 */
  LPUART_InitStruct.BaudRate = 115200;
  LPUART_InitStruct.DataWidth = LL_LPUART_DATAWIDTH_8B;
  LPUART_InitStruct.StopBits = LL_LPUART_STOPBITS_1;
  LPUART_InitStruct.Parity = LL_LPUART_PARITY_NONE;
  LPUART_InitStruct.TransferDirection = LL_LPUART_DIRECTION_TX_RX;
  LPUART_InitStruct.HardwareFlowControl = LL_LPUART_HWCONTROL_NONE;
  LL_LPUART_Init(LPUART1, &LPUART_InitStruct);
  /* USER CODE BEGIN LPUART1_Init 2 */

  /* USER CODE END LPUART1_Init 2 */

}
/* USART1 init function */

void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  LL_USART_InitTypeDef USART_InitStruct = {0};

  LL_GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* Peripheral clock enable */
  LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_USART1);

  LL_IOP_GRP1_EnableClock(LL_IOP_GRP1_PERIPH_GPIOA);
  /**USART1 GPIO Configuration
  PA9   ------> USART1_TX
  PA10   ------> USART1_RX
  */
  GPIO_InitStruct.Pin = GSM_TX_Pin;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_ALTERNATE;
  GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
  GPIO_InitStruct.Alternate = LL_GPIO_AF_4;
  LL_GPIO_Init(GSM_TX_GPIO_Port, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = GSM_RX_Pin;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_ALTERNATE;
  GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
  GPIO_InitStruct.Alternate = LL_GPIO_AF_4;
  LL_GPIO_Init(GSM_RX_GPIO_Port, &GPIO_InitStruct);

  /* USART1 DMA Init */

  /* USART1_TX Init */
  LL_DMA_SetPeriphRequest(DMA1, LL_DMA_CHANNEL_2, LL_DMA_REQUEST_3);

  LL_DMA_SetDataTransferDirection(DMA1, LL_DMA_CHANNEL_2, LL_DMA_DIRECTION_MEMORY_TO_PERIPH);

  LL_DMA_SetChannelPriorityLevel(DMA1, LL_DMA_CHANNEL_2, LL_DMA_PRIORITY_LOW);

  LL_DMA_SetMode(DMA1, LL_DMA_CHANNEL_2, LL_DMA_MODE_NORMAL);

  LL_DMA_SetPeriphIncMode(DMA1, LL_DMA_CHANNEL_2, LL_DMA_PERIPH_NOINCREMENT);

  LL_DMA_SetMemoryIncMode(DMA1, LL_DMA_CHANNEL_2, LL_DMA_MEMORY_INCREMENT);

  LL_DMA_SetPeriphSize(DMA1, LL_DMA_CHANNEL_2, LL_DMA_PDATAALIGN_BYTE);

  LL_DMA_SetMemorySize(DMA1, LL_DMA_CHANNEL_2, LL_DMA_MDATAALIGN_BYTE);

  /* USART1_RX Init */
  LL_DMA_SetPeriphRequest(DMA1, LL_DMA_CHANNEL_3, LL_DMA_REQUEST_3);

  LL_DMA_SetDataTransferDirection(DMA1, LL_DMA_CHANNEL_3, LL_DMA_DIRECTION_PERIPH_TO_MEMORY);

  LL_DMA_SetChannelPriorityLevel(DMA1, LL_DMA_CHANNEL_3, LL_DMA_PRIORITY_LOW);

  LL_DMA_SetMode(DMA1, LL_DMA_CHANNEL_3, LL_DMA_MODE_NORMAL);

  LL_DMA_SetPeriphIncMode(DMA1, LL_DMA_CHANNEL_3, LL_DMA_PERIPH_NOINCREMENT);

  LL_DMA_SetMemoryIncMode(DMA1, LL_DMA_CHANNEL_3, LL_DMA_MEMORY_INCREMENT);

  LL_DMA_SetPeriphSize(DMA1, LL_DMA_CHANNEL_3, LL_DMA_PDATAALIGN_BYTE);

  LL_DMA_SetMemorySize(DMA1, LL_DMA_CHANNEL_3, LL_DMA_MDATAALIGN_BYTE);

  /* USART1 interrupt Init */
  NVIC_SetPriority(USART1_IRQn, 0);
  NVIC_EnableIRQ(USART1_IRQn);

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  USART_InitStruct.BaudRate = 115200;
  USART_InitStruct.DataWidth = LL_USART_DATAWIDTH_8B;
  USART_InitStruct.StopBits = LL_USART_STOPBITS_1;
  USART_InitStruct.Parity = LL_USART_PARITY_NONE;
  USART_InitStruct.TransferDirection = LL_USART_DIRECTION_TX_RX;
  USART_InitStruct.HardwareFlowControl = LL_USART_HWCONTROL_NONE;
  USART_InitStruct.OverSampling = LL_USART_OVERSAMPLING_16;
  LL_USART_Init(USART1, &USART_InitStruct);
  LL_USART_ConfigAsyncMode(USART1);
  LL_USART_Enable(USART1);
  /* USER CODE BEGIN USART1_Init 2 */
  /* Enable DMA transfer complete/error interrupts  */
  
    if (m_ringbuffer_uart1_tx.buff == NULL)
    {
        lwrb_init(&m_ringbuffer_uart1_tx, m_uart1_tx_buffer, sizeof(m_uart1_tx_buffer));
    }
    uart1_hw_uart_rx_raw(m_uart1_rx_buffer, sizeof(m_uart1_rx_buffer));


    LL_DMA_EnableIT_TC(DMA1, LL_DMA_CHANNEL_3);
    LL_DMA_EnableIT_TE(DMA1, LL_DMA_CHANNEL_3);
    LL_DMA_EnableIT_TC(DMA1, LL_DMA_CHANNEL_2);
    LL_DMA_EnableIT_TE(DMA1, LL_DMA_CHANNEL_2);
    LL_USART_EnableIT_IDLE(USART1);
    uart1_hw_uart_rx_raw(m_uart1_rx_buffer, UART1_RX_BUFFER_SIZE);
  /* USER CODE END USART1_Init 2 */

}

/* USER CODE BEGIN 1 */
static inline void config_dma_tx(uint8_t *data, uint32_t len)
{
    LL_DMA_DisableChannel(DMA1, LL_DMA_CHANNEL_2);
    
    LL_DMA_ConfigAddresses(DMA1, LL_DMA_CHANNEL_2,
                         (uint32_t)data,
                         (uint32_t)&(USART1->TDR),
                         LL_DMA_DIRECTION_MEMORY_TO_PERIPH);
    
    LL_DMA_SetDataLength(DMA1, LL_DMA_CHANNEL_2, len);
    LL_DMA_SetPeriphRequest(DMA1, LL_DMA_CHANNEL_2, LL_DMA_REQUEST_4);
    
    /* Enable DMA TX Interrupt */
    LL_USART_EnableDMAReq_TX(USART1);
    LL_DMA_EnableChannel(DMA1, LL_DMA_CHANNEL_2);
}


static volatile bool m_tx_uart_run = false;
volatile uint32_t m_last_transfer_size = 0;
static bool m_uart1_is_enabled = true;
void uart1_control(bool enable)
{	
	if (m_uart1_is_enabled == enable)
	{
		DEBUG_PRINTF("UART state : no changed\r\n");
		return;
	}
	
	m_uart1_is_enabled = enable;
	
	if (!m_uart1_is_enabled)
	{
		while (m_tx_uart_run);
		LL_GPIO_InitTypeDef GPIO_InitStruct = {0};

		GPIO_InitStruct.Pin = GSM_TX_Pin;
		GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
		GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_LOW;
		GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
		LL_GPIO_Init(GSM_TX_GPIO_Port, &GPIO_InitStruct);

		GPIO_InitStruct.Pin = GSM_RX_Pin;
		GPIO_InitStruct.Mode = LL_GPIO_MODE_ANALOG;
		GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_LOW;
		GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
		LL_GPIO_Init(GSM_RX_GPIO_Port, &GPIO_InitStruct);
        
        // TX
		LL_DMA_DisableIT_TC(DMA1, LL_DMA_CHANNEL_2);
		LL_DMA_DisableIT_TE(DMA1, LL_DMA_CHANNEL_2);
        
        // RX
		LL_DMA_DisableIT_TC(DMA1, LL_DMA_CHANNEL_3);
		LL_DMA_DisableIT_TE(DMA1, LL_DMA_CHANNEL_3);
		
		__HAL_RCC_DMA1_CLK_DISABLE();
		NVIC_DisableIRQ(USART1_IRQn);
		LL_USART_Disable(USART1);
		/* Peripheral clock enable */
		LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_USART1);
	}
	else
	{
	    /* DMA controller clock enable */
		__HAL_RCC_DMA1_CLK_ENABLE();
		MX_USART1_UART_Init();
	}
}
static inline void uart1_hw_transmit_dma(void)
{
    if (lwrb_get_full(&m_ringbuffer_uart1_tx) == 0)	// No more data
    {
        m_tx_uart_run = false;
        m_last_transfer_size = 0;
        return;
    }	
	
    uint8_t *addr = lwrb_get_linear_block_read_address(&m_ringbuffer_uart1_tx);
    m_last_transfer_size = lwrb_get_linear_block_read_length(&m_ringbuffer_uart1_tx);
    uint32_t bytes_need_to_transfer =  lwrb_get_full(&m_ringbuffer_uart1_tx);
    
    if (bytes_need_to_transfer < m_last_transfer_size)
    {
        m_last_transfer_size = bytes_need_to_transfer;
    }
    
    NVIC_DisableIRQ(DMA1_Channel2_3_IRQn);
    if (m_tx_uart_run == false)
    {
        m_tx_uart_run = true;
        NVIC_EnableIRQ(DMA1_Channel2_3_IRQn);
        config_dma_tx(addr, m_last_transfer_size);
    }
    else
    {
        NVIC_EnableIRQ(DMA1_Channel2_3_IRQn);
    }
}

void uart_tx_complete_callback(bool status)
{
    m_tx_uart_run = false;
    lwrb_skip(&m_ringbuffer_uart1_tx, m_last_transfer_size);
    uart1_hw_transmit_dma();
}

void uart1_hw_uart_send_raw(uint8_t* raw, uint32_t length)
{
    if (length == 0 || m_uart1_is_enabled == false)
    {
        DEBUG_PRINTF("[%s] Invalid params\r\n", __FUNCTION__);
        return;
    }

    if (lwrb_get_full(&m_ringbuffer_uart1_tx) == 0)
    {
        lwrb_reset(&m_ringbuffer_uart1_tx);
    }

    for (uint32_t i = 0; i < length; i++)
    {
        while (lwrb_write(&m_ringbuffer_uart1_tx, raw + i, 1) == 0)
        {
            DEBUG_PRINTF("UART TX queue full\r\n");
            HAL_Delay(5);
        }
    }
    uart1_hw_transmit_dma();
}

static volatile bool m_uart_rx_ongoing = false;
static inline void uart1_hw_uart_rx_raw(uint8_t *data, uint32_t length)
{
    NVIC_DisableIRQ(DMA1_Channel2_3_IRQn);
    if (!m_uart_rx_ongoing)
    {  
        m_uart_rx_ongoing = true;
        NVIC_EnableIRQ(DMA1_Channel2_3_IRQn);
                
        /* Enable DMA Channel Rx */
        LL_DMA_DisableChannel(DMA1, LL_DMA_CHANNEL_3);
    
        LL_DMA_ConfigAddresses(DMA1, LL_DMA_CHANNEL_3,
                              (uint32_t)&(USART1->RDR),
                             (uint32_t)data,
                             LL_DMA_DIRECTION_PERIPH_TO_MEMORY);
        
        LL_DMA_SetDataLength(DMA1, LL_DMA_CHANNEL_3, length);
        LL_DMA_SetPeriphRequest(DMA1, LL_DMA_CHANNEL_3, LL_DMA_REQUEST_4);
        
        /* Enable DMA RX Interrupt */
        LL_USART_EnableDMAReq_RX(USART1);
        LL_DMA_EnableChannel(DMA1, LL_DMA_CHANNEL_3);
        
    }
    else
    {
        NVIC_EnableIRQ(DMA1_Channel2_3_IRQn);
    }
}



void uart_rx_complete_callback(bool status)
{
    if (status)
    {
        static volatile size_t old_pos;
        size_t pos;

        /* Calculate current position in buffer */
        pos = UART1_RX_BUFFER_SIZE - LL_DMA_GetDataLength(DMA1, LL_DMA_CHANNEL_3);
        if (pos != old_pos) 
        {                       /* Check change in received data */
            if (pos > old_pos) 
            {   /* Current position is over previous one */
                /* We are in "linear" mode */
                /* Process data directly by subtracting "pointers" */
                  DEBUG_RAW("%.*s", pos - old_pos, &m_uart1_rx_buffer[old_pos]);
//                usart_process_data(&usart_rx_dma_buffer[old_pos], pos - old_pos);   // TODO puts data to GSM buffer
            } 
            else 
            {
                /* We are in "overflow" mode */
                /* First process data to the end of buffer */
                /* Check and continue with beginning of buffer */
                if (pos > 0) 
                {
//                    usart_process_data(&usart_rx_dma_buffer[0], pos);
                        DEBUG_RAW("%.*s", pos, &m_uart1_rx_buffer[0]);
                }
            }
            old_pos = pos;                          /* Save current position as old */
        }
//        m_uart_rx_ongoing = false;
    }
    else
    {
        m_uart_rx_ongoing = false;
    }
    uart1_hw_uart_rx_raw(m_uart1_rx_buffer, sizeof(m_uart1_rx_buffer));
}
/* USER CODE END 1 */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/

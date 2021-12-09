/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    spi.h
  * @brief   This file contains all the function prototypes for
  *          the spi.c file
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
#ifndef __SPI_H__
#define __SPI_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* USER CODE BEGIN Private defines */
#define SPI_EXT_FLASH_CS(x)         {   \
                                        if (x)  LL_GPIO_SetOutputPin(EXT_FLASH_CS_GPIO_Port, EXT_FLASH_CS_Pin); \
                                        else    LL_GPIO_ResetOutputPin(EXT_FLASH_CS_GPIO_Port, EXT_FLASH_CS_Pin); \
                                    }
/* USER CODE END Private defines */

void MX_SPI2_Init(void);

/* USER CODE BEGIN Prototypes */
/**
 * @brief       Send data to spi port
 * @retval      RX data
 */
uint8_t spi_flash_transmit(uint8_t ch);

/**
 * @brief       Send data and received data from spi port
 */
void spi_flash_transmit_receive(uint8_t *tx_ptr, uint8_t *rx_ptr, uint32_t size);

/**
 * @brief       Receive data from spi
 * @param[in]   rx_ptr Pointer to RX buffer
 * @param[in]   Buffer size
 */
void spi_flash_receive(uint8_t *rx_ptr, uint32_t size);

/**
 * @brief       Init SPI flash
 */
void spi_init(void);

/**
 * @brief       Deinit spi flash
 */
void spi_deinit(void);

/* USER CODE END Prototypes */

#ifdef __cplusplus
}
#endif

#endif /* __SPI_H__ */


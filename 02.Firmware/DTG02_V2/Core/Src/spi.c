/**
  ******************************************************************************
  * @file    spi.c
  * @brief   This file provides code for the configuration
  *          of the SPI instances.
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
#include "spi.h"

/* USER CODE BEGIN 0 */
#define EXT_FLASH_SPI	SPI2
#define EXT_FLASH_IRQn	SPI2_IRQn
/* USER CODE END 0 */

/* SPI2 init function */
void MX_SPI2_Init(void)
{

  /* USER CODE BEGIN SPI2_Init 0 */

  /* USER CODE END SPI2_Init 0 */

  LL_SPI_InitTypeDef SPI_InitStruct = {0};

  LL_GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* Peripheral clock enable */
  LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_SPI2);

  LL_IOP_GRP1_EnableClock(LL_IOP_GRP1_PERIPH_GPIOC);
  LL_IOP_GRP1_EnableClock(LL_IOP_GRP1_PERIPH_GPIOB);
  /**SPI2 GPIO Configuration
  PC2   ------> SPI2_MISO
  PB13   ------> SPI2_SCK
  PB15   ------> SPI2_MOSI
  */
  GPIO_InitStruct.Pin = EXT_FLASH_MISO_Pin;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_ALTERNATE;
  GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
  GPIO_InitStruct.Alternate = LL_GPIO_AF_2;
  LL_GPIO_Init(EXT_FLASH_MISO_GPIO_Port, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = EXT_FLASH_SCK_Pin;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_ALTERNATE;
  GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
  GPIO_InitStruct.Alternate = LL_GPIO_AF_0;
  LL_GPIO_Init(EXT_FLASH_SCK_GPIO_Port, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = EXT_FLASH_MOSI_Pin;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_ALTERNATE;
  GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
  GPIO_InitStruct.Alternate = LL_GPIO_AF_0;
  LL_GPIO_Init(EXT_FLASH_MOSI_GPIO_Port, &GPIO_InitStruct);

  /* USER CODE BEGIN SPI2_Init 1 */

  /* USER CODE END SPI2_Init 1 */
  SPI_InitStruct.TransferDirection = LL_SPI_FULL_DUPLEX;
  SPI_InitStruct.Mode = LL_SPI_MODE_MASTER;
  SPI_InitStruct.DataWidth = LL_SPI_DATAWIDTH_8BIT;
  SPI_InitStruct.ClockPolarity = LL_SPI_POLARITY_LOW;
  SPI_InitStruct.ClockPhase = LL_SPI_PHASE_1EDGE;
  SPI_InitStruct.NSS = LL_SPI_NSS_SOFT;
  SPI_InitStruct.BaudRate = LL_SPI_BAUDRATEPRESCALER_DIV2;
  SPI_InitStruct.BitOrder = LL_SPI_MSB_FIRST;
  SPI_InitStruct.CRCCalculation = LL_SPI_CRCCALCULATION_DISABLE;
  SPI_InitStruct.CRCPoly = 7;
  LL_SPI_Init(SPI2, &SPI_InitStruct);
  LL_SPI_SetStandard(SPI2, LL_SPI_PROTOCOL_MOTOROLA);
  /* USER CODE BEGIN SPI2_Init 2 */

  /* USER CODE END SPI2_Init 2 */

}

/* USER CODE BEGIN 1 */
uint8_t spi_flash_transmit(uint8_t ch)
{
    LL_SPI_TransmitData8(EXT_FLASH_SPI, ch);
    while (LL_SPI_IsActiveFlag_TXE(EXT_FLASH_SPI) == 0);
    
    
    while (LL_SPI_IsActiveFlag_RXNE(EXT_FLASH_SPI) == 0);
    return LL_SPI_ReceiveData8(EXT_FLASH_SPI);
}

void spi_flash_transmit_receive(uint8_t *tx_ptr, uint8_t *rx_ptr, uint32_t size)
{
    for (uint32_t i = 0; i < size; i++)
    {
        rx_ptr[i] = spi_flash_transmit(tx_ptr[i]);
    }
}

void spi_flash_receive(uint8_t *rx_ptr, uint32_t size)
{
    for (uint32_t i = 0; i < size; i++)
    {
        rx_ptr[i] = spi_flash_transmit(0xFF);
    }
}

void spi_init(void)
{
    MX_SPI2_Init();
}

void spi_deinit(void)
{
#ifdef DTG01
    NVIC_DisableIRQ(EXT_FLASH_IRQn);
    LL_SPI_Disable(EXT_FLASH_SPI);
    LL_SPI_DeInit(EXT_FLASH_SPI);
    LL_APB1_GRP1_DisableClock(LL_APB1_GRP1_PERIPH_SPI2);
    
    LL_GPIO_InitTypeDef GPIO_InitStruct = {0};
    
    GPIO_InitStruct.Pin = LL_GPIO_PIN_2;
    GPIO_InitStruct.Mode = LL_GPIO_MODE_ANALOG;
    GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
    LL_GPIO_Init(GPIOC, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = LL_GPIO_PIN_3;
    GPIO_InitStruct.Mode = LL_GPIO_MODE_ANALOG;
    GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
    GPIO_InitStruct.Alternate = LL_GPIO_AF_2;
    LL_GPIO_Init(GPIOC, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = LL_GPIO_PIN_13;
    GPIO_InitStruct.Mode = LL_GPIO_MODE_ANALOG;
    GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
    LL_GPIO_Init(GPIOB, &GPIO_InitStruct);
#else
    NVIC_DisableIRQ(EXT_FLASH_IRQn);
    LL_SPI_Disable(EXT_FLASH_SPI);
    LL_SPI_DeInit(EXT_FLASH_SPI);
    LL_APB1_GRP1_DisableClock(LL_APB1_GRP1_PERIPH_SPI2);
    
    LL_GPIO_InitTypeDef GPIO_InitStruct = {0};
    
    GPIO_InitStruct.Pin = EXT_FLASH_MISO_Pin;
    GPIO_InitStruct.Mode = LL_GPIO_MODE_ANALOG;
    GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
    LL_GPIO_Init(EXT_FLASH_MISO_GPIO_Port, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = EXT_FLASH_SCK_Pin;
    GPIO_InitStruct.Mode = LL_GPIO_MODE_ANALOG;
    GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
    LL_GPIO_Init(EXT_FLASH_SCK_GPIO_Port, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = EXT_FLASH_MOSI_Pin;
    GPIO_InitStruct.Mode = LL_GPIO_MODE_ANALOG;
    GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
    LL_GPIO_Init(EXT_FLASH_MOSI_GPIO_Port, &GPIO_InitStruct);
#endif
}
/* USER CODE END 1 */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/

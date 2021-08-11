/**
  ******************************************************************************
  * @file    crc.c
  * @brief   This file provides code for the configuration
  *          of the CRC instances.
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
#include "crc.h"

/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/* CRC init function */
void MX_CRC_Init(void)
{

  /* USER CODE BEGIN CRC_Init 0 */

  /* USER CODE END CRC_Init 0 */

  /* Peripheral clock enable */
  LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_CRC);

  /* USER CODE BEGIN CRC_Init 1 */

  /* USER CODE END CRC_Init 1 */
  LL_CRC_SetInputDataReverseMode(CRC, LL_CRC_INDATA_REVERSE_NONE);
  LL_CRC_SetOutputDataReverseMode(CRC, LL_CRC_OUTDATA_REVERSE_NONE);
  LL_CRC_SetPolynomialCoef(CRC, LL_CRC_DEFAULT_CRC32_POLY);
  LL_CRC_SetPolynomialSize(CRC, LL_CRC_POLYLENGTH_32B);
  LL_CRC_SetInitialData(CRC, LL_CRC_DEFAULT_CRC_INITVALUE);
  /* USER CODE BEGIN CRC_Init 2 */

  /* USER CODE END CRC_Init 2 */

}

/* USER CODE BEGIN 1 */

uint32_t crc32_calculate(uint8_t *p_data, uint32_t nb_of_elements)
{
	uint32_t val;
	
	LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_CRC);
	LL_CRC_ResetCRCCalculationUnit(CRC);
	MX_CRC_Init();
	
	register uint32_t tmp = 0;
	register uint32_t index = 0;

	/* Compute the CRC of Data Buffer array*/
	for (index = 0; index < (nb_of_elements / 4); index++)
	{
		tmp = (uint32_t)((p_data[4 * index + 3] << 24) | (p_data[4 * index + 2] << 16) | (p_data[4 * index + 1] << 8) | p_data[4 * index]);
		LL_CRC_FeedData32(CRC, tmp);
	}

	/* Last bytes specific handling */
	uint32_t offset = nb_of_elements % 4;
	if ((nb_of_elements % 4) != 0)
	{
		if  (offset == 1)
		{
			LL_CRC_FeedData8(CRC, p_data[4 * index]);
		}
		else if  (offset == 2)
		{
			LL_CRC_FeedData16(CRC, (uint16_t)((p_data[4 * index + 1]<<8) | p_data[4 * index]));
		}
		else if  (offset == 3)
		{
		  LL_CRC_FeedData16(CRC, (uint16_t)((p_data[4 * index + 1]<<8) | p_data[4 * index]));
		  LL_CRC_FeedData8(CRC, p_data[4 * index + 2]);
		}
	}

	/* Return computed CRC value */
	val = (LL_CRC_ReadData32(CRC));
  
	LL_AHB1_GRP1_DisableClock(LL_AHB1_GRP1_PERIPH_CRC);
	
	return val;
}
/* USER CODE END 1 */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/

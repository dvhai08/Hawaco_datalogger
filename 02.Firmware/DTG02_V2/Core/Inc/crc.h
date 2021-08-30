/**
  ******************************************************************************
  * @file    crc.h
  * @brief   This file contains all the function prototypes for
  *          the crc.c file
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
/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __CRC_H__
#define __CRC_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

void MX_CRC_Init(void);

/* USER CODE BEGIN Prototypes */
/**
 * @brief		Reset and start CRC32 again
 */
void crc32_restart(void);

/**
 * @brief		Stop CRC32
 */
void crc32_stop(void);

/**
 * @brief		Estimate crc32 value
 * @param[in]	data CRC32 data
 * @length		length Data length
 * @retval		CRC32 value
 */
uint32_t crc32_calculate(uint8_t *data, uint32_t length);
/* USER CODE END Prototypes */

#ifdef __cplusplus
}
#endif

#endif /* __CRC_H__ */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/

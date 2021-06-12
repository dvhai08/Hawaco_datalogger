/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __BSP_BOARD_H
#define __BSP_BOARD_H
/* Includes ------------------------------------------------------------------*/
//�������йص�ͷ�ļ�
#include "gd32e10x.h"

#define HAL_OK 0
#define HAL_ERROR 1

extern uint32_t sys_get_ms(void);

#define bsp_board_get_tick() sys_get_ms()

/* USER CODE END Includes */

/* Private define ------------------------------------------------------------*/

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#endif
/********END OF FILE****/

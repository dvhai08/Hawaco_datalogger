/******************************************************************************
 * @file    	HardwareManager.c
 * @author  	Phinht
 * @version 	V1.0.0
 * @date    	05/09/2015
 * @brief   	
 ******************************************************************************/

/******************************************************************************
                                   INCLUDES					    			 
 ******************************************************************************/
#include <stdio.h>
#include <string.h>
#include "Main.h"
#include "DataDefine.h"
#include "hardware.h"
#include "HardwareManager.h"
#include "app_wdt.h"

/******************************************************************************
                                   GLOBAL VARIABLES					    			 
 ******************************************************************************/
extern System_t xSystem;

/******************************************************************************
                                   GLOBAL FUNCTIONS					    			 
 ******************************************************************************/

/******************************************************************************
                                   DATA TYPE DEFINE					    			 
 ******************************************************************************/

/******************************************************************************
                                   PRIVATE VARIABLES					    			 
 ******************************************************************************/
/******************************************************************************
                                   LOCAL FUNCTIONS					    			 
 ******************************************************************************/

/*****************************************************************************/
/**
 * @brief	:  
 * @param	:  
 * @retval	:
 * @author	:	
 * @created	:	05/09/2015
 * @version	:
 * @reviewer:	
 */
void DetectResetReason(void)
{
    xSystem.HardwareInfo.ResetReasion = 0;

    DEBUG_PRINTF("Reset reason\r\n");
#if 0
	if(RCC_GetFlagStatus(RCC_FLAG_PINRST) != RESET)
    {
//        xSystem.HardwareInfo.ResetReasion |= 1;
		DEBUG_PRINTF("PIN, ");
    }

    if(RCC_GetFlagStatus(RCC_FLAG_SFTRST) != RESET)
    {
//        xSystem.HardwareInfo.ResetReasion |= 4;
		DEBUG_PRINTF("SFT, ");
    }

    if(RCC_GetFlagStatus(RCC_FLAG_IWDGRST) != RESET)
    {
//        xSystem.HardwareInfo.ResetReasion |= 8;
		DEBUG_PRINTF("IWD, ");
    }

    if(RCC_GetFlagStatus(RCC_FLAG_WWDGRST) != RESET)
    {
//        xSystem.HardwareInfo.ResetReasion |= 16;
		DEBUG_PRINTF("WWDG, ");
    }

    if(RCC_GetFlagStatus(RCC_FLAG_LPWRRST) != RESET)
    {
//        xSystem.HardwareInfo.ResetReasion |= 32;
		DEBUG_PRINTF("LPWR");
    }		
		
    if(RCC_GetFlagStatus(RCC_FLAG_PORRST) != RESET)
    {
//        xSystem.HardwareInfo.ResetReasion |= 2;
		DEBUG_PRINTF("POR, ");
    }
	RCC_ClearFlag();
#else
    if (RCC_GetFlagStatus(RCU_FLAG_EPRST) != RESET)
    {
        //        xSystem.HardwareInfo.ResetReasion |= 1;
        DEBUG_PRINTF("PIN, ");
    }
    if (RCC_GetFlagStatus(RCU_FLAG_PORRST) != RESET)
    {
        //        xSystem.HardwareInfo.ResetReasion |= 2;
        DEBUG_PRINTF("POR, ");
    }

    if (RCC_GetFlagStatus(RCU_FLAG_SWRST) != RESET)
    {
        //        xSystem.HardwareInfo.ResetReasion |= 4;
        DEBUG_PRINTF("SFT, ");
    }

    if (RCC_GetFlagStatus(RCU_FLAG_FWDGTRST) != RESET)
    {
        //        xSystem.HardwareInfo.ResetReasion |= 8;
        DEBUG_PRINTF("IWD, ");
    }

    if (RCC_GetFlagStatus(RCU_FLAG_WWDGTRST) != RESET)
    {
        //        xSystem.HardwareInfo.ResetReasion |= 16;
        DEBUG_PRINTF("WWDG, ");
    }

    if (RCC_GetFlagStatus(RCU_FLAG_LPRST) != RESET)
    {
        //        xSystem.HardwareInfo.ResetReasion |= 32;
        DEBUG_PRINTF("LPWR");
    }

    /* clear the reset flag */
    rcu_all_reset_flag_clear();

#endif
}

void DetectHardwareVersion(void)
{
}
/*****************************************************************************/
/**
 * @brief	:  
 * @param	:  
 * @retval	:
 * @author	:	Phinht
 * @created	:	15/01/2014
 * @version	:
 * @reviewer:	
 */
/*      
    Ma          Ham goi 
    0   :   ResetSystemManagement 
    1   :   CheckInitDone
    2   :   ProcessCMD
    3   :   SysTick_Handler     : GSMResetCount
    4   :   SysTick_Handler     : MainLoopCount = 120
    5   :   PingProcess         
    6   :   TestHardware
    7   :   ProcessSetConfig    :   CFG_RESET
*/
void SystemReset(uint8_t NguyenNhanReset)
{
    DEBUG_PRINTF("\r\n---- System reset: %d ----\r\n", NguyenNhanReset);
    __disable_irq();
    NVIC_SystemReset();
}

#if 0
void GPIOToggle(GPIO_TypeDef* GPIOx, uint16_t GPIO_Pin)
{
	GPIO_WriteBit(GPIOx, GPIO_Pin, (BitAction)(1 - GPIO_ReadOutputDataBit(GPIOx, GPIO_Pin)));
}
#endif


/*******************************************************************************
 * Function Name	: Hardware_XoaCoLoi
 * Return			: None
 * Parameters		: None
 * Description		: Xoa cac co loi cua USART
*******************************************************************************/
void Hardware_XoaCoLoi(void)
{
    // OverRun Error Flag
    if (USART_GetFlagStatus(USART0, USART_FLAG_ORE) != RESET)
    {
        DEBUG_PRINTF("USART0: ORE set!\r\n");
        USART_ClearFlag(USART0, USART_FLAG_ORE);
    }
    //Framing Error Flag
    if (USART_GetFlagStatus(USART0, USART_FLAG_FE) != RESET)
    {
        DEBUG_PRINTF("USART0: FE set!\r\n");
        USART_ClearFlag(USART0, USART_FLAG_FE);
    }

#if __USE_SENSOR_UART__
    // OverRun Error Flag
    if (USART_GetFlagStatus(USART1, USART_FLAG_ORE) != RESET)
    {
        DEBUG_PRINTF("USART1: ORE set!\r\n");
        USART_ClearFlag(USART1, USART_FLAG_ORE);
    }
    //Framing Error Flag
    if (USART_GetFlagStatus(USART1, USART_FLAG_FE) != RESET)
    {
        DEBUG_PRINTF("USART1: FE set!\r\n");
        USART_ClearFlag(USART1, USART_FLAG_FE);
    }
#endif

    // OverRun Error Flag
    if (USART_GetFlagStatus(USART2, USART_FLAG_ORE) != RESET)
    {
        DEBUG_PRINTF("USART2: ORE set!\r\n");
        USART_ClearFlag(USART2, USART_FLAG_ORE);
    }
    //Framing Error Flag
    if (USART_GetFlagStatus(USART2, USART_FLAG_FE) != RESET)
    {
        DEBUG_PRINTF("USART2: FE set!\r\n");
        USART_ClearFlag(USART2, USART_FLAG_FE);
    }
}

/********************************* END OF FILE *******************************/

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
#include "hardware_manager.h"
#include "app_wdt.h"

/******************************************************************************
                                   GLOBAL VARIABLES					    			 
 ******************************************************************************/
extern System_t xSystem;

static hardware_manager_reset_reason_t m_reset_reason = 
{
    .value = 0,
};

hardware_manager_reset_reason_t *hardware_manager_get_reset_reason(void)
{
    if (m_reset_reason.value == 0)
    {
        DEBUG_PRINTF("Reset reason : ");
    }
#if 0
	if(RCC_GetFlagStatus(RCC_FLAG_PINRST) != RESET)
    {
//        xSystem.HardwareInfo.rst_reason |= 1;
		DEBUG_PRINTF("PIN, ");
    }

    if(RCC_GetFlagStatus(RCC_FLAG_SFTRST) != RESET)
    {
//        xSystem.HardwareInfo.rst_reason |= 4;
		DEBUG_PRINTF("SFT, ");
    }

    if(RCC_GetFlagStatus(RCC_FLAG_IWDGRST) != RESET)
    {
//        xSystem.HardwareInfo.rst_reason |= 8;
		DEBUG_PRINTF("IWD, ");
    }

    if(RCC_GetFlagStatus(RCC_FLAG_WWDGRST) != RESET)
    {
//        xSystem.HardwareInfo.rst_reason |= 16;
		DEBUG_PRINTF("WWDG, ");
    }

    if(RCC_GetFlagStatus(RCC_FLAG_LPWRRST) != RESET)
    {
//        xSystem.HardwareInfo.rst_reason |= 32;
		DEBUG_PRINTF("LPWR");
    }		
		
    if(RCC_GetFlagStatus(RCC_FLAG_PORRST) != RESET)
    {
//        xSystem.HardwareInfo.rst_reason |= 2;
		DEBUG_PRINTF("POR, ");
    }
	RCC_ClearFlag();
#else

    if (RCC_GetFlagStatus(RCU_FLAG_EPRST) != RESET)
    {
        //        xSystem.HardwareInfo.rst_reason |= 1;
        DEBUG_RAW("External pin reset, ");
        m_reset_reason.name.pin_reset = 1;
    }
    if (RCC_GetFlagStatus(RCU_FLAG_PORRST) != RESET)
    {
        //        xSystem.HardwareInfo.rst_reason |= 2;
        DEBUG_RAW("Power on reset, ");
        m_reset_reason.name.power_on = 1;
    }

    if (RCC_GetFlagStatus(RCU_FLAG_SWRST) != RESET)
    {
        //        xSystem.HardwareInfo.rst_reason |= 4;
        DEBUG_RAW("software reset, ");
        m_reset_reason.name.software = 1;
    }

    if (RCC_GetFlagStatus(RCU_FLAG_FWDGTRST) != RESET)
    {
        //        xSystem.HardwareInfo.rst_reason |= 8;
        DEBUG_RAW("IWD watchdog, ");
        m_reset_reason.name.watchdog = 1;
    }

    if (RCC_GetFlagStatus(RCU_FLAG_WWDGTRST) != RESET)
    {
        //        xSystem.HardwareInfo.rst_reason |= 16;
        m_reset_reason.name.watchdog = 1;
        DEBUG_RAW("WWDG watchdog, ");
    }

    if (RCC_GetFlagStatus(RCU_FLAG_LPRST) != RESET)
    {
        //        xSystem.HardwareInfo.rst_reason |= 32;
        m_reset_reason.name.low_power = 1;
        DEBUG_RAW("low power reset");
    }

    DEBUG_RAW("\r\n");

    /* clear the reset flag */
    rcu_all_reset_flag_clear();
#endif
	return &m_reset_reason;
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
void hardware_manager_sys_reset(uint8_t reset_reason)
{
    DEBUG_PRINTF("\r\n---- System reset: %d ----\r\n", reset_reason);
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
 * Function Name	: hardware_manager_clear_uart_error_flag
 * Return			: None
 * Parameters		: None
 * Description		: Xoa cac co loi cua USART
*******************************************************************************/
void hardware_manager_clear_uart_error_flag(void)
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

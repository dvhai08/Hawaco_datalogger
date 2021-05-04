/******************************************************************************
 * @file    	Debug.c
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
#include "DataDefine.h"
#include "Utilities.h"
#include "HardwareManager.h"
#include "Hardware.h"
#include "I2CFM.h"
#include "main.h"


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
#define	TEST_INTERNAL_WDT	2
#define	TEST_EXTERNAL_WDT	3
#define	TEST_REBOOT_CORE	4

/******************************************************************************
                                   PRIVATE VARIABLES					    			 
 ******************************************************************************/
SmallBuffer_t DebugBuffer;
/******************************************************************************
                                   LOCAL FUNCTIONS					    			 
 ******************************************************************************/
static void ProcessNewDebugData(void);

/*****************************************************************************/
/**
 * @brief	:  	Tick every 10ms
 * @param	:  
 * @retval	:
 * @author	:	Phinht
 * @created	:	05/09/2015
 * @version	:
 * @reviewer:	
 */
static void DebugTick(void) 
{     
   if(DebugBuffer.State)
	{
		DebugBuffer.State--;
		if(DebugBuffer.State == 0)
		{
			ProcessNewDebugData();
			DebugBuffer.BufferIndex = 0;
		}
	}
}

/*****************************************************************************/
/**
 * @brief	:  
 * @param	:  
 * @retval	:
 * @author	:	Phinht
 * @created	:	05/09/2015
 * @version	:
 * @reviewer:	
 */
static void DebugInit(void) 
{ 

}

	
/*****************************************************************************/
/**
 * @brief	:  
 * @param	:  
 * @retval	:
 * @author	:	Phinht
 * @created	:	10/03/2016
 * @version	:
 * @reviewer:	
 */
static void ProcessNewDebugData(void)
{	
	uint8_t MaLenh; 
	
	DEBUG_PRINTF("DEBUG: %s\r\n", DebugBuffer.Buffer);
	
	if(strstr((char *)DebugBuffer.Buffer, "TEST,")) 
	{
		MaLenh = GetNumberFromString(5, (char*)DebugBuffer.Buffer);
		switch(MaLenh)
		{			
			case TEST_INTERNAL_WDT:
				DEBUG_PRINTF("Test Internal WDT\r\n");
				xSystem.Status.TestInternalWDT = 1;
				break;
						
			case 14:
				DEBUG_PRINTF("Thuc hien RESET...\r\n");
				NVIC_SystemReset();
				break;
		
//			case 7: 	//TEST ISO output 1
//				DEBUG_PRINTF("Test ISO output1...\r\n");
//				GPIOToggle(ISO_OUT1_PORT, ISO_OUT1_PIN);
//				break;
//			
//			case 8:	//ISO output2
//				DEBUG_PRINTF("Test ISO output2...\r\n");
//				GPIOToggle(ISO_OUT2_PORT, ISO_OUT2_PIN);
//				break;
			
//			case 9:	//TEST LED
//				DEBUG_PRINTF("Test LED1...\r\n");
//				GPIO_Toggle(LED1_PORT, LED1_PIN);
//				break;
//			case 10:	//TEST LED
//				DEBUG_PRINTF("Test LED RED...\r\n");
//				GPIO_Toggle(LED1_RED_PORT, LED1_RED_PIN);
//				GPIO_Toggle(LED2_RED_PORT, LED2_RED_PIN);
//				break;
			
			default:
				break;
		}
	}	
}

/*****************************************************************************/
/**
 * @brief	:  
 * @param	:  
 * @retval	:
 * @author	:	Phinht
 * @created	:	05/09/2015
 * @version	:
 * @reviewer:	
 */
static void DebugAddData(uint8_t Data) 
{ 
	DebugBuffer.Buffer[DebugBuffer.BufferIndex++] = Data;
	
	if(DebugBuffer.BufferIndex == SMALL_BUFFER_SIZE)
		DebugBuffer.BufferIndex = 0;
    DebugBuffer.Buffer[DebugBuffer.BufferIndex] = 0;
    
    DebugBuffer.State = 7;
}

#if 0
/*******************************************************************************
 * Function Name  	: USART1_IRQHandler 
 * Return         	: None
 * Parameters 		: None
 * Created by		: Phinht
 * Date created	: 02/03/2016
 * Description		: This function handles USART1 global interrupt request. 
 * Notes			: 
 *******************************************************************************/
void USART1_IRQHandler(void) 
{		
//	if(USART_GetITStatus(USART1, USART_IT_RXNE) != RESET)
//	{	
//		//Debug
//		USART_ClearITPendingBit(USART1, USART_IT_RXNE);
//		DebugAddData(USART_ReceiveData(USART1));
//	}
}
#endif

//Debug_t DriverDebug = 
//{
//	DebugInit,
//	DebugTick,
//	DebugAddData
//};

/********************************* END OF FILE *******************************/

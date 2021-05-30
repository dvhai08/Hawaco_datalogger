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
#include "gsm_utilities.h"
#include "HardwareManager.h"
#include "hardware.h"
#include "main.h"
#include "Debug.h"
#include "gsm.h"

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
TinyBuffer_t debugBuffer;
char tmpRTTBuff[20] = {0};

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
void Debug_Tick(void) 
{     
   if(debugBuffer.State)
	{
		debugBuffer.State--;
		if(debugBuffer.State == 0)
		{
			ProcessNewDebugData();
			debugBuffer.BufferIndex = 0;
			memset(debugBuffer.Buffer, 0, sizeof(debugBuffer.Buffer));
		}
	}
	
	/** Receive command from RTT - FIFO buffer max size = 15 ki tu -> nhan nhieu lan */
	if (SEGGER_RTT_HASDATA(0))
	{
		memset(tmpRTTBuff, 0, sizeof(tmpRTTBuff));
		uint8_t readByte = SEGGER_RTT_Read(0, tmpRTTBuff, 16);
		if(readByte > 0)
		{
			memcpy(&debugBuffer.Buffer[debugBuffer.BufferIndex], tmpRTTBuff, readByte);
			debugBuffer.BufferIndex += readByte;
			debugBuffer.State = 7;
		}
	}
}

///*****************************************************************************/
///**
// * @brief	:  
// * @param	:  
// * @retval	:
// * @author	:	Phinht
// * @created	:	05/09/2015
// * @version	:
// * @reviewer:	
// */
//static void DebugInit(void) 
//{ 

//}
	
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

	DEBUG_PRINTF("DEBUG: %s\r\n", debugBuffer.Buffer);
	
	if(strstr((char *)debugBuffer.Buffer, "TEST,")) 
	{
		MaLenh = GetNumberFromString(5, (char*)debugBuffer.Buffer);
		switch(MaLenh)
		{			
//			case TEST_INTERNAL_WDT:
//				DEBUG_PRINTF("Test Internal WDT\r\n");
//				xSystem.Status.TestInternalWDT = 1;
//				break;
						
			case 14:
				DEBUG_PRINTF("Thuc hien RESET...\r\n");
				SystemReset(14);
				break;
			case 27:
				xSystem.Status.ADCOut = 1 - xSystem.Status.ADCOut;
				DEBUG_PRINTF("ADC out: %d", xSystem.Status.ADCOut);
				break;
			case 15:
				{	
					uint32_t voltage;
					voltage = GetNumberFromString(8, (char*)debugBuffer.Buffer);
					xSystem.Parameters.output420ma = voltage;
					DEBUG_PRINTF("Set DAC %uMv\r\n", voltage);
				}
				break;
			case 16:
				if (gsm_data_layer_is_module_sleeping())
				{
					xSystem.Status.GSMSleepTime = xSystem.Parameters.TGGTDinhKy*60;
				}
				break;

			case 18:
				if (!gsm_data_layer_is_module_sleeping())
				{
					gsm_set_timeout_to_sleep(1);
				}
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
		return;
	}	
	
	/* Xu ly gui lenh AT */
	if(strstr((char *)debugBuffer.Buffer,"AT"))
	{
		strcat((char *)debugBuffer.Buffer, "\r");
		
#if __HOPQUY_GSM__
		DEBUG_PRINTF(char*)debugBuffer.Buffer);
		com_put_at_string((char*)debugBuffer.Buffer);
#else
		DEBUG ("\r\nLenh AT: %s", debugBuffer.Buffer);
		gsm_process_at_cmd((char*)debugBuffer.Buffer);
#endif
		return;
	}
	if(strstr((char *)debugBuffer.Buffer,"SMS"))
	{
#if __HOPQUY_GSM__
		debugBuffer.Buffer[debugBuffer.BufferIndex++] = 26;
		debugBuffer.Buffer[debugBuffer.BufferIndex++] = 13;
		DEBUG_PRINTF(char*)debugBuffer.Buffer);
		com_put_at_string((char*)debugBuffer.Buffer);
#endif
		return;
	}
}

#if 0
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
	debugBuffer.Buffer[debugBuffer.BufferIndex++] = Data;
	
	if(debugBuffer.BufferIndex == SMALL_BUFFER_SIZE)
		debugBuffer.BufferIndex = 0;
    debugBuffer.Buffer[debugBuffer.BufferIndex] = 0;
    
    debugBuffer.State = 7;
}

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

Debug_t DriverDebug = 
{
	DebugInit,
	DebugTick,
	DebugAddData
};
#endif

/********************************* END OF FILE *******************************/

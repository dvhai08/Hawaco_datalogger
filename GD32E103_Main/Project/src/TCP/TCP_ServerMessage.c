/******************************************************************************
 * @file    	TCP_ServerMessage.c
 * @author  	
 * @version 	V1.0.0
 * @date    	10/11/2014
 * @brief   	
 ******************************************************************************/

/******************************************************************************
                                   INCLUDES					    			 
 ******************************************************************************/
#include <stdio.h> 
#include <string.h>
#include "TCP.h"
#include "DataDefine.h"
#include "GPS.h"
#include "gsm.h"
#include "Configurations.h"
#include "Parameters.h"
#include "gsm_utilities.h"
#include "HardwareManager.h"
#include "UpdateFirmwareFTP.h"

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
static uint8_t SoLanNhanPhanHoi = 0;

/******************************************************************************
                                   LOCAL FUNCTIONS					    			 
 ******************************************************************************/

/*****************************************************************************/
/**
 * @brief	:  
 * @param	:  
 * @retval	:
 * @author	:	
 * @created	:	15/01/2016
 * @version	:
 * @reviewer:	
 */
uint8_t ProcessPacketTCP(uint8_t *Buffer, uint16_t Length)
{
	uint8_t i = 0;

	if(Length > 0)
			DEBUG_PRINTF("\rSRV: packet length: %d", Length);
	else 
		return 0;
		
	DEBUG_PRINTF("\rSRV: Noi dung ban tin: %c", Buffer[0]);
	
	//Gui 2 lan ban tin -> Moi lan gui nhan duoc 2 phan hoi tu SRV ('1' & '0')
	if(SoLanNhanPhanHoi++ >= 3) 
	{
		SoLanNhanPhanHoi = 0;
		
		/* Dong ket noi */
		xSystem.Status.SendGPRSTimeout = 0;
		xSystem.Status.SendGPRSRequest = 0;
		xSystem.Status.ResendGPRSRequest = 0;
		xSystem.Status.TCPNeedToClose = 1;
		xSystem.Status.WaitToCloseTCP = 50;
		
		//Clear Flag LED change state
		if(xSystem.Status.LEDStateChangeFlag) {
			xSystem.Status.LEDStateChangeFlag = 0;
			RTC_WriteBackupRegister(LED_STATE_CHANGE_ADDR, 0);
		}
		
		//Neu dang gui Alarm che do DEN CAU
		if(xSystem.Status.SendAlarmRequest)
			xSystem.Status.SendAlarmRequest = 10;	//timeout 10s truoc khi OFF GSM 
	}
		
	if(Buffer[0] == '1') {
		//Kiem tra xem con ban tin nao trong GPRS Buffer ko
		for(i = 0; i < NUM_OF_GPRS_BUFFER; i++)
		{
			if((xSystem.TCP_Data.GPRS_Buffer[i].State == BUFFER_STATE_IDLE || 
				xSystem.TCP_Data.GPRS_Buffer[i].State == BUFFER_STATE_IDLE2) &&
				xSystem.TCP_Data.GPRS_Buffer[i].BufferIndex > 5) 
			{
				return 0;
			}						
		}
			
//		/* Khong con ban tin trong Buffer */
//		xSystem.Status.SendGPRSTimeout = 0;
//		xSystem.Status.SendGPRSRequest = 0;
//		xSystem.Status.ResendGPRSRequest = 0;
//		xSystem.Status.TCPNeedToClose = 1;
//		xSystem.Status.WaitToCloseTCP = 50;
	}
	else if(Buffer[0] == '0')
	{
		DEBUG_PRINTF("\rSRV: Yeu cau gui lai!");
		xSystem.Status.ResendGPRSRequest = 1;
	}
	
	return 1;
}

/********************************* END OF FILE *******************************/

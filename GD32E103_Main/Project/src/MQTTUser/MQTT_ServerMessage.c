/******************************************************************************
 * @file    	MQTT_ServerMessage.c
 * @author  	Phinht
 * @version 	V1.0.0
 * @date    	20/03/2016
 * @brief   	
 ******************************************************************************/

/******************************************************************************
                                   INCLUDES					    			 
 ******************************************************************************/
#include <string.h>
#include "MQTTUser.h"
#include "UpdateFirmwareFTP.h"
#include "DataDefine.h"
#include "GSM.h"
#include "aes.h"
#include "base64.h"
//#include "Parameters.h"
//#include "Configurations.h"
#include "Utilities.h"
#include "MQTTPacket.h"

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
#define MAXBUFFERTCP		2048
#define LIMITBUFFER			1738	
#define NOTFOUND			65535

#define	VALID_OK			1
#define	VALID_FAILCHECK		0
#define	VALID_NOTCOMPLETE	2
#define	PROCESSOK			1

/******************************************************************************
                                   PRIVATE VARIABLES					    			 
 ******************************************************************************/
uint8_t MessageBufferBase64[MEDIUM_BUFFER_SIZE];
uint8_t MessageBufferAES128[MEDIUM_BUFFER_SIZE];

uint8_t ValidAndDecodePacket(uint8_t* Buffer, uint16_t buffLength)
{
	uint16_t i = 0;
	uint32_t CalcChecksum = 0, MessageChecksum;
	uint16_t Length = 0, AESMessageLength;

	//Debug only
//	DEBUG("\rEncode buffer: %s", Buffer);
	
	// Check header and tail
//	if(Buffer[0] != '[' && Buffer[0] != '{' && Buffer[Length - 1] != '}' && Buffer[Length - 1] != ']') 
//		return VALID_NOTCOMPLETE;
	
	for(i = 0; i < buffLength; i++) 
	{
		if(Buffer[i] == '}' || Buffer[i] == ']') 
		{
			Length = i + 1;
			break;
		}
	}
	
	// Check header and tail
	if(Buffer[0] != '[' && Buffer[0] != '{' && Buffer[Length - 1] != '}' && Buffer[Length - 1] != ']') {
		//NOTE: cu vua gui subscribe thi nhan duoc ban tin: 123bytech.buoymonitoring
		//DEBUG("\rEncode buffer: %s", Buffer);
		return VALID_NOTCOMPLETE;
	}
	
	// Check checksum
	MessageChecksum = GetNumberFromString(Length - 6, (char*)Buffer);
	CalcChecksum = CRC16(&Buffer[1], Length - 7);
	
	if(MessageChecksum != CalcChecksum) return VALID_FAILCHECK;
	
	memset(MessageBufferBase64, 0, sizeof(MessageBufferBase64));
	memset(MessageBufferAES128, 0, sizeof(MessageBufferAES128));
	
	AESMessageLength = b64_decode(&Buffer[1], Length - 7, MessageBufferBase64);
	AES_ECB_decrypt(MessageBufferBase64, (uint8_t*)PUBLIC_KEY, MessageBufferAES128,AESMessageLength);

  return VALID_OK;
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
uint8_t MQTT_ProcessDataFromServer(uint8_t *buffer, uint16_t length)
{
	uint8_t CheckData = 0;
	
	xSystem.Status.GSMSendFailedTimeout = 0;
	
#if 1
	/** TEST: Check ban tin UDFW khong ma hoa */
	if(strstr((char*)buffer, "UDFW,"))
	{
		DEBUG("\rSRV: Ban tin UDFW");
		ProcessUpdateFirmwareCommand((char*)buffer, CF_SERVER);
		return length;
	}
	if(strstr((char*)buffer, "SET,"))
	{
		DEBUG("\rSRV: SET config");
		xSystem.Status.SendGPRSTimeout = 15;
		ProcessSetParameters((char*)buffer, CF_SERVER);
		return length;
	}
	if(strstr((char*)buffer, "GET,"))
	{
		DEBUG("\rSRV: GET config");
		xSystem.Status.SendGPRSTimeout = 15;
		ProcessGetParameters((char*)buffer, CF_SERVER);
		return length;
	}
#endif
	
	/* Kiem tra goi tin */
	CheckData = ValidAndDecodePacket(buffer, length);	

	/* Xu ly truong hop goi tin khong hop le, goi tin chua nhan xong */
	if(CheckData == VALID_NOTCOMPLETE)
	{
		DEBUG("\rERROR.01: Packet sai dinh dang!");
		return VALID_NOTCOMPLETE;
	}
	if(CheckData == VALID_FAILCHECK)
	{
		DEBUG("\rERROR.02: Packet sai checksum!");
		return VALID_FAILCHECK;
	}
	
	xSystem.Status.PingTimeout = 180;
				
	//Debug only
	DEBUG("\rSRV plain: %s", MessageBufferAES128);
	
	// [2018-12-16 10:00:00,S1,1,123] - Phan hoi ban tin !AIVDO,1
	if(strstr((char*)MessageBufferAES128,"!AIVDO,1"))
	{
		xSystem.Status.SendGPRSTimeout = 15;
	}
	
	// [2018-12-16 10:00:00,S2,1,123] -  Phan hoi ban tin !AIVDO,2
	if(strstr((char*)MessageBufferAES128,"!AIVDO,2"))
	{
		xSystem.Status.SendGPRSTimeout = 15;
	}
	
	// [2018-12-16 10:00:00,S3,1,123] -  Phan hoi ban tin !AIVDO,3
	if(strstr((char*)MessageBufferAES128,"!AIVDO,3"))
	{
		xSystem.Status.SendGPRSTimeout = 15;
	}	
	
	// [2018-12-16 10:00:00,S4,1,123] -  Phan hoi ban tin !AIVDO,4
	if(strstr((char*)MessageBufferAES128,"!AIVDO,4"))
	{
		xSystem.Status.SendGPRSTimeout = 15;
	}
	
	// [2018-12-16 10:00:00,S5,1,123] -  Phan hoi ban tin T5
	if(strstr((char*)MessageBufferAES128,"S5"))
	{
		xSystem.Status.SendGPRSTimeout = 15;
	}
	
	// [2018-12-16 10:00:00,S5,1,123] -  Phan hoi ban tin T6
	if(strstr((char*)MessageBufferAES128,"S6"))
	{
		xSystem.Status.SendGPRSTimeout = 15;
	}
	
	// SET,xx,(yy),SET,zz,(tt)...: Ban tin set cau hinh
	if(strstr((char*)MessageBufferAES128,"SET,"))
	{
		xSystem.Status.SendGPRSTimeout = 15;
		ProcessSetParameters((char*)MessageBufferAES128, CF_SERVER);
	}
	// GET,1,2,3,4...: Ban tin get cau hinh
	if(strstr((char*)MessageBufferAES128,"GET,"))
	{
		xSystem.Status.SendGPRSTimeout = 15;
		ProcessGetParameters((char*)MessageBufferAES128, CF_SERVER);
	}
	
	// UDFW,<ftp IP>,<ftp username>,<ftp pass>,<filename>,<Size>,<CRC> - Ban tin update FW
	if(strstr((char*)MessageBufferAES128,"UDFW"))
	{
		DEBUG("\rSRV: Ban tin UDFW");
		ProcessUpdateFirmwareCommand((char*)MessageBufferAES128, CF_SERVER);
	}
	
  return PROCESSOK;
}

/********************************* END OF FILE *******************************/

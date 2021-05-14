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
#include "InternalFlash.h"
#include "Utilities.h"
#include "Hardware.h"
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
 #if 0
uint8_t MessageBufferBase64[MEDIUM_BUFFER_SIZE];
uint8_t MessageBufferAES128[MEDIUM_BUFFER_SIZE];

uint8_t ValidAndDecodePacket(uint8_t* Buffer, uint16_t buffLength)
{
	uint16_t i = 0;
	uint32_t CalcChecksum = 0, MessageChecksum;
	uint16_t Length = 0, AESMessageLength;

	//Debug only
//	DEBUG ("\r\nEncode buffer: %s", Buffer);
	
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
		//DEBUG ("\r\nEncode buffer: %s", Buffer);
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
#endif

/*****************************************************************************/
/**
 * @brief	:  
 * @param	:  SET,Cyclewakeup(x),CycleSendWeb(y),...
 * @retval	:
 * @author	:	Phinht
 * @created	:	15/01/2014
 * @version	:
 * @reviewer:	
 */
void ProcessSetParameters(char *buffer)
{
	uint8_t hasNewConfig = 0;
	
	char *cycleWake = strstr(buffer, "CYCLEWAKEUP(");
	if(cycleWake != NULL)
	{
		uint16_t wakeTime = GetNumberFromString(12, cycleWake);
		if(xSystem.Parameters.TGDoDinhKy != wakeTime) {
			xSystem.Parameters.TGDoDinhKy = wakeTime;
			hasNewConfig++;
		}
	}
	
	char *cycleSend = strstr(buffer, "CYCLESENDWEB(");
	if(cycleSend != NULL)
	{
		uint16_t sendTime = GetNumberFromString(13, cycleSend);
		if(xSystem.Parameters.TGGTDinhKy != sendTime) {
			DEBUG("CYCLESENDWEB changed\r\n");
			xSystem.Parameters.TGGTDinhKy = sendTime;
			hasNewConfig++;
		}
	}

	char *output1 = strstr(buffer, "OUTPUT1(");
	if(output1 != NULL)
	{
		uint8_t out1 = GetNumberFromString(8, output1) & 0x1;
		if(xSystem.Parameters.outputOnOff != out1) {
			xSystem.Parameters.outputOnOff = out1;
			hasNewConfig++;
			DEBUG("Output 1 changed\r\n");
			//Dk ngoai vi luon
			TRAN_OUTPUT(out1);
		}
	}
	
	char *output2 = strstr(buffer, "OUTPUT2(");
	if(output2 != NULL)
	{
		uint8_t out2 = GetNumberFromString(8, output2);
		if(xSystem.Parameters.output420ma != out2) {
			DEBUG("Output 2 changed\r\n");
			xSystem.Parameters.output420ma = out2;
			hasNewConfig++;
			
			//Dk ngoai vi luon
			//TRAN_OUTPUT(out2);
		}
	}
	
	char *input1 = strstr(buffer, "INPUT1(");
	if(input1 != NULL)
	{
		uint8_t in1 = GetNumberFromString(7, input1) & 0x1;
		if(xSystem.Parameters.input.name.pulse != in1) {
			DEBUG("INPUT1 changed\r\n");
			xSystem.Parameters.input.name.pulse = in1;
			hasNewConfig++;
		}
	}	
	
	char *input2 = strstr(buffer, "INPUT2(");
	if(input2 != NULL)
	{
		uint8_t in2 = GetNumberFromString(7, input2) & 0x1;
		if(xSystem.Parameters.input.name.ma420 != in2) {
			DEBUG("INPUT2 changed\r\n");
			xSystem.Parameters.input.name.ma420 = in2;
			hasNewConfig++;
		}
	}	
	
	char *rs485 = strstr(buffer, "RS485(");
	if(rs485 != NULL)
	{
		uint8_t in485 = GetNumberFromString(7, rs485) & 0x1;
		if(xSystem.Parameters.input.name.rs485 != in485) {
			DEBUG("in485 changed\r\n");
			xSystem.Parameters.input.name.rs485 = in485;
			hasNewConfig++;
		}
	}
	
	char *alarm = strstr(buffer, "WARNING(");
	if(alarm != NULL)
	{
		uint8_t alrm = GetNumberFromString(8, alarm) & 0x1;
		if(xSystem.Parameters.alarm != alrm) {
			DEBUG("WARNING changed\r\n");
			xSystem.Parameters.alarm = alrm;
			hasNewConfig++;
		}
	}
	
	char *phoneNum = strstr(buffer, "PHONENUM(");
	if(phoneNum != NULL)
	{
		char tmpPhone[30] = {0};
		if(CopyParameter(phoneNum, tmpPhone, '(', ')'))
		{
			#if 0
			uint8_t changed = 0;
			for(uint8_t i = 0; i < 15; i++)
			{
				if(tmpPhone[i] != xSystem.Parameters.PhoneNumber[i]) {
					changed = 1;
					hasNewConfig ++;
				}
				xSystem.Parameters.PhoneNumber[i] = tmpPhone[i];
			}
			if (changed)
			{
				DEBUG("PHONENUM changed\r\n");
			}
			#endif
		}
	}
	
	//Luu config moi
	if(hasNewConfig)
	{
		InternalFlash_WriteConfig();
	}
	else
	{
		DEBUG ("CFG: has no new config\r\n");
	}
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
uint8_t MQTT_ProcessDataFromServer(char *buffer, uint16_t length)
{	
	xSystem.Status.GSMSendFailedTimeout = 0;
	
	if(strstr(buffer, "SET,") == NULL)
		toUpperCase(buffer);
	
#if 1
	/** TEST: Check ban tin UDFW khong ma hoa */
	if(strstr(buffer, "UDFW,"))
	{
		DEBUG ("\r\nSRV: Ban tin UDFW");
//		ProcessUpdateFirmwareCommand(buffer, 1);
//		return length;
	}
	
	if(strstr(buffer, "SET,") || strstr(buffer,"CYCLEWAKEUP"))
	{
		DEBUG ("\r\nSRV: SET config");
		xSystem.Status.SendGPRSTimeout = 15;
		ProcessSetParameters(buffer);
		GSMSleepAfterSecond(5);
		return length;
	}
//	if(strstr(buffer, "GET,"))
//	{
//		DEBUG ("\r\nSRV: GET config");
//		xSystem.Status.SendGPRSTimeout = 15;
//		ProcessGetParameters((char*)buffer, 1);
//		return length;
//	}
#endif
	
#if 0
	uint8_t CheckData = 0;
	
	/* Kiem tra goi tin */
	CheckData = ValidAndDecodePacket(buffer, length);	

	/* Xu ly truong hop goi tin khong hop le, goi tin chua nhan xong */
	if(CheckData == VALID_NOTCOMPLETE)
	{
		DEBUG ("\r\nERROR.01: Packet sai dinh dang!");
		return VALID_NOTCOMPLETE;
	}
	if(CheckData == VALID_FAILCHECK)
	{
		DEBUG ("\r\nERROR.02: Packet sai checksum!");
		return VALID_FAILCHECK;
	}
	
	xSystem.Status.PingTimeout = 180;
				
	//Debug only
	DEBUG ("\r\nSRV plain: %s", MessageBufferAES128);
	
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
		DEBUG ("\r\nSRV: Ban tin UDFW");
		ProcessUpdateFirmwareCommand((char*)MessageBufferAES128, CF_SERVER);
	}
#endif

  return PROCESSOK;
}

/********************************* END OF FILE *******************************/

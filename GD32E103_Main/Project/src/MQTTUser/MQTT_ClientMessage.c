/******************************************************************************
 * @file    	MQTT_ClientMessage.c
 * @author  	Phinht
 * @version 	V1.0.0
 * @date    	20/03/2016
 * @brief   	
 ******************************************************************************/

/******************************************************************************
                                   INCLUDES					    			 
 ******************************************************************************/
#include "DataDefine.h"

#if ( __USE_MQTT__ == 1)

#include <string.h>
#include "Net_Config.h"
#include "RTL.h"
#include "GSM.h"
#include "Main.h"
//#include "TCP.h"
#include "Utilities.h"
#include "Version.h"
//#include "Parameters.h"
#include "MQTTPacket.h"
#include "MQTTConnect.h"
#include "MQTTUser.h"
#include "HardwareManager.h"
#include "Measurement.h"
#include "aes.h"
#include "base64.h"

/******************************************************************************
                                   GLOBAL VARIABLES					    			 
 ******************************************************************************/
extern System_t xSystem;
extern GSM_Manager_t	GSM_Manager;

extern uint8_t isSubscribed;
extern uint16_t pubPackageId;

extern uint8_t TCPConnectFail;
uint8_t TrangThaiDen = 0;
/******************************************************************************
                                   GLOBAL FUNCTIONS					    			 
 ******************************************************************************/

/******************************************************************************
                                   DATA TYPE DEFINE					    			 
 ******************************************************************************/
#define	GPRS_SEND_TIMEOUT	15

/******************************************************************************
                                   PRIVATE VARIABLES					    			 
 ******************************************************************************/
static uint8_t MqttPubBuff[MEDIUM_BUFFER_SIZE];	
static uint16_t MqttPubBufflen;
LargeBuffer_t mqttBuffer;

//uint8_t AES_Buffer[512];
//uint8_t Base64Buffer[512];
//static char AIVDOBuffer[150];

unsigned char pub_dup = 0;
unsigned char pub_retained = 0;
MQTTString PubTopicString = MQTTString_initializer;

static uint16_t TimeoutDinhKyTick1000ms;

/******************************************************************************
                                   LOCAL FUNCTIONS					    			 
 ******************************************************************************/
static void GuiBanTinDinhKiTick(void);

/*****************************************************************************/
/**
 * @brief	:  	MQTT_ClientOneSecMessageTick, call evey 10ms
 * @param	:  
 * @retval	:
 * @author	:	
 * @created	:	10/03/2016
 * @version	:
 * @reviewer:	
 */
void MQTT_ClientOneSecMessageTick(void)
{
	if(++TimeoutDinhKyTick1000ms >= 100)
	{
		TimeoutDinhKyTick1000ms = 0;
		
		GuiBanTinDinhKiTick();
	}
}

/*****************************************************************************/
/**
 * @brief	:   Ham gui ban tin dinh ky, tick every 1000ms. Moi khi gui message
 * @param	:  set thoi gian 'SendGPRSTimeout' > 0 -> trong HardwareLayer se
 * @retval	: thuc hien ket noi TCP connection va gui tin
 * @author	:	
 * @created	:	15/03/2016
 * @version	:
 * @reviewer:	
 */

static uint16_t send_msg_when_device_wakeup = 0;
void MqttClientSendFirstMessageWhenWakeup(void)
{
	send_msg_when_device_wakeup = 1;
}

volatile uint32_t SendMessageTick = 0;

static void GuiBanTinDinhKiTick(void)
{
#if 1
	static uint8_t SendSubReqTick = 0;
	static uint16_t SendKeepAliveTick = 0;

//	static uint8_t LastMinute = 0xFF;
	uint8_t ThoiGianGuiSubscribe;
	
	//Test 10'
	if(SendMessageTick >= xSystem.Parameters.TGGTDinhKy || send_msg_when_device_wakeup)
	{
		if ((SendMessageTick % 10) == 0)
		{
			DEBUG_PRINTF("SendMessageTick %u-%u, svr %d\r\n", SendMessageTick, xSystem.Parameters.TGGTDinhKy, xSystem.Status.MQTTServerState);
		}
		if (xSystem.Status.MQTTServerState == MQTT_CONNECTED
			|| xSystem.Status.MQTTServerState == MQTT_LOGINED)
		{
			SendMessageTick = 0;
			send_msg_when_device_wakeup = 0;
			MQTT_PublishDataMsg();
		}
	}
#else
	
	/** Kiem tra thoi gian gui tin so voi thoi gian gui tin cai dat, check tung phut */
	if((LastMinute == 0xFF || LastMinute != RTC_GetDateTime().Minute))
	{
		LastMinute = RTC_GetDateTime().Minute;
				
		TimeoutSendDistanceUpdate++;
		if(TimeoutSendDistanceUpdate >= xSystem.Parameters.TGGTDinhKy)
		{
			TimeoutSendDistanceUpdate = 0;
			DataMessage(DATA_DINHKY);
		}
	}
		
	/* YEU CAU GUI BAN TIN DINH KY KHI THUC DAY */
	if(xSystem.Status.YeuCauGuiTin == 3)
	{
		xSystem.Status.YeuCauGuiTin = 0;
		
		//Neu co canh bao nao thi chi gui ban tin canh bao
		if(xSystem.Status.DistanceAlarm == 1)
			DataMessage(DATA_CANHBAOVITRI);
		if(xSystem.Status.LedBulbAlarm == 1)
			DataMessage(DATA_CANHBAODEN);
		if(xSystem.Status.LowBattAlarm == 1)
			DataMessage(DATA_CANHBAOACQUY);
		
		//Neu khong co canh bao nao thi gui Ban tin Dinh ky
		if(!xSystem.Status.DistanceAlarm && !xSystem.Status.LedBulbAlarm && !xSystem.Status.LowBattAlarm)
			DataMessage(DATA_DINHKY);
	}
			
	/** Timeout thoi gian gui ban tin TCP (180s), trong khoang thoi gian gui tin nay
	* Thuc hien gui ban tin dinh ky moi 30s -> Cho phep gui nhieu ban tin hon, dam bao viec den
	* khong bi mat tin hieu (Gui thua con hon thieu!)
	*/
	if(xSystem.Status.SendGPRSTimeout)
	{
		xSystem.Status.SendGPRSTimeout--;
		
		/* Gui lai ban tin moi 15s, neu server khong phan hoi ban tin */
		if(xSystem.Status.SendGPRSTimeout && (xSystem.Status.SendGPRSTimeout % 15 == 0))
		{
			DEBUG ("\r\nMQTT: Gui lai ban tin...");
			DataMessage(xSystem.Status.LastTCPMessageType);
		}
			
		if(xSystem.Status.SendGPRSTimeout == 0)
		{
			DEBUG ("\r\nMQTT: Het thoi gian gui tin!");
			xSystem.Status.TCPNeedToClose = 1;
			xSystem.Status.LastTCPMessageType = 0;
			MQTT_InitBufferQueue();
		}
	}
#endif	//0			

	/** ============== Gui cac ban tin Subscribe/PING khi connected broker =========== */
	if(xSystem.Status.MQTTServerState == MQTT_CONNECTED)
	{
		/** MQTT: Send subscribe request every 5s if not done yet */
		if(!isSubscribed)
			ThoiGianGuiSubscribe = 5;
		else
			ThoiGianGuiSubscribe = 120;
			
		if(SendSubReqTick++ >= ThoiGianGuiSubscribe) {
			SendSubReqTick = 0;
			MQTT_SubscribeNewTopic(TOPIC_SUB_HEADER);
		}
		
		/* Send Keep Alive: PING_REQ */ 
		if(SendKeepAliveTick++ >= MQTT_KEEP_ALIVE_INTERVAL - 10)
		{
			if(MQTT_SendPINGReq()) SendKeepAliveTick = 0;
			else SendKeepAliveTick -= 5;
		}
	}
}

void MQTT_InitBufferQueue(void) 
{ 
	uint8_t i;
	
	DEBUG ("MQTT: init buffer\r\n");
	
	/* Reset cac buffer */
	for(i = 0; i < NUM_OF_MQTT_BUFFER; i++)
	{
		xSystem.MQTTData.Buffer[i].BufferIndex = 0;
		xSystem.MQTTData.Buffer[i].State = BUFFER_STATE_IDLE;
	}
		
	MqttPubBufflen = sizeof(MqttPubBuff);
	xSystem.Status.MQTTServerState = MQTT_INIT;
}

void MQTT_ClearBufferQueue(void)
{
	/* Reset cac buffer */
	for(uint8_t i = 0; i < NUM_OF_MQTT_BUFFER; i++)
	{
		xSystem.MQTTData.Buffer[i].BufferIndex = 0;
		xSystem.MQTTData.Buffer[i].State = BUFFER_STATE_IDLE;
	}
}

/*****************************************************************************/
/**
 * @brief	:  
 * @param	:  
 * @retval	:  Index cua buffer hoac 0xFF neu khong co buffer nao trong
 * @author	:	
 * @created	:	
 * @version	:
 * @reviewer:	
 */
uint8_t CheckMQTTGPRSBufferState(uint16_t RequestLength)
{
	uint8_t i;

	for(i = 0; i < NUM_OF_MQTT_BUFFER; i++)
	{
		if(xSystem.MQTTData.Buffer[i].State != BUFFER_STATE_IDLE) continue;
		if(xSystem.MQTTData.Buffer[i].BufferIndex > 10) continue;
		if((xSystem.MQTTData.Buffer[i].BufferIndex + RequestLength) > MEDIUM_BUFFER_SIZE) continue;
		
		return i;
	}	
	return 0xFF;
}

/*****************************************************************************/
/**
 * @brief	:  	Send buffer message to server
 * @param	:  
 * @retval	:
 * @author	:	
 * @created	:	15/03/2016
 * @version	:
 * @reviewer:	
 */
uint16_t MQTT_SendBufferToServer(char* BufferToSend, char *LoaiBanTin)
{
#if 0
	uint16_t Checksum;
	MediumBuffer_t *pBuffer;
	uint8_t BufferAvailable;
	int16_t TotalLen = 0;
		
	/* Timeout thoi gian gui ban tin TCP, het thoi gian -> close connection */
	if(!xSystem.Status.SendGPRSTimeout)
		xSystem.Status.SendGPRSTimeout = GPRS_SEND_TIMEOUT;	/* 180s */
	
	BufferAvailable = CheckMQTTGPRSBufferState(250);
	if(BufferAvailable == 0xFF || BufferAvailable >= NUM_OF_MQTT_BUFFER)
	{
		DEBUG ("\r\nMQTT_SendBuffer: full buffer!");		
		return 0;
	}
	pBuffer = &(xSystem.MQTTData.Buffer[BufferAvailable]);
	pBuffer->State = BUFFER_STATE_BUSY;
	pBuffer->BufferIndex = 0;
 
	mqttBuffer.BufferIndex = sprintf((char *)mqttBuffer.Buffer,"%s,%s", LoaiBanTin, BufferToSend);
		
	/* Ma hoa AES128: Buffer -> EncryptedBuffer */
	memset(AES_Buffer, 0, sizeof(AES_Buffer));
	memset(Base64Buffer, 0, sizeof(Base64Buffer));
		
	AES_ECB_encrypt(mqttBuffer.Buffer, (uint8_t*)PUBLIC_KEY, AES_Buffer, mqttBuffer.BufferIndex);
	mqttBuffer.BufferIndex = 16 * ((mqttBuffer.BufferIndex / 16) + 1);
	
	/* Ma hoa Base64 */
	b64_encode((char *)AES_Buffer, mqttBuffer.BufferIndex, (char *)Base64Buffer);
	Checksum = CRC16(Base64Buffer, strlen((char*)Base64Buffer));	
	
	/* Add checksum : ban tin Txx -> [] */
	memset(mqttBuffer.Buffer, 0, sizeof(mqttBuffer.Buffer));
	mqttBuffer.BufferIndex = sprintf((char *)mqttBuffer.Buffer, "[%s%05u]", Base64Buffer,Checksum);
	
	/* =================== Build MQTT message =============== */
	if(pubPackageId == 0) pubPackageId = 1;
	
	char pubTopic[50] = {0};
	sprintf(pubTopic, "%s%s", TOPIC_PUB_HEADER, xSystem.Parameters.GSM_IMEI);
	PubTopicString.cstring = pubTopic;
	
	/* Dong goi MQTT */
	TotalLen = MQTTSerialize_publish(MqttPubBuff + TotalLen, MqttPubBufflen - TotalLen, pub_dup, 
		1, pub_retained, pubPackageId++, PubTopicString, mqttBuffer.Buffer, mqttBuffer.BufferIndex);	
		
	/* Copy MQTT packet vao TCP_Buffer */
	memcpy(pBuffer->Buffer, MqttPubBuff, TotalLen);
	pBuffer->BufferIndex = TotalLen;
	pBuffer->State = BUFFER_STATE_IDLE;
			
#if 1	
	DEBUG ("\r\nMQTT: SendBuffer @%u: %u - %s", BufferAvailable, pBuffer->BufferIndex, LoaiBanTin);
#endif
	
	return pBuffer->BufferIndex;
#endif	//0

	return 0;
}

uint16_t MQTT_PublishDataMsg(void)
{
	MediumBuffer_t *pBuffer;
	uint8_t BufferAvailable;
	int16_t TotalLen = 0;
	/* Timeout thoi gian gui ban tin TCP, het thoi gian -> close connection */
	if(!xSystem.Status.SendGPRSTimeout)
		xSystem.Status.SendGPRSTimeout = GPRS_SEND_TIMEOUT;	/* 180s */
	
	BufferAvailable = CheckMQTTGPRSBufferState(250);
	if(BufferAvailable == 0xFF || BufferAvailable >= NUM_OF_MQTT_BUFFER)
	{
		DEBUG ("\r\nMQTT_PublishDebug: full!");		
		return 0;
	}
	pBuffer = &(xSystem.MQTTData.Buffer[BufferAvailable]);
	pBuffer->State = BUFFER_STATE_BUSY;
	pBuffer->BufferIndex = 0;
 
	if(pubPackageId == 0) pubPackageId = 1;
	mqttBuffer.BufferIndex = sprintf((char *)mqttBuffer.Buffer,"{\"Timestamp\":\"%d\",", xSystem.Status.TimeStamp);	//second since 1970
	mqttBuffer.BufferIndex += sprintf((char *)&mqttBuffer.Buffer[mqttBuffer.BufferIndex],"\"ID\":\"%s\",", xSystem.Parameters.GSM_IMEI);
	mqttBuffer.BufferIndex += sprintf((char *)&mqttBuffer.Buffer[mqttBuffer.BufferIndex],"\"PhoneNum\":\"%s\",", xSystem.Parameters.PhoneNumber);
	mqttBuffer.BufferIndex += sprintf((char *)&mqttBuffer.Buffer[mqttBuffer.BufferIndex],"\"Money\":\"%d\",", 0);
	mqttBuffer.BufferIndex += sprintf((char *)&mqttBuffer.Buffer[mqttBuffer.BufferIndex],"\"Input1\":\"%d\",", xSystem.MeasureStatus.PulseCounterInBkup);		//so xung
	mqttBuffer.BufferIndex += sprintf((char *)&mqttBuffer.Buffer[mqttBuffer.BufferIndex],"\"Input2\":\"%.1f\",", xSystem.MeasureStatus.Input420mA);		//dau vao 4-20mA
	mqttBuffer.BufferIndex += sprintf((char *)&mqttBuffer.Buffer[mqttBuffer.BufferIndex],"\"Output1\":\"%d\",", xSystem.Parameters.outputOnOff);		//dau ra on/off
	mqttBuffer.BufferIndex += sprintf((char *)&mqttBuffer.Buffer[mqttBuffer.BufferIndex],"\"Output2\":\"%d\",", xSystem.Parameters.output420ma);		//dau ra 4-20mA	
	mqttBuffer.BufferIndex += sprintf((char *)&mqttBuffer.Buffer[mqttBuffer.BufferIndex],"\"SignalStrength\":\"%d\",", xSystem.Status.CSQ);
	mqttBuffer.BufferIndex += sprintf((char *)&mqttBuffer.Buffer[mqttBuffer.BufferIndex],"\"WarningLevel\":\"%d\",", xSystem.Status.Alarm);
	
	
	mqttBuffer.BufferIndex += sprintf((char *)&mqttBuffer.Buffer[mqttBuffer.BufferIndex],"\"BatteryLevel\":\"%d\",", xSystem.MeasureStatus.batteryPercent);
	mqttBuffer.BufferIndex += sprintf((char *)&mqttBuffer.Buffer[mqttBuffer.BufferIndex],"\"BatteryDebug\":\"%d\"}", xSystem.MeasureStatus.Vin);
//	mqttBuffer.BufferIndex += sprintf((char *)&mqttBuffer.Buffer[mqttBuffer.BufferIndex],"\"Countconnect\":\"%d\"}", pubPackageId);
			
	/* =================== Build MQTT message =============== */
	char pubTopic[50] = {0};
	sprintf(pubTopic, "%s%s", TOPIC_PUB_HEADER, xSystem.Parameters.GSM_IMEI);
	PubTopicString.cstring = pubTopic;
	
	/* Dong goi MQTT */
	TotalLen = MQTTSerialize_publish(MqttPubBuff + TotalLen, MqttPubBufflen - TotalLen, pub_dup, 
		1, pub_retained, pubPackageId++, PubTopicString, mqttBuffer.Buffer, mqttBuffer.BufferIndex);	
		
	/* Copy MQTT packet vao TCP_Buffer */
	memcpy(pBuffer->Buffer, MqttPubBuff, TotalLen);
	pBuffer->BufferIndex = TotalLen;
	pBuffer->State = BUFFER_STATE_IDLE;
			
#if 1
	DEBUG ("MQTT: Publish msg len: %d - %s", pBuffer->BufferIndex, mqttBuffer.Buffer);
#endif
	
	return pBuffer->BufferIndex;
}


/*****************************************************************************/
/**
 * @brief	:  	Send debug message khong ma hoa
 * @param	:  
 * @retval	:
 * @author	:	
 * @created	:	15/03/2016
 * @version	:
 * @reviewer:	
 */
uint16_t MQTT_PublishDebugMsg(char* msgHeader, char* msgBody)
{
	MediumBuffer_t *pBuffer;
	uint8_t BufferAvailable;
	int16_t TotalLen = 0;
		
	/* Timeout thoi gian gui ban tin TCP, het thoi gian -> close connection */
	if(!xSystem.Status.SendGPRSTimeout)
		xSystem.Status.SendGPRSTimeout = GPRS_SEND_TIMEOUT;	/* 180s */
	
	BufferAvailable = CheckMQTTGPRSBufferState(250);
	if(BufferAvailable == 0xFF || BufferAvailable >= NUM_OF_MQTT_BUFFER)
	{
		DEBUG ("\r\nMQTT_PublishDebug: full!");		
		return 0;
	}
	pBuffer = &(xSystem.MQTTData.Buffer[BufferAvailable]);
	pBuffer->State = BUFFER_STATE_BUSY;
	pBuffer->BufferIndex = 0;
 
	mqttBuffer.BufferIndex = sprintf((char *)mqttBuffer.Buffer,"%s: %s", msgHeader, msgBody);
			
	/* =================== Build MQTT message =============== */
	if(pubPackageId == 0) pubPackageId = 1;
	
	char pubTopic[50] = {0};
	sprintf(pubTopic, "%s%s", TOPIC_DEBUG_HEADER, xSystem.Parameters.GSM_IMEI);
	PubTopicString.cstring = pubTopic;
	
	/* Dong goi MQTT */
	TotalLen = MQTTSerialize_publish(MqttPubBuff + TotalLen, MqttPubBufflen - TotalLen, pub_dup, 
		1, pub_retained, pubPackageId++, PubTopicString, mqttBuffer.Buffer, mqttBuffer.BufferIndex);	
		
	/* Copy MQTT packet vao TCP_Buffer */
	memcpy(pBuffer->Buffer, MqttPubBuff, TotalLen);
	pBuffer->BufferIndex = TotalLen;
	pBuffer->State = BUFFER_STATE_IDLE;
			
#if 1	
	DEBUG ("\r\nMQTT: SendDebug: %u - %s", pBuffer->BufferIndex, msgHeader);
#endif
	
	return pBuffer->BufferIndex;
}

#endif	/* __USE_MQTT__ */

/********************************* END OF FILE *******************************/

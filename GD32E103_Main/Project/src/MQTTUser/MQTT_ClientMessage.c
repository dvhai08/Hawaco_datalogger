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
//#include "Measurement.h"
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
#define	GPRS_SEND_TIMEOUT	120

/******************************************************************************
                                   PRIVATE VARIABLES					    			 
 ******************************************************************************/
static uint8_t MqttPubBuff[MEDIUM_BUFFER_SIZE];	
static uint16_t MqttPubBufflen;
LargeBuffer_t mqttBuffer;

uint8_t AES_Buffer[512];
uint8_t Base64Buffer[512];
static char AIVDOBuffer[150];

unsigned char pub_dup = 0;
unsigned char pub_retained = 0;
MQTTString PubTopicString = MQTTString_initializer;

static uint8_t TimeoutAlarmTick1000ms;
static uint8_t TimeoutDinhKyTick1000ms;

static uint16_t Timeout30m_Move = 0;
static uint16_t Timeout360m_Bulb = 0;
static uint16_t Timeout360m_Bat = 0;

static uint16_t TimeoutSendAlarm30m = 0;
static uint16_t TimeoutSendAlarm360m = 0;
static uint16_t TimeoutSendDistanceUpdate = 0;

static uint8_t SendAllConfigTimeout = 0;

/******************************************************************************
                                   LOCAL FUNCTIONS					    			 
 ******************************************************************************/
static void GuiBanTinDinhKiTick(void);

/*****************************************************************************/
/**
 * @brief	:  	MQTT_ClientMessageTick, call evey 10ms
 * @param	:  
 * @retval	:
 * @author	:	
 * @created	:	10/03/2016
 * @version	:
 * @reviewer:	
 */
void MQTT_ClientMessageTick(void)
{
	if(TimeoutDinhKyTick1000ms++ >= 99)
	{
		TimeoutDinhKyTick1000ms = 0;
		
		GuiBanTinDinhKiTick();
	}
}

/*****************************************************************************/
/**
 * @brief	:   Xu ly gui ban tin canh bao trong che do DEN CAU, GSM OFF, call 10ms/lan
 * @param	:  
 * @retval	:
 * @author	:	
 * @created	:	15/03/2016
 * @version	:
 * @reviewer:	
 */
void MQTT_SendAlarmTick(void)
{
	if(TimeoutAlarmTick1000ms++ < 100) return;
	TimeoutAlarmTick1000ms = 0;
		
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
static void GuiBanTinDinhKiTick(void)
{
#if 0
	static uint8_t SendSubReqTick = 0;
	static uint16_t SendKeepAliveTick = 0;
	static uint8_t LastMinute = 0xFF;
	uint8_t ThoiGianGuiSubscribe;
	
	/** Kiem tra thoi gian gui tin so voi thoi gian gui tin cai dat, check tung phut */
	if((LastMinute == 0xFF || LastMinute != RTC_GetDateTime().Minute))
	{
		LastMinute = RTC_GetDateTime().Minute;
		
		char TimeStamp[5] = {0};
		sprintf(TimeStamp, "%02u%02u", RTC_GetDateTime().Hour, LastMinute);
		
		TimeoutSendDistanceUpdate++;
		if(strstr(xSystem.Parameters.TimeSend, TimeStamp) != NULL ||
			TimeoutSendDistanceUpdate >= xSystem.Parameters.TGGTDinhKy)
		{
			TimeoutSendDistanceUpdate = 0;
			DataMessage(DATA_DINHKY);
		}
	}
	
	/* YEU CAU GUI BAN TIN TUC THI */
	if(xSystem.Status.YeuCauGuiTin == 1)
	{
		/* Neu den dang o trang thai ON 
			-> Doi do duoc dong tieu thu Den moi gui tin
			-> Doi lay duoc toa do GPS thi gui tin, hoac qua 5 phut k lay duoc toa do thi cung gui
			-> Hoac qua 60s ma khong do duoc dong tieu thu thi van phai gui ban tin thay doi trang thai
		*/
		if(TrangThaiDen == 1)	//STATE_ON
		{
			if((xSystem.MeasureStatus.Current > 0 && GPS_IsValid()) || xSystem.MeasureStatus.operatingTime > 300) {
				xSystem.Status.YeuCauGuiTin = 0;
				DataMessage(DATA_DINHKY);
			}
		} else {
			/* Dong tieu thu den = 0 khi o trang thai OFF */
			xSystem.Status.YeuCauGuiTin = 0;
			xSystem.MeasureStatus.Current = 0;
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
			DEBUG("\rMQTT: Gui lai ban tin...");
			DataMessage(xSystem.Status.LastTCPMessageType);
		}
			
		if(xSystem.Status.SendGPRSTimeout == 0)
		{
			DEBUG("\rMQTT: Het thoi gian gui tin!");
			xSystem.Status.TCPNeedToClose = 1;
			xSystem.Status.LastTCPMessageType = 0;
			MQTT_InitBufferQueue();
		}
	}
	
	
	/* Gui ban tin canh bao troi phao moi 30 phut */
	/* Edited 05-Sep: Chi gui ban tin canh bao 30 phut cho DistanceAlarm*/
	if(xSystem.Status.DistanceAlarm == 1)
	{
		if((Timeout30m_Move == 0) || (Timeout30m_Move >= 1800 && xSystem.Status.Alarm30Min == 0) || 
			(Timeout30m_Move >= 21000 && xSystem.Status.Alarm30Min == 1))
		{
			if(Timeout30m_Move) Timeout30m_Move = 1;
			
			if(xSystem.Status.DistanceAlarm == 1)
				DataMessage(DATA_CANHBAOVITRI);
		}
		Timeout30m_Move++;
	} else 
		Timeout30m_Move = 0;
	
	/** Gui ban tin canh bao den 6 tieng 1 lan */
	if(xSystem.Status.LedBulbAlarm == 1)
	{
		if(Timeout360m_Bulb == 0 || Timeout360m_Bulb >= 21600)	//6 tieng
		{
			if(Timeout360m_Bulb) Timeout360m_Bulb = 1;
			
			if(xSystem.Status.LedBulbAlarm == 1)
				DataMessage(DATA_CANHBAODEN);
		}
		Timeout360m_Bulb++;
	} else 
		Timeout360m_Bulb = 0;	
	
	/** Gui ban tin canh bao acquy 6 tieng 1 lan */
	if(xSystem.Status.LowBattAlarm == 1)
	{
		if(Timeout360m_Bat == 0 || Timeout360m_Bat >= 21600)	//6 tieng
		{
			if(Timeout360m_Bat) Timeout360m_Bat = 1;
						
			if(xSystem.Status.LowBattAlarm == 1)
				DataMessage(DATA_CANHBAOACQUY);
		}
		Timeout360m_Bat++;
	} else
		Timeout360m_Bat = 0;	
			

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
	
#if 1
		/** Sau khi connected thi gui 1 ban tin thong so cau hinh */
		if(SendAllConfigTimeout < 30)
		{
			SendAllConfigTimeout++;
			if(SendAllConfigTimeout == 5)
			{
				DataMessage(DATA_TRANGTHAI);
			}
//			if(SendAllConfigTimeout == 8)
//			{
//				DataMessage(DATA_CAUHINH);
//			}
			if(SendAllConfigTimeout == 11)
			{
				//Neu chua duoc cau hinh ID -> gui 1 ban tin debug
				if(strstr(xSystem.Parameters.DeviceID, "LOR4G.ID"))
				{
					memset(AIVDOBuffer, 0, sizeof(AIVDOBuffer));
					sprintf(AIVDOBuffer, "%s:%s:%s", xSystem.Parameters.GSM_IMEI, 
						xSystem.Parameters.SIM_IMEI, xSystem.Parameters.DeviceID);
					MQTT_PublishDebugMsg("IMEI", AIVDOBuffer);
				}
			}
		}
#endif
	}
	else
		SendAllConfigTimeout = 0;
#endif	//0
}

void MQTT_InitBufferQueue(void) 
{ 
	uint8_t i;
	
	DEBUG("\rMQTT: init buffer");
	
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


#if __USE_NEW_MSG_FORMAT__
/*****************************************************************************/
/**
 * @brief	:  	Cac ban tin gui len Server qua GPRS theo cau truc moi
 * @param	:  
 * @retval	:
 * @author	:	
 * @created	:	15/03/2016
 * @version	:
 * @reviewer:	
 */
uint16_t DataMessage(uint8_t LoaiBanTin)
{
	MediumBuffer_t *pBuffer;
	uint8_t BufferAvailable;
	int16_t TotalLen = 0;
	uint16_t Checksum;
		
	/* Timeout thoi gian gui ban tin TCP, het thoi gian -> close connection */
	if(!xSystem.Status.SendGPRSTimeout)
		xSystem.Status.SendGPRSTimeout = GPRS_SEND_TIMEOUT;	/* 180s */
	
	BufferAvailable = CheckMQTTGPRSBufferState(250);
	if(BufferAvailable == 0xFF || BufferAvailable >= NUM_OF_MQTT_BUFFER)
	{
		DEBUG("\rMQTT_DataMessage: full buffer!");
		return 0;
	}
	pBuffer = &(xSystem.MQTTData.Buffer[BufferAvailable]);
	pBuffer->State = BUFFER_STATE_BUSY;
	pBuffer->BufferIndex = 0;
	
	/** =========================== BAN TIN DINH KY ================================*/
	if(LoaiBanTin == DATA_DINHKY)		/* T1 */
	{
		/* Dien ap acquy - Volt */
		float DienApAcQuy = (float)GetBatteryPower()/1000;
		
		/* Trang thai thiet bi */
		DeviceStatus_t DeviceStatus;
		DeviceStatus.Value = 0;
		
		if(xSystem.Status.DistanceAlarm) DeviceStatus.Name.DistanceAlarm = 1;
		if(xSystem.Status.LedBulbAlarm) DeviceStatus.Name.LedBulbAlarm = 1;
		if(xSystem.Status.LowBattAlarm) DeviceStatus.Name.LowBattAlarm = 1;
		if(xSystem.Status.VminAlarm) DeviceStatus.Name.VminAlarm = 1;
		if(xSystem.Status.Alarm30Min) DeviceStatus.Name.Alarm30Min = 1;
		DeviceStatus.Name.FlashState = xSystem.Status.FlashState;
		
		/** Dinh dang ban tin du lieu
		* MessageID, DeviceID, ViDo, KinhDo, Dien ap Acquy (V), Dong tieu thu (mA), Ma kieu chop, Trang thai thiet bi,	// Cac thong so chinh //
		* CSQ,4G Techno state,HW Reset reason,SW Reset reason
		*/	
		/* Loai ban tin */
		mqttBuffer.BufferIndex = sprintf((char *)mqttBuffer.Buffer,"T%u,%s,%.4f,%.4f,%4.2f,%u,%u,%u,%u,%u,%u,%u", 
			LoaiBanTin, xSystem.Parameters.DeviceID,
			GPS_GetPosition().ViDo.value, GPS_GetPosition().KinhDo.value,
			DienApAcQuy, xSystem.MeasureStatus.Current,
			Indicator_GetUserBlinkCode(xSystem.Parameters.BlinkType),
			DeviceStatus.Value,
			xSystem.Status.CSQ, GSM_Manager.AccessTechnology,
			xSystem.HardwareInfo.HWResetReason, xSystem.HardwareInfo.SWResetReason);
	}
	/** =========================== BAN TIN CAU HINH ================================ */
	else if(LoaiBanTin == DATA_CAUHINH)		/* T5 */
	{
		/** Dinh dang ban tin cau hinh: 
		* MessageID,DeviceID,FWVersion,IMEI,Che do den cau,Thoi gian bat GSM ban ngay,Thoi gian bat GSM ban dem,
		* Thoi gian gui tin dinh ky,LED duty,Che do GPS,Thoi gian DEN bat toi da,Canh bao roi vi tri,Vi tri cai dat (lat:lng)
		*/
		mqttBuffer.BufferIndex = sprintf((char *)mqttBuffer.Buffer,"T%u,%s,%s%02u,%s,%u,%u,%u,%s,%u,%u,%u,%u,%.4f:%.4f",
			LoaiBanTin, xSystem.Parameters.DeviceID, FIRMWARE_VERSION_HEADER, FIRMWARE_VERSION_CODE, xSystem.Parameters.GSM_IMEI,
			xSystem.Parameters.DenCauMode, xSystem.Parameters.DenCauGSMTimeOn2Off, xSystem.Parameters.DenCauGSMTimeOff2On,
			xSystem.Parameters.TimeSend, xSystem.Parameters.LEDDuty, xSystem.Parameters.GPSMode, xSystem.Parameters.OptimeLimit,
			xSystem.Parameters.MoveAlarmEnable, xSystem.Parameters.Position.ViDo.value, xSystem.Parameters.Position.KinhDo.value);	/* Canh bao roi vi tri */
	}
		
#if 1		/** Ban tin su dung ma hoa kep */
	/* Ma hoa AES128: Buffer -> EncryptedBuffer */
	memset(AES_Buffer, 0, sizeof(AES_Buffer));
	memset(Base64Buffer, 0, sizeof(Base64Buffer));
		
	AES_ECB_encrypt(mqttBuffer.Buffer, (uint8_t*)PUBLIC_KEY, AES_Buffer, mqttBuffer.BufferIndex);
	mqttBuffer.BufferIndex = 16 * ((mqttBuffer.BufferIndex / 16) + 1);
	
	/* Ma hoa Base64 */
	b64_encode((char *)AES_Buffer, mqttBuffer.BufferIndex, (char *)Base64Buffer);
	Checksum = CRC16(Base64Buffer, strlen((char*)Base64Buffer));	
	
	/* Add checksum & header/footer [] */
	memset(mqttBuffer.Buffer, 0, sizeof(mqttBuffer.Buffer));
	mqttBuffer.BufferIndex = sprintf((char *)mqttBuffer.Buffer, "[%s%05u]", Base64Buffer, Checksum);
#endif 

	/* =================== Build MQTT message ===============*/
	if(pubPackageId == 0) pubPackageId = 1;
	
	char pubTopic[50] = {0};
	sprintf(pubTopic, "%s%s", TOPIC_PUB_HEADER, xSystem.Parameters.GSM_IMEI);
	PubTopicString.cstring = pubTopic;
	
	/* Dong goi MQTT */
	TotalLen = MQTTSerialize_publish(MqttPubBuff + TotalLen, MqttPubBufflen - TotalLen, pub_dup, 
		xSystem.Parameters.PubQos, pub_retained, pubPackageId++, PubTopicString, mqttBuffer.Buffer, mqttBuffer.BufferIndex);	
		
	/* Copy MQTT packet vao TCP_Buffer */
	memcpy(pBuffer->Buffer, MqttPubBuff, TotalLen);
	pBuffer->BufferIndex = TotalLen;
	pBuffer->State = BUFFER_STATE_IDLE;
		
#if 1	
	DEBUG("\rMQTT: Ban tin %u, length %u", LoaiBanTin, pBuffer->BufferIndex);
#endif
	
	return pBuffer->BufferIndex;	
}
#endif	//__USE_NEW_MSG_FORMAT__


/**
* Ban tin dinh ky khoang cach
* !AIVDO,ID,DeviceId,Lat,Lng,Vacquy,Dong dien,Khoang cach,Ton,Toff
*/
static uint8_t BuildPacketSendServer(uint8_t LoaiBanTin)
{
	uint8_t ViTri, ViTriTinhCheckSum = 0, CheckSum = 0;
//	float DienApAcQuy = 0;
//	
//	memset(AIVDOBuffer, 0, sizeof(AIVDOBuffer));
//	ViTri = 0;
//	
//	/* Add header message */
//	ViTri += sprintf(&AIVDOBuffer[ViTri], "!AIVDO,");
//	
//	ViTriTinhCheckSum = ViTri;
//	
//	/* Loai ban tin */
//	ViTri += sprintf(&AIVDOBuffer[ViTri], "%d,", LoaiBanTin);
//	
//	/* Ma thiet bi */
//	ViTri += sprintf(&AIVDOBuffer[ViTri], "%s,", xSystem.Parameters.DeviceID);
//	
//	/* Trong cac truong hop sau khong gui truong toa do ve:
//		- GPS mode = 0
//		- Khong co toa do
//		- Khong bat GPS
//	*/
//	if(LoaiBanTin == DATA_DINHKY || LoaiBanTin == DATA_CANHBAOVITRI) {
//		if(GPS_GetPosition().ViDo.value != 0 && GPS_GetPosition().KinhDo.value != 0
//			&& xSystem.Parameters.GPSMode != 0)
//		{
//			/* Vi Do */
//			ViTri += sprintf(&AIVDOBuffer[ViTri], "%f", GPS_GetPosition().ViDo.value);	//9.4lf
//		}
//	}

//	ViTri += sprintf(&AIVDOBuffer[ViTri], ",");

//	if(LoaiBanTin == DATA_DINHKY || LoaiBanTin == DATA_CANHBAOVITRI) {
//		if(GPS_GetPosition().ViDo.value != 0 && GPS_GetPosition().KinhDo.value != 0
//			&& xSystem.Parameters.GPSMode != 0)
//		{
//			/* Kinh Do */
//			ViTri += sprintf(&AIVDOBuffer[ViTri], "%f", GPS_GetPosition().KinhDo.value); //10.4lf
//		}
//	}
//	
//	ViTri += sprintf(&AIVDOBuffer[ViTri], ",");

//	if(LoaiBanTin == DATA_DINHKY || LoaiBanTin == DATA_CANHBAOACQUY) {
//		/* Dien ap Solar - Vin */		
//		DienApAcQuy = (float)xSystem.MeasureStatus.Vin/1000;
//		ViTri += sprintf(&AIVDOBuffer[ViTri], "%4.2f", DienApAcQuy);		//5.2f
//	}
//	
//	ViTri += sprintf(&AIVDOBuffer[ViTri], ",");
//	
//	if(LoaiBanTin == DATA_DINHKY || LoaiBanTin == DATA_CANHBAODEN) {
//		/* Dong tieu thu den - mA*/
//		ViTri += sprintf(&AIVDOBuffer[ViTri], "%u", xSystem.MeasureStatus.Current);
//	}

//	ViTri += sprintf(&AIVDOBuffer[ViTri], ",");
//	
//	/* Khoang cach do duoc: Uu tien do laser hon */
//	if(xSystem.MeasureStatus.DistanceLaser > 0)
//		ViTri += sprintf(&AIVDOBuffer[ViTri], "%u", xSystem.MeasureStatus.DistanceLaser);
//	else
//		ViTri += sprintf(&AIVDOBuffer[ViTri], "%u", xSystem.MeasureStatus.DistanceUltra);
//	
//	ViTri += sprintf(&AIVDOBuffer[ViTri], ",");
//	
//	/* Thoi gian chop den: Ton, Toff */
//	ViTri += sprintf(&AIVDOBuffer[ViTri], "%u,%u", xSystem.Parameters.ThoiGianLed7Sang, xSystem.Parameters.ThoiGianLed7Tat);	
//	
//	/* Tinh checksum XOR */
//	CheckSum = CalculateCheckSum((uint8_t*)AIVDOBuffer, ViTriTinhCheckSum, ViTri - ViTriTinhCheckSum) & 0xFF;
//		
//	/* Add tail & checksum */
//	ViTri += sprintf(&AIVDOBuffer[ViTri], "*%02X", CheckSum);
	
	return ViTri;
}

/*****************************************************************************/
/**
 * @brief	:  	Cac ban tin gui len Server qua GPRS, van dung header !AIVDO
 * @param	:  
 * @retval	:
 * @author	:	
 * @created	:	15/03/2016
 * @version	:
 * @reviewer:	
 */
uint16_t DataMessage(uint8_t LoaiBanTin)
{
#if 0
	MediumBuffer_t *pBuffer;
	uint8_t BufferAvailable;
	int16_t TotalLen = 0;
	uint16_t Checksum;
	
	/* Neu GSM dang sleep -> wakeup truoc */
	if(GSM_Manager.State == GSM_SLEEP) {
		DEBUG("\rGSM dang sleep, wakeup...");
		MQTT_ClearBufferQueue();
		ChangeGSMState(GSM_WAKEUP);
	}
			
	/* Timeout thoi gian gui ban tin TCP, het thoi gian -> close connection */
	if(!xSystem.Status.SendGPRSTimeout)
		xSystem.Status.SendGPRSTimeout = GPRS_SEND_TIMEOUT;	/* 180s */
	
	BufferAvailable = CheckMQTTGPRSBufferState(250);
	if(BufferAvailable == 0xFF || BufferAvailable >= NUM_OF_MQTT_BUFFER)
	{
		DEBUG("\rMQTT_DataMessage: full buffer!");
		BufferAvailable = 0;
//		return 0;
	}
	pBuffer = &(xSystem.MQTTData.Buffer[BufferAvailable]);
	pBuffer->State = BUFFER_STATE_BUSY;
	pBuffer->BufferIndex = 0;
	
	
	/** =========================== BAN TIN !AIVDO ================================*/
	if(LoaiBanTin == DATA_DINHKY || LoaiBanTin == DATA_CANHBAOACQUY || 
		LoaiBanTin == DATA_CANHBAODEN || LoaiBanTin == DATA_CANHBAOVITRI)		/* !AIVDO,1,2,3,4,7 */
	{
		//Den tat -> dong tieu thu = 0
		if(TrangThaiDen != 1)
			xSystem.MeasureStatus.Current = 0;
		
		/* Build body ban tin theo format cu !AIVDO */
		uint8_t aivdoBufferLeng = BuildPacketSendServer(LoaiBanTin);
		
		DEBUG("\rSend message: %u - %s", LoaiBanTin, AIVDOBuffer);
		
		/* Copy Packet Body = !AIVDO message */
		memset(mqttBuffer.Buffer, 0, sizeof(mqttBuffer.Buffer));
		memcpy(mqttBuffer.Buffer, AIVDOBuffer, aivdoBufferLeng);
		mqttBuffer.BufferIndex = aivdoBufferLeng;
		
		/* Nho loai ban tin de gui lai neu gui khong nhan duoc phan hoi */
		xSystem.Status.LastTCPMessageType = LoaiBanTin;
	}
	/** =========================== BAN TIN CAU HINH ================================ */
	else if(LoaiBanTin == DATA_CAUHINH)		/* T5 */
	{
		/** Dinh dang ban tin cau hinh: 
		* MessageID,DeviceID,FWVersion,IMEI,Che do den cau,Thoi gian bat GSM ban ngay,Thoi gian bat GSM ban dem,
		* Thoi gian gui tin dinh ky,LED duty,Che do GPS,Thoi gian DEN bat toi da,Canh bao roi vi tri,Vi tri cai dat (lat:lng)
		*/
		mqttBuffer.BufferIndex = sprintf((char *)mqttBuffer.Buffer,"T%u,%s,%s%02u,%s,%u,%u,%s,%u,%u,%u,%u,%u,%u,%.4f:%.4f",
			LoaiBanTin, xSystem.Parameters.DeviceID, FIRMWARE_VERSION_HEADER, FIRMWARE_VERSION_CODE, xSystem.Parameters.GSM_IMEI,
			xSystem.Parameters.DenCauMode, /*xSystem.Parameters.DenCauGSMTimeOn2Off, xSystem.Parameters.DenCauGSMTimeOff2On,*/
			xSystem.Parameters.TGGTDinhKy, xSystem.Parameters.TimeSend,
			xSystem.Parameters.LEDDuty, xSystem.Parameters.GPSMode, xSystem.Parameters.OptimeLimit,
			xSystem.Parameters.AccuAlarmEnable, xSystem.Parameters.LEDAlarmEnable, xSystem.Parameters.MoveAlarmEnable, 
			xSystem.Parameters.Position.ViDo.value, xSystem.Parameters.Position.KinhDo.value);	/* Canh bao roi vi tri */
	}
	/** =========================== BAN TIN TRANG THAI ================================ */
	else if(LoaiBanTin == DATA_TRANGTHAI)		/* T6 */
	{
		/* Dien ap acquy - Volt */
		float DienApAcQuy = (float)GetBatteryPower()/1000;
		
		/* Trang thai thiet bi */
		DeviceStatus_t DeviceStatus;
		DeviceStatus.Value = 0;
		
		if(xSystem.Status.DistanceAlarm) DeviceStatus.Name.DistanceAlarm = 1;
		if(xSystem.Status.LedBulbAlarm) DeviceStatus.Name.LedBulbAlarm = 1;
		if(xSystem.Status.LowBattAlarm) DeviceStatus.Name.LowBattAlarm = 1;
		if(xSystem.Status.VminAlarm) DeviceStatus.Name.VminAlarm = 1;
		if(xSystem.Status.Alarm30Min) DeviceStatus.Name.Alarm30Min = 1;
		DeviceStatus.Name.FlashState = xSystem.Status.FlashState;
		
		/** Dinh dang ban tin du lieu
		* MessageID, DeviceID, ViDo, KinhDo, Dien ap Acquy (V), Dong tieu thu (mA), Ma kieu chop, Trang thai thiet bi,	// Cac thong so chinh //
		* CSQ,4G Techno state,HW Reset reason,SW Reset reason,FWVersion
		*/	
		/* Loai ban tin */
		mqttBuffer.BufferIndex = sprintf((char *)mqttBuffer.Buffer,"T%u,%s,%.f,%.f,%4.2f,%u,%u,%u,%u,%u,%u,%u,%s%u", 
			LoaiBanTin, xSystem.Parameters.DeviceID,
			GPS_GetPosition().ViDo.value, GPS_GetPosition().KinhDo.value,
			DienApAcQuy, xSystem.MeasureStatus.Current,
			0, /*Indicator_GetUserBlinkCode(xSystem.Parameters.BlinkType), */
			DeviceStatus.Value,
			xSystem.Status.CSQ, GSM_Manager.AccessTechnology,
			xSystem.HardwareInfo.HWResetReason, xSystem.HardwareInfo.SWResetReason,
			FIRMWARE_VERSION_HEADER, FIRMWARE_VERSION_CODE);
	}
		
#if 1		/** Ban tin su dung ma hoa kep */
	/* Ma hoa AES128: Buffer -> EncryptedBuffer */
	memset(AES_Buffer, 0, sizeof(AES_Buffer));
	memset(Base64Buffer, 0, sizeof(Base64Buffer));
		
	AES_ECB_encrypt(mqttBuffer.Buffer, (uint8_t*)PUBLIC_KEY, AES_Buffer, mqttBuffer.BufferIndex);
	mqttBuffer.BufferIndex = 16 * ((mqttBuffer.BufferIndex / 16) + 1);
	
	/* Ma hoa Base64 */
	b64_encode((char *)AES_Buffer, mqttBuffer.BufferIndex, (char *)Base64Buffer);
	Checksum = CRC16(Base64Buffer, strlen((char*)Base64Buffer));	
	
	/* Add checksum & header/footer [] */
	memset(mqttBuffer.Buffer, 0, sizeof(mqttBuffer.Buffer));
	mqttBuffer.BufferIndex = sprintf((char *)mqttBuffer.Buffer, "[%s%05u]", Base64Buffer, Checksum);
#endif 

	DEBUG("\rEncoded: %u - %s", mqttBuffer.BufferIndex, mqttBuffer.Buffer);
	
	/* =================== Build MQTT message ===============*/
	if(pubPackageId == 0) pubPackageId = 1;
	
	char pubTopic[50] = {0};
	sprintf(pubTopic, "%s%s", TOPIC_PUB_HEADER, xSystem.Parameters.GSM_IMEI);
	PubTopicString.cstring = pubTopic;
	
	/* Dong goi MQTT */
	TotalLen = MQTTSerialize_publish(MqttPubBuff + TotalLen, MqttPubBufflen - TotalLen, pub_dup, 
		xSystem.Parameters.PubQos, pub_retained, pubPackageId++, PubTopicString, mqttBuffer.Buffer, mqttBuffer.BufferIndex);
			
	/* Copy MQTT packet vao TCP_Buffer */
	memcpy(pBuffer->Buffer, MqttPubBuff, TotalLen);
	pBuffer->BufferIndex = TotalLen;
	pBuffer->State = BUFFER_STATE_IDLE;
		
#if 1	
	DEBUG("\rMQTT: Ban tin %u, index %u, length %u", LoaiBanTin, BufferAvailable, pBuffer->BufferIndex);
#endif
	
	/* Clear thời gian sleep GSM mỗi lần gửi tin */
	xSystem.Status.GSMSleepTime = 0;
	
	return pBuffer->BufferIndex;	
#endif	//0

	return 0;
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
		DEBUG("\rMQTT_SendBuffer: full buffer!");		
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
	DEBUG("\rMQTT: SendBuffer @%u: %u - %s", BufferAvailable, pBuffer->BufferIndex, LoaiBanTin);
#endif
	
	return pBuffer->BufferIndex;
#endif	//0

	return 0;
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
//	uint16_t Checksum;
	MediumBuffer_t *pBuffer;
	uint8_t BufferAvailable;
	int16_t TotalLen = 0;
		
	/* Timeout thoi gian gui ban tin TCP, het thoi gian -> close connection */
	if(!xSystem.Status.SendGPRSTimeout)
		xSystem.Status.SendGPRSTimeout = GPRS_SEND_TIMEOUT;	/* 180s */
	
	BufferAvailable = CheckMQTTGPRSBufferState(250);
	if(BufferAvailable == 0xFF || BufferAvailable >= NUM_OF_MQTT_BUFFER)
	{
		DEBUG("\rMQTT_PublishDebug: full!");		
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
	DEBUG("\rMQTT: SendDebug: %u - %s", pBuffer->BufferIndex, msgHeader);
#endif
	
	return pBuffer->BufferIndex;
}

#endif	/* __USE_MQTT__ */

/********************************* END OF FILE *******************************/

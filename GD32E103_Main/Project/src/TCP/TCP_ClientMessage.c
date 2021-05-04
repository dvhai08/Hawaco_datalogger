/******************************************************************************
 * @file    	TCP_ClientMessage.c
 * @author  	
 * @version 	V1.0.0
 * @date    	10/03/2016
 * @brief   	
 ******************************************************************************/

/******************************************************************************
                                   INCLUDES					    			 
 ******************************************************************************/
#include <stdio.h>
#include <string.h>
#include "Version.h"
#include "Utilities.h"
#include "TCP.h"
#include "GSM.h"
#include "GPS.h"
#include "HardwareManager.h"
#include "Main.h"

/******************************************************************************
                                   GLOBAL VARIABLES					    			 
 ******************************************************************************/
extern System_t xSystem;
extern GSM_Manager_t	GSM_Manager;
extern GPS_Manager_t GPS_Manager;
extern char SendSMSBuffer[160];
extern uint8_t TCPConnectFail;
extern uint8_t TrangThaiDen;

/******************************************************************************
                                   GLOBAL FUNCTIONS					    			 
 ******************************************************************************/
extern uint8_t SendSMS(char* PhoneNumber, char* Message);

/******************************************************************************
                                   DATA TYPE DEFINE					    			 
 ******************************************************************************/
#define	GPRS_SEND_TIMEOUT	120

/******************************************************************************
                                   PRIVATE VARIABLES					    			 
 ******************************************************************************/ 
static uint8_t Timeout1000ms;
static uint16_t Timeout30m_Move = 0;
static uint16_t Timeout360m_Bulb = 0;
static uint16_t Timeout360m_Bat = 0;

static uint16_t TimeoutSendAlarm30m = 0;
static uint16_t TimeoutSendAlarm360m = 0;

static char TimeStamp[5];
static char PublicBuffer[100];

/******************************************************************************
                                   LOCAL FUNCTIONS					    			 
 ******************************************************************************/
static void GuiBanTinDinhKiTick(void);
static void SendSMSToServer(uint8_t LoaiBanTin);
static void BuildMessageSendServer(void);

/*****************************************************************************/
/**
 * @brief	:   Xu ly gui ban tin canh bao trong che do DEN CAU, GSM OFF, call 100ms/lan
 * @param	:  
 * @retval	:
 * @author	:	
 * @created	:	15/03/2016
 * @version	:
 * @reviewer:	
 */
void TCP_SendAlarmTick(void)
{
	if(Timeout1000ms++ < 10) return;
	Timeout1000ms = 0;
	
	if(xSystem.Status.SendAlarmRequest) {
		xSystem.Status.SendAlarmRequest --;
		if(xSystem.Status.SendAlarmRequest == 1)
		{
			//Tat GSM
			if(GSM_Manager.isGSMOff == 0)
				GSM_PowerControl(GSM_OFF);
			
			Timeout30m_Move = 0;
			Timeout360m_Bulb = 0;
			Timeout360m_Bat = 0;
		}
	}

	if(GSM_Manager.isGSMOff == 0) return;
	
	if(xSystem.Status.DistanceAlarm == 1)
	{
		if(TimeoutSendAlarm30m++ >= 1800)	//30 phut
		{
			TimeoutSendAlarm30m = 0;
			if(xSystem.Status.VminAlarm == 0)
			{
				if(GSM_Manager.isGSMOff)
					GSM_PowerControl(GSM_ON);

				Timeout30m_Move = 0;
				xSystem.Status.SendAlarmRequest = 180;	//Timeout send alarm 180s
			}
		}
	}
	
	//Gui canh bao Den, Accu moi 6h
	if(TimeoutSendAlarm360m++ >= 21600)
	{
		TimeoutSendAlarm360m = 0;
		if(xSystem.Status.DistanceAlarm == 1 || xSystem.Status.LedBulbAlarm == 1)
		{
			if(xSystem.Status.VminAlarm == 0)
			{
				if(GSM_Manager.isGSMOff)
					GSM_PowerControl(GSM_ON);

				Timeout360m_Bulb = 0;
				Timeout360m_Bat = 0;
				xSystem.Status.SendAlarmRequest = 180;	//Timeout send alarm 180s
			}
		}
	}
}

/*****************************************************************************/
/**
 * @brief	:   Xu li gui ban tin theo dinh ki 100ms/lan
 * @param	:  
 * @retval	:
 * @author	:	
 * @created	:	15/03/2016
 * @version	:
 * @reviewer:	
 */
void TCP_ClientMessageTick(void)
{	
	if(Timeout1000ms++ >= 10)
	{
		Timeout1000ms = 0;
		GuiBanTinDinhKiTick();
	}
}
	
/*****************************************************************************/
/**	Ham gui ban tin dinh ky, Tick every 1000ms
*/
static void GuiBanTinDinhKiTick(void)
{
	static uint8_t LastMinute = 0xFF;
	
	if((LastMinute == 0xFF || LastMinute != RTC_GetDateTime().Minute))
	{
		LastMinute = RTC_GetDateTime().Minute;
		
		/* Kiem tra thoi gian gui tin */
		memset(TimeStamp, 0, 5);
		sprintf(TimeStamp, "%02u%02u", RTC_GetDateTime().Hour, LastMinute);
		
		if(strstr(xSystem.Parameters.TimeSend, TimeStamp) != NULL)
		{			
			BuildMessageSendServer();
		}
	}
	
	/* FOR TEST ONLY */
	if(xSystem.Status.YeuCauGuiTin == 1)
	{
		/* Neu den dang o trang thai ON 
			-> Doi do duoc dong tieu thu Den moi gui tin
			-> Hoac qua 60s ma khong do duoc dong tieu thu thi van phai gui ban tin thay doi trang thai
		*/
		if(TrangThaiDen == 1)	//STATE_ON
		{
			if(xSystem.MeasureStatus.Current > 0 || xSystem.MeasureStatus.operatingTime > 60) {
				xSystem.Status.YeuCauGuiTin = 0;
				BuildMessageSendServer();
			}
		} else {
			/* Dong tieu thu den = 0 khi o trang thai OFF */
			xSystem.Status.YeuCauGuiTin = 0;
			xSystem.MeasureStatus.Current = 0;
			BuildMessageSendServer();
		}
	}
		
	/* Giam sat thoi gian gui ban tin GPRS */
	if(xSystem.Status.SendGPRSTimeout)
	{
		if(xSystem.Status.SendGPRSTimeout == GPRS_SEND_TIMEOUT)
		{
			//Neu lan truoc fail thi reset trang thai
			TCPConnectFail = 0;
		}
			
		//Server yeu cau gui lai ban tin
		if(xSystem.Status.ResendGPRSRequest == 1)
		{
			xSystem.Status.ResendGPRSRequest = 0;
			DataMessage(xSystem.Status.SendGPRSRequest);
		}
		
		xSystem.Status.SendGPRSTimeout--;
		if(xSystem.Status.SendGPRSTimeout == 0)
		{
			/* Khong gui duoc bang GPRS, gui bang SMS */
			xSystem.Status.ResendGPRSRequest = 0;
			xSystem.Status.TCPNeedToClose = 1;
			
			SendSMSToServer(xSystem.Status.SendGPRSRequest);
			xSystem.Status.SendGPRSRequest = 0;
		}
	}
	
	/* Gui ban tin canh bao troi phao moi 30 phut */
	/*Edited 05-Sep: Chi gui ban tin canh bao 30 phut cho DistanceAlarm*/
	if(xSystem.Status.DistanceAlarm == 1)
	{
		if((Timeout30m_Move == 0) || (Timeout30m_Move >= 1800 && xSystem.Status.Alarm30Min ==0) || (Timeout30m_Move >= 21000 && xSystem.Status.Alarm30Min ==1))
		{
			if(Timeout30m_Move) Timeout30m_Move = 1;
			
			if(xSystem.Status.DistanceAlarm == 1)
				DataMessage(DATA_CANHBAOVITRI);
		}
		Timeout30m_Move++;
	} else 
		Timeout30m_Move = 0;
	
	/*Gui ban tin canh bao den 6 tieng 1 lan*/
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
	
	/*Gui ban tin canh bao acquy 6 tieng 1 lan*/
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
}


static void BuildMessageSendServer(void)
{
	//Mod: 17/07 .Gui trang thai hien tai cua den:
	// - neu co canh bao -> gui ban tin canh bao
	// - neu khong co canh bao nao -> gui ban tin dinh ky
//	if(xSystem.Status.DistanceAlarm == 0 &&
//		xSystem.Status.LowBattAlarm == 0 &&
//		xSystem.Status.LedBulbAlarm == 0)
//	{
//		DataMessage(DATA_DINHKY);
//	}
//	else {
//		if(xSystem.Status.DistanceAlarm == 1)
//			DataMessage(DATA_CANHBAOVITRI);
//		
//		if(xSystem.Status.LowBattAlarm == 1)
//			DataMessage(DATA_CANHBAOACQUY);
//		
//		if(xSystem.Status.LedBulbAlarm == 1)
//			DataMessage(DATA_CANHBAODEN);	
//	}
	DataMessage(DATA_DINHKY);	
}
	
/*****************************************************************************/
/**
 * @brief	:  
 * @param	:  
 * @retval	:
 * @author	:	
 * @created	:	10/03/2016
 * @version	:
 * @reviewer:	
 */
void InitTCP_ClientMessage(void) 
{ 
	uint8_t i;
	
	/* Reset cac buffer */
    for(i = 0; i < NUM_OF_GPRS_BUFFER; i++)
    {
        xSystem.TCP_Data.GPRS_Buffer[i].BufferIndex = 0;
        xSystem.TCP_Data.GPRS_Buffer[i].State = BUFFER_STATE_IDLE;
    }
	
	xSystem.Status.ServerState = NOT_CONNECTED;
}

/*****************************************************************************/
/**
 * @brief	:  Kiem tra cac buffer GPRS available de ghi vao
 * @param	:  Luong du lieu can gi vao  
 * @retval	:  Index cua buffer hoac 0xFF neu khong co buffer nao trong
 * @author	:	
 * @created	:	15/03/2016
 * @version	:
 * @reviewer:	
 */
uint8_t CheckGPRSBufferState(uint16_t RequestLength)
{
    uint8_t i, Retval = 0xFF;
 
    for(i = 0; i < NUM_OF_GPRS_BUFFER; i++)
    {
        if(xSystem.TCP_Data.GPRS_Buffer[i].State == BUFFER_STATE_IDLE &&
			xSystem.TCP_Data.GPRS_Buffer[i].BufferIndex == 0)
		{
			Retval = i;
			break;
		}
    }
	
	/* Chi reset buffer khi khong con buffer nao trong */
	if(Retval == 0xFF)
	{
		for(i = 0; i < NUM_OF_GPRS_BUFFER; i++)	
		{
			if(xSystem.TCP_Data.GPRS_Buffer[i].State ==  BUFFER_STATE_PROCESSING2 ||	//Gui lan 2 van khong duoc
				xSystem.TCP_Data.GPRS_Buffer[i].State == BUFFER_STATE_IDLE2)		//Mat ket noi Server nen IDLE2 khong -> PROCESSING2 -> reset buffer
			{
				xSystem.TCP_Data.GPRS_Buffer[i].BufferIndex = 0;
				xSystem.TCP_Data.GPRS_Buffer[i].State = BUFFER_STATE_IDLE;
				Retval = i;
				break;
			}
		}
	}
		
	/* Chuyen trang thai cac buffer chua gui duoc */
	for(i = 0; i < NUM_OF_GPRS_BUFFER; i++)	
	{
		if((xSystem.TCP_Data.GPRS_Buffer[i].State == BUFFER_STATE_IDLE && 	//IDLE & index > 20 nhung ko ket noi duoc voi Server 
			xSystem.TCP_Data.GPRS_Buffer[i].BufferIndex > 5) ||									//-> k chuyen duoc sang PROCESSING & Index van giu nguyen >20
			xSystem.TCP_Data.GPRS_Buffer[i].State == BUFFER_STATE_PROCESSING)	//Gui khong thanh cong
		{
			xSystem.TCP_Data.GPRS_Buffer[i].State = BUFFER_STATE_IDLE2;
			break;
		}
	}
		
    return Retval;
}

static uint8_t BuildPacketSendServer(uint8_t LoaiBanTin)
{
	uint8_t ViTri, ViTriTinhCheckSum = 0, CheckSum = 0;
	float DienApAcQuy = 0;
	
	memset(PublicBuffer, 0, 100);
	ViTri = 0;
	
	/* Add header message */
	if(xSystem.Parameters.ServerType == MAKIPOS)
		ViTri += sprintf(&PublicBuffer[ViTri], "!AIVDO,");		//VJCOM -> Server Makipos chua sua thanh VJCOM
	else if(xSystem.Parameters.ServerType == CUSTOMER)
		ViTri += sprintf(&PublicBuffer[ViTri], "!AIVDO,");
	
	ViTriTinhCheckSum = ViTri;
	
	/* Loai ban tin */
	ViTri += sprintf(&PublicBuffer[ViTri], "%d,", LoaiBanTin);
	
	/* Ma thiet bi */
	ViTri += sprintf(&PublicBuffer[ViTri], "%s,", xSystem.Parameters.DeviceID);
	
	/* Trong cac truong hop sau khong gui truong toa do ve:
		- GPS mode = 0
		- Khong co toa do
		- Khong bat GPS
	*/
	if(LoaiBanTin == DATA_DINHKY || LoaiBanTin == DATA_CANHBAOVITRI) {
		if(GPS_GetPosition().ViDo.value != 0 && GPS_GetPosition().KinhDo.value != 0
			&& xSystem.Parameters.GPSMode != 0)
		{
			/* Vi Do */
			ViTri += sprintf(&PublicBuffer[ViTri], "%f", GPS_GetPosition().ViDo.value);	//9.4lf
		}
	}

	ViTri += sprintf(&PublicBuffer[ViTri], ",");

	if(LoaiBanTin == DATA_DINHKY || LoaiBanTin == DATA_CANHBAOVITRI) {
		if(GPS_GetPosition().ViDo.value != 0 && GPS_GetPosition().KinhDo.value != 0
			&& xSystem.Parameters.GPSMode != 0)
		{
			/* Kinh Do */
			ViTri += sprintf(&PublicBuffer[ViTri], "%f", GPS_GetPosition().KinhDo.value); //10.4lf
		}
	}
	
	ViTri += sprintf(&PublicBuffer[ViTri], ",");

	if(LoaiBanTin == DATA_DINHKY || LoaiBanTin == DATA_CANHBAOACQUY) {
		/* Dien ap ac quy - Volt */
		DienApAcQuy = (float)GetBatteryPower()/1000;
		ViTri += sprintf(&PublicBuffer[ViTri], "%4.2f", DienApAcQuy);		//5.2f
	}

	ViTri += sprintf(&PublicBuffer[ViTri], ",");
	
	if(LoaiBanTin == DATA_DINHKY || LoaiBanTin == DATA_CANHBAODEN) {
		/* Dong tieu thu den - mA*/
		ViTri += sprintf(&PublicBuffer[ViTri], "%u", xSystem.MeasureStatus.Current);
	}

	ViTri += sprintf(&PublicBuffer[ViTri], ",");
	
	if(LoaiBanTin == DATA_DINHKY) {
		/* Chu ky chop den */
		ViTri += sprintf(&PublicBuffer[ViTri], "%03u", Indicator_GetUserBlinkCode(xSystem.Parameters.BlinkType));
	}
		
	/* Tinh checksum */
	CheckSum = CalculateCheckSum((uint8_t*)PublicBuffer, ViTriTinhCheckSum, ViTri - ViTriTinhCheckSum) & 0xFF;
		
	/* Add tail & checksum */
	ViTri += sprintf(&PublicBuffer[ViTri], "*%02X", CheckSum);
	
	return ViTri;
}

/*****************************************************************************/
/**
 * @brief	:  	Cac ban tin gui len Server qua GPRS
 * @param	:  
 * @retval	:
 * @author	:	
 * @created	:	15/03/2016
 * @version	:
 * @reviewer:	
 */
uint16_t DataMessage(uint8_t LoaiBanTin)
{
	LargeBuffer_t *pBuffer;
	uint8_t BufferAvailable;
	
	BufferAvailable = CheckGPRSBufferState(100);
	if(BufferAvailable == 0xFF || BufferAvailable >= NUM_OF_GPRS_BUFFER)
	{
		DEBUG("\rDataMessage(ret:0)");		
		return 0;
	}
    
    pBuffer = &(xSystem.TCP_Data.GPRS_Buffer[BufferAvailable]);
    pBuffer->State = BUFFER_STATE_BUSY;
    pBuffer->BufferIndex = 0;
	
	/* Build Packet */
	pBuffer->BufferIndex = BuildPacketSendServer(LoaiBanTin);
	memcpy(pBuffer->Buffer, PublicBuffer, pBuffer->BufferIndex);

	pBuffer->State = BUFFER_STATE_IDLE;
	
	/* FOR DEBUG */
	DEBUG("\rAdd message to buffer: %u, len: %u", BufferAvailable, pBuffer->BufferIndex);
	DEBUG("\rMessage: %s", PublicBuffer);
	
	xSystem.Status.SendGPRSRequest = LoaiBanTin;
	xSystem.Status.ResendGPRSRequest = 0;
	xSystem.Status.SendGPRSTimeout = GPRS_SEND_TIMEOUT;	/* 120s */
		
	return pBuffer->BufferIndex;
}

/*****************************************************************************/
/**
 * @brief	:  	Cac ban tin gui len Server qua SMS
 * @param	:  
 * @retval	:
 * @author	:	
 * @created	:	15/03/2016
 * @version	:
 * @reviewer:	
 */
static void SendSMSToServer(uint8_t LoaiBanTin)
{
	uint8_t Ret;
	
	if(LoaiBanTin != GPRS_BANTINKHAC)
	{
		BuildPacketSendServer(LoaiBanTin);
		
		if(xSystem.Parameters.ServerType == MAKIPOS)
			Ret = SendSMS(xSystem.Parameters.SMSNum1, PublicBuffer);
		else if(xSystem.Parameters.ServerType == CUSTOMER)
			Ret = SendSMS(xSystem.Parameters.SMSNum3, PublicBuffer);
	}
	else
		SendSMS(xSystem.Parameters.SMSNum1, SendSMSBuffer);
		
	if(Ret == 0)
		DEBUG("\rSend SMS Error!");
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
uint16_t SendBufferToServer(uint8_t* BufferToSend, uint16_t Length) 
{ 
	LargeBuffer_t *pBuffer;
	uint8_t BufferAvailable;
	
	BufferAvailable = CheckGPRSBufferState(Length);
	if(BufferAvailable == 0xFF || BufferAvailable >= NUM_OF_GPRS_BUFFER)
	{
		DEBUG("\rSendBufferToServer(ret:0)");		
		return 0;
	}
  	
    pBuffer = &(xSystem.TCP_Data.GPRS_Buffer[BufferAvailable]);
	memset(pBuffer->Buffer, 0, 100);
    pBuffer->State = BUFFER_STATE_BUSY;
    pBuffer->BufferIndex = 0;
 
	memcpy(pBuffer->Buffer, BufferToSend, Length);

	pBuffer->BufferIndex = Length;
	pBuffer->State = BUFFER_STATE_IDLE;
	
	/* FOR DEBUG */
	DEBUG("\rAdd message to buffer: %u, len: %u", BufferAvailable, Length);
	DEBUG("\rMessage: %s", pBuffer->Buffer);
	
	xSystem.Status.SendGPRSRequest = GPRS_BANTINKHAC;
	xSystem.Status.ResendGPRSRequest = 0;
	xSystem.Status.SendGPRSTimeout = GPRS_SEND_TIMEOUT;	/* 120s */
		
	return pBuffer->BufferIndex;
}


/********************************* END OF FILE *******************************/

/******************************************************************************
 * @file    	GSM_SMSLayer.c
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
#include "gsm.h"
#include "gsm_utilities.h"
#include "Version.h"
#include "DataDefine.h"
#include "hardware.h"
#include "Parameters.h"
#include "Configurations.h"
#include "HardwareManager.h"
#include "UpdateFirmwareFTP.h"
#include "MQTTUser.h"

/******************************************************************************
                                   GLOBAL VARIABLES					    			 
 ******************************************************************************/
SMSStruct_t SMSMemory[3];
extern System_t xSystem;
extern GSM_Manager_t gsm_manager;

/******************************************************************************
                                   GLOBAL FUNCTIONS					    			 
 ******************************************************************************/

/******************************************************************************
                                   DATA TYPE DEFINE					    			 
 ******************************************************************************/

/******************************************************************************
                                   PRIVATE VARIABLES					    			 
 ******************************************************************************/
char SMSContent[160];
char SoDienThoai[20];
char ConfigBuffer[120];
char SendSMSBuffer[160];
char SMSResponseBuffer[160];

/******************************************************************************
                                   LOCAL FUNCTIONS					    			 
 ******************************************************************************/

/*****************************************************************************/

/*****************************************************************************/
/**
 * @brief:  	Tach so dien thoai trong ban tin den thiet bi
 * @param:  	Chuoi du lieu tu GSM
 * @retval:   	So dien thoai duoc luu vao bien SoDienThoai
 *	@author:	
 *	@created:	20/04/2012
 */
static uint8_t TachSoDienThoai(char* DataBuffer)
{
    uint16_t i = 0;
    uint16_t j = 0;
    int16_t ViTriDuLieu = 0;

    memset(SoDienThoai, 0, 20);

    /* Tim vi tri so dien thoai trong chuoi du lieu */
    i = 0;
    ViTriDuLieu = -1;
    while(DataBuffer[i] && i < 255)
    {
			if(++i > 3)
				if(DataBuffer[i] == '"' && DataBuffer[i - 1] == ',' && DataBuffer[i - 2] == '"')
				{
						ViTriDuLieu = i + 1;
						break;
				}
    }

    /* Copy so dien thoai */
    j = 0;
    if(ViTriDuLieu > -1)
    {
        for(i = 0; i < 20; i++)
        {
            if(DataBuffer[ViTriDuLieu + i] != '"')
                SoDienThoai[j++] = DataBuffer[ViTriDuLieu + i];
            else break;
        }
    }

	if(strlen(SoDienThoai) < 7) return 0;

    return 1;
}
/*****************************************************************************/
/**
 * @brief:  		Tach noi dung SMS
 * @param:  		Chuoi du lieu tu GSM
 * @retval:         Noi dung duoc dua vao SMS Content
 *	@author:		
 *	@created:		02/07/2013
 */
static void CopySMSContent(char* Buffer)
{
	uint16_t i = 0;
    uint16_t j = 0;
    int16_t ViTriDuLieu = 0;
	uint8_t	k = 0;
	
	ViTriDuLieu = -1;
	
	//Dem bo qua 3 dau ,
	k = 0;
    while(Buffer[i] && i < 255)
    {
		if(Buffer[i] == ',') 
		{
			k++;
			if(k == 3) break;
		}
		i++;
    }
	
	//Bo qua 2 dau "
	k = 0;
	while(Buffer[i] && i < 255)
    {
		if(Buffer[i] == '"') 
		{
			k++;
			if(k == 2) 
			{
				ViTriDuLieu = i + 3;
				break;
			}	
		}
		i++;
    }
	
	//Copy du lieu vao o nho
	j = 0;
    memset(SMSContent,0,160);
	while(Buffer[ViTriDuLieu] && ViTriDuLieu < 512)
	{
		SMSContent[j++] = Buffer[ViTriDuLieu++];
		
		if(Buffer[ViTriDuLieu] == 13 && Buffer[ViTriDuLieu + 1] == 10 &&
			Buffer[ViTriDuLieu + 2] == 13 && Buffer[ViTriDuLieu + 3] == 10 &&
			Buffer[ViTriDuLieu + 4] == 'O' && Buffer[ViTriDuLieu + 5] == 'K')
		{
			break;
		}
	}
}

/*****************************************************************************/
/**
 * @brief	:  	Ham nay nhan yeu cau gui tin nhan, dua vao hang doi xu ly sau
 * @param	:  
 * @retval	:
 * @author	:	
 * @created	:	15/01/2014
 * @version	:
 * @reviewer:	
 */
uint8_t SendSMS(char* PhoneNumber, char* Message) 
{ 
	uint8_t ucCount = 0;

	/* Kiem tra dieu kien ve do dai */
	if(strlen(Message) >= 160) return 0;
	if((strlen(PhoneNumber) >= 15) || (strlen(PhoneNumber) == 0)) return 0;

	/* Tim bo nho rong */
	for(ucCount = 0; ucCount < 3; ucCount++)
	{
		if(SMSMemory[ucCount].NeedToSent == 0)
		{
			memset(SMSMemory[ucCount].PhoneNumber, 0, 15);
			memset(SMSMemory[ucCount].Message, 0, 160);

			strcpy(SMSMemory[ucCount].PhoneNumber, PhoneNumber);
			strcpy(SMSMemory[ucCount].Message, Message);

			SMSMemory[ucCount].NeedToSent = 2;		//1
			SMSMemory[ucCount].RetryCount = 0;

			DEBUG ("\r\nAdd tin nhan vao buffer %u: %s,SDT: %s", ucCount, Message, PhoneNumber);

			return 1;
		}
	}
	return 0;
}

/*****************************************************************************/
/**
 * @brief	:  
 * @param	:  
 * @retval	:
 * @author	:	
 * @created	:	10/11/2014
 * @version	:
 * @reviewer:	
 */
static void GuiINFO()
{
    /* IMEI, BKS, XN, Loai thiet bi */
    memset(SendSMSBuffer, 0, 160);

    //Build thong tin
	sprintf(SendSMSBuffer, "%d:%d - IMEI: %s TBI: %s FIRM: %s%03u, RD: %s %s, BLDER: %u, FLASH: %u",
        RTC_GetDateTime().Hour, RTC_GetDateTime().Minute,xSystem.Parameters.GSM_IMEI, 
        xSystem.Parameters.DeviceID, FIRMWARE_VERSION_HEADER,FIRMWARE_VERSION_CODE,
			__RELEASE_DATE_KEIL__, __RELEASE_TIME_KEIL__, xSystem.Parameters.BootloaderVersion,
			xSystem.Status.FlashState);
	
    //Gui lai SMS
    SendSMS(SoDienThoai, SendSMSBuffer);
}

/*****************************************************************************/
/**
 * @brief	:  Gui thong tin trang thai
 * @param	:  
 * @retval	:
 * @author	:	
 * @created	:	10/11/2014
 * @version	:
 * @reviewer:	
 */
void gsm_send_status_to_mobilephone(char *SDT)
{
  sprintf(SoDienThoai,"%s",SDT);
		
	sprintf(SendSMSBuffer, "IMEI: %s, ID: %s, TD: %.4f,%.4f, MoveAlr: %u, BatAlr: %u, BulbAlr: %u, 30Alr: %u, DisSet: %u, DisCal: %.1f, OpTime: %u,Laser:%d,Ultra:%d",
		xSystem.Parameters.GSM_IMEI, xSystem.Parameters.DeviceID, 
		GPS_GetPosition().ViDo.value, GPS_GetPosition().KinhDo.value, 
		xSystem.Status.DistanceAlarm, xSystem.Status.LowBattAlarm, xSystem.Status.LedBulbAlarm, xSystem.Status.Alarm30Min,
		xSystem.Parameters.Distance, xSystem.Status.Distance, xSystem.MeasureStatus.operatingTime,
		xSystem.MeasureStatus.DistanceLaser, xSystem.MeasureStatus.DistanceUltra);
		
	SendSMS(SoDienThoai, SendSMSBuffer);	
}

/*****************************************************************************/
/**
 * @brief	:  
 * @param	:  
 * @retval	:
 * @author	:	
 * @created	:	10/11/2014
 * @version	:
 * @reviewer:	
 */
void gsm_send_sms(char* Buffer, uint8_t CallFrom)
{
    uint16_t ViTri = 0;
    uint16_t i = 0;

    if(CallFrom == 0)
        SendSMS(SoDienThoai, "Nhan duoc yeu cau nhan tin.");

    memset(SoDienThoai, 0, 20);

    /* Lay so dien thoai */
    for(ViTri = 0; ViTri < 20; ViTri++)
    {
        if((Buffer[ViTri + 8] > 0) && (Buffer[ViTri + 8] != ','))
        {
            SoDienThoai[ViTri] = Buffer[ViTri + 8];
        }
        else break;
    }

    ViTri += 9;
    memset(SendSMSBuffer, 0, 160);

    while((Buffer[ViTri] > 0) && (Buffer[ViTri] != ')'))
    {
        if(ViTri > 130) break;

        SendSMSBuffer[i] = Buffer[ViTri];
        i++;
        ViTri++;
    }

    /* Check lai so dien thoai */
    for(ViTri = 0; ViTri < 20; ViTri++)
    {
        if(SoDienThoai[ViTri])
        {
            if(SoDienThoai[ViTri] < '0' || SoDienThoai[ViTri] > '9') return;
        }
        else break;
    }

    if(strlen(SoDienThoai) < 3) return;

    DEBUG ("\r\nDen so: %s", SoDienThoai);
    DEBUG ("\r\nNoi dung: %s", SendSMSBuffer);

    SendSMS(SoDienThoai, SendSMSBuffer);
}

/*****************************************************************************/
/**
 * @brief	:  
 * @param	:  
 * @retval	:
 * @author	:	
 * @created	:	10/11/2014
 * @version	:
 * @reviewer:	
 */
//static void TinNhanCauHinh(char* Buffer, uint8_t cfgSource)
//{	    
//	//Xu ly nhieu cau hinh va tra loi trong 1 tin nhan
//	uint8_t configResult, SoLanXuLy = 0;
//	uint16_t replyLength = 0;
//	memset(SendSMSBuffer, 0, 160);
//	
//	while(strstr(Buffer, "SET") && SoLanXuLy < 10 && replyLength < 140)
//	{
//		SoLanXuLy++;
//		Buffer = strstr(Buffer, "SET");
//		if (Buffer == NULL) break;
//		
//		configResult = ProcessSetConfig(Buffer, cfgSource);
//		memset(SMSResponseBuffer, 0, 100);
//		
//		if (replyLength > 0) replyLength += sprintf(&SendSMSBuffer[replyLength], "%s", "|");
//		
//		if(configResult < 0xFF && configResult > 0)
//		{
//			replyLength += sprintf(&SendSMSBuffer[replyLength], "CH%u: ", configResult);
//			
//			Buffer[0] = 'G';
//			ProcessGetConfig(Buffer, &SendSMSBuffer[replyLength]);
//			replyLength = strlen(SendSMSBuffer);
//		}
//		else if(configResult == 0xFF)
//		{
//			//Lenh khong can phan hoi
//		}
//		else 
//			sprintf(&SendSMSBuffer[replyLength], "SyntaxER");
//		
//		/* Dich vi tri con tro */
//		Buffer = &Buffer[1];
//	}
//	
//	if (replyLength > 0) {
//		if(cfgSource == CF_SMS)
//			SendSMS(SoDienThoai, SendSMSBuffer);
//		
//		if(cfgSource == CF_SERVER)
//			MQTT_SendBufferToServer(SendSMSBuffer, "SET_CFG");
//	}
//}


/*****************************************************************************/
/**
 * @brief	:  Xu ly ban tin SET cau hinh tu server
 * @param	:  
 * @retval	:
 * @author	:	
 * @created	:	07/05/2020
 * @version	:
 * @reviewer:	
 */
void ProcessSetParameters(char *Buffer, uint8_t Source)
{
	//Xu ly nhieu cau hinh va tra loi trong 1 ban tin
	uint8_t SoLanXuLy = 0;
	uint16_t replyLength = 0;
	memset(SMSResponseBuffer, 0, 160);
	
	while(SoLanXuLy < 10 && replyLength < 145)
	{
		Buffer = strstr(Buffer, "SET");
		if (Buffer == NULL) break;
		SoLanXuLy++;
		
		uint8_t configResult = ProcessSetConfig(Buffer, Source);
		
		/* Ket qua cau hinh lan >= 2 thi them dau ngan cach | */
		if (replyLength > 0) replyLength += sprintf(&SMSResponseBuffer[replyLength], "%s", ",");
		
		if(configResult < 0xFF && configResult > 0)
		{
			replyLength += sprintf(&SMSResponseBuffer[replyLength], "CH%u:", configResult);
			
			Buffer[0] = 'G';
			ProcessGetConfig(Buffer, &SMSResponseBuffer[replyLength]);
			replyLength = strlen(SMSResponseBuffer);
		}
		else if(configResult == 0xFF)
		{
			//Lenh khong can phan hoi
		}
		else
			sprintf(&SMSResponseBuffer[replyLength], "SyntaxER");
		
		/* Dich vi tri con tro */
		Buffer = &Buffer[1];
	}
	
	/** Gui phan hoi */
	if (replyLength > 0) {
		if(Source == CF_SMS)
			SendSMS(SoDienThoai, SMSResponseBuffer);
		
		if(Source == CF_SERVER)
			MQTT_SendBufferToServer(SMSResponseBuffer, "SET_CFG");
	}
}

/*****************************************************************************/
/**
 * @brief	:  
 * @param	:  
 * @retval	:
 * @author	:	
 * @created	:	10/11/2014
 * @version	:
 * @reviewer:	
 */
void ProcessGetParameters(char* Buffer, uint8_t source)
{
	/** Xu ly nhieu cau hinh va tra loi trong 1 tin nhan
	* Cu phap: GET,1,2,3,...,N
	*/
	uint8_t KetQua = 0;
	uint8_t index = 0;

	memset(SendSMSBuffer, 0, 160);
	memset(SMSResponseBuffer, 0, 160);

	char *mToken = NULL;
	uint16_t malenh = 0;
	
	/* Split c�c truong du lieu, cach nhau boi dau ',' */
	mToken = strtok(Buffer, ",");
	
	/** Duyet cac truong */
	while(mToken != NULL) {
		malenh = GetNumberFromString(0, mToken);
		if(malenh && index < 145)
		{
			memset(ConfigBuffer, 0, 20);
			sprintf(ConfigBuffer, "GET,%u", malenh);
			memset(SMSResponseBuffer, 0, 50);
			KetQua = ProcessGetConfig(ConfigBuffer, SMSResponseBuffer);
			if(KetQua > 0)
			{
				index += sprintf(&SendSMSBuffer[index], "CH%u:%s,", KetQua, SMSResponseBuffer);
			}
			else
			{	
				index += sprintf(&SendSMSBuffer[index], "CH%u:SyntaxER", KetQua);
			}
		}
		
		/* Next token */
		mToken = strtok(NULL, ",");
	}
	
	if(index) {
		if(source == CF_SMS)
			SendSMS(SoDienThoai, SendSMSBuffer);
		if(source == CF_SERVER)
			MQTT_SendBufferToServer(SendSMSBuffer, "GET_CFG");
	}
}


/*****************************************************************************/
/**
 * @brief	:  	Gui SMS den tat ca cac SDT duoc cai dat
 * @param	:  
 * @retval	:
 * @author	:	
 * @created	:	15/01/2014
 * @version	:
 * @reviewer:	
 */
void SMSControlBroadcast()
{
    if(strlen(xSystem.Parameters.SMSNum1) > 7)
        SendSMS(xSystem.Parameters.SMSNum1, SendSMSBuffer);

    if(strlen(xSystem.Parameters.SMSNum2) > 7)
        SendSMS(xSystem.Parameters.SMSNum2, SendSMSBuffer);

    if(strlen(xSystem.Parameters.SMSNum3) > 7)
        SendSMS(xSystem.Parameters.SMSNum3, SendSMSBuffer);
}

/*****************************************************************************/
/**
 * @brief	:  	Check SDT truyen vao co nam trong danh sach cai dat truoc ko
 * @param	:  
 * @retval	:
 * @author	:	
 * @created	:	15/01/2014
 * @version	:
 * @reviewer:	
 */
uint8_t ValidSMSInListControl(char* SoDienThoai)
{
    if(strlen(xSystem.Parameters.SMSNum1) > 7)
    {
        if(strstr(SoDienThoai, &xSystem.Parameters.SMSNum1[1]))
        {
            return 1;
        }
    }

    if(strlen(xSystem.Parameters.SMSNum2) > 7)
    {
        if(strstr(SoDienThoai, &xSystem.Parameters.SMSNum2[1]))
        {
            return 1;
        }
    }

    if(strlen(xSystem.Parameters.SMSNum3) > 7)
    {
        if(strstr(SoDienThoai, &xSystem.Parameters.SMSNum3[1]))
        {
            return 1;
        }
    }

    return 0;
}

	
/*****************************************************************************/
/**
 * @brief:  
 * @param:  
 * @retval:
 *	@author:	
 *	@created:	20/04/2012
 */
void ThongBaoTrangThaiNguonQuaSMS(uint8_t TrangThaiNguon)
{
    memset(SendSMSBuffer, 0, 120);
    SMSControlBroadcast();
}

uint8_t isAllowSymbol(char ch)
{
	if((ch >= 48 && ch <=57) || (ch >= 65 && ch <=90) || (ch >=97 && ch <=122))
		return 1;
	
	return 0;
}

/*****************************************************************************/
/**
 * @brief:  Gui phan hoi khi co lenh cau hinh:
	- SMS gui tu so dt Server: Gui TCP tra loi, neu TCP fail -> gui lai bang SMS tuong ung voi tung server
	- SMS gui tu so dt Operator: Tra loi SMS toi operator
 * @param:  
 * @retval:
 *	@author:	
 *	@created:	
 */
void SendResponseMessage(void)
{
	/* Gui tra loi bang SMS hay GPRS */
	
	//Gui tu SDT server Makipos - Vijalight
	if(strstr(SoDienThoai, &xSystem.Parameters.SMSNum1[1])) {
//		SendBufferToServer((uint8_t*)SendSMSBuffer, strlen(SendSMSBuffer));
		SendSMS(SoDienThoai, SendSMSBuffer);
		return;
	}
	
	if(strstr(SoDienThoai, &xSystem.Parameters.SMSNum2[1]))	{
		SendSMS(SoDienThoai, SendSMSBuffer);
		return;
	}

	if(strstr(SoDienThoai, &xSystem.Parameters.SMSNum3[1]))
		SendSMS(SoDienThoai, SendSMSBuffer);  
}

/*****************************************************************************/
/**
 * @brief:  Doi mat khau cho thiet bi, CPASS xxxxxx,yyyyyy,zzzzzz
 * @param:  
 * @retval:
 *	@author:	
 *	@created:
 */
void DoiMatKhau(char* Buffer)
{
	char tmpBuf[20] = {0};
	uint8_t pos = 5, i = 0, result = 0, tmpByte;
	uint8_t leng = strlen(Buffer);
	
	char *Message = strstr(&Buffer[0],"CPASS");
	if(Message == NULL) return;

	//Lay mat khau cu
	while(Message[pos] != ',' && pos < leng)
	{
		if(isAllowSymbol(Message[pos]))
		{
			tmpBuf[i++] = Message[pos];
		}
		pos++;
	}
	//Neu mat khau qua 6 ky tu -> sai
	if(i >= 7) return;
	//So sanh voi mat khau cu
	if(strcmp(tmpBuf, xSystem.Parameters.Password) != 0) return;

	//Lay mat khau moi
	memset(tmpBuf, 0, 20);
	pos++; i = 0;
	while(Message[pos] != ',' && pos < leng)
	{
		if(isAllowSymbol(Message[pos]))
		{
			tmpBuf[i++] = Message[pos];
		}
		pos++;
	}
	//Neu mat khau qua 6 ky tu -> sai
	if(i >= 7) return;	
	
    memset(SendSMSBuffer, 0, 160);
    memset(ConfigBuffer, 0, 120);
	//Set cau hinh
	sprintf(ConfigBuffer, "SET,%d,(%s)", CFG_Password, tmpBuf);
	result = ProcessSetConfig(ConfigBuffer, CF_SMS);
	
	tmpByte = sprintf(SendSMSBuffer, "RMSG %d,", result > 0 ? 1 :0);
	pos++; i = 0;
	while(Message[pos] != NULL && pos < leng)
		SendSMSBuffer[tmpByte++] = Message[pos++];
	
	SendResponseMessage(); 
}

/*****************************************************************************/
/**
 * @brief:  Dang ky so dien thoai, CPN xxxxxx,y,zzzzzz
 * @param:  
 * @retval:
 *	@author:	
 *	@created:
 */
void DangKySDT(char* Buffer)
{
	char tmpBuf[7] = {0};
	uint8_t pos, i = 0, result = 0, tmpByte, Macauhinh;
	uint8_t leng = strlen(Buffer);
	
	char *Message = strstr(&Buffer[0],"CPN");
	if(Message == NULL) return;
	
	//Lay mat khau
	pos = 3;
	while(Message[pos] != ',' && pos < leng)
	{
		if(isAllowSymbol(Message[pos]))
		{
			tmpBuf[i++] = Message[pos];
		}
		pos++;
	}
	//Neu mat khau qua 6 ky tu -> sai
	if(i >= 7) return;
	if(strcmp(tmpBuf, xSystem.Parameters.Password) != 0) return;	//Sai mat khau
	
	pos++;
	tmpByte = GetNumberFromString(pos, Message);
		
	if(tmpByte == 1) Macauhinh = CFG_SMSNumber1;
	else if(tmpByte == 2) Macauhinh = CFG_SMSNumber2;
	else return;
	
	//Set cau hinh
    memset(SendSMSBuffer, 0, 160);
    memset(ConfigBuffer, 0, 120);
	sprintf(ConfigBuffer, "SET,%d,(%s)", Macauhinh, SoDienThoai);
	result = ProcessSetConfig(ConfigBuffer, CF_SMS);

	tmpByte = sprintf(SendSMSBuffer, "RMSG %d,", result > 0 ? 1 :0);
	pos += 2; //Bo qua dau ','
	i = 0;
	while(Message[pos] != NULL && pos < leng)
		SendSMSBuffer[tmpByte++] = Message[pos++];
	
	SendResponseMessage();  	
}

/*****************************************************************************/
/**
 * @brief:  Cai dat toa do, CPOS xxxxxxxx,yyyyyyyyy,zzzzzz
 * @param:  
 * @retval:
 *	@author:	
 *	@created:
 */
void CaiDatToaDo(char* Buffer)
{
	char tmpBuf[22] = {0};
	uint8_t pos, i = 0, result = 0, tmpByte, Comma = 0;
	uint8_t leng = strlen(Buffer);
	
	char *Message = strstr(&Buffer[0],"CPOS");
	if(Message == NULL) return;
	
	//CPOS zzzzz
	while(Message[i++] && i < leng)
	{
		if(Message[i] == ',') Comma++;
	}
	if(Comma < 1)	//Cai dat theo toa do thuc te
	{
		sprintf(tmpBuf, "%f,%f", GPS_GetPosition().ViDo.value, 
			GPS_GetPosition().KinhDo.value);
	}
	else {
		//CPOS xxxxxxxx,yyyyyyyyy,zzzzzz
		pos = 4; i = 0;
		while(Message[pos] != ',' && pos < leng)
		{
			tmpBuf[i++] = Message[pos++];
		}	
		pos++;
		tmpBuf[i++] = ',';
		while(Message[pos] != ',' && pos < leng)
		{
			tmpBuf[i++] = Message[pos++];
		}		
	}	
	
	//Set cau hinh
	memset(SendSMSBuffer, 0, 160);
	memset(ConfigBuffer, 0, 120);
	sprintf(ConfigBuffer, "SET,%d,(%s)", CFG_Position, tmpBuf);
	result = ProcessSetConfig(ConfigBuffer, CF_SMS);

	tmpByte = sprintf(SendSMSBuffer, "RMSG %d,", result > 0 ? 1 :0);
	if(Comma < 2) pos = 4;
	else pos++;
	while(Message[pos] != NULL && pos < leng)
		SendSMSBuffer[tmpByte++] = Message[pos++];
	
	SendResponseMessage();  	
}

/*****************************************************************************/
/**
 * @brief:  Cai dat khoang cach, CDIS  xxx,zzzzzz
 * @param:  
 * @retval:
 *	@author:	
 *	@created:
 */
void CaiDatKhoangCach(char* Buffer)
{
	char tmpBuf[10] = {0};
	uint8_t pos, i = 0, result = 0, tmpByte;
	uint8_t leng = strlen(Buffer);
	
	char *Message = strstr(&Buffer[0],"CDIS");
	if(Message == NULL) return;
	
	//CDIS  xxx,zzzzzz
	pos = 4;
	while(Message[pos] != ',' && i < 10)
	{
		if(Message[pos] != 32) //Space
		{
			tmpBuf[i++] = Message[pos];
		}
		pos++;
	}	
	tmpByte = GetNumberFromString(0, tmpBuf);
				
	//Set cau hinh
    memset(SendSMSBuffer, 0, 160);
    memset(ConfigBuffer, 0, 120);
	sprintf(ConfigBuffer, "SET,%d,(%d)", CFG_Distance, tmpByte);
	result = ProcessSetConfig(ConfigBuffer, CF_SMS);

	tmpByte = sprintf(SendSMSBuffer, "RMSG %d,", result > 0 ? 1 :0);
	pos++;
	while(Message[pos] != NULL && pos < leng)
		SendSMSBuffer[tmpByte++] = Message[pos++];
	
	SendResponseMessage();  	
}

/*****************************************************************************/
/**
 * @brief:  Cai dat cong suat, CCAP x,zzzzzz
 * @param:  
 * @retval:
 *	@author:	
 *	@created:
 */
void CaiDatCongSuat(char* Buffer)
{
	char tmpBuf[5] = {0};
	uint8_t pos, i = 0, result = 0, tmpByte;
	uint8_t leng = strlen(Buffer);
	
	char *Message = strstr(&Buffer[0],"CCAP");
	if(Message == NULL) return;
	
	//Lay muc cong suat cai dat
	pos = 4;
	while(Message[pos] != ',' && i < 5)
	{
		if(Message[pos] != 32) //Space
		{
			tmpBuf[i++] = Message[pos];
		}
		pos++;
	}	
	tmpByte = GetNumberFromString(0, tmpBuf);
				
	//Set cau hinh
    memset(SendSMSBuffer, 0, 160);
    memset(ConfigBuffer, 0, 120);
	sprintf(ConfigBuffer, "SET,%d,(%d)", CFG_Capacity, tmpByte);
	result = ProcessSetConfig(ConfigBuffer, CF_SMS);

	tmpByte = sprintf(SendSMSBuffer, "RMSG %d,", result > 0 ? 1 :0);
	pos++;
	while(Message[pos] != NULL && pos < leng)
		SendSMSBuffer[tmpByte++] = Message[pos++];
	
	SendResponseMessage();  	
}

/*****************************************************************************/
/**
 * @brief:  Cai dat ma phao, CID xxxxxxxxxx,zzzzzz
 * @param:  
 * @retval:
 *	@author:	
 *	@created:
 */
void CaiDatMaPhao(char* Buffer)
{
	char tmpBuf[20] = {0};
	uint8_t pos, i = 0, result = 0, tmpByte;
	uint8_t leng = strlen(Buffer);
	
	char *Message = strstr(&Buffer[0],"CID");
	if(Message == NULL) return;

	//Lay chuoi Ma phao
	pos = 3;
	while(Message[pos] != ',' && i < 20)
	{
		if(Message[pos] != 32) //space
		{
			tmpBuf[i++] = Message[pos];
		}
		pos++;
	}

    memset(SendSMSBuffer, 0, 160);
    memset(ConfigBuffer, 0, 120);
	//Set cau hinh
	sprintf(ConfigBuffer, "SET,%d,(%s)", CFG_DeviceID, tmpBuf);
	result = ProcessSetConfig(ConfigBuffer, CF_SMS);
	
	tmpByte = sprintf(SendSMSBuffer, "RMSG %d,", result > 0 ? 1 :0);
	pos++;
	while(Message[pos] != NULL && pos < leng)
		SendSMSBuffer[tmpByte++] = Message[pos++];
	
	SendResponseMessage(); 
}

/*****************************************************************************/
/**
 * @brief:  Cai dat server, CSV xxxxxxx,yyyy,zzzzzz
 * @param:  
 * @retval:
 *	@author:	
 *	@created:
 */
void CaiDatServer(char* Buffer)
{
	char tmpBuf[36] = {0};
	uint8_t pos, i = 0, result = 0, tmpByte, Macauhinh;
	uint8_t leng = strlen(Buffer);
	
	char *Message = strstr(&Buffer[0],"CSV");
	if(Message == NULL) return;
	
	//CSV xxxxxxx,yyyy,zzzzzz
	pos = 3;
	while(Message[pos] != ',' && i < 36)
	{
		if(Message[pos] != 32) //space
			tmpBuf[i++] = Message[pos];
		pos++;
	}	
	pos++;
	tmpBuf[i++] = ',';
	while(Message[pos] != ',' && i < 36)
	{
		if(Message[pos] != 32) //space
			tmpBuf[i++] = Message[pos];
		pos++;
	}		
			
	//Set cau hinh
    memset(SendSMSBuffer, 0, 160);
    memset(ConfigBuffer, 0, 120);
	if(xSystem.Parameters.ServerType == MAKIPOS)
		Macauhinh = CFG_MainIP;
	else if(xSystem.Parameters.ServerType == CUSTOMER)
		Macauhinh = CFG_AlterIP;
	else return;
	
	sprintf(ConfigBuffer, "SET,%d,(%s)", Macauhinh, tmpBuf);
	result = ProcessSetConfig(ConfigBuffer, CF_SMS);

	tmpByte = sprintf(SendSMSBuffer, "RMSG %d,", result > 0 ? 1 :0);
	pos++;
	while(Message[pos] != NULL && pos < leng)
		SendSMSBuffer[tmpByte++] = Message[pos++];
	
	SendResponseMessage();  	
}

/*****************************************************************************/
/**
 * @brief:  Cai dat kieu chop, CTB xxx,zzzzzz
 * @param:  
 * @retval:
 *	@author:	
 *	@created:
 */
void CaiDatKieuChop(char* Buffer)
{
	char tmpBuf[10] = {0};
	uint8_t pos, i = 0, result = 0;
	uint16_t tmpInt;
	uint8_t leng = strlen(Buffer);
	
	char *Message = strstr(&Buffer[0],"CTB");
	if(Message == NULL) return;
	
	//CTB xxx,zzzzzz
	pos = 3;
	while(Message[pos] != ',' && i < 10)
	{
		if(Message[pos] != 32) //Space
		{
			tmpBuf[i++] = Message[pos];
		}
		pos++;
	}	
	tmpInt = GetNumberFromString(0, tmpBuf);
				
	//Set cau hinh
    memset(SendSMSBuffer, 0, 160);
    memset(ConfigBuffer, 0, 120);
	sprintf(ConfigBuffer, "SET,%d,(%d)", CFG_BlinkType, tmpInt);
	result = ProcessSetConfig(ConfigBuffer, CF_SMS);

	tmpInt = sprintf(SendSMSBuffer, "RMSG %d,", result > 0 ? 1 :0);
	pos++;
	while(Message[pos] != NULL && pos < leng)
		SendSMSBuffer[tmpInt++] = Message[pos++];
	
	SendResponseMessage();  	
}

/*****************************************************************************/
/**
 * @brief:  Cai dat che do GPS - CGPS x,zzzzzz
 * @param:  
 * @retval:
 *	@author:	
 *	@created:
 */
void CaiDatGPSMode(char* Buffer)
{
	char tmpBuf[5] = {0};
	uint8_t pos, i = 0, result = 0, tmpByte;
	uint8_t leng = strlen(Buffer);
	
	char *Message = strstr(&Buffer[0],"CGPS");
	if(Message == NULL) return;
	
	//CGPS x,zzzzzz
	pos = 4;
	while(Message[pos] != ',' && i < 5)
	{
		if(Message[pos] != 32) //Space
		{
			tmpBuf[i++] = Message[pos];
		}
		pos++;
	}	
	tmpByte = GetNumberFromString(0, tmpBuf);
				
	//Set cau hinh
    memset(SendSMSBuffer, 0, 160);
    memset(ConfigBuffer, 0, 120);
	sprintf(ConfigBuffer, "SET,%d,(%d)", CFG_GPSMode, tmpByte);
	result = ProcessSetConfig(ConfigBuffer, CF_SMS);

	tmpByte = sprintf(SendSMSBuffer, "RMSG %d,", result > 0 ? 1 :0);
	pos++;
	while(Message[pos] != NULL && pos < leng)
		SendSMSBuffer[tmpByte++] = Message[pos++];
	
	SendResponseMessage();  	
}

/*****************************************************************************/
/**
 * @brief:  Cai dat thoi gian gui tin, CTIME zzzzzz,xxxx,yyyy,...
 * @param:  
 * @retval:
 *	@author:	
 *	@created:
 */
void	CaiDatThoiGian(char* Buffer)
{
	char tmpBuf[15] = {0};
	uint8_t pos, i = 0, result = 0, tmpByte;
	uint8_t leng = strlen(Buffer);
	
	char *Message = strstr(&Buffer[0],"CTIME");
	if(Message == NULL) return;
	
	//CTIME zzzzzz,xxxx,yyyy,...
	pos = 5;
	while(Message[pos] != ',' && i < 15)
	{
		if(Message[pos] != 32) //Space
		{
			tmpBuf[i++] = Message[pos];
		}
		pos++;
	}	
	
	//Set cau hinh
    memset(SendSMSBuffer, 0, 160);
    memset(ConfigBuffer, 0, 120);
	tmpByte = sprintf(&ConfigBuffer[0], "SET,%d,(", CFG_TimeSend);
	
	pos++; i = 0;
	while(Message[pos] != NULL && i < 120)
	{
		ConfigBuffer[tmpByte + i] = Message[pos++];
		i++;
	}
	strcat(ConfigBuffer, ")");
	result = ProcessSetConfig(ConfigBuffer, CF_SMS);
	sprintf(SendSMSBuffer, "RMSG %d,%s", result > 0 ? 1 :0, tmpBuf);
	
	SendResponseMessage();  		
}

/*
* Build ban tin phan hoi lenh CINFO
*/
static uint8_t BuildResponseMessage(uint8_t LoaiBanTin)
{
	uint8_t ViTri, ViTriTinhCheckSum = 0, CheckSum = 0;
	float DienApAcQuy = 0;
	
	memset(SendSMSBuffer, 0, 160);
	ViTri = 0;
	
	/* Add header message */
	if(xSystem.Parameters.ServerType == MAKIPOS)
		ViTri += sprintf(&SendSMSBuffer[ViTri], "!AIVDO,");		//VJCOM -> Server Makipos chua sua thanh VJCOM
	else if(xSystem.Parameters.ServerType == CUSTOMER)
		ViTri += sprintf(&SendSMSBuffer[ViTri], "!AIVDO,");
	
	ViTriTinhCheckSum = ViTri;
	
	/* Loai ban tin */
	ViTri += sprintf(&SendSMSBuffer[ViTri], "%d,", LoaiBanTin);
	
	/* Ma thiet bi */
	ViTri += sprintf(&SendSMSBuffer[ViTri], "%s,", xSystem.Parameters.DeviceID);
	
	/* Trong cac truong hop sau khong gui truong toa do ve:
		- GPS mode = 0
		- Khong co toa do
		- Khong bat GPS
	*/
	if(LoaiBanTin == DATA_DINHKY || LoaiBanTin == DATA_CANHBAOVITRI) {
		if(GPS_GetPosition().ViDo.value != 0 && GPS_GetPosition().KinhDo.value != 0
			&& xSystem.Parameters.GPSMode != 0) {
				/* Vi Do */
				ViTri += sprintf(&SendSMSBuffer[ViTri], "%f", GPS_GetPosition().ViDo.value);	//9.4lf
		}
	}

	ViTri += sprintf(&SendSMSBuffer[ViTri], ",");

	if(LoaiBanTin == DATA_DINHKY || LoaiBanTin == DATA_CANHBAOVITRI) {
		if(GPS_GetPosition().ViDo.value != 0 && GPS_GetPosition().KinhDo.value != 0
			&& xSystem.Parameters.GPSMode != 0) {
				/* Kinh Do */
				ViTri += sprintf(&SendSMSBuffer[ViTri], "%f", GPS_GetPosition().KinhDo.value); //10.4lf
			}
	}
	
	ViTri += sprintf(&SendSMSBuffer[ViTri], ",");

	if(LoaiBanTin == DATA_DINHKY || LoaiBanTin == DATA_CANHBAOACQUY) {
		/* Dien ap ac quy - Volt */
		DienApAcQuy = (float)GetBatteryPower()/1000;
		ViTri += sprintf(&SendSMSBuffer[ViTri], "%4.2f", DienApAcQuy);		//5.2f
	}

	ViTri += sprintf(&SendSMSBuffer[ViTri], ",");
	
	if(LoaiBanTin == DATA_DINHKY || LoaiBanTin == DATA_CANHBAODEN) {
		/* Dong tieu thu den - mA*/
		ViTri += sprintf(&SendSMSBuffer[ViTri], "%u", xSystem.MeasureStatus.Current);
	}

	ViTri += sprintf(&SendSMSBuffer[ViTri], ",");
	
//	if(LoaiBanTin == DATA_DINHKY) {
//		/* Chu ky chop den */
//		ViTri += sprintf(&SendSMSBuffer[ViTri], "%03u", Indicator_GetUserBlinkCode(xSystem.Parameters.BlinkType));
//	}
		
	/* Tinh checksum */
	CheckSum = CalculateCheckSum((uint8_t*)SendSMSBuffer, ViTriTinhCheckSum, ViTri - ViTriTinhCheckSum) & 0xFF;
		
	/* Add tail & checksum */
	ViTri += sprintf(&SendSMSBuffer[ViTri], "*%02X", CheckSum);
	
	return ViTri;
}


/*****************************************************************************/
/**
 * @brief	:  
 * @param	:  
 * @retval	:
 * @author	:	
 * @created	:	10/11/2014
 * @version	:
 * @reviewer:	
 */
static void GuiTrangThai(void)
{
//31-10, QT: Gui trang thai chi gui DATA_DINHKY
//	if(xSystem.Status.DistanceAlarm == 0 &&
//		xSystem.Status.LowBattAlarm == 0 &&
//		xSystem.Status.LedBulbAlarm == 0)
	{
		BuildResponseMessage(DATA_DINHKY);
		SendResponseMessage();  
		return;
	}
}

/*****************************************************************************/
/**
 * @brief	:  
 * @param	:  
 * @retval	:
 * @author	:	
 * @created	:	10/11/2014
 * @version	:
 * @reviewer:	
 */
void gsm_process_cmd_from_sms(char* Buffer)
{     	  
	/* Lenh khong thuoc 1 trong 4 lenh nay thi upcase */
	if(strstr(Buffer, "SET,") == NULL || strstr(Buffer, "CSV") == NULL || 
		strstr(Buffer, "UDFW(") == NULL || strstr(Buffer, "CPASS") == NULL)
	{
		UpcaseData(Buffer);
	}

	if(TachSoDienThoai(Buffer) == 0) return;
	
	//Copy noi dung SMS
	CopySMSContent(Buffer);
			
	//Gan lai noi dung
	Buffer = SMSContent;
 
	DEBUG ("\r\nSMS: From %s, content: %s. ", SoDienThoai, Buffer);

	if(strstr(Buffer, "SET,"))
	{
		/** Gui SDT cau hinh len server */
		char SDT[25] = {0};
		sprintf(SDT, "SMS:%s", SoDienThoai);
		MQTT_SendBufferToServer(SMSContent, SDT);
		
		DEBUG_PRINTF("SET config");
		ProcessSetParameters(Buffer, CF_SMS);
		return;
	}
	
	if(strstr(Buffer, "GET,"))
	{
		DEBUG_PRINTF("GET config");
		ProcessGetParameters(strstr(Buffer, "GET,"), CF_SMS);
		return;
	} 

	if(strstr(Buffer, "INFO#"))
	{
		GuiINFO();
		return;
	} 
	
	if(strstr(Buffer, "TRANGTHAI#"))
	{
		gsm_send_status_to_mobilephone(SoDienThoai);
		return;
	} 

	/** Update Firmware: 
	* UDFW,<ftp IP>,<ftp username>,<ftp pass>,<ftp filename>,<file size>,<ftp file CRC>
	*/
	if(strstr(Buffer,"UDFW("))
	{
		if(xSystem.Status.FlashState == TRANGTHAI_FAIL)
		{
			SendSMS(SoDienThoai, ":( Flash ERR!");
			return;
		}
		
		SendSMS(SoDienThoai, "Chuan bi update FW");
		ProcessUpdateFirmwareCommand(Buffer, CF_SMS);
	}
			
	if(strstr(Buffer, "CLEARALARM#"))
	{
		xSystem.Status.DistanceAlarm = 0;
		xSystem.Status.LowBattAlarm = 0;
		xSystem.Status.LedBulbAlarm = 0;
		xSystem.Status.Alarm30Min = 0;
		xSystem.Status.VminAlarm = 0;
		
		//23/08/20: Clear in memory
		RTC_WriteBackupRegister(ALARM_STATE_ADDR, 0);
		SendSMS(SoDienThoai, "Da clear all alarm!");
		return;
	}
	
	if(strstr(Buffer, "GUIBANTIN#"))
	{
		xSystem.Status.YeuCauGuiTin = 1;
		return;
	}
	
	if(strstr(Buffer, "SA30M#"))
	{
		xSystem.Status.Alarm30Min = 1;
		return;
	}
	
	//Add 03-11: Clear Operation time
	if(strstr(Buffer, "CLEAROPTIME#"))
	{
		xSystem.MeasureStatus.operatingTime = 1;
		RTC_WriteBackupRegister(OPERATINGTIME_ADDR, 1);		

		SendSMS(SoDienThoai, "Da reset OpTime");
		return;
	}	
		
	if(strstr(Buffer, "RESET#"))
	{
		SystemReset(5);
		return;
	}
	
	if(strstr(Buffer, "CPN"))
	{
		DEBUG_PRINTF("Dang ky SDT");
		DangKySDT(Buffer);
		return;
	}	
	
	/* ====================== LENH SMS ======================= */
	/* Kiem tra so dien thoai nhan tin co thuoc danh sach cho phep */
	if(!ValidSMSInListControl(SoDienThoai)) 
	{
		DEBUG ("\r\nSDT khong duoc cho phep cau hinh");
		return;
	}
	
	if(strstr(Buffer, "CPASS"))
	{
		DEBUG_PRINTF("Doi mat khau");
		DoiMatKhau(Buffer);
		return;
	}

	if(strstr(Buffer, "CPOS"))
	{
		DEBUG_PRINTF("Cai dat toa do");
		CaiDatToaDo(Buffer);
		return;
	}	

	if(strstr(Buffer, "CDIS"))
	{
			DEBUG_PRINTF("Cai dat khoang cach");
			CaiDatKhoangCach(Buffer);
			return;
	}
	
	if(strstr(Buffer, "CCAP"))
	{
			DEBUG_PRINTF("Cai dat cong suat");
			CaiDatCongSuat(Buffer);
			return;
	}

	if(strstr(Buffer, "CID"))
	{
			DEBUG_PRINTF("Cai dat ma phao");
			CaiDatMaPhao(Buffer);
			return;
	}

	if(strstr(Buffer, "CSV"))
	{
		DEBUG_PRINTF("Cai dat server");
		CaiDatServer(Buffer);
		return;
	}

	if(strstr(Buffer, "CTB"))
	{
			DEBUG_PRINTF("Cai dat kieu chop");
			CaiDatKieuChop(Buffer);
			return;
	}

	if(strstr(Buffer, "CGPS"))
	{
			DEBUG_PRINTF("Cai dat che do GPS");
			CaiDatGPSMode(Buffer);
			return;
	}

	if(strstr(Buffer, "CTIME"))
	{
			DEBUG_PRINTF("Cai dat thoi gian gui tin");
			CaiDatThoiGian(Buffer);
			return;
	}
	
	if(strstr(Buffer, "CINFO"))
	{
			DEBUG_PRINTF("Gui trang thai");
			GuiTrangThai();
			return;
	}
	
	if(strstr(Buffer, "GPS+") || strstr(Buffer, "GPS-"))
	{
		if(strstr(Buffer, "GPS+"))
			xSystem.Parameters.GPSEnable = 1;
		else if(strstr(Buffer, "GPS-")) 
			xSystem.Parameters.GPSEnable = 0;
		
		memset(ConfigBuffer, 0, 120);
		sprintf(ConfigBuffer, "SET,%d,(%d)", CFG_GPSEnable, xSystem.Parameters.GPSEnable);
		ProcessSetConfig(ConfigBuffer, CF_SMS);
		return;
	}

	if(strstr(Buffer, "SYN+") || strstr(Buffer, "SYN-"))
	{
		if(strstr(Buffer, "SYN+")) xSystem.Parameters.GPSSync = 1;
		else if(strstr(Buffer, "SYN-")) xSystem.Parameters.GPSSync = 0;
		
		memset(ConfigBuffer, 0, 120);
		sprintf(ConfigBuffer, "SET,%d,(%d)", CFG_GPSSync, xSystem.Parameters.GPSSync);
		ProcessSetConfig(ConfigBuffer, CF_SMS);
		return;
	}
	
#if	__USE_MQTT__ == 0
	if(strstr(Buffer, "VJL+"))
	{
		xSystem.Parameters.ServerType = MAKIPOS;
		memset(ConfigBuffer, 0, 120);
		sprintf(ConfigBuffer, "SET,%d,(%d)", CFG_ServerType, MAKIPOS);
		ProcessSetConfig(ConfigBuffer, CF_SMS);
		xSystem.Parameters.CurrentHost = xSystem.Parameters.MainHost;
		return;
	}
	
	if(strstr(Buffer, "VJL-"))
	{
		xSystem.Parameters.ServerType = CUSTOMER;
		memset(ConfigBuffer, 0, 120);
		sprintf(ConfigBuffer, "SET,%d,(%d)", CFG_ServerType, CUSTOMER);
		ProcessSetConfig(ConfigBuffer, CF_SMS);
		xSystem.Parameters.CurrentHost = xSystem.Parameters.AlterHost;
		return;
	}
#endif

#if __TEST_UDFW_FTP__	
	//Update Firmware tu server mac dinh
	//Cu phap: UDFW(file size,file CRC)
	if(strstr(Buffer,"UDFW("))
	{		
		char UDFWBuffer[20] = {0};
		char CRCBuff[12] = {0};
				
		if(xSystem.Status.FlashState == TRANGTHAI_FAIL)
		{
			SendSMS(SoDienThoai, ":( No Flash!");
			return;
		}
		
		if(CopyParameter((char*)Buffer, UDFWBuffer, '(', ')') == 0) return;
		
		//Get file size
		xSystem.FileTransfer.FileSize.value = GetNumberFromString(0, UDFWBuffer);
		
		//Get file CRC
		strcat(UDFWBuffer, ")");
		if(CopyParameter(UDFWBuffer, CRCBuff, ',', ')') == 0) return;
		xSystem.FileTransfer.FileCRC.value = GetNumberFromString(0, CRCBuff);
		
		SendSMS(SoDienThoai, "Chuan bi update FW");
		
		UDFW_UpdateMainFWTest(0);
		return;
	}

	//Update Firmware tu server tuy chon, server FTP, Port mac dinh 21, mac dinh ten file phai la LOB_MAIN.bin hoac LOB_BLDER.bin
	//UDFW2(124.35.166.24,user:abcd,pass:abcd,size:131415,crc:536327)
	if(strstr(Buffer,"UDFW2("))
	{
		uint8_t i = 0;
		char* pos = NULL;
		char tmpBuff[12] = {0};
		
		if(xSystem.Status.FlashState == TRANGTHAI_FAIL)
		{
			SendSMS(SoDienThoai, ":( No Flash!");
			return;
		}
		
		memset(SMSResponseBuffer, 0, 160);
		if(CopyParameter((char*)Buffer, SMSResponseBuffer, '(', ')') == 0) return;
		
		//Get IP Address
		while(SMSResponseBuffer[i] != ',' && i < 15)
		{
			xSystem.FileTransfer.FTP_IPAddress[i] = SMSResponseBuffer[i];
			i++;
		}
		GetIPAddressFromString(xSystem.FileTransfer.FTP_IPAddress, &xSystem.FileTransfer.FTP_IP);
		
		//Get user
		pos = strstr(SMSResponseBuffer, "USER:");
		if(pos == NULL) return;
		i = 5;
		while(pos[i] != ',' && i < 16)
		{
			xSystem.FileTransfer.FTP_UserName[i-5] = pos[i];
			i++;
		}
		
		//Get password
		pos = NULL;
		pos = strstr(SMSResponseBuffer, "PASS:");
		if(pos == NULL) return;
		i = 5;
		while(pos[i] != ',' && i < 16)
		{
			xSystem.FileTransfer.FTP_Pass[i-5] = pos[i];
			i++;
		}
		
		//Get File size
		pos = NULL;
		pos = strstr(SMSResponseBuffer, "SIZE:");
		if(pos == NULL) return;
		i = 5;
		while(pos[i] != ',' && i < 14)
		{
			tmpBuff[i-5] = pos[i];
			i++;
		}
		xSystem.FileTransfer.FileSize.value = GetNumberFromString(0, tmpBuff);
		
		//Get File CRC
		pos = NULL;
		memset(tmpBuff, 0, 12);
		pos = strstr(SMSResponseBuffer, "CRC:");
		if(pos == NULL) return;
		i  = 4;
		while(pos[i] != NULL && i < 13)
		{
			tmpBuff[i-4] = pos[i];
			i++;
		}
		xSystem.FileTransfer.FileCRC.value = GetNumberFromString(0, tmpBuff);
		
		DEBUG ("\r\nUDFW: Download FW. IP:%s, User:%s, Pass:%s, Size:%d, CRC:%d", xSystem.FileTransfer.FTP_IPAddress, 
			xSystem.FileTransfer.FTP_UserName, xSystem.FileTransfer.FTP_Pass,
			xSystem.FileTransfer.FileSize.value, xSystem.FileTransfer.FileCRC.value);
		
		SendSMS(SoDienThoai, "Chuan bi update FW");
		
		UDFW_UpdateMainFWTest(1);
		return;
	}
#endif	//__TEST_UDFW_FTP__
}

/********************************* END OF FILE *******************************/

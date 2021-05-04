/******************************************************************************
 * @file    	GSM_DataLayer.c
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
#include "GSM.h"
#include "DataDefine.h"
#include "Utilities.h"
#include "Main.h"
//#include "Parameters.h"
#include "Hardware.h"
#include "HardwareManager.h"

/******************************************************************************
                                   GLOBAL VARIABLES					    			 
 ******************************************************************************/
extern System_t xSystem;
//extern SMSStruct_t SMSMemory[3];
extern GSM_Manager_t GSM_Manager;

/******************************************************************************
                                   GLOBAL FUNCTIONS					    			 
 ******************************************************************************/

/******************************************************************************
                                   DATA TYPE DEFINE					    			 
 ******************************************************************************/
#define	GET_BTS_INFOR_TIMEOUT 300
/******************************************************************************
                                   PRIVATE VARIABLES					    			 
 ******************************************************************************/
static char tmpBuffer[50] = {0};
static char LenhATCanGui[30] = {0};
static uint8_t YeuCauGuiATC = 0;
static uint8_t TimeOutGuiLenhAT = 0;

uint8_t WaitGSMReady = 0;
uint8_t GSMNotReady = 0;
uint8_t SendATOPeriod = 0;
uint8_t ModuleNotMounted = 0;
uint8_t InSleepModeTick = 0;
uint8_t TimeoutToSleep = 0;
uint16_t GetSignalLevelTimeOut = 0;

/******************************************************************************
                                   LOCAL FUNCTIONS					    			 
 ******************************************************************************/
void PowerOnModuleGSM(GSM_ResponseEvent_t event, void *ResponseBuffer);
void OpenPPPStack(GSM_ResponseEvent_t event, void *ResponseBuffer);
void QuerySMS(void);
void GSM_ReadSMS(GSM_ResponseEvent_t event, void *ResponseBuffer);
void GSM_SendSMS(GSM_ResponseEvent_t event, void *ResponseBuffer);
void GSM_GetBTSInfor(GSM_ResponseEvent_t event, void *ResponseBuffer);
void GSM_SendATCommand(GSM_ResponseEvent_t event, void *ResponseBuffer);
void GSM_GotoSleepMode(GSM_ResponseEvent_t event, void *ResponseBuffer);
void GSM_ExitSleepMode(GSM_ResponseEvent_t event, void *ResponseBuffer);
void GSM_HardReset(void);
uint8_t CheckReadyStatus(void);
uint8_t CheckGSMIdle(void);

uint8_t isGSMSleeping(void)
{
//	return GPIO_ReadOutputDataBit(GSM_DTR_PORT, GSM_DTR_PIN);
}

void GSM_GotoSleep(void)
{
	ChangeGSMState(GSM_GOTOSLEEP);
}
	
/******************************************************************************/
/**
 * @brief	:  GSM thức dậy gửi tin định kỳ
 * @param	:  
 * @retval	:
 * @author	:	
 * @created	:	15/01/2016
 * @version	:
 * @reviewer:	
 */
void gsmWakeUpPeriodically(void)
{
//	xSystem.Status.GSMSleepTime++;
//	if(xSystem.Status.GSMSleepTime >= xSystem.Parameters.TGGTDinhKy*60)
//	{
//		xSystem.Status.GSMSleepTime = 0;

//#if (__GSM_SLEEP_MODE__)		
//		DEBUG("\rGSM: Wakeup to send msg");
//		xSystem.Status.YeuCauGuiTin = 2;
//		ChangeGSMState(GSM_WAKEUP);
//#else
//		xSystem.Status.YeuCauGuiTin = 3;
//#endif
//	}
	
//	//Lưu lại biến đếm vào RTC Reg sau mỗi 10s
//	if(xSystem.Status.GSMSleepTime % 10 == 0)
//	{
//		RTC_WriteBackupRegister(GSM_SLEEP_TIME_ADDR, xSystem.Status.GSMSleepTime);
//	}
}
	

/******************************************************************************/
/**
 * @brief	:  
 * @param	:  
 * @retval	:
 * @author	:	
 * @created	:	15/01/2016
 * @version	:
 * @reviewer:	
 */
void GSM_ManagerTask(void) 
{		
	if(xSystem.Status.InitSystemDone != 1) return;
	if(ModuleNotMounted == 1) return;
	if(GSM_Manager.isGSMOff == 1) return;

	ResetWatchdog();
	
#if	__GSM_CHECK_STATUS__
	/* Kiem tra Module Ready truoc khi gui lenh AT */
	if(GSM_Manager.FirstTimePower == 0)
	{
		if(CheckReadyStatus())
		{
			if(WaitGSMReady == 0) DEBUG("\rGSM: Wait for ready in 5s");
			WaitGSMReady++;
			if(WaitGSMReady >= 5)
			{
				WaitGSMReady = 0;
				GSM_Manager.FirstTimePower = 1;
				InitGSM_DataLayer();
			} else {
				DEBUG(".");
				return;
			}
		}
		else {
			DEBUG("\rGSM: Initiating...");
			GSMNotReady++;
			
			/* Tao xung |_| de Power Off module, min 1s  */
			if(GSMNotReady == 5) {
				GPIO_WriteBit(GSM_POWERKEY_PORT, GSM_POWERKEY_PIN, Bit_SET);
			}
			if(GSMNotReady == 7) {
				GPIO_WriteBit(GSM_POWERKEY_PORT, GSM_POWERKEY_PIN, Bit_RESET);
			}
			
			/* Tao xung |_| de Power On module, min 1s  */
			if(GSMNotReady == 10) {
				GPIO_WriteBit(GSM_POWERKEY_PORT, GSM_POWERKEY_PIN, Bit_SET);
			}
			if(GSMNotReady == 12) {
				GPIO_WriteBit(GSM_POWERKEY_PORT, GSM_POWERKEY_PIN, Bit_RESET);
			}
			
			if(GSMNotReady > 20) {
				ModuleNotMounted = 1;
				SystemReset(11);	//Phinht: Add 07/06/18
			}
			return;
		}
	}
#else
	if(GSM_Manager.FirstTimePower == 0)
	{
		GSM_Manager.FirstTimePower = 1;
		InitGSM_DataLayer();
	}
#endif	//__GSM_CHECK_STATUS__

	
	/* Cac trang thai lam viec module GSM */
	switch(GSM_Manager.State)
	{
		case GSM_POWERON:
			if(GSM_Manager.Step == 0)
			{
				GSM_Manager.Step = 1;
				SendATCommand("ATV1\r","OK",1000,30, PowerOnModuleGSM);	
			}
			break;
			
		case GSM_OK :	/* PPP data mode */
			if(GSM_Manager.GSMReady == 2)
			{
				SendATOPeriod++;
				if(SendATOPeriod > 2)
				{
					SendATOPeriod = 0;
					SendATCommand("ATO\r","CONNECT",1000,10,NULL);   
				}
			} else SendATOPeriod = 0;
			QuerySMS();
			
			/* Nếu không thực hiện công việc gì khác -> vào sleep sau 60s */
#if (__GSM_SLEEP_MODE__)
			if(TimeoutToSleep++ >= 60)
			{
				TimeoutToSleep = 0;
				/* Chỉ sleep khi :
					- Đang không UDFW 
					- Đang không trong thời gian gửi tin
				*/
				if(xSystem.FileTransfer.Retry == 0 && xSystem.Status.SendGPRSTimeout == 0) {
					DEBUG("\rGSM: Het viec roi, ngu thoi em...");
					GSM_Manager.Step = 0;
					GSM_Manager.State = GSM_GOTOSLEEP;
				}
			}
#else
			gsmWakeUpPeriodically();
#endif
			break;
		
		case GSM_RESET:		/* Hard Reset */
//			if(GSM_Manager.FirstTimePower != 0)
			{
				GSM_Manager.GSMReady = 0;
				GSM_HardReset();
			}
			break;
		
		case GSM_READSMS:		/* Read SMS */
			if(GSM_Manager.Step == 0)
			{
				GSM_Manager.Step = 1;
				if(GSM_Manager.PPPCommandState == 0)
					SendATCommand ("+++", "OK", 2200, 5, GSM_ReadSMS);  
				else
					SendATCommand ("ATV1\r", "OK", 1000, 1, GSM_ReadSMS); 
			}
			break;
			
		case GSM_SENSMS :		/* Send SMS */
			if(!GSM_Manager.GSMReady) break;
			if(GSM_Manager.Step == 0)
			{			
				GSM_Manager.Step = 1;
				SendATCommand ("ATV1\r", "OK", 100, 1, GSM_SendSMS); 
			}
			break;
			
		case GSM_REOPENPPP:		/* Reopen PPP if lost */	
			if(GSM_Manager.Step == 0)
			{
				GSM_Manager.Step = 1;
				SendATCommand ("ATV1\r", "OK", 1000, 1, OpenPPPStack);
			}
			break;		
			
		case GSM_GETBTSINFOR:		/* Get GSM Signel level */
			if(GSM_Manager.Step == 0)
			{
				GSM_Manager.Step = 1;
				SendATCommand ("ATV1\r", "OK", 100, 1, GSM_GetBTSInfor); 
			}			
			break;
			
		case GSM_SENDATC:		/* Send AT command */
			if(GSM_Manager.Step == 0)
			{
				GSM_Manager.Step = 1;
				SendATCommand ("ATV1\r", "OK", 100, 1, GSM_SendATCommand); 
			}	
			break;
			
		case GSM_GOTOSLEEP:	/* Vao che do sleep */
			if(GSM_Manager.Step == 0)
			{
				GSM_Manager.Step = 1;
				SendATCommand ("ATV1\r", "OK", 100, 1, GSM_GotoSleepMode); 
			}	
			break;
		case GSM_WAKEUP:	/* Thoat che do sleep */
			if(GSM_Manager.Step == 0)
			{
				GSM_Manager.Step = 1;
				SendATCommand ("ATV1\r", "OK", 100, 1, GSM_ExitSleepMode); 
			}
			break;
		case GSM_SLEEP:	/* Dang trong che do Sleep */
			if(InSleepModeTick % 10 == 0) {
				DEBUG("\rGSM is sleeping...");
			}
			InSleepModeTick++;
			
			/* Thức dậy gửi tin định kỳ */
			gsmWakeUpPeriodically();
			break;
	}

	//Timeout TAT GSM/GPS sau khi reset neu la DEN CAU
#if	__DENCAU__
	if(GSM_Manager.TimeOutOffAfterReset)
	{
		GSM_Manager.TimeOutOffAfterReset--;
		if(GSM_Manager.TimeOutOffAfterReset == 0 && xSystem.Parameters.DenCauMode) {
			if(GSM_Manager.isGSMOff == 0 && xSystem.Status.SendGPRSTimeout == 0) {
				DEBUG("\rAfter reset: GSM/GPS OFF");
				ppp_close();
				GSM_PowerControl(GSM_OFF);
				GPS_PowerControl(GPS_OFF);
			}
		}				
	}
#endif
		
	//Khong co SIM
	if(strlen(xSystem.Parameters.SIM_IMEI) < 5)
		return;
	
	/* Giám sát thời gian gửi bản tin, 3 lần thức dậy ko gửi được phát nào -> reset GSM, reset mạch */
	xSystem.Status.GSMSendFailedTimeout++;
	if(xSystem.Status.GSMSendFailedTimeout > 3*xSystem.Parameters.TGGTDinhKy*60)
	{
		xSystem.Status.GSMSendFailedTimeout = 0;
		
		DEBUG("\rGSM: Send Failed timeout...");
		ChangeGSMState(GSM_RESET);
		return;
	}
	
	/* Lay cuong do song sau moi 5 phut  */
	if(GSM_Manager.GSMReady == 1)
	{
		GetSignalLevelTimeOut++;
		if(GetSignalLevelTimeOut >= GET_BTS_INFOR_TIMEOUT)	//300 
		{
			if(CheckGSMIdle() && GSM_Manager.Dial == 0)
			{
				GetSignalLevelTimeOut = 0;
				GSM_Manager.Step = 0;
				GSM_Manager.GetBTSInfor = 0;
				GSM_Manager.State = GSM_GETBTSINFOR;
			}
			else GetSignalLevelTimeOut = GET_BTS_INFOR_TIMEOUT - 30;	//check lai sau 30s
		}
	}
	
	/* Yeu cau gui lenh AT */
	if(GSM_Manager.GSMReady == 1 && YeuCauGuiATC == 1)
	{
		if(TimeOutGuiLenhAT && (TimeOutGuiLenhAT % 5 == 0))	//check lai sau 5s
		{
			if(CheckGSMIdle())
			{
				//Du dieu kien thi gui lenh AT
				GSM_Manager.Step = 0;
				GSM_Manager.State = GSM_SENDATC;
			}
		}
		if(TimeOutGuiLenhAT) {
			TimeOutGuiLenhAT--;
			if(TimeOutGuiLenhAT == 0) {
				YeuCauGuiATC = 0;
				DEBUG("\rHet thoi gian thuc hien lenh AT!");
			}
		}
	}
}

/*
* Kiem tra trang thai chan STATUS module GSM
*/
#if 0
uint8_t CheckReadyStatus(void)
{
	return (GPIO_ReadInputDataBit(GSM_STATUS_PORT, GSM_STATUS_PIN));
}
#endif

uint8_t CheckGSMIdle(void)
{
	if(!GSM_Manager.GSMReady) return 0;
	if(GSM_Manager.RISignal) return 0;
	if(GSM_Manager.State != GSM_OK) return 0;
	if(xSystem.FileTransfer.State != FT_NO_TRANSER) return 0;
	
	//Dang gui TCP -> busy
	if(xSystem.Status.GuiGoiTinTCP) return 0;
	if(xSystem.Status.SendGPRSTimeout) return 0;
	
	return 1;
}
	
/*****************************************************************************/
/**
 * @brief	:  
 * @param	:  
 * @retval	:
 * @author	:	
 * @created	:	15/01/2014
 * @version	:
 * @reviewer:	
 */

void InitGSM_DataLayer(void)
{
	GSM_Manager.RISignal = 0;
	GSM_Manager.Dial = 0;
	GSM_Manager.TimeOutConnection = 0;
	
	ChangeGSMState(GSM_POWERON);		
}

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
void ChangeGSMState(GSM_State_t NewState)
{      
	if(NewState == GSM_OK)	//Command state -> Data state trong PPP mode
	{
		SendATCommand("ATO\r","CONNECT",1000,10,NULL);   
		GSM_Manager.GSMReady = 2;
		GSM_Manager.Mode = GSM_PPP_MODE;
		GSM_Manager.PPPCommandState = 0;	

		TimeoutToSleep = 0;
	}
	else
	{
		GSM_Manager.Mode = GSM_AT_MODE;
		
		if(NewState == GSM_RESET) {
			ModuleNotMounted = 0;
			GSM_Manager.FirstTimePower = 1;
		}
	}
		
	DEBUG("\rChange GSM state to: "); 
	switch ((uint8_t)NewState)
	{
		case 0: DEBUG("OK"); break;
		case 1: DEBUG("RESET"); break;
		case 2: DEBUG("SENSMS"); break;
		case 3: DEBUG("READSMS"); break;
		case 4: DEBUG("POWERON"); break;
		case 5: DEBUG("REOPENPPP"); break;
		case 6: DEBUG("GETSIGNAL"); break;
		case 7: DEBUG("SENDATC"); break;
		case 8: DEBUG("GOTOSLEEP"); break;
		case 9: DEBUG("WAKEUP"); break;
		case 10: DEBUG("SLEEP"); break;
		default: break;
	}		
	GSM_Manager.State = NewState;
	GSM_Manager.Step = 0; 
}

#if __USING_MODULE_2G__
/*****************************************************************************/
/**
 * @brief	:  
 * @param	:  
 * @retval	:
 * @author	:	
 * @created	:	10/11/2016
 * @version	:
 * @reviewer:	
 */
void PowerOnModuleGSM(GSM_ResponseEvent_t event, void *ResponseBuffer)
{	
	switch(GSM_Manager.Step)
	{
		case 1:
			DEBUG("\rKet noi module GSM: ");
			if(event == EVEN_OK) {
				DEBUG("[OK]");
				GSM_Manager.FirstTimePower = 1;
			}
			else
				DEBUG("[FAIL]");
			SendATCommand ("ATE0\r", "OK", 1000, 10, PowerOnModuleGSM);
			break;
		case 2:
			DEBUG("\rThiet lap che do command echo: %s",(event == EVEN_OK) ? "[OK]" : "[FAIL]");	            						
			SendATCommand ("AT+CMEE=2\r", "OK", 1000, 10, PowerOnModuleGSM);
			break;
		case 3:
			DEBUG("\rThiet lap che do thong bao loi: %s",(event == EVEN_OK) ? "[OK]" : "[FAIL]");	
			SendATCommand ("AT+CGSN\r", "OK", 1000, 5, PowerOnModuleGSM);
			break;
		case 4:		
			GSM_GetIMEI(GSMIMEI,ResponseBuffer);
			DEBUG("\rThuc hien lay GSM IMEI: %s", xSystem.Parameters.GSM_IMEI);
			SendATCommand ("AT+CIMI\r", "OK", 1000, 10, PowerOnModuleGSM);			
			break;
		case 5:
			GSM_GetIMEI(SIMIMEI,ResponseBuffer);
			DEBUG("\rThuc hien lay SIM IMEI: %s", xSystem.Parameters.SIM_IMEI);
			if(strlen(xSystem.Parameters.SIM_IMEI) < 15) {
				DEBUG("Khong co SIM");
				//Neu ko nhan SIM -> reset luon
				//SystemReset(13);
			}
			SendATCommand ("AT+QIDEACT=1\r", "OK", 3000, 1, PowerOnModuleGSM);
			break;
		case 6:
			DEBUG("\rDeactive PDP context: %s",(event == EVEN_OK) ? "[OK]" : "[FAIL]");
			SendATCommand ("AT+CMGF=1\r", "OK", 1000, 10, PowerOnModuleGSM);
			break;	
		case 7:	
			DEBUG("\rThiet lap SMS o che do text: %s", (event == EVEN_OK) ? "[OK]" : "[FAIL]");  			
			SendATCommand ("AT+CNMI=2,1,0,0,0\r", "OK", 1000, 10, PowerOnModuleGSM);
            break;
		case 8:
			DEBUG("\rThiet lap che do nhan SMS: %s", (event == EVEN_OK) ? "[OK]" : "[FAIL]");
			SendATCommand ("AT+CMGD=1,4\r", "OK", 1000, 5, PowerOnModuleGSM);
			break;
		case 9:
			printf("\rXoa tat ca tin nhan SMS: %s",(event == EVEN_OK) ? "[OK]" : "[FAIL]");            
			SendATCommand ("AT+CGDCONT=1,\"IP\",\"v-internet\"\r", "OK", 1000, 2, PowerOnModuleGSM);            
			break;
		case 10:
			printf("\rThiet lap APN : %s",(event == EVEN_OK) ? "[OK]" : "[FAIL]");										
			SendATCommand ("AT+QIACT=1\r", "OK", 5000, 5, PowerOnModuleGSM);
			break;
		case 11:
			printf("\rActive PDP: %s",(event == EVEN_OK) ? "[OK]" : "[FAIL]");
			SendATCommand ("AT+CGREG=1\r", "OK", 1000, 3, PowerOnModuleGSM);			
			break;
		case 12:
			printf("\rRegister GPRS network: %s",(event == EVEN_OK) ? "[OK]" : "[FAIL]");
			SendATCommand ("AT+QSCLK=1\r", "OK", 1000, 5, PowerOnModuleGSM);
			break;
		case 13:
			DEBUG("\rAuto Sleep: %s",(event == EVEN_OK) ? "[OK]" : "[FAIL]");
			SendATCommand ("AT+CSQ\r", "OK", 1000, 5, PowerOnModuleGSM);
			break;
		case 14:
			GSM_Manager.Step = 0;
			if(event != EVEN_OK) { 
				/* Reset GSM */
				ChangeGSMState(GSM_RESET);
			}
			else {
				GSM_GetSignalStrength(ResponseBuffer);
				DEBUG("\rCuong do song: %d", xSystem.Status.CSQ);
				SendATCommand("ATV1\r", "OK", 1000, 1, OpenPPPStack);
			}
			break;		
	}
	GSM_Manager.Step++;
}
#endif	//__USING_MODULE_2G__

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
void PowerOnModuleGSM(GSM_ResponseEvent_t event, void *ResponseBuffer)
{	
	switch(GSM_Manager.Step)
	{
		case 1:
			if(event == EVEN_OK)
			{
				DEBUG("\rConnect modem OK");
			}
			else
			{
				DEBUG("\rConnect modem ERR!");
				//SystemReset(11);
			}
			SendATCommand ("ATE0\r", "OK", 1000, 10, PowerOnModuleGSM);
			break;
		case 2:		/* Use AT+CMEE=2 to enable result code and use verbose values */
			DEBUG("\rDisable AT echo : %s",(event == EVEN_OK) ? "[OK]" : "[FAIL]");	            
			SendATCommand ("AT+CMEE=2\r", "OK", 1000, 10, PowerOnModuleGSM);
			break;
		case 3:
			DEBUG("\rSet CMEE report: %s",(event == EVEN_OK) ? "[OK]" : "[FAIL]");
			SendATCommand ("ATI\r", "OK", 1000, 10, PowerOnModuleGSM);	
		case 4:
			DEBUG("\rGet module info: %s", ResponseBuffer);
			SendATCommand ("AT+QURCCFG=\"URCPORT\",\"uart1\"\r", "OK", 1000, 5, PowerOnModuleGSM);
			break;
		case 5:
			DEBUG("\rSet URC port: %s", (event == EVEN_OK) ? "[OK]" : "[FAIL]");
			SendATCommand ("AT+QCFG=\"urc/ri/smsincoming\",\"pulse\",2000\r", "OK", 1000, 10, PowerOnModuleGSM);
			break;
		case 6:
			DEBUG("\rSet URC ringtype: %s", (event == EVEN_OK) ? "[OK]" : "[FAIL]");
			SendATCommand ("AT+CNMI=2,1,0,0,0\r", "OK", 1000, 10, PowerOnModuleGSM);
			break;
		case 7:
			DEBUG("\rConfig SMS event report: %s",(event == EVEN_OK) ? "[OK]" : "[FAIL]");
			SendATCommand ("AT+CMGF=1\r", "OK", 1000, 10, PowerOnModuleGSM);	
			break;	
		case 8:
			DEBUG("\rSet SMS text mode: %s",(event == EVEN_OK) ? "[OK]" : "[FAIL]");
			SendATCommand ("AT+CMGD=1,4\r", "OK", 2000, 5, PowerOnModuleGSM);
			break;
		case 9:
			DEBUG("\rDelete all SMS: %s",(event == EVEN_OK) ? "[OK]" : "[FAIL]");
			SendATCommand ("AT+CGSN\r", "OK", 1000, 5, PowerOnModuleGSM);
			break;
		case 10:
			GSM_GetIMEI(GSMIMEI, ResponseBuffer);
			DEBUG("\rGet GSM IMEI: %s", xSystem.Parameters.GSM_IMEI);
			if(strlen(xSystem.Parameters.GSM_IMEI) < 15) {
				DEBUG("IMEI's invalid!");
				//Khong doc dung IMEI -> reset module GSM!
				ChangeGSMState(GSM_RESET);
				return;
			}
			SendATCommand ("AT+CIMI\r", "OK", 1000, 10, PowerOnModuleGSM);
			break;	
		case 11:		
			GSM_GetIMEI(SIMIMEI,ResponseBuffer);
			DEBUG("\rGet SIM IMSI: %s",xSystem.Parameters.SIM_IMEI);
			if(strlen(xSystem.Parameters.SIM_IMEI) < 15) {
				DEBUG("SIM's not inserted!");
				//Neu ko nhan SIM -> reset module GSM!
				ChangeGSMState(GSM_RESET);
				return;
			}
			SendATCommand ("AT+QIDEACT=1\r", "OK", 3000, 1, PowerOnModuleGSM);
//			SendATCommand ("AT+CSQ\r", "OK", 1000, 5, PowerOnModuleGSM);
			break;	
		case 12:
			DEBUG("\rDe-activate PDP: %s",(event == EVEN_OK) ? "[OK]" : "[FAIL]");		
			SendATCommand ("AT+QCFG=\"nwscanmode\",0\r", "OK", 5000, 2, PowerOnModuleGSM);	// Select mode AUTO
			break;
		case 13:
			DEBUG("\rNetwork search mode AUTO: %s",(event == EVEN_OK) ? "[OK]" : "[FAIL]");
			SendATCommand ("AT+CGDCONT=1,\"IP\",\"v-internet\"\r", "OK", 1000, 2, PowerOnModuleGSM);     /** <cid> = 1-24 */       
			break;
		case 14:
			DEBUG("\rDefine PDP context: %s",(event == EVEN_OK) ? "[OK]" : "[FAIL]");
			//SendATCommand ("AT+QIACT=1\r", "OK", 5000, 5, PowerOnModuleGSM);	/** Bật QIACT lỗi gửi tin với 1 số SIM dùng gói cước trả sau! */
			SendATCommand ("AT+CSQ\r", "OK", 1000, 5, PowerOnModuleGSM);
			break;
		case 15:
			DEBUG("\rActivate PDP: %s",(event == EVEN_OK) ? "[OK]" : "[FAIL]");
			SendATCommand ("AT+CGREG=2\r", "OK", 1000, 3, PowerOnModuleGSM);		/** Query CGREG? => +CGREG: <n>,<stat>[,<lac>,<ci>[,<Act>]] */	
			break;
		case 16:
			DEBUG("\rNetwork Registration Status: %s",(event == EVEN_OK) ? "[OK]" : "[FAIL]");
			SendATCommand ("AT+CGREG?\r", "OK", 1000, 5, PowerOnModuleGSM);
			break;
		case 17:
			DEBUG("\rQuery Network Status: %s",(event == EVEN_OK) ? "[OK]" : "[FAIL]");	/** +CGREG: 2,1,"3279","487BD01",7 */
			if(event == EVEN_OK)
			{
				GSM_GetNetworkStatus(ResponseBuffer);
			}
			SendATCommand ("AT+COPS?\r", "OK", 1000, 5, PowerOnModuleGSM);	
			break;
		case 18:
			DEBUG("\rQuery Network Operator: %s",(event == EVEN_OK) ? "[OK]" : "[FAIL]");	/** +COPS: 0,0,"Viettel Viettel",7 */
			if(event == EVEN_OK)
			{
				GSM_GetNetworkOperator(ResponseBuffer);
			}
#if (__GSM_SLEEP_MODE__)
			SendATCommand ("AT+QSCLK=1\r", "OK", 1000, 5, PowerOnModuleGSM);
#else
			SendATCommand ("AT+QSCLK=0\r", "OK", 1000, 5, PowerOnModuleGSM);
#endif
			break;
		case 19:
			DEBUG("\rEnable Sleep mode: %s",(event == EVEN_OK) ? "[OK]" : "[FAIL]");
			SendATCommand ("AT+CSQ\r", "OK", 1000, 5, PowerOnModuleGSM);	
			break;
		case 20:
			GSM_Manager.Step = 0;
			if(event != EVEN_OK) {
				DEBUG("\rGSM: Khoi tao Fail, Reset modem...");
				ChangeGSMState(GSM_RESET);
				return;
			}
			GSM_GetSignalStrength(ResponseBuffer);
			DEBUG("\rCuong do song: %d", xSystem.Status.CSQ);
	
#if (__HOPQUY_GSM__ == 0)
			SendATCommand("ATV1\r", "OK", 1000, 3, OpenPPPStack);
#endif
			break;
	}
	
#if __HOPQUY_GSM__
	xSystem.Status.DebugTimeOut = 1;
	if(GSM_Manager.Step == 0) {
		DEBUG("\rGSM: Khoi tao thanh cong...!\r");
		xSystem.Status.DebugTimeOut = 0;
	}
	else DEBUG("\rStep: %u/20", GSM_Manager.Step);
#endif	/* __HOPQUY_GSM__ */
	
	GSM_Manager.Step++;
}

/*****************************************************************************/
/**
 * @brief	:  
 * @param	:  
 * @retval	:
 * @author	:	
 * @created	:	10/11/2016
 * @version	:
 * @reviewer:	
 */
void OpenPPPStack(GSM_ResponseEvent_t event, void *ResponseBuffer)
{
	static uint8_t RetryCount = 0;
	
	switch(GSM_Manager.Step)
	{
		case 1:
			ResetWatchdog();			
			ppp_connect("*99#","","");
			SendATCommand ("ATD*99***1#\r", "CONNECT", 1000, 10, OpenPPPStack);
			GSM_Manager.Step = 2;
			break;
		case 2 :
			DEBUG("\rMo ket noi PPP: %s",(event == EVEN_OK) ? "[OK]" : "[FAIL]");
		
			if(event != EVEN_OK)
			{
				RetryCount++;
				if(RetryCount  > 5)
				{
					RetryCount = 0;
						
					DEBUG("\rMo ket noi PPP qua 5 lan. Reset GSM!");
#if	(__GSM_SLEEP_MODE__)
					//Nếu đang sleep thì không reset module
					if(!isGSMSleeping())
					{
						/* Reset GSM */
						ChangeGSMState(GSM_RESET);
					}
					else
					{
						//Tiep tuc open PPP
						GSM_Manager.Step = 1;
						ppp_close();
						SendATCommand ("ATV1\r", "OK", 1000, 5, OpenPPPStack);
					}
#else
					/* Reset GSM */
					ChangeGSMState(GSM_RESET);
#endif
				}
				else
				{
					GSM_Manager.Step = 1;
					ppp_close();
					SendATCommand ("ATV1\r", "OK", 1000, 5, OpenPPPStack);
				}
			}
			else	//Truong hop AT->PPP, khong ChangeState de gui lenh ATO
			{
				RetryCount = 0;
				GSM_Manager.Step = 0;
				GSM_Manager.State = GSM_OK;	
				GSM_Manager.Mode = GSM_PPP_MODE;		//Response "CONNECT" chua chac ppp_is_up() = 1!
				GSM_Manager.GSMReady = 1;
				GSM_Manager.PPPCommandState = 0;
			}
			break;
		default:
			break;
	}
}

/**
* Check RI pin every 10ms
*/
static uint8_t isNewSMSComing = 0;
static uint8_t isRetryReadSMS = 0;
static uint8_t RISignalTimeCount = 0;
static uint8_t RILowLogicTime = 0;
void QuerySMSTick(void)
{
	if(GSM_Manager.RISignal == 1)
	{
		uint8_t RIState = GPIO_ReadInputDataBit(GSM_RI_PORT, GSM_RI_PIN);
		if(RIState == 0) RILowLogicTime++;
		
		RISignalTimeCount++;
		if(RISignalTimeCount >= 100)	//50 - 500ms, 100 - 1000ms
		{
			if(RIState == 0)
			{
				DEBUG("\rNew SMS coming!");
				
#if (__GSM_SLEEP_MODE__)
				/* Neu dang sleep thi wake truoc */
//				if(GPIO_ReadOutputDataBit(GSM_DTR_PORT, GSM_DTR_PIN)) {
//					DEBUG("\rGSM is sleeping, wake up...");
//					ChangeGSMState(GSM_WAKEUP);
//				}
#endif	//__GSM_SLEEP_MODE__
				
				GSM_Manager.RISignal = 0;
				RISignalTimeCount = 0;
				isNewSMSComing = 1;
			} else {
				DEBUG("\rRI by other URC: %ums", RILowLogicTime*10);
				GSM_Manager.RISignal = 0;
				RISignalTimeCount = 0;
				isNewSMSComing = 0;
			}
			RILowLogicTime = 0;
		}
	}
}

#if __RI_WITHOUT_INTERRUPT__
/*****************************************************************************/
/**
 * @brief	:  	Check RI pin to get new SMS, call every 10ms
 * @param	:  
 * @retval	:
 * @author	:	phinht
 * @created	:	15/01/2014
 * @version	:
 * @reviewer:	
 */
void GSM_CheckSMSTick(void)
{
    static uint16_t RILowCount = 0xFFFF;

    if (GSM_Manager.GSMReady == 0 || isNewSMSComing)
       return;

    if (GPIO_ReadInputDataBit(GSM_RI_PORT, GSM_RI_PIN) == 0)
    {
        if (RILowCount == 0xFFFF)
           RILowCount = 0;
        else
           RILowCount++;
    }
    else
    {
        if (RILowCount >= 50 && RILowCount < 250)	// 500ms - 2500ms
        {
            DEBUG("\rNew SMS coming...");
            isNewSMSComing = 1;
        } else {
			  //DEBUG("\rRI caused by other URCs: %ums", RILowCount*10);
		  }
        RILowCount = 0xFFFF;
    }
}
#endif	//__RI_WITHOUT_INTERRUPT__

/*****************************************************************************/
/**
 * @brief	:  Ham lay GSM signal level
 * @author	:	
 * @created	:	10/03/2016
 * @version	:
 * @reviewer:	
 */
void GSM_GetBTSInfor(GSM_ResponseEvent_t event, void *ResponseBuffer)
{	
	switch(GSM_Manager.Step)
	{
		case 1:
			SendATCommand ("+++", "OK", 2000, 5, GSM_GetBTSInfor);
			break;
		case 2:
			if(event == EVEN_OK)
			{
				GSM_Manager.PPPCommandState = 1;
				SendATCommand ("AT+CSQ\r", "OK", 1000, 3, GSM_GetBTSInfor);
			}
			else
			{
				GSM_Manager.Step = 0;
				GSM_Manager.State = GSM_OK;
				GSM_Manager.PPPCommandState = 0;
				return;
			}
			break;
		case 3:
			if(event == EVEN_OK)
			{
				GSM_GetSignalStrength(ResponseBuffer);
				GSM_Manager.TimeOutCSQ = 0;
				DEBUG("\rCSQ: %d", xSystem.Status.CSQ );
				
				/* Lay thong tin Network access selected */
				SendATCommand ("AT+CGREG?\r", "OK", 1000, 3, GSM_GetBTSInfor);
			}
			break;
		case 4:
			if(event == EVEN_OK)
			{
				DEBUG("\r%s", ResponseBuffer);
				GSM_GetNetworkStatus(ResponseBuffer);
			}
			ChangeGSMState(GSM_OK);
			
			/* Gửi tin khi thức dậy định kỳ */
			if(xSystem.Status.YeuCauGuiTin == 2)
				xSystem.Status.YeuCauGuiTin = 3;
			break;
		
		default:
			break;
	}
	GSM_Manager.Step++;
}

/*****************************************************************************/
/**
 * @brief	:  Ham thuc hien lenh AT
 * @param	:  
 * @retval	:
 * @author	:	
 * @created	:	29/03/2016
 * @version	:
 * @reviewer:	
 */
void GSM_SendATCommand(GSM_ResponseEvent_t event, void *ResponseBuffer)
{		
	switch(GSM_Manager.Step)
	{
		case 1:
			SendATCommand ("+++", "OK", 2000, 5, GSM_SendATCommand);	
			break;
		case 2:
			if(event == EVEN_OK)
			{
				GSM_Manager.PPPCommandState = 1;
				if(strstr(LenhATCanGui, "+CUSD"))
					SendATCommand (LenhATCanGui, "+CUSD", 5000, 5, GSM_SendATCommand);
				else
					SendATCommand (LenhATCanGui, "OK", 2000, 5, GSM_SendATCommand);
			}
			else
			{
				YeuCauGuiATC = 0;
				GSM_Manager.Step = 0;
				GSM_Manager.State = GSM_OK;
				GSM_Manager.PPPCommandState = 0;
				return;
			}
			break;
		case 3:
			if(event == EVEN_OK)
			{
				memset(LenhATCanGui, 0, 20);
				DEBUG("Phan hoi: %s", ResponseBuffer);
			}
			YeuCauGuiATC = 0;
			TimeOutGuiLenhAT = 0;
			ChangeGSMState(GSM_OK);
			break;
			
		default:
			break;
	}
	GSM_Manager.Step++;
}

/*****************************************************************************/
/**
 * @brief	:  Vao che do sleep
 * @param	:  
 * @retval	:
 * @author	:	
 * @created	:	29/03/2016
 * @version	:
 * @reviewer:	
 */
void GSM_GotoSleepMode(GSM_ResponseEvent_t event, void *ResponseBuffer)
{		
	switch(GSM_Manager.Step)
	{
		case 1:
			SendATCommand ("+++", "OK", 2000, 5, GSM_GotoSleepMode);	
			break;
		case 2:
			if(event == EVEN_OK)
			{
				SendATCommand ("AT+QSCLK=1\r", "OK", 1000, 5, GSM_GotoSleepMode);
			}
			else
			{		
				DEBUG("\rKhong phan hoi lenh, bat buoc sleep!");
				//Dieu khien chan DTR vao sleep
				GSM_GotoSleep();
				GSM_Manager.State = GSM_SLEEP;
				return;
			}
			break;
		case 3:
			if(event == EVEN_OK)
			{
				DEBUG("\rEntry Sleep OK!");
			}
			//Dieu khien chan DTR vao sleep
			GSM_GotoSleep();
			GSM_Manager.State = GSM_SLEEP;
			break;
			
		default:
			break;
	}
	GSM_Manager.Step++;
}

/*****************************************************************************/
/**
 * @brief	:  Thoat che do sleep
 * @param	:  
 * @retval	:
 * @author	:	
 * @created	:	29/03/2016
 * @version	:
 * @reviewer:	
 */
void GSM_ExitSleepMode(GSM_ResponseEvent_t event, void *ResponseBuffer)
{		
	switch(GSM_Manager.Step)
	{
		case 1:
//			GSM_WAKEUP();
			SendATCommand ("ATV1\r", "OK", 1000, 5, GSM_ExitSleepMode);
			break;
		case 2:
			SendATCommand ("AT+QSCLK=1\r", "OK", 1000, 10, GSM_ExitSleepMode);
			break;
		case 3:
			if(event == EVEN_OK)
			{
				DEBUG("\rExit Sleep!");
				GetSignalLevelTimeOut = GET_BTS_INFOR_TIMEOUT - 3;
				ChangeGSMState(GSM_OK);
			}
			else
			{
				DEBUG("\rKhong phan hoi lenh, reset module...");
				ChangeGSMState(GSM_RESET);
			}
			break;
			
		default:
			break;
	}
	GSM_Manager.Step++;
}
	
void GSM_TestReadSMS()
{
	if(GSM_Manager.State == GSM_SLEEP) {
		DEBUG("\rWakeup GSM to read SMS...");
		ChangeGSMState(GSM_WAKEUP);
	}
	isNewSMSComing = 1;
}
	
/*
* Reset module GSM
*/
void GSM_HardReset(void)
{
	static uint8_t Step = 0;
	
	switch(Step)
	{
		case 0:
			GSM_Manager.GSMReady = 0;
			GSM_Manager.Mode = GSM_AT_MODE;
			UART_Init(GSM_UART,115200);		
			GPIO_WriteBit(GSM_RESET_PORT, GSM_RESET_PIN, (BitAction)1);	//RESET_N pin LOW
			Step++;
			break;
		case 1:
			GPIO_WriteBit(GSM_RESET_PORT, GSM_RESET_PIN, (BitAction)0);	//RESET_N pin HI
			Step++;
			break;
		case 2: 
		case 3:
		case 4: 
		case 5: 
			Step++;
			break;
		case 6:
			Step = 0;
			ChangeGSMState(GSM_POWERON);
			break;
		default: 
			break;
	}
}

/*****************************************************************************/
/**
 * @brief	:  Kiem tra tin nhan den khi dang o mode PPP
 * @param	:  
 * @retval	:
 * @author	:	
 * @created	:	10/03/2016
 * @version	:
 * @reviewer:	
 */
uint8_t LastRIState = 0xFF, CallCounter = 0xFF;
void QuerySMS(void)
{
//	uint8_t iTemp;
//			
//	/* New SMS */
//	if(isNewSMSComing || isRetryReadSMS) {
//		ChangeGSMState(GSM_READSMS);
//		return;
//	}
//	
//    /* Kiem tra cac buffer SMS de gui SMS */
//    for(iTemp = 0; iTemp < 3; iTemp++)
//    {
//        if(SMSMemory[iTemp].NeedToSent == 1 || SMSMemory[iTemp].NeedToSent == 2)
//        {
//            SMSMemory[iTemp].RetryCount++;
//            DEBUG("\rGui SMS tai buffer %u", iTemp);

//            /* Neu gui thanh cong thi xoa di */
//            if(SMSMemory[iTemp].RetryCount < 5)
//            {
//					SMSMemory[iTemp].NeedToSent = 2;
//					ChangeGSMState(GSM_SENSMS);
//            }
//            else 
//            {
//                DEBUG("\rBuffer SMS %u da gui qua %u lan, huy gui",iTemp,SMSMemory[iTemp].RetryCount);
//                SMSMemory[iTemp].NeedToSent = 0;	

//					//Add: 05/05/17
//					GSM_Manager.SendSMSAfterRead = 0;
//					ChangeGSMState(GSM_OK);
//            }
//            break;
//        }
//    }
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
void GSM_ReadSMS(GSM_ResponseEvent_t event, void *ResponseBuffer)
{
#if 0
	static uint8_t SMSIndex = 0xFF;
	static uint8_t RetryRead = 0;
	
	if(SMSIndex == 0xFF)
	{
		SMSIndex = 0;	/* Start at index 0 */
		if(RetryRead == 0) RetryRead = 6;
		
//		//Test
//		if(RetryRead == 0) {
//				RetryRead = 3;
//				isRetryReadSMS = 1;
//				SMSIndex = 0xFF;
//				DEBUG("\rRead SMS failed, reset modem...");
//				ChangeGSMState(GSM_RESET);
//				return;
//		}
	}
	else if(SMSIndex == 0xAA)   //Da xu ly xong SMS moi
	{
		SMSIndex = 0xFF;
		
		/* Kiem tra xem co SMS nao can gui khong */
		GSM_Manager.SendSMSAfterRead = 1;
		QuerySMS(); 
		
		/* Neu ko co SMS nao can gui thi chuyen trang thai ve GSM_OK */
		if(GSM_Manager.State != GSM_SENSMS)
		{
			GSM_Manager.SendSMSAfterRead = 0;
			ChangeGSMState(GSM_OK);
		}
		return;
	}
	else
	{
		if(SMSIndex < 10) SMSIndex++;
		if(SMSIndex == 10) SMSIndex = 0x55;
	}
			
	DEBUG("\rRead SMS resp: %s", (char*)ResponseBuffer);
	if(strstr(ResponseBuffer,"UNREAD") || strstr(ResponseBuffer,"READ")) //REC UNREAD | REC READ
	{
		SMSIndex = 0xAA;
		ProcessCMDfromSMS(ResponseBuffer);
		SendATCommand ("AT+CMGD=1,4\r", "OK", 1000, 10, GSM_ReadSMS);
		isNewSMSComing = 0;
		isRetryReadSMS = 0;
		RetryRead = 0;
	}
	else
	{
		if(SMSIndex < 0x55) 
		{
			sprintf(tmpBuffer,"AT+CMGR=%u\r",SMSIndex);
			SendATCommand(tmpBuffer,"OK",1000,3,GSM_ReadSMS);
			DEBUG("\rCheck SMS o buffer %u: %u,%s",SMSIndex, event, (char *)ResponseBuffer);
		}
		else
		{
			if(RetryRead) RetryRead--;
			
			DEBUG("\rCannot read SMS, retry: %u", RetryRead);
			SMSIndex = 0xFF;
				
//			if(RetryRead) {
//				isNewSMSComing = 1;
//			} else {
//				isNewSMSComing = 0;
//			}
//			ChangeGSMState(GSM_OK);
			
			if(RetryRead == 3) {
				//Doc 3 lan khong duoc -> reset module
				isRetryReadSMS = 1;
				DEBUG("\rRead SMS failed, reset modem...");
				ChangeGSMState(GSM_RESET);
				return;
			}
			else 
			{
				if(RetryRead == 0) {
					isNewSMSComing = 0;
					isRetryReadSMS = 0;
				}
				ChangeGSMState(GSM_OK);
			}
		}
	}
#endif	//0
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

void GSM_SendSMS(GSM_ResponseEvent_t event, void *ResponseBuffer)
{		
//	uint8_t ucCount;
//    static uint8_t RetryCount = 0;
//		
//    DEBUG("\rDebug SEND SMS: %u %u,%s",GSM_Manager.Step,event, (char *)ResponseBuffer);   
//    
//	switch(GSM_Manager.Step)
//	{
//		case 1:
//			if(GSM_Manager.SendSMSAfterRead || GSM_Manager.PPPCommandState)
//			{
////				GSM_Manager.SendSMSAfterRead = 0;	//Neu gui loi lan 1 -> lan 2,3 se gui "+++"  -> khong thanh cong
//				SendATCommand ("ATV1\r", "OK", 1000, 3, GSM_SendSMS);	
//			}
//			else
//				SendATCommand ("+++", "OK", 2200, 5, GSM_SendSMS);	
//			break;
//		
//		case 2: 
//			if(GSM_Manager.PPPCommandState == 0 && event != EVEN_OK)	//Data -> Command state Fail
//			{
//				GSM_Manager.Step = 0;
//				GSM_Manager.SendSMSAfterRead = 0;
//				GSM_Manager.State = GSM_OK;
//				return;
//			}
//			//Add: 22/12
//			if(event == EVEN_OK && GSM_Manager.PPPCommandState == 0) 
//				GSM_Manager.PPPCommandState = 2;
//				
//			for(ucCount = 0; ucCount < 3; ucCount++)
//			{
//				if(SMSMemory[ucCount].NeedToSent == 2)
//				{
//					DEBUG("\rNhan tin den so: %s. Noi dung: %s",
//						SMSMemory[ucCount].PhoneNumber,SMSMemory[ucCount].Message);
//					
//					sprintf(tmpBuffer,"AT+CMGS=\"%s\"\r",SMSMemory[ucCount].PhoneNumber);					
//					SendATCommand (tmpBuffer, ">", 3000, 5, GSM_SendSMS);
//					break;
//				}
//			}
//			break;
//		case 3:
//			if(event == EVEN_OK)			
//			{                
//				for(ucCount = 0; ucCount < 3; ucCount++)
//				{
//					if(SMSMemory[ucCount].NeedToSent == 2)
//					{
//						SMSMemory[ucCount].Message[strlen(SMSMemory[ucCount].Message)] = 26;
//						SMSMemory[ucCount].Message[strlen(SMSMemory[ucCount].Message)] = 13;
//						
//						SendATCommand (SMSMemory[ucCount].Message, "+CMGS", 15000, 1, GSM_SendSMS);
//						DEBUG("\rBat dau gui SMS o buffer %u",ucCount);
//						break;
//					}
//				}
//			}
//			else			
//			{				
//				RetryCount++;
//				if(RetryCount < 3)
//				{
//					ChangeGSMState(GSM_SENSMS);
//					return;
//				}
//				else
//					goto SENDSMSFAIL;
//			}
//			break;
//		case 4:
//			if(event == EVEN_OK)			
//			{
//				DEBUG("\rSMS: Gui SMS thanh cong.");

//				for(ucCount = 0; ucCount < 3; ucCount++)
//				{
//					if(SMSMemory[ucCount].NeedToSent == 2)
//					{
//            SMSMemory[ucCount].NeedToSent = 0;
//						
//						//Kiem tra xem con SMS can gui trong buffer khong, neu con -> quay lai gui tiep: add 22/12
//						for(ucCount = ucCount; ucCount < 3; ucCount++)
//						{
//							if(SMSMemory[ucCount].NeedToSent == 2)
//							{
//								RetryCount = 0;
//								GSM_Manager.Step = 0; 
//								GSM_Manager.State = GSM_OK;
//								return; 
//							}
//						}
//					}
//				}
//				RetryCount = 0;
//				GSM_Manager.SendSMSAfterRead = 0;	
//				
//				//Clear Flag LED change state
//				if(xSystem.Status.LEDStateChangeFlag) {
//					xSystem.Status.LEDStateChangeFlag = 0;
//					RTC_WriteBackupRegister(LED_STATE_CHANGE_ADDR, 0);
//				}
//				ChangeGSMState(GSM_OK);
//			}
//			else
//			{
//				DEBUG("\rSMS: Nhan tin khong thanh cong.");
//				RetryCount++;
//				if(RetryCount < 3)
//				{
//					ChangeGSMState(GSM_SENSMS);
//					return;
//				}
//				else
//					goto SENDSMSFAIL;	                				
//			}
//		return;
//	}	
//	GSM_Manager.Step++;
//	return;
//    
//SENDSMSFAIL:	
//	for(ucCount = 0; ucCount < 3; ucCount++)
//	{
//		if(SMSMemory[ucCount].NeedToSent == 2)
//		{
//				SMSMemory[ucCount].NeedToSent = 1;
////			ChangeGSMState(GSM_OK);
//		}
//	}
//	RetryCount = 0;
//	GSM_Manager.SendSMSAfterRead = 0;
//	ChangeGSMState(GSM_OK);
}

void ThucHienLenhAT(char *LenhAT)
{
	uint8_t i = 0;
	memset(LenhATCanGui, 0, 30);
	while(LenhAT[i] && i < 30)
	{
		LenhATCanGui[i] = LenhAT[i];
		i++;
	}
	if(LenhATCanGui[i] != '\r')
		LenhATCanGui[i] = '\r';
	
	YeuCauGuiATC = 1;
	TimeOutGuiLenhAT = 60;
}

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
void ReconnectTCP(void)
{
  uint8_t i;
    
	DEBUG("\rKet noi lai Server...");
	    
	/* Clear het cac buffer */
	for(i = 0; i < NUM_OF_MQTT_BUFFER; i++)
	{
		xSystem.MQTTData.Buffer[i].State = BUFFER_STATE_IDLE;
		xSystem.MQTTData.Buffer[i].BufferIndex = 0;
	}        
    
  xSystem.Status.TCPNeedToClose = 2;
}

/********************************* END OF FILE *******************************/

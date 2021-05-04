#ifndef __GSM_H__
#define __GSM_H__

#include "RTL.h"
#include "DataDefine.h"

#define	GSM_ON	1
#define	GSM_OFF	2

#define	GSMIMEI         0
#define	SIMIMEI         1

#define 	MAX_GSMRESETSYSTEMCOUNT	600	//10 phut

#define __HOPQUY_GSM__ 0
#define	__GSM_SLEEP_MODE__	1

#if (__GSM_SLEEP_MODE__)
	#warning CHU Y DANG BUILD __GSM_SLEEP_MODE__ = 1
#endif

typedef enum {
    EVEN_OK = 0, // GSM response dung
    EVEN_TIMEOUT, // Het timeout ma chua co response
    EVEN_ERROR, // GSM response ko dung
} GSM_ResponseEvent_t;

typedef enum {
	GSM_OK = 0,
	GSM_RESET = 1,
	GSM_SENSMS = 2,
	GSM_READSMS = 3,
	GSM_POWERON = 4,
	GSM_REOPENPPP = 5,
	GSM_GETBTSINFOR = 6,
	GSM_SENDATC = 7,
	GSM_GOTOSLEEP = 8,
	GSM_WAKEUP = 9,
	GSM_SLEEP
} GSM_State_t;

typedef enum {
    GSM_AT_MODE = 1,
    GSM_PPP_MODE
} GSM_Mode_t;

typedef struct{		
	GSM_State_t State;
	GSM_Mode_t	Mode;
	uint8_t Step;
	uint8_t RISignal;
	uint8_t Dial;
	uint8_t GetBTSInfor;
	uint8_t GSMReady;
	uint8_t FirstTimePower;
	uint8_t SendSMSAfterRead;
	uint8_t PPPCommandState;
	uint16_t TimeOutConnection;
	uint16_t TimeOutCSQ;
	uint8_t TimeOutOffAfterReset;
	uint8_t isGSMOff;
	uint8_t AccessTechnology;
}GSM_Manager_t;

typedef void (*GSM_SendATCallBack_t) (GSM_ResponseEvent_t event, void *ResponseBuffer);

void SendATCommand(char *Command, char *ExpectResponse, uint16_t Timeout,
        uint8_t RetryCount, GSM_SendATCallBack_t CallBackFunction);

void InitGSM(void);
void InitGSM_DataLayer(void);
void GSM_UARTHandler(void);
void GSM_ManagerTask(void);
void GSM_GetIMEI(uint8_t LoaiIMEI, uint8_t *IMEI_Buffer);
void GSM_GetShortAPN(char *ShortAPN);
void GSM_GetSignalStrength(uint8_t *Buffer);
void GetCellIDAndSignalStrength(char* Buffer);
void GSM_ProcessCUSDMessage(char *buffer);
void GSM_GetNetworkStatus(char *Buffer);
void GSM_GetNetworkOperator(char *Buffer);

void QuerySMS(void);
void ProcessCMDfromSMS(char* Buffer);
void GSM_SendSMS(GSM_ResponseEvent_t event, void *ResponseBuffer);
void ChangeGSMState(GSM_State_t NewState);
void GSM_PowerControl(uint8_t State);
void GuiTrangThaiToiSDT(char *SDT);
void NhanTin(char* Buffer, uint8_t CallFrom);
void ThucHienLenhAT(char *Lenh);
void ReconnectTCP(void);
void GSM_GotoSleep(void);
void GSM_TestReadSMS(void);

void QuerySMSTick(void);
void GSM_CheckSMSTick(void);

BOOL com_put_at_string (char *str);

//======================== FOR MODEM ========================//
#define MODEM_BUFFER_SIZE   1436

#define MODEM_IDLE      0
#define MODEM_ERROR     1
#define MODEM_READY     2
#define MODEM_LISTEN    3
#define MODEM_ONLINE    4
#define MODEM_DIAL      5
#define MODEM_HANGUP    6 

typedef struct{
	char *CMD;
	char *ExpectResponseFromATC;
	uint16_t TimeoutATC;
	uint16_t CurrentTimeoutATC;
	uint8_t RetryCountATC;
	SmallBuffer_t ReceiveBuffer;
	GSM_SendATCallBack_t SendATCallBack;
}ATCommand_t;

typedef struct {
	uint16_t IndexIn;
	uint16_t IndexOut;
	uint8_t Buffer[MODEM_BUFFER_SIZE];
}ModemBuffer_t;

typedef struct{
	uint8_t Step;
	uint8_t State;
	uint8_t TX_Active;
	
	ModemBuffer_t TxBuffer;
	ModemBuffer_t RxBuffer;
	
	uint8_t *DialNumber;
} Modem_t;

typedef struct{
	ATCommand_t	ATCommand;
	Modem_t		Modem;
} GSM_Hardware_t;

#endif // __GSM_H__


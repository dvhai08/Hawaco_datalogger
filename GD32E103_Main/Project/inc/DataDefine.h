#ifndef __DATA_DEFINE_H__
#define __DATA_DEFINE_H__

// #include "stm32f0xx.h"
#include "gd32e10x.h"

#include "hardware.h"
#include "DriverUART.h"

#include "SEGGER_RTT.h"
#include "app_debug.h"
#define DEBUG DEBUG_PRINTF
#define DEBUG_RAW DEBUG_PRINTF

//#define 	DEBUG_PRINTFString...) 	SEGGER_RTT_printf(0, String)
//#define DEBUG_RAW(String...)        SEGGER_RTT_printf(0, String)
//#define DEBUG_PRINTF(String...)     { SEGGER_RTT_printf(0, "[%d] ", sys_get_ms()); SEGGER_RTT_printf(0, String);}

#define LARGE_BUFFER_SIZE 1024
#define MEDIUM_BUFFER_SIZE 512
#define SMALL_BUFFER_SIZE 256
#define TINY_BUFFER_SIZE 50

#define NUM_OF_GPRS_BUFFER 5

#define STATUS_OK 1
#define STATUS_FAIL 0

#define PTT_PRESSED 0
#define PTT_RELEASED 1

#define __USE_MQTT__ 0

#define BUFFER_STATE_BUSY 1       // Trang thai dang them du lieu
#define BUFFER_STATE_IDLE 2       // Trang thai cho
#define BUFFER_STATE_PROCESSING 3 // Trang thai du lieu dang duoc xu ly
#define BUFFER_STATE_IDLE2 4
#define BUFFER_STATE_PROCESSING2 5

typedef enum
{
    NOT_CONNECTED = 0, //Chua ket noi
    DNS_GET_HOST_IP,   //Phan giai ten mien  -> Ip, vd: "iot.eclipse.org" -> "198.41.30.241"
    ESTABLISHING,      //Da phan giai ten mien -> lay socket & mo ket noi Tcp
    ESTABLISHED        //Ket noi Tcp thanh cong toi Broker
} ServerState_t;

typedef enum
{
    FT_NO_TRANSER = 0,
    FT_REQUEST_SRV_RESPONSE,
    FT_WAIT_TRANSFER_STATE,
    FT_TRANSFERRING,
    FT_WAIT_RESPONSE,
    FT_TRANSFER_DONE
} FileTransferState_t;

typedef struct
{
    uint8_t Buffer[LARGE_BUFFER_SIZE];
    uint16_t BufferIndex;
    uint8_t State;
} LargeBuffer_t;

typedef struct
{
    uint8_t Buffer[MEDIUM_BUFFER_SIZE];
    uint16_t BufferIndex;
    uint8_t State;
} MediumBuffer_t;

typedef struct
{
    uint8_t Buffer[SMALL_BUFFER_SIZE];
    uint8_t BufferIndex;
    uint8_t State;
} SmallBuffer_t;

typedef struct
{
    uint8_t Buffer[TINY_BUFFER_SIZE];
    uint8_t BufferIndex;
    uint8_t State;
} TinyBuffer_t;

typedef union
{
    float value;
    uint8_t bytes[4];
} Float_t;

typedef union
{
    uint32_t value;
    uint8_t bytes[4];
} Long_t;

typedef union
{
    uint16_t value;
    uint8_t bytes[2];
} Int_t;

typedef struct
{
    uint8_t Sub[4];
    uint16_t Port;
} IPStruct_t;

typedef struct
{
    char Name[35];
    uint8_t Sub[4];
    uint16_t Port;
} HostStruct_t;

typedef struct
{
    uint8_t Year;
    uint8_t Month;
    uint8_t Day;
    uint8_t Hour;
    uint8_t Minute;
    uint8_t Second;
} DateTime_t;

typedef enum
{
    FTP_UPLOAD = 0,
    FTP_DOWNLOAD
} FileTransferType_t;

typedef struct
{
    char FTP_FileName[31];
    Long_t FileSize;
    Long_t FileCRC;
    char FTP_Path[31];
    char FTP_IPAddress[16];
    IPStruct_t FTP_IP;
    char FTP_UserName[16];
    char FTP_Pass[16];

    uint32_t FileAddress;
    uint32_t DataTransfered;
    FileTransferState_t State;
    FileTransferType_t Type;
    uint8_t UDFWTarget;
    uint8_t Retry;
} FileTransferStruct_t;

typedef struct
{
    uint8_t HardwareVersion;
    uint32_t ResetReasion;
    uint8_t SelfResetReasion;
    uint32_t SoLanReset;
} HardwareInfo_t;

typedef struct
{
    uint32_t Vin;
    uint32_t PulseCounterInBkup;
    uint32_t PulseCounterInFlash;
    uint16_t Vsens;
    float Input420mA;
    uint8_t batteryLevel;
    uint16_t Pressure[4];
    uint8_t batteryPercent;
} MeasureStatus_t;

typedef struct
{
    uint8_t InitSystemDone;
    uint8_t TestInternalWDT;

    uint16_t PingTimeout;
    uint8_t YeuCauGuiTin;
    uint8_t GuiGoiTinTCP;
    uint16_t WaitToCloseTCP;

    uint8_t ServerState;
    uint8_t MQTTServerState;
    uint8_t TCPSocket;
    uint8_t TCPNeedToClose;
    uint8_t TCPCloseDone;

    uint8_t SendGPRSTimeout;
    uint16_t GSMSleepTime;
    uint32_t GSMSendFailedTimeout;
    uint8_t CSQ;
    uint8_t CSQPercent;
    uint8_t Alarm;

    //Time stamp
    uint32_t TimeStamp;
    uint32_t DisconnectTimeout;

    //Debug
    uint8_t ADCOut;

} Status_t;

typedef union
{
    struct input
    {
        uint16_t pulse : 1;
        uint16_t ma420 : 1;
        uint16_t rs485;
    } name;
    uint16_t value;
} InputConfig_t;

typedef struct
{
    char GSM_IMEI[25];
    char SIM_IMEI[25];

    char MQTTUser[25];
    char MQTTPass[25];
    HostStruct_t MQTTHost;

    //Thong so cau hinh
    char PhoneNumber[15];
    uint16_t TGGTDinhKy; //phut
    uint16_t TGDoDinhKy; //phut
    uint8_t outputOnOff; //0/1
    uint8_t output420ma; //4-20mA
    InputConfig_t input;
    uint8_t alarm;
    uint32_t input1Offset;
    uint32_t kFactor;
} Parameters_t;

#if (__USE_MQTT__ == 1)
#define NUM_OF_MQTT_BUFFER 6

typedef struct
{
    MediumBuffer_t Buffer[NUM_OF_MQTT_BUFFER];
} MQTT_ClientData_t;
#else       // use http
#define NUM_OF_HTTP_BUFFER 6
typedef struct
{
    MediumBuffer_t Buffer[NUM_OF_HTTP_BUFFER];
} HTTP_ClientData_t;
// typedef struct
// {
//     SmallBuffer_t GPRS_Buffer[NUM_OF_GPRS_BUFFER];
//     SmallBuffer_t LoginBuffer;

//     uint8_t NewDataToSend;
//     uint16_t DataLengthToSend;
// } TCP_ClientData_t;
#endif //__USE_MQTT__

typedef struct
{
    //	Driver_t Driver;
    //	Driver_RTC_t *Rtc;
    //	Debug_t	*Debug;
    Status_t Status;
    Parameters_t Parameters;

    FileTransferStruct_t FileTransfer;
    HardwareInfo_t HardwareInfo;
    MeasureStatus_t MeasureStatus;

#if (__USE_MQTT__ == 1)
    MQTT_ClientData_t MQTTData;
#else
    HTTP_ClientData_t http_data;
#endif


} System_t;

#endif // __DATA_DEFINE_H__

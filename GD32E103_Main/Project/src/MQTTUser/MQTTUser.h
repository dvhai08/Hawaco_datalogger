#ifndef __MQTTUSER_H__
#define __MQTTUSER_H__

#include <stdint.h>
#include "MQTTPacket.h"

#define	MQTT_KEEP_ALIVE_INTERVAL	600

#define	TOPIC_PUB_HEADER	"Mqtt2Https/pub/"
#define	TOPIC_SUB_HEADER	"Mqtt2Https/sub/"
#define	TOPIC_DEBUG_HEADER	"Mqtt2Https/dev/"

typedef enum{
	MQTT_INIT = 0,
	MQTT_RESOLVE_DNS = 1,
	MQTT_ESTABLISHING,
	MQTT_SEND_CONREQ,
	MQTT_CONNECTED,
	MQTT_LOGINED,
	
	MQTT_CONNECT,
	MQTT_WAITCONACK,
	MQTT_SUB,
	MQTT_WAITSUBACK,
	MQTT_PUB,
	MQTT_WAITPUBACK,
	MQTT_DISCONNECT,
	MQTT_PUBREC,
	MQTT_PUBREL,
	MQTT_WAITPUBCOMP
} MQTTState_e;

//uint8_t TCP_SendData(uint8_t *Buffer, uint16_t Length);
//void TCP_Tick(void);
//void DisconnectTCP(void);
//void TCP_ClientMessageTick(void) ;
//void TCP_SendAlarmTick(void);
//void InitTCP_ClientMessage(void) ;
//uint8_t ProcessPacketTCP(uint8_t *Buffer, uint16_t Length);

typedef enum{
	DATA_DINHKY = 1,
	DATA_CANHBAOVITRI = 2,
	DATA_CANHBAOACQUY = 3,
	DATA_CANHBAODEN = 4,
	DATA_CAUHINH = 5,		/* Ban tin cac cau hinh den */
	DATA_TRANGTHAI = 6,	/* Ban tin cac trang thai thiet bi */
} MessageType_t;

typedef enum{
	GPRS_BANTINDULIEU = 1,
	GPRS_BANTINKHAC = 10
} GPRSRequestType_t;

void MQTT_Init(void);
void MQTT_Tick(void);
void MQTT_ClientMessageTick(void);
void MQTT_SendAlarmTick(void);
void MQTT_ProcThread(void);

void MQTT_InitBufferQueue(void);
void MQTT_SubscribeNewTopic(char* topic);
uint8_t MQTT_SendPINGReq(void);
uint16_t MQTT_SendPackage(uint8_t* BufferToSend, uint16_t Length);
void MQTT_ProcessRecvPacket(MQTTString fromTopic, uint8_t *Buffer, uint16_t Length);

uint16_t MQTT_SendBufferToServer(char* BufferToSend, char *LoaiBanTin);
uint8_t MQTT_ProcessDataFromServer(char *Buffer, uint16_t Length);
uint16_t MQTT_PublishDebugMsg(char* msgHeader, char* msgBody);
uint16_t MQTT_PublishDataMsg(void);

uint16_t DataMessage(uint8_t LoaiBanTin);
uint16_t SendBufferToServer(uint8_t* BufferToSend, uint16_t Length);

#endif // __MQTTUSER_H__


#ifndef __MQTTUSER_H__
#define __MQTTUSER_H__

#include <stdint.h>
#include "MQTTPacket.h"

#define	MQTT_KEEP_ALIVE_INTERVAL	600

#define	TOPIC_PUB_HEADER	"buoy/pub/"
#define	TOPIC_SUB_HEADER	"buoy/sub/"
#define	TOPIC_DEBUG_HEADER	"buoy/dev/"

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
uint8_t MQTT_ProcessDataFromServer(uint8_t *Buffer, uint16_t Length);
uint16_t MQTT_PublishDebugMsg(char* msgHeader, char* msgBody);

#endif // __MQTTUSER_H__


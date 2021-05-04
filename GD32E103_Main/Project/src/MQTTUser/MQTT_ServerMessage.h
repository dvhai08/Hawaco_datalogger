#ifndef __MQTT_SERVERMESSAGE_H__
#define __MQTT_SERVERMESSAGE_H__

#include "MQTT_Proccess.h"

#define HEADER_LENGTH 	5
#define TIME_LENGTH		6
#define FOOTER_LENGTH	4

typedef enum{
	MQTT_VALID_FAILCHECK,
	MQTT_VALID_OK,
	MQTT_PROCCESS_OK
}MqttValid_e;

typedef struct ServerMsgStruct{
	uint8_t Header[5];
	uint8_t Id;
	uint8_t SubId;
	uint8_t Time[6];
	uint8_t SecurityCode[8];
	uint16_t PayloadLength;
	SmallBuffer_t Payload;
	uint16_t Checksum;
	uint8_t Footer[4];
} ServerMsg_t;

typedef union ServerMsgUnion{
	ServerMsg_t ServerMsg;
	uint8_t byte[sizeof(ServerMsg_t)];
}uServerMsg_t;

typedef enum TransferRequestCmd{
	REQUEST_TO_UPLOAD		,
	CONFIRM_ALLOW_UPLOAD	,
	CONFIRM_UPLOAD_DONE		,
	UPDATE_MAIN_FIRMWARE    ,	
	UPDATE_BOOTLOADER		,
	UPDATE_BAFM_FIRMWARE	,
	UPDATE_RFID_FIRMWARE	,
	UPDATE_TAXI_FIRMWARE	,
	DOWNLOAD_FILE			,
	DOWNLOAD_DONE			
}TransferRequestCmd_e;

#endif

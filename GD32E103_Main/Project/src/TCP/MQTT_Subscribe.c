/******************************************************************************
 * @file    	MQTT_Subscribe.c
 * @author  	Phinht
 * @version 	V1.0.0
 * @date    	20/03/2016
 * @brief   	
 ******************************************************************************/


/******************************************************************************
                                   INCLUDES					    			 
 ******************************************************************************/
 #include <string.h>
#include "DataDefine.h"
#include "RTL.h"
#include "TCP.h"
#include "gsm.h"
#include "Main.h"
#include "gsm_utilities.h"
#include "Parameters.h"
#include "MQTTClient.h"
#include "MQTTConnect.h"
#include "MQTTPacket.h"
#include "MQTTUser.h"
#include "HardwareManager.h"
#include "Configurations.h"

/******************************************************************************
                                   GLOBAL VARIABLES					    			 
 ******************************************************************************/
extern System_t xSystem;
extern GSM_Manager_t gsm_manager;
extern uint8_t HostNo;
extern uint8_t LastHost; 
extern uint8_t DNSResolved;

/******************************************************************************
                                   GLOBAL FUNCTIONS					    			 
 ******************************************************************************/

/******************************************************************************
                                   DATA TYPE DEFINE					    			 
 ******************************************************************************/
#define	KEEP_ALIVE_INTERVAL	120
/******************************************************************************
                                   PRIVATE VARIABLES					    			 
 ******************************************************************************/
MQTTPacket_connectData SubData = MQTTPacket_connectData_initializer;
MQTTString SubTopicString = MQTTString_initializer;

static unsigned char Buffer[200];
static uint8_t BufferLen;
static uint8_t MQTTLen;
int msgid = 1, req_qos = 0;

SOCKADDR_IN addr;

/******************************************************************************
                                   LOCAL FUNCTIONS					    			 
 ******************************************************************************/
static void ProcessSetConfigFromBroker(uint8_t* Buffer);
static void ProcessGetConfigFromBroker(uint8_t* Buffer);
	
static void dns_callback (U8 event, U8 *ip);

int getdata(unsigned char *buf, int count)
{
	return recv(xSystem.Status.BSDsocket, (char*)buf, count, MSG_DONTWAIT);
}

//Return: socket number or SCK_ERROR
void creat_socket(void)
{
	xSystem.Status.BSDsocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);	//IPPROTO_TCP);	//0 - The system selects a matching protocol for the socket type
}
	
void mqtt_init(void)
{
	addr.sin_port      = htons(xSystem.Parameters.Host[HostNo].Port);
	addr.sin_family    = AF_INET;
    addr.sin_addr.s_b1 = xSystem.Parameters.Host[HostNo].Sub[0];
    addr.sin_addr.s_b2 = xSystem.Parameters.Host[HostNo].Sub[1];
    addr.sin_addr.s_b3 = xSystem.Parameters.Host[HostNo].Sub[2];
    addr.sin_addr.s_b4 = xSystem.Parameters.Host[HostNo].Sub[3];
}

/*
* Send data to broker
* return: len if success, <0 if fail
*/
int mqtt_write(int sockfd, unsigned char *buf,  int len)
{
    int rc = send(sockfd, (char*)buf, len, MSG_DONTWAIT); //0
    return rc;
}


/*****************************************************************************/
/**
 * @brief	:  MQTT Subscribe Init
 * @param	:  
 * @retval	:
 * @author	:	Phinht
 * @created	:	10/03/2016
 * @version	:
 * @reviewer:	
 */
void MQTTSub_Init(void)
{
	xSystem.Status.MQTTState = MQTTSUB_INIT;
	SubData.clientID.cstring = xSystem.Parameters.UniqueID;
	SubData.cleansession = 1;
	SubData.keepAliveInterval = KEEP_ALIVE_INTERVAL;
	BufferLen = sizeof(Buffer);
	SubTopicString.cstring = xSystem.Parameters.MQTT.SubTopic;	//xSystem.Parameters.GSM_IMEI;	
}
	
/*****************************************************************************/
/**
 * @brief	:  MQTT Subscribe task, duoc goi 100ms/1 lan
 * @param	:  
 * @retval	:
 * @author	:	Phinht
 * @created	:	10/11/2014
 * @version	:
 * @reviewer:	
 */
static uint8_t TimeoutTick = 0;
static uint16_t SubTimeout = 0;
void MQTTSubscribe_Task(void)
{    	
	if(xSystem.Parameters.ServerType != MQTT_SRV) return;
	if(xSystem.Status.GuiTinKhiStop == 1) return;

	ResetWatchdog();

	if(ppp_is_up() && xSystem.Status.ServerState == LOGINED)
	{
		TimeoutTick++;	
		switch(xSystem.Status.MQTTState)
		{
			case MQTTSUB_INIT:	//Init						
				if(xSystem.Parameters.HostActive > 0) HostNo = xSystem.Parameters.HostActive  - 1;
				else HostNo = 0;
				if(HostNo != LastHost) DNSResolved = DNS_ERROR_NAME;	//Neu change Host -> Resolve lai DNS
				LastHost = HostNo;
				if(xSystem.Parameters.Host[HostNo].Type == 1)
					xSystem.Status.MQTTState = MQTTSUB_RESOLVE_DNS;
				else
					xSystem.Status.MQTTState = MQTTSUB_ESTABLISHING;		
				break;
				
			case MQTTSUB_RESOLVE_DNS:	//DNS resolved
				if(DNSResolved != DNS_RES_OK)
				{
					if(TimeoutTick >= 20)	//every 2s
					{
						DNSResolved = get_host_by_name ((U8*)xSystem.Parameters.Host[HostNo].Name, dns_callback);
						TimeoutTick = 0;
					}
				}
				else  xSystem.Status.MQTTState = MQTTSUB_ESTABLISHING;
				break;
			
			case MQTTSUB_ESTABLISHING:	//get socket and connect
				if(xSystem.Status.BSDsocket == 0)
				{
					creat_socket();
					DebugPrint("\rSocket creat: %d", xSystem.Status.BSDsocket);	
				}
				else
				{
					if(TimeoutTick >= 20)	//if not connect, send connect to broker every 2s
					{
						TimeoutTick = 0;
						DebugPrint("\rMQTTSub: Mo ket noi toi dia chi: %u.%u.%u.%u:%u", xSystem.Parameters.Host[HostNo].Sub[0],
								xSystem.Parameters.Host[HostNo].Sub[1],xSystem.Parameters.Host[HostNo].Sub[2],
								xSystem.Parameters.Host[HostNo].Sub[3],xSystem.Parameters.Host[HostNo].Port);
						mqtt_init();
						if(connect (xSystem.Status.BSDsocket, (SOCKADDR *)&addr, sizeof (addr)) == SCK_SUCCESS)
						{
							SubTimeout = 0;
							xSystem.Status.MQTTState = MQTTSUB_SEND_CONREQ; 
						}
					}
				}
				break;
				
			case MQTTSUB_SEND_CONREQ:	//send subscribe connect request
				if(TimeoutTick >= 10)
				{
					TimeoutTick = 0;
					MQTTLen = MQTTSerialize_connect(Buffer, BufferLen, &SubData);
					if(mqtt_write(xSystem.Status.BSDsocket, Buffer, MQTTLen) > 0)
					{
						TimeoutTick = 0; SubTimeout = 0;
						xSystem.Status.MQTTState = MQTTSUB_WAIT_CONACK;
					}
					else
					{
						SubTimeout++;
						if(SubTimeout >= 3)	//Wait for send 3 times
							xSystem.Status.MQTTState = MQTTSUB_ESTABLISHING;
					}
				}					
				break;
				
			case MQTTSUB_WAIT_CONACK:	//Wait for connack
				if(MQTTPacket_read(Buffer, BufferLen, getdata) == CONNACK)
				{
					unsigned char sessionPresent, connack_rc;
					DebugPrint("\rMQTT: Received CONNACK");

					if(MQTTDeserialize_connack(&sessionPresent, &connack_rc, Buffer, BufferLen) != 1 || connack_rc != 0)
					{
						DebugPrint("\rUnable to connect, return code: %d", connack_rc);
						TimeoutTick = 0; 
						xSystem.Status.MQTTState = MQTTSUB_SEND_CONREQ;
					}
					else
					{
						DebugPrint("\rCONAck return code: %d", connack_rc);
						TimeoutTick = 5;
						xSystem.Status.MQTTState = MQTTSUB_SEND_SUBREQ;
					}
				} 
				else	//Error: -1
				{
					if(TimeoutTick >= 30)	//wait in 3s
						xSystem.Status.MQTTState = MQTTSUB_ESTABLISHING;	//Send connect again
				}
				break;
			
			case MQTTSUB_SEND_SUBREQ:	//Subscribe
				if(TimeoutTick >= 10)
				{
					TimeoutTick = 0;
					MQTTLen = MQTTSerialize_subscribe(Buffer, BufferLen, 0, msgid, 1, &SubTopicString, &req_qos);						
					if(mqtt_write(xSystem.Status.BSDsocket, Buffer, MQTTLen) > 0)
					{
						TimeoutTick = 0; SubTimeout = 0;
						xSystem.Status.MQTTState = MQTTSUB_WAIT_SUBACK;
					}
					else
					{
						SubTimeout++;
						if(SubTimeout >= 3)
							xSystem.Status.MQTTState = MQTTSUB_ESTABLISHING;
					}
				}
				break;
			case MQTTSUB_WAIT_SUBACK:
				if(MQTTPacket_read(Buffer, BufferLen, getdata) == SUBACK)	//Wait for suback
				{
					unsigned short submsgid;
					int subcount, granted_qos;
					DebugPrint("\rMQTT: Received SUBAck.");

					MQTTDeserialize_suback(&submsgid, 1, &subcount, &granted_qos, Buffer, BufferLen);
					if(granted_qos != 0)
					{
						DebugPrint("Granted QoS != 0, %d", granted_qos);
						xSystem.Status.MQTTState = MQTTSUB_SEND_CONREQ;
					}
					else 
					{
						DebugPrint("Granted QoS: %d", granted_qos);
						TimeoutTick = 0;
						xSystem.Status.MQTTState = MQTTSUB_RECEIVE_DATA;
					}
				}
				else
				{
					if(TimeoutTick >= 30)	//Wait in 3s
						xSystem.Status.MQTTState = MQTTSUB_ESTABLISHING;	//Send connect again
				}
				break;
				
			case MQTTSUB_RECEIVE_DATA:	//Receive published data, read socket every 500ms
				if(TimeoutTick >= 5)
				{
					TimeoutTick = 0;
					SubTimeout++;
					if(SubTimeout >= KEEP_ALIVE_INTERVAL - 20)	//not received any data in Keep Alive Interval time
					{
//							SubTimeout = 0;
//							xSystem.Status.MQTTState = MQTTSUB_INIT;	//send connect again to broker
						MQTTLen = MQTTSerialize_pingreq(Buffer, BufferLen);	//send PINGREQ message
						if(MQTTLen > 0)
						{
							if(mqtt_write(xSystem.Status.BSDsocket, Buffer, MQTTLen) <= 0)	//Send failed
								SubTimeout -= 4;	//Resend after 2s
							else SubTimeout = 0;
						}
					}
					if(MQTTPacket_read(Buffer, BufferLen, getdata) == PUBLISH)	//Received Publish Message
					{
						unsigned char dup, retained;
						unsigned short msgid;
						unsigned char *payload;
						int qos, payloadlen;
						MQTTString ReceiveTopic;
						SubTimeout = 0;

						MQTTDeserialize_publish(&dup, &qos, &retained, &msgid, &ReceiveTopic, &payload, &payloadlen, Buffer, BufferLen);
						//Do something
						DebugPrint("\rMQTT Received: %s", payload);
						
						/* Xu ly ban tin tu Broker */
						ProcessMQTTPacket((char*)payload, payloadlen);	//Test
						memset(Buffer, 0 , BufferLen);
						recv(xSystem.Status.BSDsocket, (char*)Buffer, 199, MSG_DONTWAIT);	//Doc lai de clean socket buffer
//							ProcessPacketTCP((uint8_t*)payload, payloadlen);	//Xu ly du lieu theo cau truc ban tin cu
					}
				}
				break;
			default:
				break;
		}				
	}
}

/* Xu ly ban tin nhan duoc tu Broker */
void ProcessMQTTPacket(char *Buffer, uint16_t Length)
{
	if(Length < 3 || Length > 200) return;
	
	//Lenh set cau hinh
	if(strstr(Buffer, "CM$") && strstr(Buffer, "="))
	{
		DebugPrint("\rBroker: Lenh Set cau hinh");
		ProcessSetConfigFromBroker((uint8_t*)Buffer);
		return;
	}
	//Lenh get cau hinh
	if(strstr(Buffer, "CM$") && strstr(Buffer, "?"))
	{
		DebugPrint("\rBroker: Lenh Get cau hinh");
		ProcessGetConfigFromBroker((uint8_t*)Buffer);
		return;
	}
}

/*****************************************************************************/
/**
 * @brief	:  	Thuc hien cau hinh cho thiet bi tu server
 * @param	:  
 * @retval	:
 * @author	:	Phinht
 * @created	:	15/01/2014
 * @version	:
 * @reviewer:	
 */
static char ServerResponseBuffer[150];
static void ProcessSetConfigFromBroker(uint8_t* Buffer)
{
	 uint8_t KetQua = 0;
	
    /* Cu phap co dang CM$<Lenh>= */
    char* BufferData = strstr((char*) Buffer, "CM$");
    if(!BufferData) return;
	if(!strstr((char*)BufferData, "=")) return;
    if(xSystem.Status.ServerState != LOGINED) return;

	KetQua = ProcessSetConfig((char*)BufferData, CF_SERVER);
    if(KetQua)
    {
        KetQua = sprintf(ServerResponseBuffer,"OK: ");
        ProcessGetConfig((char*)BufferData, &ServerResponseBuffer[KetQua]);        
    }
    else
	{
        sprintf(ServerResponseBuffer,"ERROR: %s", BufferData);
	}

    /* Phan hoi ket qua cau hinh len server */	
	SendBufferToServer((uint8_t *)ServerResponseBuffer, DATA_PHANHOI, strlen(ServerResponseBuffer));
}

/*****************************************************************************/

/**
 * @brief	:  	Thuc hien gui cau hinh thiet bi len server
 * @param	:  
 * @retval	:
 * @author	:	Phinht
 * @created	:	15/01/2014
 * @version	:
 * @reviewer:	
 */
static void ProcessGetConfigFromBroker(uint8_t* Buffer)
{
	 uint8_t KetQua = 0;

    /* Cu phap co dang CM$<Lenh>? */
    char* BufferData = strstr((char*) Buffer, "CM$");
    if(!BufferData) return;
	if(!strstr((char*)BufferData, "?")) return;
    if(xSystem.Status.ServerState != LOGINED) return;
	
	memset(ServerResponseBuffer, 0, 150);
	sprintf(ServerResponseBuffer, "OK: ");
    KetQua = ProcessGetConfig(BufferData, &ServerResponseBuffer[4]);

	if(KetQua == 0)
	{
		char Lenh[12] = {0};
		memset(ServerResponseBuffer, 0, 150);
		CopyParameter(BufferData, Lenh, '$', '?');
		sprintf(ServerResponseBuffer, "ERROR: %s", Lenh);
	}
	
    /* Phan hoi ket qua cau hinh len server */	
	SendBufferToServer((uint8_t *)ServerResponseBuffer, DATA_PHANHOI, strlen(ServerResponseBuffer));
}

/*****************************************************************************/
/**
 * @brief	:  	This function is called on DNS event
 * @param	:  
 * @retval	:
 * @author	:	Phinht
 * @created	:	10/11/2016
 * @version	:
 * @reviewer:	
 */
static void dns_callback(U8 event, U8 *ip) 
{
	switch (event) 
	{
		case DNS_EVT_SUCCESS:
			DebugPrint("\rDNS IP: %d.%d.%d.%d", ip[0],ip[1],ip[2],ip[3]);
			memcpy(xSystem.Parameters.Host[HostNo].Sub, ip, 4);
			xSystem.Status.MQTTState = MQTTSUB_ESTABLISHING;
			break;
		case DNS_EVT_NONAME:
			DebugPrint("Host name does not exist.\n");
			break;
		case DNS_EVT_TIMEOUT:
			DebugPrint("DNS Resolver Timeout expired!\n");
			break;
		case DNS_EVT_ERROR:
			DebugPrint("DNS Resolver Error, check the host name, labels, etc.\n");
			break;
	}
}

/********************************* END OF FILE *******************************/

/******************************************************************************
 * @file    	MQTT_HardwareLayer.c
 * @author  	Phinht
 * @version 	V1.0.0
 * @date    	20/03/2016
 * @brief   	
 ******************************************************************************/

/******************************************************************************
                                   INCLUDES					    			 
 ******************************************************************************/
#include "DataDefine.h"

#if (__USE_MQTT__ == 1)

#include <string.h>
#include "Net_Config.h"
#include "RTL.h"
#include "GSM.h"
#include "Main.h"
#include "Utilities.h"
//#include "Parameters.h"
#include "MQTTPacket.h"
#include "MQTTConnect.h"
#include "MQTTUser.h"
#include "HardwareManager.h"

/******************************************************************************
                                   GLOBAL VARIABLES					    			 
 ******************************************************************************/
extern System_t xSystem;
extern GSM_Manager_t GSM_Manager;

/******************************************************************************
                                   GLOBAL FUNCTIONS					    			 
 ******************************************************************************/

/******************************************************************************
                                   DATA TYPE DEFINE					    			 
 ******************************************************************************/

/******************************************************************************
                                   PRIVATE VARIABLES					    			 
 ******************************************************************************/
MQTTPacket_connectData MqttClientInfo = MQTTPacket_connectData_initializer;
MQTTString SubTopicString = MQTTString_initializer;
MQTTString RecvSubTopic = MQTTString_initializer;

uint8_t DNSResolved = DNS_ERROR_NAME;

static uint8_t isNewTCPPackage = 0;
static uint8_t TCPSendDataError = 0;

static unsigned char MqttSendBuff[SMALL_BUFFER_SIZE];
static uint16_t MqttSendBufflen;
static uint16_t MQTTLen;
uint16_t pubPackageId = 1;

unsigned char *subPayload = NULL;
uint8_t isSubscribed = 0;

/* Buffer luu du lieu TCP nhan duoc */
MediumBuffer_t mqttTcpRecvBuff;
char mqttClientIdBuff[35];

static uint8_t MQTTSocket;
static uint8_t TCPCloseStep = 0;
static uint8_t TCPDisconnectFromServer = 0;
/******************************************************************************
                                   LOCAL FUNCTIONS					    			 
 ******************************************************************************/
static void mqtt_dns_callback(U8 event, U8 *ip);
static uint16_t mqtt_tcp_callback(uint8_t soc, uint8_t event, uint8_t *ptr, uint16_t par);
static int8_t MQTT_TCPSend(uint8_t *Buffer, uint16_t Length);
static void ProcessMqttTCPPackage(uint8_t *ptr, uint16_t par);

/*
* Author: Phinht
*/
int getdata(unsigned char *buf, int count)
{
    uint16_t i;

    if (mqttTcpRecvBuff.BufferIndex == 0)
        return SCK_ERROR;
    if (mqttTcpRecvBuff.State + count > MEDIUM_BUFFER_SIZE)
        return SCK_ERROR;

    for (i = mqttTcpRecvBuff.State; i < mqttTcpRecvBuff.State + count; i++)
    {
        buf[i - mqttTcpRecvBuff.State] = mqttTcpRecvBuff.Buffer[i];
    }
    mqttTcpRecvBuff.State += count;

    return count;
}

/*****************************************************************************/
/**
 * @brief	:  MQTT Init
 * @param	:  
 * @retval	: none
 * @author	:	Phinht
 * @created	:	10/03/2016
 * @version	:
 * @reviewer:	
 */
void MQTT_Init(void)
{
    DEBUG("MQTT_Init..\r\n");

    /* state init */
    xSystem.Status.MQTTServerState = MQTT_INIT;

    sprintf(xSystem.Parameters.MQTTHost.Name, "%s", "mqtt2https.bytech.vn");
    xSystem.Parameters.MQTTHost.Port = 1883;

    MqttClientInfo.cleansession = 1;
    MqttClientInfo.username.cstring = xSystem.Parameters.MQTTUser;
    MqttClientInfo.password.cstring = xSystem.Parameters.MQTTPass;
    MqttClientInfo.keepAliveInterval = MQTT_KEEP_ALIVE_INTERVAL;
    MqttSendBufflen = sizeof(MqttSendBuff);

    MQTT_InitBufferQueue();
}

/*****************************************************************************/
/**
 * @brief	:  MQTT_Tick, duoc goi 10ms/1 lan
 * @param	:  
 * @retval	:
 * @author	:	Phinht
 * @created	:	10/11/2014
 * @version	:
 * @reviewer:	
 */
static uint16_t TimeoutTick = 0;
static uint8_t SendRetry = 0;
uint8_t IsTheFirstTime = 1;
void MQTT_Tick(void)
{
    /** Yeu cau close connection */
    if (xSystem.Status.TCPNeedToClose)
        goto CLOSE_TCP_SOCKET;

    /* Publish client messages */
    MQTT_ClientOneSecMessageTick();

    if (ppp_is_up()) /* Online */
    {
        ResetWatchdog();

        if (GSM_Manager.Mode == GSM_AT_MODE)
            return;
        if (GSM_Manager.State != GSM_OK)
            return; //Ex: Dang xu ly SMS, BTS, ATC...

        /** Neu het thoi gian gui tin TCP thi ko xu ly nua */
        //		if(xSystem.Status.SendGPRSTimeout == 0) return;

        /** ========================= Xu ly gui/nhan ban tin MQTT ========================== */
        TimeoutTick++;

        if (isNewTCPPackage)
        {
            MQTT_ProcThread();
        }

        switch (xSystem.Status.MQTTServerState)
        {
        case MQTT_INIT: /* Init MQTT params */
            memset(mqttClientIdBuff, 0, sizeof(mqttClientIdBuff));
            sprintf(mqttClientIdBuff, "HWC_%s", xSystem.Parameters.GSM_IMEI);
            MqttClientInfo.clientID.cstring = mqttClientIdBuff;

            xSystem.Status.MQTTServerState = MQTT_RESOLVE_DNS;
            TimeoutTick = 400;
            DNSResolved = DNS_ERROR_NAME;
            break;

        case MQTT_RESOLVE_DNS: /* DNS resolves the host name every 5s */
            if (DNSResolved != DNS_RES_OK)
            {
                if (TimeoutTick >= 500)
                {
                    TimeoutTick = 0;
                    DEBUG("MQTT: DNS resolving host...%s\r\n", xSystem.Parameters.MQTTHost.Name);
                    get_host_by_name((U8 *)xSystem.Parameters.MQTTHost.Name, mqtt_dns_callback);
                }
            }
            else
            {
                xSystem.Status.MQTTServerState = MQTT_ESTABLISHING;
                SendRetry = 0;
            }
            break;

        case MQTT_ESTABLISHING: /* TCP get socket and connect to broker */
            if (MQTTSocket == 0 && TimeoutTick >= 10)
            {
                DEBUG("MQTT: Get TCP socket...\r\n");
                MQTTSocket = tcp_get_socket(TCP_TYPE_CLIENT, 0, MQTT_KEEP_ALIVE_INTERVAL, mqtt_tcp_callback);
                TimeoutTick = 0;
                DEBUG("MQTT socket %u\r\n", MQTTSocket);
            }
            else
            {
                if (TimeoutTick >= 200) /* send connect TCP to Broker every 2s if it's not connected */
                {
                    TimeoutTick = 0;

                    if (tcp_get_state(MQTTSocket) != TCP_STATE_CONNECT)
                    {
                        DEBUG("MQTT: connecting to: %u.%u.%u.%u:%u, soc: %u\r\n",
                              xSystem.Parameters.MQTTHost.Sub[0], xSystem.Parameters.MQTTHost.Sub[1],
                              xSystem.Parameters.MQTTHost.Sub[2], xSystem.Parameters.MQTTHost.Sub[3],
                              xSystem.Parameters.MQTTHost.Port, MQTTSocket);

                        if (!tcp_connect(MQTTSocket, xSystem.Parameters.MQTTHost.Sub, xSystem.Parameters.MQTTHost.Port, 1000)) /* local port 1000 */
                        {
                            DEBUG("TCP connect error\r\n");
                        }

                        TCPSendDataError++;
                        if (TCPSendDataError >= 30)
                        {
                            DEBUG("\r\nTCP: cannot connect to server..!\r\n");
                            TCPSendDataError = 0;
                            xSystem.Status.TCPNeedToClose = 1;
                        }
                    }
                    else
                        xSystem.Status.MQTTServerState = MQTT_SEND_CONREQ;
                }
            }
            break;

        case MQTT_SEND_CONREQ: /* send MQTT CONNECT request to Broker every 3s */
            if (TimeoutTick >= 300)
            {
                TimeoutTick = 0;
                TCPSendDataError = 0;
                isSubscribed = 0;

                DEBUG("MQTT: Send CONREQ: \r\n");
                MQTTLen = MQTTSerialize_connect(MqttSendBuff, MqttSendBufflen, &MqttClientInfo);

                if (MQTT_TCPSend(MqttSendBuff, MQTTLen) > 0)
                {
                    DEBUG("OK\r\n");
                    SendRetry = 0;
                }
                else
                {
                    DEBUG("ERR\r\n");
                    if (SendRetry++ > 5)
                    {
                        SendRetry = 0;
                        //Close socket and reconnect tcp...
                        DEBUG("TCP: Send CONREQ error many times..!\r\n");
                        xSystem.Status.TCPNeedToClose = 1;
                    }
                }
            }
            break;

#if 0 //No need login to backend	server	
			case MQTT_CONNECTED:	/* Connected to Broker */
				/* Publish login message periodically */
				if(TimeoutTick >= 200)
				{
					TimeoutTick = 0;
						
					if(xSystem.MQTTData.LoginBuffer.State == BUFFER_STATE_IDLE && xSystem.MQTTData.LoginBuffer.BufferIndex > 20)
					{						
						DEBUG("\r\nMQTT send login, leng: %u", xSystem.MQTTData.LoginBuffer.BufferIndex);  
						if(MQTT_TCPSend(xSystem.MQTTData.LoginBuffer.Buffer, xSystem.MQTTData.LoginBuffer.BufferIndex))
						{
							xSystem.MQTTData.LoginBuffer.State = BUFFER_STATE_PROCESSING;
						}
						else {
							if(TCPSendDataError++ >= 5) {
								TCPSendDataError = 0;
								TCPCloseRequest = 1;
							}
						}
					}
				}
				break;
#endif

        case MQTT_CONNECTED: /* Connected to Broker */
            if (IsTheFirstTime)
            {
                IsTheFirstTime = 0;
                DEBUG_PRINTF("GSM will sleep in 5s\r\n");
                GSMSleepAfterSecond(5);
                MqttClientSendFirstMessageWhenWakeup();
            }
            xSystem.Status.DisconnectTimeout = 0;
            if (TimeoutTick >= 200)
            {
                TimeoutTick = 0;

                /* Publish data **********************************************************/
                for (uint8_t i = 0; i < NUM_OF_GPRS_BUFFER; i++)
                {
                    if (xSystem.MQTTData.Buffer[i].State == BUFFER_STATE_IDLE 
                        && xSystem.MQTTData.Buffer[i].BufferIndex > 5)
                    {
                        DEBUG("MQTT: send buffer %u, leng:%u, state:%u\r\n", i, xSystem.MQTTData.Buffer[i].BufferIndex, xSystem.MQTTData.Buffer[i].State);

                        if (MQTT_TCPSend(xSystem.MQTTData.Buffer[i].Buffer, xSystem.MQTTData.Buffer[i].BufferIndex) > 0)
                        {
                            xSystem.MQTTData.Buffer[i].State = BUFFER_STATE_PROCESSING;
                            TCPSendDataError = 0;
                        }
                        else
                        {
                            if (TCPSendDataError++ >= 5)
                            {
                                DEBUG("TCP: Send error many times..!\r\n");
                                TCPSendDataError = 0;
                                xSystem.Status.TCPNeedToClose = 1;
                            }
                        }
                        break;
                    }
                }
            }
            break;

        default:
            break;
        }
    }

    /* Yeu cau dong ket noi TCP */
CLOSE_TCP_SOCKET:
    if (xSystem.Status.WaitToCloseTCP)
        xSystem.Status.WaitToCloseTCP--;
    if (xSystem.Status.TCPNeedToClose && (xSystem.Status.WaitToCloseTCP == 0))
    {
        switch (TCPCloseStep)
        {
        case 0: /* Kiem tra ket noi TCP */
            xSystem.Status.TCPCloseDone = 0;
            DEBUG("Check TCP connection...\r\n");
            if (tcp_get_state(MQTTSocket) == TCP_STATE_CONNECT)
            {
                DEBUG("Connect\r\n");
                TCPCloseStep = 1;
                TCPDisconnectFromServer = 0;
            }
            else
            {
                DEBUG("Disconnect\r\n");
                tcp_close(MQTTSocket);
                TCPCloseStep = 3;
                TCPDisconnectFromServer = 1; /* TCP da bi close truoc do boi server */
            }
            break;

        case 1: /* Abort or close TCP connection */
            xSystem.Status.TCPCloseDone = 0;
            DEBUG("Abort TCP socket...\r\n");
            if (tcp_abort(MQTTSocket))
            {
                DEBUG("OK");
                TCPCloseStep = 2;
            }
            else
                DEBUG("ERR");
            break;

        case 2: /* Release TCP socket */
            xSystem.Status.TCPCloseDone = 0;
            DEBUG("Release TCP socket...\r\n");
            if (tcp_release_socket(MQTTSocket))
            {
                DEBUG("OK");
                TCPCloseStep = 3;
            }
            else
                DEBUG("ERR");
            break;

        case 3: /* Finish */
            DEBUG("TCP: close socket OK!\r\n");
            TCPCloseStep = 0;
            xSystem.Status.TCPNeedToClose = 0;
            xSystem.Status.GuiGoiTinTCP = 0;
            isSubscribed = 0;
            xSystem.Status.TCPCloseDone = 1;
            xSystem.Status.MQTTServerState = MQTT_INIT;

            /* Neu socket khong bi close tu phia server -> huy socket, lan sau get socket moi */
            if (TCPDisconnectFromServer == 0)
            {
                MQTTSocket = 0;
            }
            TCPDisconnectFromServer = 0;

#if (__GSM_SLEEP_MODE__)
            DEBUG("GSM Sleeping...\r\n");
            GSM_GotoSleep();
#endif
            break;

        default:
            break;
        }
    }
}

/*****************************************************************************/
/**
 * @brief	:  Xu ly ban tin MQTT incom, call evey 10ms
 * @param	:  
 * @retval	:
 * @author	:	
 * @created	:	10/03/2016
 * @version	:
 * @reviewer:	
 */
void MQTT_ProcThread(void)
{
    isNewTCPPackage = 0;

    unsigned char sessionPresent, connack_rc = 0;
    unsigned char dup, retained;
    int qos, payloadlen;
    int subcount, granted_qos = 0;
    unsigned short packageId;

    uint8_t msgType = MQTTPacket_read(MqttSendBuff, MqttSendBufflen, getdata);

    //FIX: 2 packages income simulately (only reset this after call getdata() above)
    mqttTcpRecvBuff.BufferIndex = 0;

    /*	CONNECT = 1, CONNACK, PUBLISH, PUBACK, PUBREC, PUBREL,
	*	PUBCOMP, SUBSCRIBE, SUBACK, UNSUBSCRIBE, UNSUBACK,
	*	PINGREQ, PINGRESP, DISCONNECT
	*/
    switch (msgType)
    {
    case CONNECT:
        DEBUG("MQTT: Received CONNECT\r\n");
        break;

    case DISCONNECT:
        DEBUG("\r\nMQTT: Received DISCONNECT\r\n");
        break;

    case CONNACK: /* Connect ack */
        if (MQTTDeserialize_connack(&sessionPresent, &connack_rc, MqttSendBuff, MqttSendBufflen) != 1 || connack_rc != 0)
        {
            DEBUG("MQTT: CONACK is WRONG, code: %d\r\n", connack_rc);
        }
        else
        {
            DEBUG("MQTT: Received CONACK. Broker's connected!\r\n");
            if (xSystem.Status.MQTTServerState == MQTT_SEND_CONREQ)
            {
                xSystem.Status.MQTTServerState = MQTT_CONNECTED;

                /* Send subscribe request now */
                MQTT_SubscribeNewTopic(TOPIC_SUB_HEADER);
            }
        }
        break;

    case SUBACK: /* Subscribe ack */
        DEBUG("MQTT: Received SUBACK\r\n");
        if (MQTTDeserialize_suback(&packageId, 1, &subcount, &granted_qos, MqttSendBuff, MqttSendBufflen) == 1)
        {
            DEBUG("Granted QoS = %d\r\n", granted_qos);
            isSubscribed = 1;
            xSystem.Status.PingTimeout = 180;
        }
        else
        {
            DEBUG("Deserialize FAILED!\r\n");
        }
        break;

    case PUBACK:
        DEBUG("MQTT: Received PUBACK\r\n");
        xSystem.Status.PingTimeout = 180;
        break;

    case PUBLISH: /* Received msg from other peer published */
    {
        DEBUG("MQTT: Received PUBLISH\r\n");
        RecvSubTopic.cstring = NULL;
        RecvSubTopic.lenstring.data = NULL;
        RecvSubTopic.lenstring.len = 0;
        int err = MQTTDeserialize_publish(&dup, &qos, &retained, &packageId, &RecvSubTopic, &subPayload, &payloadlen, MqttSendBuff, MqttSendBufflen);
        if (err == 1)
        {
            DEBUG("MQTT received, payload len: %d, QoS: %d, retain: %d, dup: %u\r\n" /*, msgid: %u"*/, payloadlen, qos, retained, dup /*, packageId*/);

            /* Xu ly ban tin tu Broker */
            MQTT_ProcessDataFromServer((char *)subPayload, payloadlen);

            memset(MqttSendBuff, 0, MqttSendBufflen);
            subPayload = NULL;
        }
        else
        {
            DEBUG("Deserialize FAILED!, err %d\r\n", err);
        }
    }
    break;

    case PUBREC: /* Publish QoS 2 ack */
        //Xac nhan bang ma PUBREL
        break;

    case PINGRESP: /* PING Response Message */
        DEBUG("MQTT: received PING reply!\r\n");
        xSystem.Status.PingTimeout = 180;
        break;

    case UNSUBACK:
        DEBUG("MQTT: received UNSUBACK\r\n");
        /* Send subscribe request again */
        MQTT_SubscribeNewTopic(TOPIC_SUB_HEADER);
        break;

    default: /* Other messages */
        DEBUG("MQTT: msgType %d\r\n", msgType);
        break;
    }
}

static void MQTT_ReconnectTcp(void)
{
    xSystem.Status.MQTTServerState = MQTT_ESTABLISHING;
    SendRetry = 0;
}

/*****************************************************************************/
/**
 * @brief	:  	This function is called on TCP event
 * @param	:  
 * @retval	:
 * @author	:	
 * @created	:	10/03/2016
 * @version	:
 * @reviewer:	
 */
uint16_t mqtt_tcp_callback(uint8_t soc, uint8_t event, uint8_t *ptr, uint16_t par)
{
    uint8_t i;

    DEBUG("TCP callback: socket: %u, event: %u, param: %u\r\n", soc, event, par);

    switch (event)
    {
    case TCP_EVT_CONREQ:
        /* Connect request received event */
        break;

    case TCP_EVT_CONNECT:
        /* Connection established event */
        if (xSystem.Status.MQTTServerState == MQTT_ESTABLISHING)
        {
            xSystem.Status.MQTTServerState = MQTT_SEND_CONREQ;
            SendRetry = 0;
        }
        break;

    case TCP_EVT_CLOSE:
        /* Connection was properly closed  */
        MQTT_ReconnectTcp();
        break;

    case TCP_EVT_ABORT:
        /* Connection is for some reason aborted   */
        MQTT_ReconnectTcp();
        break;

    case TCP_EVT_ACK:
        /* Previously send data acknowledged */
        TCPSendDataError = 0;

#if 0 //If use Login message
			if(xSystem.MQTTData.LoginBuffer.State == BUFFER_STATE_PROCESSING)
			{
				xSystem.MQTTData.LoginBuffer.State = BUFFER_STATE_IDLE;
				xSystem.MQTTData.LoginBuffer.BufferIndex = 0;
			}
#endif

        for (i = 0; i < NUM_OF_MQTT_BUFFER; i++)
        {
            if (xSystem.MQTTData.Buffer[i].State == BUFFER_STATE_PROCESSING)
            {
                xSystem.MQTTData.Buffer[i].BufferIndex = 0;
                xSystem.MQTTData.Buffer[i].State = BUFFER_STATE_IDLE;
                break;
            }
        }
        break;

    case TCP_EVT_DATA:
        /* Data received event */
        ProcessMqttTCPPackage(ptr, par);
        break;
    }
    return (0);
}

uint8_t MQTT_SendPINGReq(void)
{
    if ((MQTTLen = MQTTSerialize_pingreq(MqttSendBuff, MqttSendBufflen)) > 0)
    {
        DEBUG("\r\nMQTT: Send PING ");
        if (MQTT_TCPSend(MqttSendBuff, MQTTLen) < 0)
        {
            DEBUG("[ER]");
            return 0;
        }
        else
        {
            DEBUG("[OK]");
        }
    }
    return 1;
}

/*
* Process TCP received package
* Author: Phinht
*/
static void ProcessMqttTCPPackage(uint8_t *ptr, uint16_t par)
{
    uint16_t dataLen;
    if (par == 0)
        return;

    dataLen = (par < SMALL_BUFFER_SIZE) ? par : SMALL_BUFFER_SIZE;
    memset(mqttTcpRecvBuff.Buffer, 0, SMALL_BUFFER_SIZE);
    memcpy(mqttTcpRecvBuff.Buffer, ptr, dataLen);

    mqttTcpRecvBuff.BufferIndex = dataLen;
    mqttTcpRecvBuff.State = 0;
    isNewTCPPackage = 1;
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
static void mqtt_dns_callback(U8 event, U8 *ip)
{
    switch (event)
    {
    case DNS_EVT_SUCCESS:
        DEBUG("DNS success, host IP: %d.%d.%d.%d\r\n", ip[0], ip[1], ip[2], ip[3]);
        if (mem_test(ip, 0, 4))
            break;
        else
        {
            memcpy(xSystem.Parameters.MQTTHost.Sub, ip, 4);
            DNSResolved = DNS_RES_OK;
        }
        break;
    case DNS_EVT_NONAME:
        DEBUG("DNS: Host's not exist!\r\n");
        break;
    case DNS_EVT_TIMEOUT:
        DEBUG("DNS: Timeout!\r\n");
        break;
    case DNS_EVT_ERROR:
        DEBUG("DNS: ERR!\r\n");
        break;
    }
}

void MQTT_SubscribeNewTopic(char *topic)
{
    char subTopic[50] = {0};
    sprintf(subTopic, "%s%s", TOPIC_SUB_HEADER, xSystem.Parameters.GSM_IMEI);
    SubTopicString.cstring = subTopic;
    DEBUG("MQTT: subscribe topic: %s\r\n", subTopic);

    int subQos = 1;
    MQTTLen = MQTTSerialize_subscribe(MqttSendBuff, MqttSendBufflen, 0, pubPackageId, 1, &SubTopicString, &subQos);
    MQTT_TCPSend(MqttSendBuff, MQTTLen);
}

/*****************************************************************************/
/**
 * @brief	:  	This function is called on TCP event
 * @param	:  
 * @retval	:
 * @author	:	
 * @created	:	10/11/2014
 * @version	:
 * @reviewer:	
 */
static int8_t MQTT_TCPSend(uint8_t *Buffer, uint16_t Length)
{
    if (tcp_check_send(MQTTSocket)) /* OK, socket is ready to send data */
    {
        uint16_t MaxLength = tcp_max_dsize(MQTTSocket);
        uint8_t *SendBuffer = tcp_get_buf(Length);

        if (Length > MaxLength)
        {
            DEBUG("MQTT: Send error, Leng: %u, Max: %u\r\n", Length, MaxLength);
            return -1;
        }
        if (SendBuffer)
        {
            memcpy(SendBuffer, Buffer, Length);
            if (tcp_send(MQTTSocket, SendBuffer, Length) == __FALSE)
            {
                DEBUG("TCP: tcp_send() error!\r\n");
                return -1;
            }
        }
        else
            return -1;
    }
    else
    {
        DEBUG("TCP: socket not ready!\r\n");
        return -1;
    }

    return 1;
}

#endif /* __USE_MQTT__ */

/********************************* END OF FILE *******************************/

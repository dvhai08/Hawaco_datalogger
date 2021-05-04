/******************************************************************************
 * @file    	TCP_ClientLayer.c
 * @author  	
 * @version 	V1.0.0
 * @date    	10/11/2014
 * @brief   	
 ******************************************************************************/


/******************************************************************************
                                   INCLUDES					    			 
 ******************************************************************************/
 #include <string.h>
#include "DataDefine.h"
#include "RTL.h"
#include "TCP.h"
#include "GSM.h"
#include "Main.h"
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
static uint8_t TimeoutTick = 0;
static uint8_t TCPCloseStep = 0;
static uint8_t TCPConnectRetry = 0;
uint8_t TCPConnectFail = 0;
static uint8_t TCPSendDataError = 0;
static uint8_t DNSResolved = DNS_ERROR_NAME;
static uint8_t TCPDisconnectFromServer = 0;
static uint8_t DNSResolveRetry = 0;

/******************************************************************************
                                   LOCAL FUNCTIONS					    			 
 ******************************************************************************/
uint16_t tcp_callback (uint8_t soc, uint8_t event, uint8_t *ptr, uint16_t par) ;
static void dns_callback (U8 event, U8 *ip);


static void ClearTCPBuffer(void)
{
	uint8_t i;
	/* Reset cac buffer */
    for(i = 0; i < NUM_OF_GPRS_BUFFER; i++)
    {
        xSystem.TCP_Data.GPRS_Buffer[i].BufferIndex = 0;
        xSystem.TCP_Data.GPRS_Buffer[i].State = BUFFER_STATE_IDLE;
    }
}

/*****************************************************************************/
/**
 * @brief	:  	Ham quan ly ket noi TCP, duoc goi 100ms/1 lan
 * @param	:  
 * @retval	:
 * @author	:	
 * @created	:	10/11/2015
 * @version	:
 * @reviewer:	
 */
void TCP_Tick(void)
{    
	uint8_t i;
    TimeoutTick++;
		
	if(xSystem.Parameters.DenCauMode == 1) {
		TCP_SendAlarmTick();
	}
	
	if(ppp_is_up())
	{  
		ResetWatchdog();
		
		TCP_ClientMessageTick();
		
		if(GSM_Manager.Mode == GSM_AT_MODE) return;
		if(xSystem.Status.TCPNeedToClose) goto CLOSE_TCP_SOCKET;
		if(GSM_Manager.State != GSM_OK) return;	//Ex: Dang xu ly SMS, BTS, ATC
		
		if(TCPConnectFail == 10 || xSystem.Status.SendGPRSTimeout == 0) return;
		
		switch(xSystem.Status.ServerState)
		{
			case NOT_CONNECTED:
				if(xSystem.Parameters.ServerType == MAKIPOS)
					xSystem.Parameters.CurrentHost = xSystem.Parameters.MainHost;
				else if(xSystem.Parameters.ServerType == CUSTOMER)
					xSystem.Parameters.CurrentHost = xSystem.Parameters.AlterHost;
				TCPSendDataError = 0;
				DNSResolveRetry = 0;
				xSystem.Status.ServerState = DNS_GET_HOST_IP;		
				break;
			
			case DNS_GET_HOST_IP:
				if(DNSResolved != DNS_RES_OK)
				{
					if(TimeoutTick >= 50)		//every 5s
					{
						DEBUG("\rTCP: DNS resolving \"%s\"...", xSystem.Parameters.CurrentHost.Name);
						get_host_by_name ((U8*)xSystem.Parameters.CurrentHost.Name, dns_callback);
						TimeoutTick = 0;
						
						//DNS resolve qua so lan cho phep -> huy gui tin TCP
						if(DNSResolveRetry++ >= 10) {
							TCPConnectFail = 10;
							ClearTCPBuffer();
						}
					}
				}
				else {
					xSystem.Status.ServerState = ESTABLISHING;
					DNSResolveRetry = 0;
				}
				break;
			
			case ESTABLISHING:
				if(xSystem.Status.TCPSocket == 0)
				{
					xSystem.Status.TCPSocket = tcp_get_socket (TCP_TYPE_CLIENT, 0, 120, tcp_callback);
					TimeoutTick = 0;
				}
				else
				{
					if(TimeoutTick >= 20)
					{
						TimeoutTick = 0;
						DEBUG("\rTCP: Mo ket noi toi dia chi: %u.%u.%u.%u:%u", xSystem.Parameters.CurrentHost.Sub[0],
								xSystem.Parameters.CurrentHost.Sub[1],xSystem.Parameters.CurrentHost.Sub[2],
								xSystem.Parameters.CurrentHost.Sub[3],xSystem.Parameters.CurrentHost.Port);
						tcp_connect (xSystem.Status.TCPSocket, xSystem.Parameters.CurrentHost.Sub, xSystem.Parameters.CurrentHost.Port, 0);		
						TCPConnectRetry++;
						if(TCPConnectRetry >= 15)
						{
							TCPConnectRetry = 0;
							xSystem.Status.TCPNeedToClose = 1;
							TCPConnectFail++;
							if(TCPConnectFail == 2)
							{
								TCPConnectFail = 10;
								ClearTCPBuffer();
							}
						}
					}
				}
				break;
							
			case ESTABLISHED:
				if(TimeoutTick >= 20)
				{
					TimeoutTick = 0;
					for(i = 0; i < NUM_OF_GPRS_BUFFER; i++)
					{
						if((xSystem.TCP_Data.GPRS_Buffer[i].State == BUFFER_STATE_IDLE || 
							xSystem.TCP_Data.GPRS_Buffer[i].State == BUFFER_STATE_IDLE2) &&
							xSystem.TCP_Data.GPRS_Buffer[i].BufferIndex > 5)
						{           
//							if(xSystem.TCP_Data.GPRS_Buffer[i].State == BUFFER_STATE_IDLE)
//								xSystem.TCP_Data.GPRS_Buffer[i].State = BUFFER_STATE_PROCESSING;     
//							if(xSystem.TCP_Data.GPRS_Buffer[i].State == BUFFER_STATE_IDLE2)
//								xSystem.TCP_Data.GPRS_Buffer[i].State = BUFFER_STATE_PROCESSING2; 
//							TCPSendDataError++;			
							
							DEBUG("\rTCP send buffer %u, leng:%u, state:%u",i,xSystem.TCP_Data.GPRS_Buffer[i].BufferIndex, xSystem.TCP_Data.GPRS_Buffer[i].State);    
							xSystem.Status.GuiGoiTinTCP = 1;

							if(TCP_SendData(xSystem.TCP_Data.GPRS_Buffer[i].Buffer, xSystem.TCP_Data.GPRS_Buffer[i].BufferIndex) == 1)
							{
								if(xSystem.TCP_Data.GPRS_Buffer[i].State == BUFFER_STATE_IDLE)
									xSystem.TCP_Data.GPRS_Buffer[i].State = BUFFER_STATE_PROCESSING;     
								if(xSystem.TCP_Data.GPRS_Buffer[i].State == BUFFER_STATE_IDLE2)
									xSystem.TCP_Data.GPRS_Buffer[i].State = BUFFER_STATE_PROCESSING2; 
							}
							else
								TCPSendDataError++;
							break;
						}
					}
				}
				break;
		}				
	}
	
	/* Giam sat trang thai TCP gui du lieu bi loi */
	if(TCPSendDataError > 5)
	{
		DEBUG("\rTCP send data error, reconnect!");
		TCPSendDataError = 0;
		xSystem.Status.TCPNeedToClose = 1;
		TCPCloseStep = 0;
	}
	
	/* Yeu cau dong ket noi TCP */
CLOSE_TCP_SOCKET:	
	if(xSystem.Status.WaitToCloseTCP) xSystem.Status.WaitToCloseTCP--;
	if(xSystem.Status.TCPNeedToClose && (xSystem.Status.WaitToCloseTCP == 0))
	{
		switch(TCPCloseStep)
		{
			case 0:	//Kiem tra ket noi TCP
				DEBUG("\rCheck TCP connection...");
				if(tcp_get_state(xSystem.Status.TCPSocket) == TCP_STATE_CONNECT) {
					TCPCloseStep = 1;
					TCPDisconnectFromServer = 0;
					DEBUG("Connect");
				}
				else {
					DEBUG("Disconnect");
					TCPCloseStep = 3;
					TCPDisconnectFromServer = 1;		//TCP da bi close tu phia server
				}
				break;
			
			case 1:	//Abort or close TCP connection
				DEBUG("\rAbort TCP socket...");
				if(tcp_abort (xSystem.Status.TCPSocket)) {
					DEBUG("OK");
					TCPCloseStep = 2;
				} else {
					DEBUG("FAIL");
				}
				break;
				
			case 2:	//Release TCP socket
				DEBUG("\rRelease TCP socket...");
				if(tcp_release_socket (xSystem.Status.TCPSocket)) {
					DEBUG("OK");
					TCPCloseStep = 3;
				} else {
					DEBUG("FAIL");
				}
				break;
			
			case 3:	//Close PPP
				DEBUG("\rTCP connection closed!");
				TCPCloseStep = 0;
				xSystem.Status.GuiGoiTinTCP = 0;
				xSystem.Status.TCPNeedToClose = 0;
			
				//Neu socket khong bi close tu phia server -> huy socket
				if(TCPDisconnectFromServer == 0)
				{
					xSystem.Status.TCPSocket = 0;
				}
				TCPDisconnectFromServer = 0;
				
				//DNS lai dia chi server moi khi gui tin cho chac an!
				xSystem.Status.ServerState = NOT_CONNECTED;
				DNSResolved = DNS_ERROR_NAME;
			
//				if(xSystem.Status.TCPNeedToClose == 2)	//Thay doi server
//				{
//					xSystem.Status.ServerState = NOT_CONNECTED;
//					DNSResolved = DNS_ERROR_NAME;
//				}
//				else {
//					if(mem_test(xSystem.Parameters.CurrentHost.Sub, 0, 4) == __TRUE)
//					{
//						xSystem.Status.ServerState = NOT_CONNECTED;
//						DNSResolved = DNS_ERROR_NAME;
//					} else
//						xSystem.Status.ServerState = ESTABLISHING;
//				}
				
				break;
			default:
				break;
		}
	}
}

/*****************************************************************************/
/**
 * @brief	:  	Thuc hien disconnect ket noi TCP
 * @param	:  
 * @retval	:
 * @author	:	
 * @created	:	10/11/2014
 * @version	:
 * @reviewer:	
 */
void DisconnectTCP(void)
{
	if(xSystem.Status.TCPNeedToClose == 1) return;
    xSystem.Status.ServerState = ESTABLISHING;
}

/*****************************************************************************/
/**
 * @brief	:  	This function is called on DNS event
 * @param	:  
 * @retval	:
 * @author	:	
 * @created	:	10/11/2016
 * @version	:
 * @reviewer:	
 */
static void dns_callback(U8 event, U8 *ip) 
{
	switch (event) 
	{
		case DNS_EVT_SUCCESS:
			ResetWatchdog();
			DEBUG("\rDNS IP: %d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
			//Neu DSN resolve OK thi moi lay dia chi, k lay dia chi 0.0.0.0
			if(ip[0]  == 0 && ip[1] == 0 && ip[2] == 0 && ip[3] == 0)
				break;
			else {
				memcpy(xSystem.Parameters.CurrentHost.Sub, ip, 4);
				DNSResolved = DNS_RES_OK;
			}
			break;
		case DNS_EVT_NONAME:
			DEBUG("Host name does not exist!\n");
			break;
		case DNS_EVT_TIMEOUT:
			DEBUG("DNS Timeout expired!\n");
			break;
		case DNS_EVT_ERROR:
			DEBUG("DNS Error, check the host name!\n");
			break;
	}
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
uint16_t tcp_callback (uint8_t soc, uint8_t event, uint8_t *ptr, uint16_t par) 
{	
	uint8_t i;
	
	DEBUG("\rTCP callback: soc: %u, event: %u, par: %u",soc,event,par);
	
	switch (event) 
	{
		case TCP_EVT_CONREQ :
			/* Connect request received event */
			break;
		case TCP_EVT_CONNECT:
			/* Connection established event */
			if(xSystem.Status.ServerState == ESTABLISHING)
			{
				xSystem.Status.ServerState = ESTABLISHED;
				TCPConnectRetry = 0;
				TCPConnectFail = 0;
			}
			break;				
		case TCP_EVT_CLOSE :
			/* Connection was properly closed  */
      DisconnectTCP();
			break;
		case TCP_EVT_ABORT :
			/* Connection is for some reason aborted   */
      DisconnectTCP();
			break;
		case TCP_EVT_ACK:
			/* Previously send data acknowledged */  
			TCPSendDataError = 0;	
	
            for(i = 0; i < NUM_OF_GPRS_BUFFER; i++)
            {
                if(xSystem.TCP_Data.GPRS_Buffer[i].State == BUFFER_STATE_PROCESSING ||
					xSystem.TCP_Data.GPRS_Buffer[i].State == BUFFER_STATE_PROCESSING2)
                {
                    xSystem.TCP_Data.GPRS_Buffer[i].BufferIndex = 0;
                    xSystem.TCP_Data.GPRS_Buffer[i].State = BUFFER_STATE_IDLE;
					xSystem.Status.GuiGoiTinTCP = 0;
                    break;
                }
            }
			
			//Con ban tin can gui khong?
			for(i = 0; i < NUM_OF_GPRS_BUFFER; i++)
			{
				if((xSystem.TCP_Data.GPRS_Buffer[i].State == BUFFER_STATE_IDLE || 
					xSystem.TCP_Data.GPRS_Buffer[i].State == BUFFER_STATE_IDLE2) &&
					xSystem.TCP_Data.GPRS_Buffer[i].BufferIndex > 5) 
				{
					TimeoutTick = 0;
					return 0;
				}						
			}
			/* Gia su ban tin Phan hoi cau hinh ko duoc Server phan hoi -> close connection */
			if(xSystem.Status.SendGPRSRequest == GPRS_BANTINKHAC)
			{
				xSystem.Status.SendGPRSRequest = 0;
				xSystem.Status.ResendGPRSRequest = 0;
				xSystem.Status.SendGPRSTimeout = 0;
				xSystem.Status.TCPNeedToClose = 1;
				xSystem.Status.WaitToCloseTCP = 50;
			}
			break;
			
		case TCP_EVT_DATA:
			/* Data received event */
			ProcessPacketTCP(ptr,par);
			break;		
	}
	return (0);
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

uint8_t TCP_SendData(uint8_t *Buffer, uint16_t Length)
{		
	if(tcp_check_send(xSystem.Status.TCPSocket))	/* OK, socket is ready to send data */
	{
		uint16_t MaxLength = tcp_max_dsize(xSystem.Status.TCPSocket);
		uint8_t *SendBuffer = tcp_get_buf(Length);
		
		if(Length > MaxLength)
		{
			DEBUG("\rTCP: Send error, Len: %u, MaxLen: %u",Length,MaxLength);
			return 0;
		}
		memcpy(SendBuffer,Buffer,Length);
		tcp_send (xSystem.Status.TCPSocket, SendBuffer, Length);
		return 1;
	}
	else {
		DEBUG("\rTCP socket not ready!");
		return 0;
	}
}
/********************************* END OF FILE *******************************/

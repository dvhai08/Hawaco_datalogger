/******************************************************************************
 * @file    	FTP_HardwareLayer.c
 * @author  	Khanhpd
 * @version 	V1.0.0
 * @date    	10/11/2014
 * @brief   	Rewrite at_ftp.c library
 ******************************************************************************/


/******************************************************************************
                                   INCLUDES					    			 
 ******************************************************************************/
#include "DataDefine.h"
#include "Net_Config.h"
#include "Utilities.h"

/******************************************************************************
                                   GLOBAL VARIABLES					    			 
 ******************************************************************************/


/******************************************************************************
                                   GLOBAL FUNCTIONS					    			 
 ******************************************************************************/


/******************************************************************************
                                   DATA TYPE DEFINE					    			 
 ******************************************************************************/
#define STATE_NONE         0
#define STATE_INITIAL      1
#define STATE_SEND_USER    2
#define STATE_USER_SENT    3
#define STATE_SEND_PASS    4
#define STATE_PASS_SENT    5
#define STATE_SEND_PORT    6
#define STATE_PORT_SENT    7
#define STATE_SEND_OPTIONS 8
#define STATE_OPTION_SENT  9
#define STATE_CONNECTED   10
#define STATE_SEND_NLST   11
#define STATE_NLST_SENT   12

#define STATE_SEND_RETR   13
#define STATE_RETR_SENT   14

#define STATE_SEND_STOR   15
#define STATE_STOR_SENT   16

#define STATE_SEND_CWD    17
#define STATE_CWD_SENT    18

#define STATE_SEND_CDUP   19
#define STATE_CDUP_SENT   20

#define STATE_SEND_QUIT   21
#define STATE_QUIT_SENT   22

#define STATE_SEND_PASV   23
#define STATE_PASV_SENT   24

#define STATE_SEND_TYPE   25
#define STATE_TYPE_SENT   26

#define FTP_DATA_PORT   62345;

#define FTP_FILE_HANDLER_DONE   1
#define FTP_FILE_HANDLER_PROCESSING   2

/******************************************************************************
                                   PRIVATE VARIABLES					    			 
 ******************************************************************************/
struct {
    uint8_t State;
    uint8_t ControlSocket;             
    uint8_t DataSocket;
    
    IPStruct_t IP;
    
    void (*NotifyCallBack) (U8 event) ;
    void *File;
    uint8_t FileHandlerState;
    uint8_t Command;
    uint16_t FTP_Timeout;
    char ControlCommand[50];
}FTPC_HardwareLayer;

uint8_t ProcessSendControlToFTP(char *ControlBuffer);

/******************************************************************************
                                   LOCAL FUNCTIONS					    			 
 ******************************************************************************/
uint16_t ftp_control_callback (uint8_t soc, uint8_t event, uint8_t *ptr, uint16_t par) ;
uint16_t ftp_data_callback (uint8_t soc, uint8_t event, uint8_t *ptr, uint16_t par) ;

void ProcessSendDataToFTP(void);
void FinishFTPTransfer(void);
void ProcessControlDataFromFTP(uint8_t *ptr);
uint16_t GetDataPortInPassiveMode(uint8_t *ptr);

/*****************************************************************************/
/**
 * @brief	:  
 * @param	:  
 * @retval	:
 * @author	:	Khanhpd
 * @created	:	10/11/2014
 * @version	:
 * @reviewer:	
 */
void ftpc_run_client (void) 
{
    if(FTPC_HardwareLayer.FTP_Timeout)
    {
        FTPC_HardwareLayer.FTP_Timeout--;
        if(FTPC_HardwareLayer.FTP_Timeout == 0)
        {
            FinishFTPTransfer();
            FTPC_HardwareLayer.NotifyCallBack(FTPC_EVT_TIMEOUT);
        }
    }
    
    switch(FTPC_HardwareLayer.State)
    {
        case STATE_SEND_USER:
            sprintf(FTPC_HardwareLayer.ControlCommand,"USER khanhpd\r\n");
            if(ProcessSendControlToFTP(FTPC_HardwareLayer.ControlCommand))
            {
                FTPC_HardwareLayer.State = STATE_USER_SENT;
            }
            break;
        case STATE_SEND_PASS:
            sprintf(FTPC_HardwareLayer.ControlCommand,"PASS 123\r\n");
            if(ProcessSendControlToFTP(FTPC_HardwareLayer.ControlCommand))
            {
                FTPC_HardwareLayer.State = STATE_PASS_SENT;
            }
            break;
        case STATE_SEND_PASV:
            sprintf(FTPC_HardwareLayer.ControlCommand,"PASV\r\n");
            if(ProcessSendControlToFTP(FTPC_HardwareLayer.ControlCommand))
            {
                FTPC_HardwareLayer.State = STATE_PASS_SENT;
            }
            break;
        case STATE_SEND_TYPE:
            sprintf(FTPC_HardwareLayer.ControlCommand,"TYPE A\r\n");
            if(ProcessSendControlToFTP(FTPC_HardwareLayer.ControlCommand))
            {
                FTPC_HardwareLayer.State = STATE_TYPE_SENT;
            }
            break;
        case STATE_SEND_STOR:
            sprintf(FTPC_HardwareLayer.ControlCommand,"STOR BA2.txt\r\n");
            if(ProcessSendControlToFTP(FTPC_HardwareLayer.ControlCommand))
            {
                FTPC_HardwareLayer.State = STATE_STOR_SENT;
                ProcessSendDataToFTP();
            }
            break;
        case STATE_SEND_RETR:
            sprintf(FTPC_HardwareLayer.ControlCommand,"RETR BA2.txt\r\n");
            if(ProcessSendControlToFTP(FTPC_HardwareLayer.ControlCommand))
            {
                FTPC_HardwareLayer.State = STATE_RETR_SENT;
            }
            break;
    }
    
}
/*****************************************************************************/
/**
 * @brief	:  
 * @param	:  
 * @retval	:
 * @author	:	Khanhpd
 * @created	:	10/11/2014
 * @version	:
 * @reviewer:	
 */
BOOL ftpc_connect (U8 *ipadr, U16 port, U8 command, void (*cbfunc)(U8 event))
{		
    memcpy(FTPC_HardwareLayer.IP.Sub,ipadr,4);
    FTPC_HardwareLayer.IP.Port = port;
    
    FTPC_HardwareLayer.State = STATE_INITIAL;
    
    FTPC_HardwareLayer.Command = command;
    
    FTPC_HardwareLayer.NotifyCallBack = cbfunc;
    FTPC_HardwareLayer.FileHandlerState = FTP_FILE_HANDLER_PROCESSING;
    
    FTPC_HardwareLayer.FTP_Timeout = 1200; // Set timeout to 120s
    
    return tcp_connect (FTPC_HardwareLayer.ControlSocket, FTPC_HardwareLayer.IP.Sub, FTPC_HardwareLayer.IP.Port, 0);		
}

/*****************************************************************************/
/**
 * @brief	:  
 * @param	:  
 * @retval	:
 * @author	:	Khanhpd
 * @created	:	10/11/2014
 * @version	:
 * @reviewer:	
 */
void ftpc_init (void) 
{
	FTPC_HardwareLayer.State = STATE_NONE;
        
    FTPC_HardwareLayer.DataSocket = tcp_get_socket (TCP_TYPE_CLIENT | TCP_TYPE_KEEP_ALIVE, 0, 120, ftp_data_callback);
	FTPC_HardwareLayer.ControlSocket = tcp_get_socket (TCP_TYPE_CLIENT | TCP_TYPE_KEEP_ALIVE, 0, 120, ftp_control_callback);
}
/*****************************************************************************/
/**
 * @brief	:  
 * @param	:  
 * @retval	:
 * @author	:	Khanhpd
 * @created	:	10/11/2014
 * @version	:
 * @reviewer:	
 */
uint16_t ftp_control_callback (uint8_t soc, uint8_t event, uint8_t *ptr, uint16_t par) 
{
    DebugPrint("\rftp_control_callback: soc : %u,event : %u,par : %u",soc,event,par);
	
	switch (event) 
	{
		case TCP_EVT_CONREQ :
			/* Connect request received event          */
			break;
		case TCP_EVT_CONNECT:
			/* Connection established event */
            
			break;				
		case TCP_EVT_CLOSE :
			/* Connection was properly closed          */
            FTPC_HardwareLayer.NotifyCallBack(FTPC_EVT_ERROR);
			break;
		case TCP_EVT_ABORT :
			/* Connection is for some reason aborted   */
            FTPC_HardwareLayer.NotifyCallBack(FTPC_EVT_ERROR);
			break;
		case TCP_EVT_ACK:
			/* Previously send data acknowledged */
			break;
		case TCP_EVT_DATA:
			/* Data received event */
            ProcessControlDataFromFTP(ptr);
			break;		
	}
	
	return (0);
}
/*****************************************************************************/
/**
 * @brief	:  
 * @param	:  
 * @retval	:
 * @author	:	Khanhpd
 * @created	:	10/11/2014
 * @version	:
 * @reviewer:	
 */
uint16_t ftp_data_callback (uint8_t soc, uint8_t event, uint8_t *ptr, uint16_t par) 
{    
    uint16_t tmpInt;
    
    DebugPrint("\rftp_data_callback: soc : %u,event : %u,par : %u",soc,event,par);
	
	switch (event) 
	{
		case TCP_EVT_CONREQ :
			/* Connect request received event          */
			break;
		case TCP_EVT_CONNECT:
			/* Connection established event */            
            if(FTPC_HardwareLayer.Command == FTPC_CMD_GET)
            {
                FTPC_HardwareLayer.File = ftpc_fopen("wb");                 
            }
            else
            {
                FTPC_HardwareLayer.File = ftpc_fopen("rb");
            }
            
            if(FTPC_HardwareLayer.File == NULL) 
            {
                tcp_close(FTPC_HardwareLayer.ControlSocket);
                FTPC_HardwareLayer.NotifyCallBack(FTPC_EVT_ERRLOCAL);
                
                return 0;
            }            
			break;				
		case TCP_EVT_CLOSE :
			/* Connection was properly closed          */
            FinishFTPTransfer();
            FTPC_HardwareLayer.NotifyCallBack(FTPC_EVT_SUCCESS);
			break;
		case TCP_EVT_ABORT :
			/* Connection is for some reason aborted   */
            FinishFTPTransfer();
            FTPC_HardwareLayer.NotifyCallBack(FTPC_EVT_ERROR);
			break;
		case TCP_EVT_ACK:
			/* Previously send data acknowledged */
            if(FTPC_HardwareLayer.Command == FTPC_CMD_PUT)
            {
                if(FTPC_HardwareLayer.FileHandlerState == FTP_FILE_HANDLER_DONE)
                {
                    FinishFTPTransfer();
                }
                else
                {
                    ProcessSendDataToFTP();                
                }
            }
			break;
		case TCP_EVT_DATA:
			/* Data received event */                        
            if(FTPC_HardwareLayer.Command == FTPC_CMD_GET)
            {
                tmpInt = ftpc_fwrite(FTPC_HardwareLayer.File,ptr,par);                
                if(tmpInt < par)
                {
                    FinishFTPTransfer();
                    FTPC_HardwareLayer.NotifyCallBack(FTPC_EVT_ERROR);
                }
            }
			break;		
	}
	
	return (0);
}

/*****************************************************************************/
/**
 * @brief	:  
 * @param	:  
 * @retval	:
 * @author	:	Khanhpd
 * @created	:	10/11/2014
 * @version	:
 * @reviewer:	
 */
void ProcessSendDataToFTP(void)
{
    uint16_t tmpInt;
    uint16_t MaxLength = tcp_max_dsize(FTPC_HardwareLayer.DataSocket);
    uint8_t *SendBuffer = tcp_get_buf(MaxLength);
    
    tmpInt = ftpc_fread(FTPC_HardwareLayer.File,SendBuffer,MaxLength);        
    if(tmpInt < MaxLength) FTPC_HardwareLayer.FileHandlerState = FTP_FILE_HANDLER_DONE;
    
    tcp_send (FTPC_HardwareLayer.DataSocket, SendBuffer, tmpInt);
}
/*****************************************************************************/
/**
 * @brief	:  
 * @param	:  
 * @retval	:
 * @author	:	Khanhpd
 * @created	:	10/11/2014
 * @version	:
 * @reviewer:	
 */
uint8_t ProcessSendControlToFTP(char *ControlBuffer)
{
    uint16_t MaxLength = tcp_max_dsize(FTPC_HardwareLayer.ControlSocket);
    uint8_t *SendBuffer = tcp_get_buf(strlen(ControlBuffer));
    uint8_t SendResult;        
    
    if(tcp_check_send (FTPC_HardwareLayer.ControlSocket))
    {
        memcpy(SendBuffer,ControlBuffer,strlen(ControlBuffer));
    
        SendResult = tcp_send (FTPC_HardwareLayer.ControlSocket, SendBuffer, strlen(ControlBuffer));

        DebugPrint("\rFTPC : ProcessSendControlToFTP : Length : %u, MaxLength : %u, Result : %u,Data : %s",strlen(ControlBuffer),MaxLength,SendResult, ControlBuffer);
        
        if(SendResult) return 1;
    }
    else
    {
        DebugPrint("\rFTPC : ProcessSendControlToFTP : Can not send Data");
    }
    
    return 0;
}
/*****************************************************************************/
/**
 * @brief	:  
 * @param	:  
 * @retval	:
 * @author	:	Khanhpd
 * @created	:	10/11/2014
 * @version	:
 * @reviewer:	
 */
void FinishFTPTransfer(void)
{
    if(FTPC_HardwareLayer.File != NULL)
    {
        ftpc_fclose(FTPC_HardwareLayer.File);
    }
    
    tcp_close(FTPC_HardwareLayer.DataSocket);
    tcp_close(FTPC_HardwareLayer.ControlSocket);        
}
/*****************************************************************************/
/**
 * @brief	:  
 * @param	:  
 * @retval	:
 * @author	:	Khanhpd
 * @created	:	10/11/2014
 * @version	:
 * @reviewer:	
 */
void ProcessControlDataFromFTP(uint8_t *ptr)
{
    uint16_t ResponseCode = GetNumberFromString(0,ptr);
    
    DebugPrint("\rFTPC : Process respone code : %u",ResponseCode);
    
    switch(ResponseCode)
    {
        case 220:   
            /* Connected, need UserName */
            if(FTPC_HardwareLayer.State == STATE_INITIAL)
            {                
                FTPC_HardwareLayer.State = STATE_SEND_USER;                
            }
            break;
        case 331:   
            /* Password required */
            if(FTPC_HardwareLayer.State == STATE_USER_SENT)
            {                
                FTPC_HardwareLayer.State = STATE_SEND_PASS;
            }
            break;
        case 230:
            /* Logged on, enter Passsive mode */
            if(FTPC_HardwareLayer.State == STATE_PASS_SENT)
            {             
                FTPC_HardwareLayer.State = STATE_SEND_PASV;                
            }            
            break;
        case 227:
            /* Enter passive mode ok */
            FTPC_HardwareLayer.IP.Port = GetDataPortInPassiveMode(ptr);
            DebugPrint("\rFTPC : Data port : %u",FTPC_HardwareLayer.IP.Port);
            tcp_connect(FTPC_HardwareLayer.DataSocket,FTPC_HardwareLayer.IP.Sub,FTPC_HardwareLayer.IP.Port,0);
            
            FTPC_HardwareLayer.State = STATE_SEND_TYPE;                  
            break;
        case 200:
            if(FTPC_HardwareLayer.State == STATE_TYPE_SENT)
            {
                if(FTPC_HardwareLayer.Command == FTPC_CMD_GET)
                {
                    FTPC_HardwareLayer.State = STATE_SEND_RETR;
                }

                if(FTPC_HardwareLayer.Command == FTPC_CMD_PUT)
                {
                    FTPC_HardwareLayer.State = STATE_SEND_STOR;
                }       
            }
            break;
        case 150 :
            /* Connection accepted */
            break;
        case 226:
            /* Transfer OK */
            FinishFTPTransfer();
            FTPC_HardwareLayer.NotifyCallBack(FTPC_EVT_SUCCESS);
            break;
        default:
            /* Unpredic response code, return error */
            FinishFTPTransfer();
            FTPC_HardwareLayer.NotifyCallBack(FTPC_EVT_ERROR);
            break;
    }
}

/*****************************************************************************/
/**
 * @brief	:  
 * @param	:  
 * @retval	:
 * @author	:	Khanhpd
 * @created	:	10/11/2014
 * @version	:
 * @reviewer:	
 */
uint16_t GetDataPortInPassiveMode(uint8_t *ptr)
{
    uint8_t i, j;
    uint16_t Port = 0;
    
    for(i = strlen((char *)ptr); i > 0; i--)
    {
        if(ptr[i] == ',')
        {
            Port = GetNumberFromString(1, &ptr[i]);
            
            for(j = i - 1; j > 0; j--)
            {
                if(ptr[j] == ',')
                {
                    Port += GetNumberFromString(1, &ptr[j]) * 256;
                    
                    return Port;
                }
            }
        }
    }
    
    return 0xFFFF;
}
/********************************* END OF FILE *******************************/

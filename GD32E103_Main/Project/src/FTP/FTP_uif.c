/******************************************************************************
 * @file    	FTPC_uif.c
 * @author  	Phinht
 * @version 	V1.0.0
 * @date    	15/01/2014
 * @brief   	
 ******************************************************************************/

/******************************************************************************
                                   INCLUDES					    			 
 ******************************************************************************/
#include "UpdateFirmwareFTP.h"

/******************************************************************************
                                   GLOBAL VARIABLES					    			 
 ******************************************************************************/
extern System_t xSystem;
/******************************************************************************
                                   GLOBAL FUNCTIONS					    			 
 ******************************************************************************/

/******************************************************************************
                                   DATA TYPE DEFINE					    			 
 ******************************************************************************/

/******************************************************************************
                                   PRIVATE VARIABLES					    			 
 ******************************************************************************/

/******************************************************************************
                                   LOCAL FUNCTIONS					    			 
 ******************************************************************************/

/*****************************************************************************/

/*****************************************************************************/

/*****************************************************************************/
/**
 * @brief	:  	Mo local file
 * @param	:  	None
 * @retval	:	None
 * @author	:	Phinht
 * @created	:	15/01/2014
 * @version	:
 * @reviewer:	
 */
void *ftpc_fopen (U8 *mode) 
{	
    /* Open local file for reading or writing. If the return value is NULL, */
    /* processing of FTP Client commands PUT, APPEND or GET is cancelled.   */
	DEBUG("\rFTPC open file, mode: %s", mode);
	
	if(xSystem.FileTransfer.State != FT_TRANSFERRING) return NULL;
		
	return ((void *) 0xFFFF);
}


/*****************************************************************************/
/**
 * @brief	:  	Close local file
 * @param	:  	None
 * @retval	:	None
 * @author	:	Phinht
 * @created	:	15/01/2014
 * @version	:
 * @reviewer:	
 */
void ftpc_fclose (void *file) 
{
    /* Close a local file. */
	DEBUG("\rFTPC close file");	
}


/*****************************************************************************/
/**
 * @brief	:  	Read data from local file
 * @param	:  	None
 * @retval	:	None
 * @author	:	Phinht
 * @created	:	15/01/2014
 * @version	:
 * @reviewer:	
 */
U16 ftpc_fread (void *file, U8 *buf, U16 len) 
{
	/* Read 'len' bytes from file to buffer 'buf'. Return number of bytes   */
    /* copied. The file will be closed, when the return value is 0.         */
    /* For optimal performance the return value should be 'len'             */
		
	return 0;
}

/*****************************************************************************/
/**
 * @brief	:  	Write data to local file
 * @param	:  	None
 * @retval	:	None
 * @author	:	Phinht
 * @created	:	15/01/2014
 * @version	:
 * @reviewer:	
 */
U16 ftpc_fwrite (void *file, U8 *buf, U16 len) 
{
	uint16_t DoDaiCanGhi;
	
	if(xSystem.FileTransfer.DataTransfered + len > xSystem.FileTransfer.FileSize.value)
		DoDaiCanGhi = xSystem.FileTransfer.FileSize.value - xSystem.FileTransfer.DataTransfered;
	else
		DoDaiCanGhi = len;
	
	DEBUG("\rFTPC write: %u bytes. Addr: %X", DoDaiCanGhi, xSystem.FileTransfer.FileAddress);
	Flash_WriteBytes(xSystem.FileTransfer.FileAddress, buf, DoDaiCanGhi);
	xSystem.FileTransfer.DataTransfered += DoDaiCanGhi;
	xSystem.FileTransfer.FileAddress += DoDaiCanGhi;
	
    return DoDaiCanGhi;
}


/*--------------------------- ftpc_cbfunc -----------------------------------*/
 /* This function is called by the FTP client to get parameters. It returns*/
  /* the number of bytes written to buffer 'buf'. This function should NEVER*/
  /* write more than 'buflen' bytes to this buffer.                         */
  /* Parameters:                                                            */
  /*   code   - function code with following values:                        */
  /*             0 = Username for FTP authentication                        */
  /*             1 = Password for FTP authentication                        */
  /*             2 = Working directory path for all commands                */
  /*             3 = Filename for PUT, GET, APPEND, DELETE, RENAME command  */
  /*             4 - New filename for RENAME command                        */
  /*             5 - Directory name for MKDIR, RMDIR command                */
  /*             6 - File filter/mask for LIST command (wildcards allowed)  */
  /*             7 = Received directory listing on LIST command             */
  /*   buf    - transmit/receive buffer                                     */
  /*   buflen - length of this buffer                                       */
  /*             on transmit, it specifies size of 'buf'                    */
  /*             on receive, specifies the number of bytes received         */

U16 ftpc_cbfunc (U8 code, U8 *buf, U16 buflen) 
{   
    U32 len = 0;

    switch (code) 
    {
        case 0:
            /* Enter Username for login. */            
            len = str_copy (buf, (U8 *)xSystem.FileTransfer.FTP_UserName);
            break;

        case 1:
            /* Enter Password for login. */
            len = str_copy (buf, (U8 *)xSystem.FileTransfer.FTP_Pass);
            break;

        case 2:
            /* Enter a working directory path. */
            len = str_copy (buf, (U8 *)xSystem.FileTransfer.FTP_Path);
            break;

        case 3:
            /* Enter a filename for file operations. */
            len = str_copy (buf, (U8 *)xSystem.FileTransfer.FTP_FileName);
            break;

        case 4:
            /* Enter a new name for rename command. */            
            break;

        case 5:
            /* Enter a directory name to create or remove. */
            break;

        case 6:
            /* Enter a file filter for list command. */
            break;

        case 7:
            /* Process received directory listing in raw format. */
            /* Function return value is don't care.              */
            break;
    }
    return ((U16)len);
}

/* FTP Client Callback Events */
/*	FTPC_EVT_SUCCESS   0		-	File operation successful               */
/*	FTPC_EVT_TIMEOUT   1	-	Timeout on file operation               */
/*	FTPC_EVT_LOGINFAIL 2	-	Login error, username/passw invalid     */
/*	FTPC_EVT_NOACCESS  3	-	File access not allowed                 */
/*	FTPC_EVT_NOTFOUND  4	-	File not found                          */
/*	FTPC_EVT_NOPATH    5		-	Working directory path not found        */
/*	FTPC_EVT_ERRLOCAL  6	-	Local file open error                   */
/*	FTPC_EVT_ERROR     7		-	Generic FTP client error                */

void ftpc_notify (U8 event) 
{
    DEBUG("\rFTPC notify: %u. ", event);
    switch (event) 
    {
        case FTPC_EVT_SUCCESS:  
            /* Command successful */		
			DEBUG("\rDownload file %s DONE", xSystem.FileTransfer.FTP_FileName);
			xSystem.FileTransfer.State = FT_TRANSFER_DONE;         
            break;

        case FTPC_EVT_TIMEOUT:
            /* Failed, timeout expired */
			DEBUG("Timeout expired");
			if(xSystem.FileTransfer.FileAddress != (FWUD_BLOCK1<<16))		//Da ghi data truoc do
			{
				Flash_EraseBlock64(FWUD_BLOCK1);
				Flash_EraseBlock64(FWUD_BLOCK2);
			}
            xSystem.FileTransfer.State = FT_WAIT_TRANSFER_STATE;
			if(xSystem.FileTransfer.Retry) xSystem.FileTransfer.Retry--;
			DEBUG("\rUDFW: Retry %d, erase Flash [DONE]", xSystem.FileTransfer.Retry);
            break;

        case FTPC_EVT_LOGINFAIL:
            /* Failed, username/password invalid */
			if(xSystem.FileTransfer.Retry) xSystem.FileTransfer.Retry--;
            break;

        case FTPC_EVT_NOACCESS:
            /* Failed, operation not allowed */
			if(xSystem.FileTransfer.Retry) xSystem.FileTransfer.Retry--;
            break;

        case FTPC_EVT_NOTFOUND:
            /* Failed, file or path not found */
			if(xSystem.FileTransfer.Retry) xSystem.FileTransfer.Retry--;
            break;

        case FTPC_EVT_NOPATH:
            /* Failed, working directory not found */
			if(xSystem.FileTransfer.Retry) xSystem.FileTransfer.Retry--;
            break;

        case FTPC_EVT_ERRLOCAL:
            /* Failed, local file open/write error */
			if(xSystem.FileTransfer.Retry) xSystem.FileTransfer.Retry--;
            break;

        case FTPC_EVT_ERROR:
            /* Failed, unspecified protocol error */
			DEBUG("client error!");
		
			if(xSystem.FileTransfer.FileAddress != (FWUD_BLOCK1<<16))		//Da ghi data truoc do
			{
				Flash_EraseBlock64(FWUD_BLOCK1);
				Flash_EraseBlock64(FWUD_BLOCK2);
			}
            xSystem.FileTransfer.State = FT_WAIT_TRANSFER_STATE;
			if(xSystem.FileTransfer.Retry) xSystem.FileTransfer.Retry--;
			DEBUG("\rUDFW: Retry %d, erase Flash [DONE]", xSystem.FileTransfer.Retry);
            break;
    }
}

/*----------------------------------------------------------------------------
 * end of file
 *---------------------------------------------------------------------------*/

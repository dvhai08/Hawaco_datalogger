/******************************************************************************
 * @file    	FTPC_uif.c
 * @author  	Khanhpd
 * @version 	V1.0.0
 * @date    	15/01/2014
 * @brief   	
 ******************************************************************************/


/******************************************************************************
                                   INCLUDES					    			 
 ******************************************************************************/
#include <Net_Config.h>
#include "DataDefine.h"
//#include "FTP.h"
//#include "NandFlash.h"
#include "Flash.h"

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
 * @author	:	Khanhpd
 * @created	:	15/01/2014
 * @version	:
 * @reviewer:	
 */
void *ftpc_fopen (U8 *mode) 
{	
    /* Open local file for reading or writing. If the return value is NULL, */
    /* processing of FTP Client commands PUT, APPEND or GET is cancelled.   */
	DebugPrint("\rFTPC open file, mode : %s",mode);
	
	if(xSystem.FileTransfer.State != FT_TRANSFERRING) return NULL;
	
#ifndef _USE_SD_CARD_	
	return ((void *) 0xFFFF);
#else
	return (fopen (xSystem.FileTransfer.FTP_FileName, (char *)mode));
#endif	
}


/*****************************************************************************/
/**
 * @brief	:  	Close local file
 * @param	:  	None
 * @retval	:	None
 * @author	:	Khanhpd
 * @created	:	15/01/2014
 * @version	:
 * @reviewer:	
 */
void ftpc_fclose (void *file) 
{
    /* Close a local file. */
	DebugPrint("\rFTPC close file");	

#ifdef _USE_SD_CARD_
	fclose (file);
#endif
}


/*****************************************************************************/
/**
 * @brief	:  	Read data from local file
 * @param	:  	None
 * @retval	:	None
 * @author	:	Khanhpd
 * @created	:	15/01/2014
 * @version	:
 * @reviewer:	
 */
U16 ftpc_fread (void *file, U8 *buf, U16 len) 
{
	/* Read 'len' bytes from file to buffer 'buf'. Return number of bytes   */
    /* copied. The file will be closed, when the return value is 0.         */
    /* For optimal performance the return value should be 'len'             */
	
#ifdef _USE_SD_CARD_
	DebugPrint("\rFTPC read : %u bytes",len);
	return (fread (buf, 1, len, file));
#else	
	uint16_t DoDaiCanDoc;
	    
    if(xSystem.FileTransfer.DataTransfered + len > xSystem.FileTransfer.FileSize.value)
		DoDaiCanDoc = xSystem.FileTransfer.FileSize.value - xSystem.FileTransfer.DataTransfered;
	else
		DoDaiCanDoc = len;
	
	DebugPrint("\rFTPC read : %u bytes",DoDaiCanDoc);
	
//	NAND_Read(buf, xSystem.FileTransfer.FileAddress + xSystem.FileTransfer.DataTransfered, DoDaiCanDoc);
	FlashReadBytes(xSystem.FileTransfer.FileAddress + xSystem.FileTransfer.DataTransfered, buf, DoDaiCanDoc);
	
	xSystem.FileTransfer.DataTransfered += DoDaiCanDoc;
	
	return DoDaiCanDoc;
#endif
}


/*****************************************************************************/
/**
 * @brief	:  	Write data to local file
 * @param	:  	None
 * @retval	:	None
 * @author	:	Khanhpd
 * @created	:	15/01/2014
 * @version	:
 * @reviewer:	
 */
U16 ftpc_fwrite (void *file, U8 *buf, U16 len) 
{
#ifdef _USE_SD_CARD_
	DebugPrint("\rFTPC write : %u bytes",len);
	return (fwrite (buf, 1, len, file));
#else	
	uint16_t DoDaiCanGhi;
	
	if(xSystem.FileTransfer.DataTransfered + len > xSystem.FileTransfer.FileSize.value)
		DoDaiCanGhi = xSystem.FileTransfer.FileSize.value - xSystem.FileTransfer.DataTransfered;
	else
		DoDaiCanGhi = len;
	
	DebugPrint("\rFTPC write : %u bytes",DoDaiCanGhi);
	
//	NAND_Write(buf, xSystem.FileTransfer.FileAddress + xSystem.FileTransfer.DataTransfered, DoDaiCanGhi);
	FlashWriteBytes(xSystem.FileTransfer.FileAddress + xSystem.FileTransfer.DataTransfered, buf, DoDaiCanGhi);
	
	xSystem.FileTransfer.DataTransfered += DoDaiCanGhi;
    
    return DoDaiCanGhi;
#endif	
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
    U32 i,len = 0;

    switch (code) 
    {
        case 0:
            /* Enter Username for login. */            
            len = str_copy (buf, xSystem.FileTransfer.FTP_UserName);
            break;

        case 1:
            /* Enter Password for login. */
            len = str_copy (buf, xSystem.FileTransfer.FTP_Pass);
            break;

        case 2:
            /* Enter a working directory path. */
            len = str_copy (buf, xSystem.FileTransfer.FTP_Path);
            break;

        case 3:
            /* Enter a filename for file operations. */
            len = str_copy (buf, xSystem.FileTransfer.FTP_FileName);
            break;

        case 4:
//            /* Enter a new name for rename command. */            
//            len = str_copy (buf, FTPC_NEWNAME);
            break;

        case 5:
//            /* Enter a directory name to create or remove. */
//            len = str_copy (buf, FTPC_DIRNAME);
            break;

        case 6:
//            /* Enter a file filter for list command. */
//            len = str_copy (buf, FTPC_LISTNAME);
            break;

        case 7:
            /* Process received directory listing in raw format. */
            /* Function return value is don't care.              */
//            for (i = 0; i < buflen; i++) 
//            {
//                putchar (buf[i]);
//            }
            break;
    }
    
    return ((U16)len);
}

/* FTP Client Callback Events */
//	FTPC_EVT_SUCCESS   0      /* File operation successful               */
//	FTPC_EVT_TIMEOUT   1      /* Timeout on file operation               */
//	FTPC_EVT_LOGINFAIL 2      /* Login error, username/passw invalid     */
//	FTPC_EVT_NOACCESS  3      /* File access not allowed                 */
//	FTPC_EVT_NOTFOUND  4      /* File not found                          */
//	FTPC_EVT_NOPATH    5      /* Working directory path not found        */
//	FTPC_EVT_ERRLOCAL  6      /* Local file open error                   */
//	FTPC_EVT_ERROR     7      /* Generic FTP client error                */

void ftpc_notify (U8 event) 
{
    DebugPrint("\rFTPC notify : %u",event);
    
    switch (event) 
    {
        case FTPC_EVT_SUCCESS:  
            /* Command successful */
			DebugPrint("\rUpload file %s thanh cong",xSystem.FileTransfer.FTP_FileName);
			xSystem.FileTransfer.State = FT_TRANSFER_DONE;                
            break;

        case FTPC_EVT_TIMEOUT:
            /* Failed, timeout expired */
            xSystem.FileTransfer.State = FT_WAIT_TRANSFER_STATE;
            break;

        case FTPC_EVT_LOGINFAIL:
            /* Failed, username/password invalid */
            break;

        case FTPC_EVT_NOACCESS:
            /* Failed, operation not allowed */
            break;

        case FTPC_EVT_NOTFOUND:
            /* Failed, file or path not found */
            break;

        case FTPC_EVT_NOPATH:
            /* Failed, working directory not found */
            break;

        case FTPC_EVT_ERRLOCAL:
            /* Failed, local file open/write error */

            break;

        case FTPC_EVT_ERROR:
            /* Failed, unspecified protocol error */
			xSystem.FileTransfer.State = FT_WAIT_TRANSFER_STATE;
            break;
    }
}


/*----------------------------------------------------------------------------
 * end of file
 *---------------------------------------------------------------------------*/

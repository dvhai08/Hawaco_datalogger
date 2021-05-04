///*----------------------------------------------------------------------------
// *      RL-ARM - TCPnet
// *----------------------------------------------------------------------------
// *      Name:    FTPC_UIF.C
// *      Purpose: FTP Client User Interface Module
// *      Rev.:    V4.54
// *----------------------------------------------------------------------------
// *      This code is part of the RealView Run-Time Library.
// *      Copyright (c) 2004-2012 KEIL - An ARM Company. All rights reserved.
// *---------------------------------------------------------------------------*/

//#include <Net_Config.h>
//#include "DataDefine.h"
//#include "FTP.h"

//#define FTPC_USERNAME   "khanhpd"
//#define FTPC_PASSWORD   "123"
//#define FTPC_PATH       ""
//#define FTPC_FILENAME   "Uploaded.bin"
////#define FTPC_FILENAME   "BA2.bin"
//#define FTPC_NEWNAME    "xxx.bin"
//#define FTPC_DIRNAME    ""
//#define FTPC_LISTNAME   ""

//#define LOCAL_FILE      "141203.bin"
////#define LOCAL_FILE      "Download.bin"



///*----------------------------------------------------------------------------
// *      FTP Client File Access and Data CallBack Functions
// *---------------------------------------------------------------------------*/

///*--------------------------- ftpc_fopen ------------------------------------*/

//void *ftpc_fopen (U8 *mode) 
//{
//	FILE *f;
//	
//    /* Open local file for reading or writing. If the return value is NULL, */
//    /* processing of FTP Client commands PUT, APPEND or GET is cancelled.   */
//	DebugPrint("\rFTPC open file, mode : %s",mode);
//	
////    f = fopen (LOCAL_FILE, (char *)mode);
////	
////	if(f == NULL)
////	{
////		DebugPrint("\rFTPC : Open file fail");
////	}
////    else
////    {
////        DebugPrint("\rFTPC : Open file OK");
////    }
//    
//    return f;
//}


///*--------------------------- ftpc_fclose -----------------------------------*/

//void ftpc_fclose (void *file) 
//{
//    /* Close a local file. */
//	DebugPrint("\rFTPC close file");
//	
////    fclose (file);
//}


///*--------------------------- ftpc_fread ------------------------------------*/

//U16 ftpc_fread (void *file, U8 *buf, U16 len) 
//{
//    /* Read 'len' bytes from file to buffer 'buf'. Return number of bytes   */
//    /* copied. The file will be closed, when the return value is 0.         */
//    /* For optimal performance the return value should be 'len'             */
//    DebugPrint("\rFTPC read : %u bytes",len);
//	
////    return (fread (buf, 1, len, file));
//}


///*--------------------------- ftpc_fwrite -----------------------------------*/

//U16 ftpc_fwrite (void *file, U8 *buf, U16 len) 
//{
//	U16 WriteLen;
//	
//    /* Write 'len' bytes from buffer 'buf' to a file. */    

////    WriteLen = fwrite (buf, 1, len, file);
//	
//	DebugPrint("\rFTPC write : %u/%u bytes",WriteLen,len);
//    
//    return WriteLen;
//}


///*--------------------------- ftpc_cbfunc -----------------------------------*/
// /* This function is called by the FTP client to get parameters. It returns*/
//  /* the number of bytes written to buffer 'buf'. This function should NEVER*/
//  /* write more than 'buflen' bytes to this buffer.                         */
//  /* Parameters:                                                            */
//  /*   code   - function code with following values:                        */
//  /*             0 = Username for FTP authentication                        */
//  /*             1 = Password for FTP authentication                        */
//  /*             2 = Working directory path for all commands                */
//  /*             3 = Filename for PUT, GET, APPEND, DELETE, RENAME command  */
//  /*             4 - New filename for RENAME command                        */
//  /*             5 - Directory name for MKDIR, RMDIR command                */
//  /*             6 - File filter/mask for LIST command (wildcards allowed)  */
//  /*             7 = Received directory listing on LIST command             */
//  /*   buf    - transmit/receive buffer                                     */
//  /*   buflen - length of this buffer                                       */
//  /*             on transmit, it specifies size of 'buf'                    */
//  /*             on receive, specifies the number of bytes received         */

//U16 ftpc_cbfunc (U8 code, U8 *buf, U16 buflen) 
//{   
//    U32 i,len = 0;

//    switch (code) 
//    {
//        case 0:
//            /* Enter Username for login. */            
//            len = str_copy (buf, FTPC_USERNAME);
//            break;

//        case 1:
//            /* Enter Password for login. */
//            len = str_copy (buf, FTPC_PASSWORD);
//            break;

//        case 2:
//            /* Enter a working directory path. */
//            len = str_copy (buf, FTPC_PATH);
//            break;

//        case 3:
//            /* Enter a filename for file operations. */
//            len = str_copy (buf, FTPC_FILENAME);
//            break;

//        case 4:
//            /* Enter a new name for rename command. */            
//            len = str_copy (buf, FTPC_NEWNAME);
//            break;

//        case 5:
//            /* Enter a directory name to create or remove. */
//            len = str_copy (buf, FTPC_DIRNAME);
//            break;

//        case 6:
//            /* Enter a file filter for list command. */
//            len = str_copy (buf, FTPC_LISTNAME);
//            break;

//        case 7:
//            /* Process received directory listing in raw format. */
//            /* Function return value is don't care.              */
////            for (i = 0; i < buflen; i++) 
////            {
////                putchar (buf[i]);
////            }
//            break;
//    }
//    
//    return ((U16)len);
//}

//void ftpc_notify (U8 event) 
//{
//    DebugPrint("\rFTPC notify : %u",event);
//    
//    switch (event) 
//    {
//        case FTPC_EVT_SUCCESS:  
//            /* Command successful */
//            break;

//        case FTPC_EVT_TIMEOUT:
//            /* Failed, timeout expired */
//            
//            break;

//        case FTPC_EVT_LOGINFAIL:
//            /* Failed, username/password invalid */
//            break;

//        case FTPC_EVT_NOACCESS:
//            /* Failed, operation not allowed */
//            break;

//        case FTPC_EVT_NOTFOUND:
//            /* Failed, file or path not found */
//            break;

//        case FTPC_EVT_NOPATH:
//            /* Failed, working directory not found */
//            break;

//        case FTPC_EVT_ERRLOCAL:
//            /* Failed, local file open/write error */

//            break;

//        case FTPC_EVT_ERROR:
//            /* Failed, unspecified protocol error */
//            break;
//    }
//}


///*----------------------------------------------------------------------------
// * end of file
// *---------------------------------------------------------------------------*/

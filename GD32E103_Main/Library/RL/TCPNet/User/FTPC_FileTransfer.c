///******************************************************************************
// * @file    	FTPC_FileTransfer.c
// * @author  	Khanhpd
// * @version 	V1.0.0
// * @date    	15/01/2014
// * @brief   	
// ******************************************************************************/


///******************************************************************************
//                                   INCLUDES					    			 
// ******************************************************************************/
//#include "DataDefine.h"
//#include "Utilities.h"
//#include "GSM.h"
//#include "TCP.h"

///******************************************************************************
//                                   GLOBAL VARIABLES					    			 
// ******************************************************************************/
//extern System_t xSystem;

///******************************************************************************
//                                   GLOBAL FUNCTIONS					    			 
// ******************************************************************************/


///******************************************************************************
//                                   DATA TYPE DEFINE					    			 
// ******************************************************************************/
//#define FILE_TRANSFER_CMD_REQUEST_TO_UPLOAD 0
//#define FILE_TRANSFER_CMD_CONFIRM_ALLOW_UPLOAD 1
//#define FILE_TRANSFER_CMD_CONFIRM_UPLOAD_DONE 2
//#define FILE_TRANSFER_CMD_REQUEST_UPDATE_MAIN_FIRMWARE 3
//#define FILE_TRANSFER_CMD_REQUEST_UPDATE_BOOTLOADER 4

///******************************************************************************
//                                   PRIVATE VARIABLES					    			 
// ******************************************************************************/
//static UploadImage_t UploadBlockInfo, tmpUploadBlockInfo;
//static char tmpBuffer[100];
//static uint8_t UploadTimeout = 0;

///******************************************************************************
//                                   LOCAL FUNCTIONS					    			 
// ******************************************************************************/
//void DisplayUploadBlockInfo(void);
//static void ProcessUploadBlock(void);
//static void ProcessConfirmUploadImage(uint8_t *Buffer);
//static void ProcessUpdateFirmware(uint8_t *Buffer);
//static void ProcessDownloadTick(void);
//static void SendUploadMessage(uint8_t MessageType);

///*****************************************************************************/
///**
// * @brief	:  
// * @param	:  
// * @retval	:
// * @author	:	Khanhpd
// * @created	:	15/01/2014
// * @version	:
// * @reviewer:	
// */
//static void UpLoadFileTick(void) 
//{ 
//    if(xSystem.GLStatus.InitSystemDone == 0) return;
//    
//    ProcessUploadBlock();
//    ProcessDownloadTick();
//    
//    if(UploadTimeout > 0)
//    {
//        UploadTimeout--;
//        if(UploadTimeout == 0)
//        {
//            DebugPrint("\rUL : Qua thoi gian upload ma khong nhan duoc response, thuc hien upload lai");
//			            
//			xSystem.FileTransfer.State = FT_NO_TRANSER;
//			xSystem.FileTransfer.FTPState = FTP_STATE_CLOSED;
//			
//            if(xSystem.Parameters.GSMModule == M95)
//                UploadTimeout = 180;    
//            else
//                UploadTimeout = 30;    
//        }
//    }
//}

///*****************************************************************************/
///**
// * @brief	:  
// * @param	:  
// * @retval	:
// * @author	:	Khanhpd
// * @created	:	15/01/2014
// * @version	:
// * @reviewer:	
// */
//static void UpLoadFileInit(void) 
//{ 
//    UploadTimeout = 0;
////    DisplayUploadBlockInfo();
//    
//    xSystem.FileTransfer.FTP_Address[0] = 0;
//    xSystem.FileTransfer.FTP_Path[0] = 0;
//    xSystem.FileTransfer.FTP_UserName[0] = 0;
//    xSystem.FileTransfer.FTP_Pass[0] = 0;
//    xSystem.FileTransfer.FTP_FileName[0] = 0;
//    
//    xSystem.FileTransfer.RequestOpenFTP = 1;
//    xSystem.FileTransfer.FTPState = FTP_STATE_CLOSED;
//    
//    xSystem.FileTransfer.UDW_State = FT_NO_TRANSER;
//}

///*****************************************************************************/

///**
// * @brief:  
// * @param:  
// * @retval:
// * @author	:	Khanhpd
// * @created	:	08/04/2014
// */
//static void ProcessUploadBlock(void)
//{
//    uint8_t i = 0;
//    
//    if(xSystem.ServerState != LOGINED) return;
//    
//    
//}
///*****************************************************************************/
///**
// * @brief	:  
// * @param	:  
// * @retval	:
// * @author	:	Khanhpd
// * @created	:	15/01/2014
// * @version	:
// * @reviewer:	
// */
//static void ProcessDownloadTick(void)
//{
//    if(xSystem.FileTransfer.State == FT_TRANSFER_DONE && xSystem.FileTransfer.Type == FTP_DOWNLOAD)
//    {
//        if(xSystem.FileTransfer.FileSize.value < 30000)
//        {
//            DebugPrint("\rDWL : Download xong bootloader, bat dau thuc hien update FW");
//            
//        }
//        
//        xSystem.FileTransfer.State = FT_NO_TRANSER;
//    }
//}
///*****************************************************************************/
///**
// * @brief	:   Thuc hien gui ban tin 
// * @param	:  
// * @retval	:
// * @author	:	Khanhpd
// * @created	:	15/01/2014
// * @version	:
// * @reviewer:	
// */
//static void SendUploadMessage(uint8_t MessageType)
//{
//    uint8_t BufferIndex = 0;
//    
//    if(MessageType == FILE_TRANSFER_CMD_REQUEST_TO_UPLOAD)
//    {
//        tmpBuffer[BufferIndex++] = FILE_TRANSFER_CMD_REQUEST_TO_UPLOAD;
//        
//        memcpy(&tmpBuffer[BufferIndex],xSystem.FileTransfer.FTP_FileName,30);
//        BufferIndex += 30;
//        
//        tmpBuffer[BufferIndex++] = xSystem.FileTransfer.FileSize.bytes[0];
//        tmpBuffer[BufferIndex++] = xSystem.FileTransfer.FileSize.bytes[1];
//        tmpBuffer[BufferIndex++] = xSystem.FileTransfer.FileSize.bytes[2];
//        tmpBuffer[BufferIndex++] = xSystem.FileTransfer.FileSize.bytes[3];
//        
//        tmpBuffer[BufferIndex++] = UploadBlockInfo.ThoiGian.Year;
//        tmpBuffer[BufferIndex++] = UploadBlockInfo.ThoiGian.Month;
//        tmpBuffer[BufferIndex++] = UploadBlockInfo.ThoiGian.Day;
//        tmpBuffer[BufferIndex++] = UploadBlockInfo.ThoiGian.Hour;
//        tmpBuffer[BufferIndex++] = UploadBlockInfo.ThoiGian.Minute;
//        tmpBuffer[BufferIndex++] = UploadBlockInfo.ThoiGian.Second;
//        
//        SendBufferToServer((uint8_t *)tmpBuffer,DATA_FILE_TRANSFER,BufferIndex);
//    }
//    
//    if(MessageType == FILE_TRANSFER_CMD_CONFIRM_UPLOAD_DONE)
//    {
//        tmpBuffer[BufferIndex++] = FILE_TRANSFER_CMD_CONFIRM_UPLOAD_DONE;
//        
//        memcpy(&tmpBuffer[BufferIndex],xSystem.FileTransfer.FTP_FileName,30);
//        BufferIndex += 30;
//        
//        tmpBuffer[BufferIndex++] = xSystem.FileTransfer.FileSize.bytes[0];
//        tmpBuffer[BufferIndex++] = xSystem.FileTransfer.FileSize.bytes[1];
//        tmpBuffer[BufferIndex++] = xSystem.FileTransfer.FileSize.bytes[2];
//        tmpBuffer[BufferIndex++] = xSystem.FileTransfer.FileSize.bytes[3];
//        
//        tmpBuffer[BufferIndex++] = UploadBlockInfo.ThoiGian.Year;
//        tmpBuffer[BufferIndex++] = UploadBlockInfo.ThoiGian.Month;
//        tmpBuffer[BufferIndex++] = UploadBlockInfo.ThoiGian.Day;
//        tmpBuffer[BufferIndex++] = UploadBlockInfo.ThoiGian.Hour;
//        tmpBuffer[BufferIndex++] = UploadBlockInfo.ThoiGian.Minute;
//        tmpBuffer[BufferIndex++] = UploadBlockInfo.ThoiGian.Second;
//        
//        tmpBuffer[BufferIndex++] = UploadBlockInfo.KinhDo.bytes[0];
//        tmpBuffer[BufferIndex++] = UploadBlockInfo.KinhDo.bytes[1];
//        tmpBuffer[BufferIndex++] = UploadBlockInfo.KinhDo.bytes[2];
//        tmpBuffer[BufferIndex++] = UploadBlockInfo.KinhDo.bytes[3];
//        
//        tmpBuffer[BufferIndex++] = UploadBlockInfo.ViDo.bytes[0];
//        tmpBuffer[BufferIndex++] = UploadBlockInfo.ViDo.bytes[1];
//        tmpBuffer[BufferIndex++] = UploadBlockInfo.ViDo.bytes[2];
//        tmpBuffer[BufferIndex++] = UploadBlockInfo.ViDo.bytes[3];
//        
//        tmpBuffer[BufferIndex++] = UploadBlockInfo.ChannelID;
//        
//        SendBufferToServer((uint8_t *)tmpBuffer,DATA_FILE_TRANSFER,BufferIndex);
//    }
//}

///*****************************************************************************/
///**
// * @brief	:  
// * @param	:  
// * @retval	:
// * @author	:	Khanhpd
// * @created	:	15/01/2014
// * @version	:
// * @reviewer:	
// */
//void DisplayUploadBlockInfo(void)
//{
//    uint8_t i = 0;

////    for(i = 0; i < MAXOFCAMERABUFFER; i++)
////    {
////        if(GetPictureInfo(i, &tmpUploadBlockInfo) != 0) continue;
////
////        DebugPrint("%s","\r------------------------------------------------------------------------------");
////        DebugPrint("\rUL: Block %d co du lieu. Channel : %u, MemID : %u", i + 1, tmpUploadBlockInfo.ChannelID, tmpUploadBlockInfo.MemID);
////        DebugPrint("\rUL: Dia chi: %u", tmpUploadBlockInfo.BeginAddress.value);
////        DebugPrint("\rUL: Do dai du lieu: %u", tmpUploadBlockInfo.DataLength.value);
////        DebugPrint("\rThoi gian : %u:%u:%u-%u/%u/%u",
////            tmpUploadBlockInfo.ThoiGian.Hour, tmpUploadBlockInfo.ThoiGian.Minute, tmpUploadBlockInfo.ThoiGian.Second,
////            tmpUploadBlockInfo.ThoiGian.Day, tmpUploadBlockInfo.ThoiGian.Month, tmpUploadBlockInfo.ThoiGian.Year);        
////    }
//}

///*****************************************************************************/
///**
// * @brief	:  
// * @param	:  
// * @retval	:
// * @author	:	Khanhpd
// * @created	:	15/01/2014
// * @version	:
// * @reviewer:	
// */
//void ProcessDataTransferCommand(uint8_t *CmdBuffer)
//{
//    /* Neu dang trong qua trinh upload thi cancel */
//    if(xSystem.FileTransfer.State == FT_WAIT_TRANSFER_STATE || xSystem.FileTransfer.State == FT_TRANSFERRING) 
//    {
//        if(xSystem.FileTransfer.Type == FTP_DOWNLOAD)
//        {
//            DebugPrint("\rWARNING : Dang trong qua trinh update FW, ko thuc hien lenh FTP tu server : %u",xSystem.FileTransfer.State);
//            return;
//        }
//    }
//    
//    switch(CmdBuffer[22])
//    {
//        case FILE_TRANSFER_CMD_CONFIRM_ALLOW_UPLOAD:            
//            ProcessConfirmUploadImage(&CmdBuffer[23]);
//            break;
//        case FILE_TRANSFER_CMD_REQUEST_UPDATE_MAIN_FIRMWARE:
//        case FILE_TRANSFER_CMD_REQUEST_UPDATE_BOOTLOADER:
//            ProcessUpdateFirmware(&CmdBuffer[23]);
//            break;        
//    }
//}

///*****************************************************************************/
///**
// * @brief	:  
// * @param	:  
// * @retval	:
// * @author	:	Khanhpd
// * @created	:	15/01/2014
// * @version	:
// * @reviewer:	
// */
//static void ProcessConfirmUploadImage(uint8_t *Buffer)
//{
//    uint8_t i;  
//    
//    /* Kiem tra ten file */
//    for(i = 0; i < 30; i++)
//        if(Buffer[i] != xSystem.FileTransfer.FTP_FileName[i]) return;
//    
//    /* Kiem tra kich thuoc */
//    for(i = 0; i < 4; i++)
//        if(Buffer[30+i] != xSystem.FileTransfer.FileSize.bytes[i]) return;    
//    
//    /* Copy cac thong so */
//    memcpy(xSystem.FileTransfer.FTP_Path,&Buffer[40],30);
//    memcpy(xSystem.FileTransfer.FTP_Address,&Buffer[70],15);
//    memcpy(xSystem.FileTransfer.FTP_UserName,&Buffer[85],10);
//    memcpy(xSystem.FileTransfer.FTP_Pass,&Buffer[95],10);
//    
//    xSystem.FileTransfer.Type = FTP_UPLOAD;
//    xSystem.FileTransfer.State = FT_WAIT_TRANSFER_STATE;
//}

///*****************************************************************************/
///**
// * @brief	:  
// * @param	:  
// * @retval	:
// * @author	:	Khanhpd
// * @created	:	15/01/2014
// * @version	:
// * @reviewer:	
// */
//static void ProcessUpdateFirmware(uint8_t *Buffer)
//{
//    if(xSystem.FileTransfer.UDW_State == FT_TRANSFERRING)
//    {
//        DebugPrint("\rNhan duoc yeu cau update FW : Dang trong qua trinh update, khong thuc hien update");
//        return;
//    }
//    
//    xSystem.FileTransfer.UDW_FileSize.bytes[0] = Buffer[30];
//    xSystem.FileTransfer.UDW_FileSize.bytes[1] = Buffer[31];
//    xSystem.FileTransfer.UDW_FileSize.bytes[2] = Buffer[32];
//    xSystem.FileTransfer.UDW_FileSize.bytes[3] = Buffer[33];    
//    
//    xSystem.FileTransfer.UDW_FileCRC.bytes[0] = Buffer[34];
//    xSystem.FileTransfer.UDW_FileCRC.bytes[1] = Buffer[35];
//    xSystem.FileTransfer.UDW_FileCRC.bytes[2] = Buffer[36];
//    xSystem.FileTransfer.UDW_FileCRC.bytes[3] = Buffer[37];        
//    
//    /* Copy cac thong so */
//    memcpy(xSystem.FileTransfer.UDW_FTP_FileName,&Buffer[0],30);
//    memcpy(xSystem.FileTransfer.UDW_FTP_Path,&Buffer[38],30);
//    memcpy(xSystem.FileTransfer.UDW_FTP_Address,&Buffer[68],15);
//    memcpy(xSystem.FileTransfer.UDW_FTP_UserName,&Buffer[83],10);
//    memcpy(xSystem.FileTransfer.UDW_FTP_Pass,&Buffer[93],10);
//    
//    DebugPrint("\rNhan duoc yeu cau update firmware qua FTP : %s, %u, user : %s, pass : %s, SRV IP : %s",
//        xSystem.FileTransfer.UDW_FTP_FileName,xSystem.FileTransfer.UDW_FileSize,xSystem.FileTransfer.UDW_FTP_UserName,
//        xSystem.FileTransfer.UDW_FTP_Pass,xSystem.FileTransfer.UDW_FTP_Address);
//    
//    xSystem.FileTransfer.UDW_State = FT_WAIT_TRANSFER_STATE;
//    xSystem.FileTransfer.FTPState = FTP_STATE_CLOSED;
//}

//UPLOAD_FILE_t Driver_UploadFile = {
//    UpLoadFileInit,
//    UpLoadFileTick
//};
//    
///********************************* END OF FILE *******************************/

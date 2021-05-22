/******************************************************************************
 * @file    	UpdateFirmwareFTP.c
 * @author  	Phinht
 * @version 	V1.0.0
 * @date    	15/01/2016
 * @brief   	
 ******************************************************************************/


/******************************************************************************
                                   INCLUDES					    			 
******************************************************************************/
#include "stm32l1xx_flash.h"
#include "UpdateFirmwareFTP.h"
#include "MQTTUser.h"
#include "Version.h"
#include "Configurations.h"

/******************************************************************************
                                   GLOBAL VARIABLES					    			 
******************************************************************************/
extern System_t xSystem;
extern GSM_Manager_t	GSM_Manager;

/******************************************************************************
                                   GLOBAL FUNCTIONS					    			 
 ******************************************************************************/
extern void ftpc_notify (U8 event) ;

/******************************************************************************
                                   DATA TYPE DEFINE					    			 
 ******************************************************************************/

/******************************************************************************
                                   PRIVATE VARIABLES					    			 
 ******************************************************************************/
static uint8_t FTPConnectRetry = 0;
/******************************************************************************
                                   LOCAL FUNCTIONS					    			 
 ******************************************************************************/
static void ProcessDownloadTick(void);
static void QueryFileTransferState(void);
static uint8_t UpdateBootloader(void);

/*****************************************************************************/
/**
 * @brief	:  Call every 1s
 * @param	:  
 * @retval	:
 * @author	:	Phinht
 * @created	:	15/05/2016
 * @version	:
 * @reviewer:	
 */
void DownloadFileTick(void) 
{     
	if(ppp_is_up())
	{
		ProcessDownloadTick();	
		QueryFileTransferState();
	}
}

static void QueryFileTransferState(void)
{	
	if(xSystem.FileTransfer.State == FT_WAIT_TRANSFER_STATE)
	{
		if(xSystem.FileTransfer.Retry == 0) 
		{
			xSystem.FileTransfer.State =  FT_NO_TRANSER;
			xSystem.FileTransfer.UDFWTarget = 0;
		}
		
		if(xSystem.FileTransfer.Type == FTP_DOWNLOAD && xSystem.FileTransfer.Retry)
		{
			if(ftpc_connect(xSystem.FileTransfer.FTP_IP.Sub, 21, FTPC_CMD_GET, ftpc_notify) == 0)
			{
				DEBUG_PRINTF("\rFTP: Khong ket noi duoc voi FTP server");
				FTPConnectRetry++;
				if(FTPConnectRetry >= 60)
				{
					DEBUG_PRINTF("\rFTP: HUY UPDATE!");
					xSystem.FileTransfer.State =  FT_NO_TRANSER;
					xSystem.FileTransfer.UDFWTarget = 0;
					xSystem.FileTransfer.Retry = 0;
					MQTT_SendBufferToServer("Connect FTP server FAILED!", "UDFW");
				}
			}
			else
			{
				DEBUG_PRINTF("\rFTP: Thuc hien download file %s", xSystem.FileTransfer.FTP_FileName);
				xSystem.FileTransfer.State = FT_TRANSFERRING;
				xSystem.FileTransfer.DataTransfered = 0;
				FTPConnectRetry = 0;
			}						
    }
	}
}

/*****************************************************************************/
/**
 * @brief	:  
 * @param	:  
 * @retval	:
 * @author	:	Phinht
 * @created	:	15/05/2016
 * @version	:
 * @reviewer:	
 */
void ProcessDownloadTick(void)
{
	if(xSystem.FileTransfer.State == FT_TRANSFER_DONE && xSystem.FileTransfer.Type == FTP_DOWNLOAD)
	{
		xSystem.FileTransfer.Retry = 0;
		if(xSystem.FileTransfer.UDFWTarget == UPDATE_BOOTLOADER)
		{			
      DEBUG_PRINTF("\rUDFW: Bat dau update Bootloader!");    
			
			/* Thuc hien update Bootloader */
			if(UpdateBootloader())
			{
				DEBUG_PRINTF("\rUpdate Bootloader [SUCCESS] Reset!");
				Delayms(100);
				SystemReset(8);
			}
			else
			{
				DEBUG_PRINTF("\rUDFW: Failed!");
				xSystem.FileTransfer.Type = FTP_UPLOAD;
				xSystem.FileTransfer.UDFWTarget = 0;
				
				/** Gui ban tin retport len server */
				MQTT_SendBufferToServer("Update bootloader FAILED!", "UDFW");
			}
    }
        
		if(xSystem.FileTransfer.UDFWTarget ==  UPDATE_MAINFW)
		{
      DEBUG_PRINTF("\rUDFW: Bat dau update Firmware!");   
			
			/* Ghi cac thong so vao EEPROM */
			EEPROM_WriteLong(UPDATEFW_SIZE, xSystem.FileTransfer.FileSize.value);
			EEPROM_WriteLong(UPDATEFW_CRC, xSystem.FileTransfer.FileCRC.value);
			EEPROM_WriteLong(UPDATEFW_NEWFWFLAG, NEW_FW_FLAG_IN_EPROM_VALUE);
			EEPROM_WriteByte(UPDATEFW_RETRY, 0);

			/* Reset */
			SystemReset(7);
		}
		
    xSystem.FileTransfer.State = FT_NO_TRANSER;
  }
}

/*****************************************************************************/
/**
 * @brief	:  
 * @param	:  
 * @retval	:
 * @author	:	Phinht
 * @created	:	15/01/2014
 * @version	:
 * @reviewer:	
 */
void DownloadFileInit(void) 
{     
  xSystem.FileTransfer.State = FT_NO_TRANSER;	
	xSystem.FileTransfer.Retry = 0;
}
  
/*****************************************************************************/
/**
 * @brief	:  
 * @param	: SMS: 	UDFW,<ftp IP>,<ftp username>,<ftp pass>,<ftp filename>,<file size>,<ftp file CRC>
 *				: SERVER: UDFW,<ftp IP>,<ftp username>,<ftp pass>,<ftp filename>,<file size>,<ftp file CRC>,<Version>
 * @retval	:
 * @author	:	
 * @created	:	05/02/2018
 * @version	:
 * @reviewer:	
 */
void ProcessUpdateFirmwareCommand(char *Buffer, uint8_t fromSource)
{
	char *pTemp = strstr(Buffer,"UDFW");
		
	if(pTemp == 0) return;
	
	/* Neu dang trong qua trinh upload thi cancel */
	if(xSystem.FileTransfer.State == FT_WAIT_TRANSFER_STATE || xSystem.FileTransfer.State == FT_TRANSFERRING) 
	{
		DEBUG_PRINTF("\rWARNING: Dang update FW: %u", xSystem.FileTransfer.State);
		return;
	}
	
	pTemp = strtok(pTemp,","); 	
	pTemp = strtok(NULL,","); // FTP IP	
	
	memset(xSystem.FileTransfer.FTP_IPAddress, 0 , sizeof(xSystem.FileTransfer.FTP_IPAddress));
	sprintf(xSystem.FileTransfer.FTP_IPAddress,"%s", pTemp);
	GetIPAddressFromString(xSystem.FileTransfer.FTP_IPAddress, &xSystem.FileTransfer.FTP_IP);
	
	pTemp = strtok(NULL,",");
	memset(xSystem.FileTransfer.FTP_UserName, 0 , sizeof(xSystem.FileTransfer.FTP_UserName));
	sprintf(xSystem.FileTransfer.FTP_UserName,"%s",pTemp);
	
	pTemp = strtok(NULL,",");
	memset(xSystem.FileTransfer.FTP_Pass, 0 , sizeof(xSystem.FileTransfer.FTP_Pass));
	sprintf(xSystem.FileTransfer.FTP_Pass,"%s",pTemp);
	
	pTemp = strtok(NULL,",");
	memset(xSystem.FileTransfer.FTP_FileName, 0 , sizeof(xSystem.FileTransfer.FTP_FileName));
	sprintf(xSystem.FileTransfer.FTP_FileName,"%s", pTemp);	
	
	pTemp = strtok(NULL,",");	
	xSystem.FileTransfer.FileSize.value = GetNumberFromString(0,pTemp);
	
	pTemp = strtok(NULL,",");	
	xSystem.FileTransfer.FileCRC.value = GetNumberFromString(0,pTemp);
	
	if(fromSource == CF_SERVER)
	{
		pTemp = strtok(NULL,",");
		uint16_t newVersion = GetNumberFromString(0,pTemp);
		
		//Neu dang cung version thi khong update nua
		if(newVersion == FIRMWARE_VERSION_CODE)
		{
			DEBUG_PRINTF("\rFW's same version, reject!");
			return;
		}
	}
	
	if(xSystem.FileTransfer.FileSize.value == 0 || xSystem.FileTransfer.FileCRC.value == 0)
	{
		return;
	}
		
	//Phan biet firmware
	if(xSystem.FileTransfer.FileSize.value <= 14000)
	{
		xSystem.FileTransfer.UDFWTarget = UPDATE_BOOTLOADER;
	}
	else
	{
		xSystem.FileTransfer.UDFWTarget = UPDATE_MAINFW;
	}
		
#if 0
	DEBUG_PRINTF("\rUDFW: %s, %u, user : %s, pass : %s, srv: %s, target: %u",
        xSystem.FileTransfer.FTP_FileName, xSystem.FileTransfer.FileSize.value,xSystem.FileTransfer.FTP_UserName,
        xSystem.FileTransfer.FTP_Pass,xSystem.FileTransfer.FTP_IPAddress, xSystem.FileTransfer.UDFWTarget);
#endif
	
	DEBUG_PRINTF("\rUDFW: %s, size: %u, crc: %u, target: %u",
        xSystem.FileTransfer.FTP_FileName, xSystem.FileTransfer.FileSize.value, 
			xSystem.FileTransfer.FileCRC.value, xSystem.FileTransfer.UDFWTarget);
			
	
	/* Xoa vung nho luu Firmware update tren External flash */
	Flash_EraseBlock64(FWUD_BLOCK1);
	Flash_EraseBlock64(FWUD_BLOCK2);
	DEBUG_PRINTF("\rUDFW: Xoa vung nho luu FW trong Flash: DONE");
	
	/* Dia chi luu du lieu FW update trong Flash */
	xSystem.FileTransfer.FileAddress = FWUD_BLOCK1 * 65536;	
	
	/* Cho phep mo FTP va Download FW */
	xSystem.FileTransfer.Retry = 3;
	xSystem.FileTransfer.Type = FTP_DOWNLOAD;
	xSystem.FileTransfer.State = FT_WAIT_TRANSFER_STATE;
}

#if 1	//__TEST_UDFW_FTP__
/**************************************************************************************
* Function Name  	: UDFW_UpdateMainFWTest
* Return         	: None
* Parameters 		: None
* Description		: 
**************************************************************************************/
void UDFW_UpdateMainFWTest(uint8_t source)
{
	if(source == 0)
	{
		DEBUG_PRINTF("\rUDFW: Download FW. Size: %d, CRC: %d", xSystem.FileTransfer.FileSize.value, xSystem.FileTransfer.FileCRC.value);
		sprintf(xSystem.FileTransfer.FTP_IPAddress,"210.245.85.158");
		xSystem.FileTransfer.FTP_IP.Sub[0] = 210;
		xSystem.FileTransfer.FTP_IP.Sub[1] = 245;
		xSystem.FileTransfer.FTP_IP.Sub[2] = 85;
		xSystem.FileTransfer.FTP_IP.Sub[3] = 158;
		sprintf(xSystem.FileTransfer.FTP_UserName,"khanhpd");
		sprintf(xSystem.FileTransfer.FTP_Pass,"123");
	}
	sprintf(xSystem.FileTransfer.FTP_Path,"");
	
	if(xSystem.FileTransfer.FileSize.value == 0 || xSystem.FileTransfer.FileCRC.value == 0)
	{
		return;
	}
		
	if(xSystem.FileTransfer.FileSize.value <= 14000)
	{
		sprintf(xSystem.FileTransfer.FTP_FileName,"LOB_BLDER.bin");
		xSystem.FileTransfer.UDFWTarget = UPDATE_BOOTLOADER;
	}
	else 
	{
		sprintf(xSystem.FileTransfer.FTP_FileName,"LORTest.bin");
		xSystem.FileTransfer.UDFWTarget = UPDATE_MAINFW;
	}
	
	/* Xoa vung nho luu Firmware update tren External flash */
	Flash_EraseBlock64(FWUD_BLOCK1);
	Flash_EraseBlock64(FWUD_BLOCK2);
	DEBUG_PRINTF("\rUDFW: Xoa vung nho luu FW trong Flash: DONE"); 
	xSystem.FileTransfer.FileAddress = FWUD_BLOCK1 * 65536;	
	
	/* Cho phep mo FTP va Download FW */
	xSystem.FileTransfer.Retry = 3;
	xSystem.FileTransfer.Type = FTP_DOWNLOAD;
	xSystem.FileTransfer.State = FT_WAIT_TRANSFER_STATE;
}

/*****************************************************************************/
/**
 * @brief	:  
 * @param	:  
 * @retval	:
 * @author	:	Phinht
 * @created	:	15/01/2014
 * @version	:
 * @reviewer:	
 */
void ProcessUpdateFirmware(uint8_t *Buffer, uint8_t UDType)
{	
	uint8_t Index = 0;
	  
	xSystem.FileTransfer.Retry = 3;
	
	/* Lay thong tin file update qua FTP */
	//Ten File - 30
	_mem_set(xSystem.FileTransfer.FTP_FileName, 0 , 31);
	memcpy(xSystem.FileTransfer.FTP_FileName, Buffer, 30);
	Index += 30;
	
	//File size & CRC
    xSystem.FileTransfer.FileSize.bytes[0] = Buffer[Index++];
    xSystem.FileTransfer.FileSize.bytes[1] = Buffer[Index++];
    xSystem.FileTransfer.FileSize.bytes[2] = Buffer[Index++];
    xSystem.FileTransfer.FileSize.bytes[3] = Buffer[Index++];    
    
    xSystem.FileTransfer.FileCRC.bytes[0] = Buffer[Index++];
    xSystem.FileTransfer.FileCRC.bytes[1] = Buffer[Index++];
    xSystem.FileTransfer.FileCRC.bytes[2] = Buffer[Index++];
    xSystem.FileTransfer.FileCRC.bytes[3] = Buffer[Index++];   
	
	//File Path - 30
	_mem_set(xSystem.FileTransfer.FTP_Path, 0 , 31);
	memcpy(xSystem.FileTransfer.FTP_Path, &Buffer[Index], 30);
	Index += 30;
	
	//IP - 15
	_mem_set(xSystem.FileTransfer.FTP_IPAddress, 0 , 16);
    memcpy(xSystem.FileTransfer.FTP_IPAddress, &Buffer[Index], 15);
	GetIPAddressFromString(xSystem.FileTransfer.FTP_IPAddress, &xSystem.FileTransfer.FTP_IP);
	Index +=15;

	//User name & Password
	_mem_set(xSystem.FileTransfer.FTP_UserName, 0 , sizeof(xSystem.FileTransfer.FTP_UserName));
    memcpy(xSystem.FileTransfer.FTP_UserName, &Buffer[Index], 10);
	Index += 10;
	
	_mem_set(xSystem.FileTransfer.FTP_Pass, 0 , sizeof(xSystem.FileTransfer.FTP_Pass));
    memcpy(xSystem.FileTransfer.FTP_Pass, &Buffer[Index], 10);
    
    DEBUG_PRINTF("\rFile: %s, Size: %d, CRC: %d", xSystem.FileTransfer.FTP_FileName, 
						xSystem.FileTransfer.FileSize.value,  xSystem.FileTransfer.FileCRC.value);
	DEBUG_PRINTF("\rPath: %s, IP: %s, User: %s, Pass: %s", xSystem.FileTransfer.FTP_Path, xSystem.FileTransfer.FTP_IPAddress,
						xSystem.FileTransfer.FTP_UserName, xSystem.FileTransfer.FTP_Pass);
	
	/* Xoa vung nho Flash ngoai luu FW */
	Flash_EraseBlock64(FWUD_BLOCK1);
	Flash_EraseBlock64(FWUD_BLOCK2);
	xSystem.FileTransfer.FileAddress = FWUD_BLOCK1 * 65536;		
		
	DEBUG_PRINTF("\rUDFW: Xoa vung nho luu FW trong Flash: DONE"); 
				
	/* Cho phep mo FTP va Download FW */
	xSystem.FileTransfer.Type = FTP_DOWNLOAD;
	xSystem.FileTransfer.State = FT_WAIT_TRANSFER_STATE;
}
#endif	//__TEST_UDFW_FTP_

static uint8_t CheckCRCExternalFlash(uint32_t FWLeng, uint32_t FWCRC)
{
	uint32_t SumValue = 0;
	uint32_t i;
	uint8_t readValue = 0;
	
	//Dia chi Base Flash luu FW
	uint32_t DiaChi = FWUD_BLOCK1 * 65536;	
	
	for(i = DiaChi; i < DiaChi + FWLeng; i++)
	{
		Flash_ReadBytes(i, &readValue, 1);
		SumValue += readValue;
		
		if(((i - DiaChi) % 200) == 0)
		{
			ResetWatchdog();
		}
	}
	
	if(SumValue != FWCRC)
	{
		DEBUG_PRINTF("\rChecksum fail: %d - %d", SumValue, FWCRC);
		return 0;
	}
	else
		return 1;
}

uint8_t ReadBuffer[5] = {0};
void CopyDataFromFlashToMCU(uint32_t Leng)
{
	Long_t tmpLong;
	uint32_t i;
	
	//Dia chi Base Flash luu FW
	uint32_t DiaChi = FWUD_BLOCK1*65536;	
	
	//Unlock Flash
	InternalFlash_Init();
    DEBUG_PRINTF("\rCopy from 0x%X to 0x%X, amount: %d Bytes", DiaChi, BOOTLOADER_ADDR, Leng);
	
	ControlAllUART(DISABLE);
	for(i = 0; i < Leng; i += 4)
	{		
		Flash_ReadBytes(DiaChi + i, ReadBuffer, 4);
		tmpLong.bytes[0] = ReadBuffer[0];
		tmpLong.bytes[1] = ReadBuffer[1];
		tmpLong.bytes[2] = ReadBuffer[2];
		tmpLong.bytes[3] = ReadBuffer[3];
		
		//BOOTLOADER_ADDR
		FLASH_FastProgramWord(BOOTLOADER_ADDR + i, tmpLong.value);
		_mem_set(ReadBuffer, 0, 4);
		
		//Reset IWDG
		if(i > 0 && (i%200 == 0))
		{
			ResetWatchdog();		
		}	
	}
	ControlAllUART(ENABLE);
	FLASH_Lock();   
}

/*****************************************************************************/
/**
 * @brief	:   Update bootloader
 * @param	:   
 * @retval	:
 * @author	:	Phinht
 * @created	:	15/01/2016
 * @version	:
 * @reviewer:	
 */
static uint8_t UpdateBootloader(void)
{
    uint32_t FWSize = xSystem.FileTransfer.FileSize.value;
	uint32_t FWCRC = xSystem.FileTransfer.FileCRC.value;
	    
	/* Kiem tra CRC */
	if(CheckCRCExternalFlash(FWSize, FWCRC) == 0)
	{
		return 0;
	}
		
	/* Xoa vung nho BootLoader: Page 0 - 47 */
	DEBUG_PRINTF("\rXoa vung nho BLDER...");
	Delayms(100);
	InternalFlash_Prepare(0, MAIN_FW_START_PAGE);

	/* Copy du lieu */
	CopyDataFromFlashToMCU(FWSize);
	
	return 1;
}


/********************************* END OF FILE *******************************/

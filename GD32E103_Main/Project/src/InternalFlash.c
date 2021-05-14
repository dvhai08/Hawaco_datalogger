/******************************************************************************
 * @file    	InternalFlash.c
 * @author  	
 * @version 	V1.0.0
 * @date    	15/01/2014
 * @brief   	
 ******************************************************************************/

/******************************************************************************
                                   INCLUDES					    			 
 ******************************************************************************/
#include <stdio.h>
#include <string.h>
#include "InternalFlash.h"
#include "HardwareManager.h"
#include "DataDefine.h"
#include "Main.h"
#include "DriverUART.h"

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
#define		FLASH_ProgramWord		fmc_word_program
#define		FLASH_ProgramHalfWord	fmc_halfword_program
#define		FLASH_Lock	fmc_lock
#define		FLASH_UnLock	fmc_unlock
#define		FLASH_ErasePage	fmc_page_erase
#define		FLASH_Status		fmc_state_enum
#define		FLASH_COMPLETE		FMC_READY
/******************************************************************************
                                   PRIVATE VARIABLES					    			 
 ******************************************************************************/

/******************************************************************************
                                   LOCAL FUNCTIONS					    			 
 ******************************************************************************/

/*****************************************************************************/
/**
 * @brief	:  
 * @param	:  
 * @retval	:
 * @author	:	
 * @created	:	15/01/2014
 * @version	:
 * @reviewer:	
 */
void InternalFlash_Init(void) 
{
#if 0
    FLASH_Unlock();

/* Clear all pending flags */      
	FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPRTERR |
									FLASH_FLAG_BSY);
#else
	/* unlock the flash program/erase controller */
    fmc_unlock();

    /* clear all pending flags */
    fmc_flag_clear(FMC_FLAG_END | FMC_FLAG_WPERR | FMC_FLAG_PGAERR | FMC_FLAG_PGERR);
#endif
}

/*****************************************************************************/
/**
 * @brief	:   Erase sector 6 & sector 7
 * @param	:   None 
 * @retval	:   0 if success, > 0 if error
 * @author	:	
 * @created	:	15/01/2014
 * @version	:
 * @reviewer:	
 */
/*****************************************************************************/
/**
 * @brief	: Erase sector de luu FW update
 * @param	:  
 * @retval	:	0 if Success, > 0 if Fail
 * @author	:	Phinht
 * @created	:	15/09/2015
 * @version	:
 * @reviewer:	
 */
uint8_t InternalFlash_Prepare(uint16_t StartPage, uint16_t EndPage)
{	
	uint16_t PageNum, i;
	uint8_t status = 0;
	
	//Unlock Flash
	InternalFlash_Init();
	
	for(PageNum = StartPage; PageNum <= EndPage; PageNum++)
	{
		ResetWatchdog();
		ControlAllUART(DISABLE);
		
		for(i = 0; i < 3; i++)
		{
#if 0
			if(FLASH_ErasePage(FLASH_BASE + PageNum * FLASH_PAGE_SIZE) == FLASH_COMPLETE) {
				status = 0;
				break;
			} else {
				status++;
				FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPRTERR | FLASH_FLAG_BSY);
				Delayms(100);
			}
#else
			if(fmc_page_erase(FLASH_BASE + PageNum * FLASH_PAGE_SIZE) == FMC_READY) {
				status = 0;
				break;
			} else {
				status++;
				fmc_flag_clear(FMC_FLAG_END | FMC_FLAG_WPERR | FMC_FLAG_PGAERR | FMC_FLAG_PGERR);
				Delayms(100);
			}
#endif
		}
		
		ControlAllUART(ENABLE);
	}
	FLASH_Lock();
	
	return status;
}

/*****************************************************************************/
/**
 * @brief	: Write bytes to internal flash
 * @param	:  Address, pBuffer, Length
 * @retval	:	0 if Success, > 0 if Fail
 * @author	:	Phinht
 * @created	:	15/09/2015
 * @version	:
 * @reviewer:	
 */
static uint8_t SoByteCu = 0;
static uint8_t MangByteCu[10] = {0};
void InternalFlashWriteBytes(uint8_t *Buffer, uint16_t Length)
{
	Long_t tmpWord;
	uint16_t SoWord = 0;
	uint8_t SoByteLe = 0;
	uint16_t ViTri, i;
	
	if(SoByteCu)	//Con byte cu can ghi
	{
		//Khong du 1 word
		if((SoByteCu + Length) < 4)	
		{
			for(i = 0; i < Length; i++) {
				MangByteCu[SoByteCu + i] = Buffer[i];		
			}
			SoByteCu += Length;
			return;
		}
		
		//Vua du 1 word
		if((SoByteCu + Length) == 4)	
		{
			for(i = 0; i < Length; i++) {
				MangByteCu[SoByteCu + i] = Buffer[i];		
			}
			//Ghi 1 word
			for(i = 0; i < 4; i++) {
				tmpWord.bytes[i] = MangByteCu[i];
			}
			FLASH_ProgramWord(xSystem.FileTransfer.FileAddress, tmpWord.value);
			xSystem.FileTransfer.FileAddress += 4;
			//Reset mang
			SoByteCu = 0;
			memset(MangByteCu, 0, 5);
			return;
		}
		
		// Nhieu hon 1 word
		if((SoByteCu + Length) > 4)
		{
			//Copy them so byte (1, 2, hoac 3) cho du 1 Word
			for(i = 0; i < 4 - SoByteCu; i++) {
				MangByteCu[SoByteCu + i]  = Buffer[i];
			}
			ViTri = 4 - SoByteCu;	//Vi tri bat dau du lieu moi cua mang

			//Ghi Word cu
			for(i = 0; i < 4; i++) {
				tmpWord.bytes[i] = MangByteCu[i];
			}
			FLASH_ProgramWord(xSystem.FileTransfer.FileAddress, tmpWord.value);
			xSystem.FileTransfer.FileAddress += 4;
			//Reset mang
			SoByteCu = 0;
			memset(MangByteCu, 0, 5);
			
			//Xet so luong byte con lai cua mang		
			SoWord = (Length - ViTri) / 4;
			SoByteLe = (Length - ViTri) % 4;
			if(SoByteLe)	//Neu co so byte le thi copy lai de ghi lan sau
			{
				for(i = 0; i < SoByteLe; i++)
				{
					MangByteCu[i] = Buffer[SoWord*4 + i + ViTri];
				}
				SoByteCu = SoByteLe;
			}
			//Ghi so Word vao Flash neu > 0
			if(SoWord > 0)
			{
				for(i = 0; i < SoWord; i++)
				{
					tmpWord.bytes[0] = Buffer[i*4 + ViTri];
					tmpWord.bytes[1] = Buffer[i*4 + 1 + ViTri];
					tmpWord.bytes[2] = Buffer[i*4 + 2 + ViTri];
					tmpWord.bytes[3] = Buffer[i*4 + 3 + ViTri];
				
					FLASH_ProgramWord(xSystem.FileTransfer.FileAddress, tmpWord.value);
					xSystem.FileTransfer.FileAddress += 4;
				}
			}
		}
	}
	else	//Lan dau ghi hoac ko con so byte cu
	{
		SoWord = Length / 4;
		SoByteLe = Length % 4;
		
		if(SoByteLe)	//Neu co so byte le thi copy lai de ghi lan sau
		{
			for(i = 0; i < SoByteLe; i++)
			{
				MangByteCu[i] = Buffer[SoWord*4 + i];
			}
			SoByteCu = SoByteLe;
		}
		//Ghi so Word vao Flash neu > 0
		if(SoWord > 0)
		{
			for(i = 0; i < SoWord; i++)
			{
				tmpWord.bytes[0] = Buffer[i*4];
				tmpWord.bytes[1] = Buffer[i*4 + 1];
				tmpWord.bytes[2] = Buffer[i*4 + 2];
				tmpWord.bytes[3] = Buffer[i*4 + 3];
				FLASH_ProgramWord(xSystem.FileTransfer.FileAddress, tmpWord.value);
				xSystem.FileTransfer.FileAddress += 4;
			}
		}
	}
}

void CheckEOF(void)
{
	if(SoByteCu > 0)	//Con byte cu dang doi ghi -> Ghi not
	{
		InternalFlashWriteBytes(MangByteCu, 4 - SoByteCu);
	}
}

uint8_t InternalFlash_WriteBytes(uint32_t Address, uint16_t *Buffer, uint16_t Length)
{
    FLASH_Status Status = FLASH_COMPLETE;
	uint32_t WriteAddress = Address;
	uint16_t i;
//	uint16_t tmpInt = 0;
	
//	//Unlock Flash
//	InternalFlash_Init();
	
	for(i = 0; i < Length; i++)
	{
//		tmpInt = Buffer[i] & 0xFFFF;
		Status = FLASH_ProgramHalfWord(WriteAddress, Buffer[i]);
		WriteAddress += 2;
	}		
	
	//check again
//	DEBUG ("\r\nRead again: ");
//	for(i = 0; i < Length; i++)
//	{
//		uint16_t readData = *(__IO uint16_t*)Address;
//		Address += 1;
//		DEBUG("%c ", (char)readData); 
//	}	
		
//	FLASH_Lock();
	
	if(Status != FLASH_COMPLETE) return 1;
	return 0;	
}

/*****************************************************************************/
/**
 * @brief	:  Read bytes from internal flash
 * @param	:  , Address, pBuffer, Length
 * @retval	:	None
 * @author	:	Phinht
 * @created	:	15/09/2015
 * @version	:
 * @reviewer:	
 */
void InternalFlash_ReadBytes(uint32_t Address, uint8_t *pBuffer, uint32_t Length)
{
	uint32_t	i = 0;
	
	for(i = 0; i < Length; i++)
	{
		pBuffer[i]  = (*(__IO uint16_t*)(Address + i)) & 0xFF;
	}
}

/*****************************************************************************/
/**
 * @brief	:   Write int number to internal flash
 * @param	:   Address, Length
 * @retval	:   0 if success, 1 if error
 * @author	:	Phinht
 * @created	:	15/01/2014
 * @version	:
 * @reviewer:	
 */
uint8_t InternalFlash_WriteInt(uint32_t Address, uint16_t Data) 
{
    FLASH_Status Status;
	
	//Unlock Flash
//	InternalFlash_Init();
	Status = FLASH_ProgramHalfWord(Address, Data);
//	FLASH_Lock();    
	
	if(Status != FLASH_COMPLETE) return 1;
	return 0;
}

/*****************************************************************************/
/**
 * @brief	:   Write long number to internal flash
 * @param	:   Address, Data
 * @retval	:   0 if success, 1 if error
 * @author	:	Phinht
 * @created	:	15/01/2014
 * @version	:
 * @reviewer:	
 */
uint8_t InternalFlash_WriteLong(uint32_t Address, uint32_t Data) 
{
    FLASH_Status Status;
    
	//Unlock Flash
	InternalFlash_Init();
	Status = FLASH_ProgramWord(Address, Data);
	FLASH_Lock();      

	if(Status != FLASH_COMPLETE) return 1;
	return 0;
}


/*****************************************************************************/
/**
 * @brief	:   Copy data in internal flash
 * @param	:   SrcAddress, DstAddress, Length
 * @retval	:   1 if success, 0 if error
 * @author	:	Phinht
 * @created	:	15/01/2014
 * @version	:
 * @reviewer:	
 */
uint8_t InternalFlash_CopyData(uint32_t DesAddress,uint32_t SourceAddress, uint32_t Length)
{
    uint32_t i;
    FLASH_Status Status = FLASH_COMPLETE;
    
	 //Unlock Flash
	InternalFlash_Init();
	
    DEBUG ("\r\nCopy from 0x%X to 0x%X, amount: %d Bytes", SourceAddress, DesAddress, Length);
	
    for(i = 0; (i < Length) && (Status == FLASH_COMPLETE); i += 4, DesAddress += 4, SourceAddress += 4)
    {         			
		Status = FLASH_ProgramWord(DesAddress, *(__IO uint32_t*) SourceAddress);

        if((*(__IO uint32_t*) SourceAddress) != (*(__IO uint32_t*) DesAddress))	//Verify write data 
        {   
            DEBUG ("\r\nCannot copy, Des: %d, Source: %d",*(__IO uint32_t*) DesAddress,*(__IO uint32_t*) SourceAddress);
            return 0;
        }
		//Reset Watchdog
		if(i > 0 && (i%100 == 0))
		{
			ResetWatchdog();
		}
    }
	
	FLASH_Lock();   
    return 1;
}

/*****************************************************************************/
/**
 * @brief	:   Check CRC Firmware
 * @param	:  
 * @retval	:
 * @author	:	Phinht
 * @created	:	15/01/2016
 * @version	:
 * @reviewer:	
 */
uint8_t CheckFirmwareCRC(uint32_t FWCRC, uint32_t BeginAddr, uint32_t Length)
{
    uint32_t Total = 0;
    uint32_t i = 0;

    for(i = 0; i < Length; i++)
	{
        Total += *(__IO uint8_t *)(BeginAddr + i);
		
		if(i > 0 && (i%500 == 0))
		{
			ResetWatchdog();
		}
	}

    if(Total == FWCRC)
    {
        return 1;
    }
    DEBUG ("\r\nSai checksum %u - %u", FWCRC, Total);

    return 0;
}


/*****************************************************************************/
/**
 * @brief	: Write config
 * @param	:  
 * @retval	:	0 if Success, > 0 if Fail
 * @author	:	Phinht
 * @created	:	15/09/2015
 * @version	:
 * @reviewer:	
 */
uint8_t InternalFlash_WriteConfig(void)
{
	uint8_t	 result = 0, retry = 3;
	
	//Unlock Flash
	InternalFlash_Init();

	while(retry)
	{
		if (FLASH_ErasePage(CONFIG_ADDR) != FLASH_COMPLETE)
		{
			DEBUG ("\r\nCFG: erase at addr 0x%08X FAILED", CONFIG_ADDR);
			Delayms(200);
			retry--;
		} else 
			break;
	}
	
	if(retry > 0)
	{
		DEBUG ("\r\nCFG: erase at addr 0x%08X OK", CONFIG_ADDR);
		
		/* Flag */
		if(FLASH_ProgramWord(CONFIG_FLAG_ADDR, CONFIG_FLAG_VALUE) != FLASH_COMPLETE)
			result++;
		fmc_flag_clear(FMC_FLAG_END | FMC_FLAG_WPERR | FMC_FLAG_PGAERR | FMC_FLAG_PGERR);
		
		/* Frequency send message */
		if(FLASH_ProgramWord(CONFIG_FREQ_SEND_ADDR, xSystem.Parameters.TGGTDinhKy) != FLASH_COMPLETE)
			result++;
		fmc_flag_clear(FMC_FLAG_END | FMC_FLAG_WPERR | FMC_FLAG_PGAERR | FMC_FLAG_PGERR);
		
		/* Frequency measure sensor */
		if(FLASH_ProgramWord(CONFIG_FREQ_MEASURE_ADDR, xSystem.Parameters.TGDoDinhKy) != FLASH_COMPLETE)
			result++;		
		fmc_flag_clear(FMC_FLAG_END | FMC_FLAG_WPERR | FMC_FLAG_PGAERR | FMC_FLAG_PGERR);

		/* input config */
		if(FLASH_ProgramWord(CONFIG_INPUT_ADDR, (uint32_t)xSystem.Parameters.input.value) != FLASH_COMPLETE)
		{
			result++;
		}
		else if (*((uint32_t*)(CONFIG_INPUT_ADDR)) != xSystem.Parameters.input.value)
		{
			DEBUG("\r\nReadback value at addr %u failed", CONFIG_INPUT_ADDR);
			result++;
		}
		fmc_flag_clear(FMC_FLAG_END | FMC_FLAG_WPERR | FMC_FLAG_PGAERR | FMC_FLAG_PGERR);
		
		/* output1 */
		if(FLASH_ProgramWord(CONFIG_OUTPUT1_ADDR, xSystem.Parameters.outputOnOff) != FLASH_COMPLETE)
			result++;
		fmc_flag_clear(FMC_FLAG_END | FMC_FLAG_WPERR | FMC_FLAG_PGAERR | FMC_FLAG_PGERR);
		
		/* output 2 */
		if(FLASH_ProgramWord(CONFIG_OUTPUT2_ADDR, xSystem.Parameters.output420ma) != FLASH_COMPLETE)
			result++;
		fmc_flag_clear(FMC_FLAG_END | FMC_FLAG_WPERR | FMC_FLAG_PGAERR | FMC_FLAG_PGERR);
		
		/* alarm */
		if(FLASH_ProgramWord(CONFIG_ALARM_ADDR, xSystem.Parameters.alarm) != FLASH_COMPLETE)
			result++;
		fmc_flag_clear(FMC_FLAG_END | FMC_FLAG_WPERR | FMC_FLAG_PGAERR | FMC_FLAG_PGERR);
		
		/* phone number */
		for(uint8_t i = 0; i < 15; i++)
		{
			if(FLASH_ProgramWord(CONFIG_PHONE_ADDR + i*4, xSystem.Parameters.PhoneNumber[i]) != FLASH_COMPLETE)
				result++;
			fmc_flag_clear(FMC_FLAG_END | FMC_FLAG_WPERR | FMC_FLAG_PGAERR | FMC_FLAG_PGERR);
		}
				
		DEBUG ("\r\nCFG: Saved: %u - %s", result, result == 0 ? "OK" : "FAIL");
	}
	
	FLASH_Lock();
	
	return result;
}

uint8_t InternalFlash_WriteMeasures(void)
{
	uint8_t	 result = 0, retry = 3;
	
	//Unlock Flash
	InternalFlash_Init();

	while(retry)
	{
		if (FLASH_ErasePage(MEASURE_STORE_ADDR) != FLASH_COMPLETE)
		{
			DEBUG ("\r\nMEASURE: erase FAILED!");
			Delayms(200);
			retry--;
		}
		else
			break;
	}
	
	if(retry > 0)
	{
		DEBUG ("\r\nMEASURE: erase OK");
		
		/* Flag */
		if(FLASH_ProgramWord(MEASURE_STORE_FLAG_ADDR, MEASURE_STORE_FLAG_VALUE) != FLASH_COMPLETE)
			result++;
		fmc_flag_clear(FMC_FLAG_END | FMC_FLAG_WPERR | FMC_FLAG_PGAERR | FMC_FLAG_PGERR);
		
		/* pulse counter */
		if(FLASH_ProgramWord(MEASURE_PULSE_COUNT_ADDR, xSystem.MeasureStatus.PulseCounterInFlash) != FLASH_COMPLETE)
			result++;
		fmc_flag_clear(FMC_FLAG_END | FMC_FLAG_WPERR | FMC_FLAG_PGAERR | FMC_FLAG_PGERR);
		
		/* pressure */
		for(uint8_t i = 0; i < 4; i++)
		{
			if(FLASH_ProgramWord(MEASURE_PRESSURE_ADDR + i*4, xSystem.MeasureStatus.Pressure[i]) != FLASH_COMPLETE)
				result++;
			fmc_flag_clear(FMC_FLAG_END | FMC_FLAG_WPERR | FMC_FLAG_PGAERR | FMC_FLAG_PGERR);
		}		
		
		DEBUG ("\r\nMEASURE : Saved: %u - %s", result, result == 0 ? "OK" : "FAIL");
	}
	
	FLASH_Lock();
		
	return result;
}

/*****************************************************************************/
/**
 * @brief	: Read config
 * @param	:  
 * @retval	:	0 if Success, > 0 if Fail
 * @author	:	Phinht
 * @created	:	15/09/2015
 * @version	:
 * @reviewer:	
 */
void InternalFlash_ReadConfig(void)
{
	if(*(__IO uint32_t*)(CONFIG_FLAG_ADDR) != CONFIG_FLAG_VALUE)
	{
		DEBUG ("\r\nCHUA CO CAU HINH, DUNG CAU HINH MAC DINH!");
		
		xSystem.Parameters.TGGTDinhKy = 30;		//phut
		xSystem.Parameters.TGDoDinhKy = 15;		//phut
		xSystem.Parameters.outputOnOff = 0;		//off
		xSystem.Parameters.output420ma = 0;		//off
		xSystem.Parameters.input.value = 0;		//all input off
		xSystem.Parameters.input.name.pulse = 1;	//on
		xSystem.Parameters.alarm = 0;		//alarm off
		sprintf(xSystem.Parameters.PhoneNumber, "%s", "000");
	}
	else 
	{
		xSystem.Parameters.TGGTDinhKy = (*(__IO uint32_t*)(CONFIG_FREQ_SEND_ADDR)) & 0xFFFF;
		xSystem.Parameters.TGDoDinhKy = (*(__IO uint32_t*)(CONFIG_FREQ_MEASURE_ADDR)) & 0xFFFF;
		xSystem.Parameters.outputOnOff = (*(__IO uint32_t*)(CONFIG_OUTPUT1_ADDR)) & 0xFF;
		xSystem.Parameters.output420ma = (*(__IO uint32_t*)(CONFIG_OUTPUT2_ADDR)) & 0xFF;
		xSystem.Parameters.input.value = (*(__IO uint32_t*)(CONFIG_INPUT_ADDR)) & 0xFF;
		xSystem.Parameters.alarm = (*(__IO uint32_t*)(CONFIG_ALARM_ADDR)) & 0xFF;
		
		for(uint8_t i = 0; i < 15; i++) {
			char ch = *(__IO uint32_t*)(CONFIG_PHONE_ADDR + i*4);
			if((ch >= '0' && ch <= '9') || ch == '+')
				xSystem.Parameters.PhoneNumber[i] = ch;
			else
				xSystem.Parameters.PhoneNumber[i] = '0';
		}
	}
	DEBUG ("\r\nFreq: %u - %u", xSystem.Parameters.TGGTDinhKy, xSystem.Parameters.TGDoDinhKy);
	DEBUG ("\r\nOut1: %u, out2: %umA", xSystem.Parameters.outputOnOff, xSystem.Parameters.output420ma); 
	DEBUG ("\r\nInput: %02X at addr 0x%08X", xSystem.Parameters.input.value, CONFIG_INPUT_ADDR);
	DEBUG ("\r\nAlarm: %u", xSystem.Parameters.alarm);
	DEBUG ("\r\nPhone: %s", xSystem.Parameters.PhoneNumber);
	DEBUG ("\r\nMeasure addr 0x%08X", MEASURE_PRESSURE_ADDR);
	
	//Doc thong so do
	if(*(__IO uint32_t*)(MEASURE_STORE_FLAG_ADDR) != MEASURE_STORE_FLAG_VALUE)
	{
		xSystem.MeasureStatus.PulseCounterInFlash = 0;
		for(uint8_t i = 0; i < 4; i++)
			xSystem.MeasureStatus.Pressure[i] = 0;
	}
	else
	{
		xSystem.MeasureStatus.PulseCounterInFlash = *(__IO uint32_t*)(MEASURE_PULSE_COUNT_ADDR);
		//pressure
		for(uint8_t i = 0; i < 4; i++) {
			xSystem.MeasureStatus.Pressure[i] = (*(__IO uint32_t*)(MEASURE_PRESSURE_ADDR + i*4)) & 0xFFFF;
		}
	}
	DEBUG ("\r\nPulse in flash: %u", xSystem.MeasureStatus.PulseCounterInFlash);
	DEBUG ("\r\nPressure: %u-%u-%u-%u", xSystem.MeasureStatus.Pressure[0], xSystem.MeasureStatus.Pressure[1],
		xSystem.MeasureStatus.Pressure[2], xSystem.MeasureStatus.Pressure[3]);
}

/********************************* END OF FILE *******************************/

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
#include "app_wdt.h"

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
#define FLASH_ProgramWord fmc_word_program
#define FLASH_ProgramHalfWord fmc_halfword_program
#define FLASH_Lock fmc_lock
#define FLASH_UnLock fmc_unlock
#define FLASH_ErasePage fmc_page_erase
#define FLASH_Status fmc_state_enum
#define FLASH_COMPLETE FMC_READY
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
    fmc_flag_clear((fmc_flag_enum)(FMC_FLAG_END | FMC_FLAG_WPERR | FMC_FLAG_PGAERR | FMC_FLAG_PGERR));
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

    for (PageNum = StartPage; PageNum <= EndPage; PageNum++)
    {
        app_wdt_feed();
        ControlAllUART(DISABLE);

        for (i = 0; i < 3; i++)
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
            if (fmc_page_erase(FLASH_BASE + PageNum * FLASH_PAGE_SIZE) == FMC_READY)
            {
                status = 0;
                break;
            }
            else
            {
                status++;
                fmc_flag_clear((fmc_flag_enum)(FMC_FLAG_END | FMC_FLAG_WPERR | FMC_FLAG_PGAERR | FMC_FLAG_PGERR));
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

    if (SoByteCu) //Con byte cu can ghi
    {
        //Khong du 1 word
        if ((SoByteCu + Length) < 4)
        {
            for (i = 0; i < Length; i++)
            {
                MangByteCu[SoByteCu + i] = Buffer[i];
            }
            SoByteCu += Length;
            return;
        }

        //Vua du 1 word
        if ((SoByteCu + Length) == 4)
        {
            for (i = 0; i < Length; i++)
            {
                MangByteCu[SoByteCu + i] = Buffer[i];
            }
            //Ghi 1 word
            for (i = 0; i < 4; i++)
            {
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
        if ((SoByteCu + Length) > 4)
        {
            //Copy them so byte (1, 2, hoac 3) cho du 1 Word
            for (i = 0; i < 4 - SoByteCu; i++)
            {
                MangByteCu[SoByteCu + i] = Buffer[i];
            }
            ViTri = 4 - SoByteCu; //Vi tri bat dau du lieu moi cua mang

            //Ghi Word cu
            for (i = 0; i < 4; i++)
            {
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
            if (SoByteLe) //Neu co so byte le thi copy lai de ghi lan sau
            {
                for (i = 0; i < SoByteLe; i++)
                {
                    MangByteCu[i] = Buffer[SoWord * 4 + i + ViTri];
                }
                SoByteCu = SoByteLe;
            }
            //Ghi so Word vao Flash neu > 0
            if (SoWord > 0)
            {
                for (i = 0; i < SoWord; i++)
                {
                    tmpWord.bytes[0] = Buffer[i * 4 + ViTri];
                    tmpWord.bytes[1] = Buffer[i * 4 + 1 + ViTri];
                    tmpWord.bytes[2] = Buffer[i * 4 + 2 + ViTri];
                    tmpWord.bytes[3] = Buffer[i * 4 + 3 + ViTri];

                    FLASH_ProgramWord(xSystem.FileTransfer.FileAddress, tmpWord.value);
                    xSystem.FileTransfer.FileAddress += 4;
                }
            }
        }
    }
    else //Lan dau ghi hoac ko con so byte cu
    {
        SoWord = Length / 4;
        SoByteLe = Length % 4;

        if (SoByteLe) //Neu co so byte le thi copy lai de ghi lan sau
        {
            for (i = 0; i < SoByteLe; i++)
            {
                MangByteCu[i] = Buffer[SoWord * 4 + i];
            }
            SoByteCu = SoByteLe;
        }
        //Ghi so Word vao Flash neu > 0
        if (SoWord > 0)
        {
            for (i = 0; i < SoWord; i++)
            {
                tmpWord.bytes[0] = Buffer[i * 4];
                tmpWord.bytes[1] = Buffer[i * 4 + 1];
                tmpWord.bytes[2] = Buffer[i * 4 + 2];
                tmpWord.bytes[3] = Buffer[i * 4 + 3];
                FLASH_ProgramWord(xSystem.FileTransfer.FileAddress, tmpWord.value);
                xSystem.FileTransfer.FileAddress += 4;
            }
        }
    }
}

void CheckEOF(void)
{
    if (SoByteCu > 0) //Con byte cu dang doi ghi -> Ghi not
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

    for (i = 0; i < Length; i++)
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
    //		DEBUG_PRINTF("%c ", (char)readData);
    //	}

    //	FLASH_Lock();

    if (Status != FLASH_COMPLETE)
        return 1;
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
    uint32_t i = 0;

    for (i = 0; i < Length; i++)
    {
        pBuffer[i] = (*(__IO uint16_t *)(Address + i)) & 0xFF;
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

    if (Status != FLASH_COMPLETE)
        return 1;
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

    if (Status != FLASH_COMPLETE)
        return 1;
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
uint8_t InternalFlash_CopyData(uint32_t DesAddress, uint32_t SourceAddress, uint32_t Length)
{
    uint32_t i;
    FLASH_Status Status = FLASH_COMPLETE;

    //Unlock Flash
    InternalFlash_Init();

    DEBUG_PRINTF("\r\nCopy from 0x%X to 0x%X, amount: %d Bytes", SourceAddress, DesAddress, Length);

    for (i = 0; (i < Length) && (Status == FLASH_COMPLETE); i += 4, DesAddress += 4, SourceAddress += 4)
    {
        Status = FLASH_ProgramWord(DesAddress, *(__IO uint32_t *)SourceAddress);

        if ((*(__IO uint32_t *)SourceAddress) != (*(__IO uint32_t *)DesAddress)) //Verify write data
        {
            DEBUG_PRINTF("\r\nCannot copy, Des: %d, Source: %d", *(__IO uint32_t *)DesAddress, *(__IO uint32_t *)SourceAddress);
            return 0;
        }
        //Reset Watchdog
        if (i > 0 && (i % 100 == 0))
        {
            app_wdt_feed();
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

    for (i = 0; i < Length; i++)
    {
        Total += *(__IO uint8_t *)(BeginAddr + i);

        if (i > 0 && (i % 500 == 0))
        {
            app_wdt_feed();
        }
    }

    if (Total == FWCRC)
    {
        return 1;
    }
    DEBUG_PRINTF("\r\nSai checksum %u - %u", FWCRC, Total);

    return 0;
}

static uint32_t write_flash_error_cnt = 0;

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
    uint8_t result = 0, retry = 3;

    //Unlock Flash
    InternalFlash_Init();

    while (retry)
    {
        if (FLASH_ErasePage(CONFIG_ADDR) != FLASH_COMPLETE)
        {
            DEBUG_PRINTF("CFG: erase at addr 0x%08X FAILED\r\n", CONFIG_ADDR);
            Delayms(200);
            retry--;
        }
        else
            break;
    }

    if (retry > 0)
    {
        DEBUG_PRINTF("CFG: erase at addr 0x%08X OK\r\n", CONFIG_ADDR);

        /* Flag */
        __disable_irq();
        if (FLASH_ProgramWord(CONFIG_FLAG_ADDR, CONFIG_FLAG_VALUE) != FLASH_COMPLETE)
            result++;
        fmc_flag_clear((fmc_flag_enum)(FMC_FLAG_END | FMC_FLAG_WPERR | FMC_FLAG_PGAERR | FMC_FLAG_PGERR));

        /* Frequency send message */
        if (FLASH_ProgramWord(CONFIG_FREQ_SEND_ADDR, xSystem.Parameters.period_send_message_to_server_min) != FLASH_COMPLETE)
            result++;
        fmc_flag_clear((fmc_flag_enum)(FMC_FLAG_END | FMC_FLAG_WPERR | FMC_FLAG_PGAERR | FMC_FLAG_PGERR));

        /* Frequency measure sensor */
        if (FLASH_ProgramWord(CONFIG_FREQ_MEASURE_ADDR, xSystem.Parameters.period_measure_peripheral) != FLASH_COMPLETE)
            result++;
        fmc_flag_clear((fmc_flag_enum)(FMC_FLAG_END | FMC_FLAG_WPERR | FMC_FLAG_PGAERR | FMC_FLAG_PGERR));
        
        __align(4) uint32_t tmp_align = xSystem.Parameters.input.value;
        /* input config */
        if (FLASH_ProgramWord(CONFIG_INPUT_ADDR, tmp_align) != FLASH_COMPLETE)
        {
            result++;
        }
        else if (*((uint32_t *)(CONFIG_INPUT_ADDR)) != tmp_align)
        {
            DEBUG_PRINTF("Readback value at addr %u failed\r\n", CONFIG_INPUT_ADDR);
            result++;
        }
        fmc_flag_clear((fmc_flag_enum)(FMC_FLAG_END | FMC_FLAG_WPERR | FMC_FLAG_PGAERR | FMC_FLAG_PGERR));

        tmp_align = xSystem.Parameters.outputOnOff;
        /* output1 */
        if (FLASH_ProgramWord(CONFIG_OUTPUT1_ADDR, tmp_align) != FLASH_COMPLETE)
            result++;
        fmc_flag_clear((fmc_flag_enum)(FMC_FLAG_END | FMC_FLAG_WPERR | FMC_FLAG_PGAERR | FMC_FLAG_PGERR));
        
        tmp_align = xSystem.Parameters.output420ma;
        /* output 2 */
        if (FLASH_ProgramWord(CONFIG_OUTPUT2_ADDR, tmp_align) != FLASH_COMPLETE)
            result++;
        fmc_flag_clear((fmc_flag_enum)(FMC_FLAG_END | FMC_FLAG_WPERR | FMC_FLAG_PGAERR | FMC_FLAG_PGERR));

        /* alarm */
        tmp_align = xSystem.Parameters.alarm;
        if (FLASH_ProgramWord(CONFIG_ALARM_ADDR, tmp_align) != FLASH_COMPLETE)
            result++;
        fmc_flag_clear((fmc_flag_enum)(FMC_FLAG_END | FMC_FLAG_WPERR | FMC_FLAG_PGAERR | FMC_FLAG_PGERR));

        /* Input 1 offset */
        tmp_align = xSystem.Parameters.input1Offset;
        if (FLASH_ProgramWord(CONFIG_INPUT1_OFFSET, tmp_align) != FLASH_COMPLETE)
            result++;
        fmc_flag_clear((fmc_flag_enum)(FMC_FLAG_END | FMC_FLAG_WPERR | FMC_FLAG_PGAERR | FMC_FLAG_PGERR));
        
        /* K factor */
        tmp_align = xSystem.Parameters.kFactor;
        if (FLASH_ProgramWord(CONFIG_INPUT1_K_FACTOR, tmp_align) != FLASH_COMPLETE)
            result++;
        fmc_flag_clear((fmc_flag_enum)(FMC_FLAG_END | FMC_FLAG_WPERR | FMC_FLAG_PGAERR | FMC_FLAG_PGERR));

        /* phone number */
        for (uint8_t i = 0; i < 15; i++)
        {
            tmp_align = xSystem.Parameters.phone_number[i];
            if (FLASH_ProgramWord(CONFIG_PHONE_ADDR + i * 4, tmp_align) != FLASH_COMPLETE)
                result++;
            fmc_flag_clear((fmc_flag_enum)(FMC_FLAG_END | FMC_FLAG_WPERR | FMC_FLAG_PGAERR | FMC_FLAG_PGERR));
        }
        __enable_irq();

        DEBUG_PRINTF("CFG: Saved: %u - %s\r\n", result, result == 0 ? "OK" : "FAIL");
    }

    FLASH_Lock();

    if (result)
    {
        write_flash_error_cnt++;
        if (write_flash_error_cnt > 100)
        {
            DEBUG_PRINTF("Reset device when flash error\r\n");
            NVIC_SystemReset();
        }
    }

    return result;
}

uint8_t InternalFlash_WriteMeasures(void)
{
    uint8_t result = 0, retry = 3;

    //Unlock Flash
    InternalFlash_Init();

    while (retry)
    {
        if (FLASH_ErasePage(MEASURE_STORE_ADDR) != FLASH_COMPLETE)
        {
            DEBUG_PRINTF("MEASURE: erase FAILED! at addr 0x%08X\r\n", MEASURE_STORE_ADDR);
            Delayms(200);
            retry--;
        }
        else
            break;
    }

    if (retry > 0)
    {
        DEBUG_PRINTF("MEASURE: erase OK! at addr 0x%08X\r\n", MEASURE_STORE_ADDR);
        __align(4) uint32_t tmp_align;
        __disable_irq();
        /* Flag */
        if (FLASH_ProgramWord(MEASURE_STORE_FLAG_ADDR, MEASURE_STORE_FLAG_VALUE) != FLASH_COMPLETE)
        {
            result++;
        }
        fmc_flag_clear((fmc_flag_enum)(FMC_FLAG_END | FMC_FLAG_WPERR | FMC_FLAG_PGAERR | FMC_FLAG_PGERR));

        /* pulse counter */
        tmp_align = xSystem.MeasureStatus.PulseCounterInFlash;
        if (FLASH_ProgramWord(MEASURE_PULSE_COUNT_ADDR, tmp_align) != FLASH_COMPLETE)
        {
            result++;
        }
        fmc_flag_clear((fmc_flag_enum)(FMC_FLAG_END | FMC_FLAG_WPERR | FMC_FLAG_PGAERR | FMC_FLAG_PGERR));

        /* pressure */
        for (uint8_t i = 0; i < 4; i++)
        {
            tmp_align = xSystem.MeasureStatus.Pressure[i];
            if (FLASH_ProgramWord(MEASURE_PRESSURE_ADDR + i * 4, tmp_align) != FLASH_COMPLETE)
            {
                result++;
            }
            fmc_flag_clear((fmc_flag_enum)(FMC_FLAG_END | FMC_FLAG_WPERR | FMC_FLAG_PGAERR | FMC_FLAG_PGERR));
        }
        __enable_irq();

        DEBUG_PRINTF("MEASURE : Saved: %u - %s at begin addr 0x%08X\r\n",
              result,
              result == 0 ? "OK" : "FAIL",
              MEASURE_PRESSURE_ADDR);
    }

    FLASH_Lock();

    if (result)
    {
        write_flash_error_cnt++;
        if (write_flash_error_cnt > 100)
        {
            DEBUG_PRINTF("Reset device when flash error\r\n");
            NVIC_SystemReset();
        }
    }

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
    if (*(__IO uint32_t *)(CONFIG_FLAG_ADDR) != CONFIG_FLAG_VALUE)
    {
        DEBUG_PRINTF("Use default parameters\r\n");

        xSystem.Parameters.period_send_message_to_server_min = 60;      //phut
        xSystem.Parameters.period_measure_peripheral = 15;      //phut
        xSystem.Parameters.outputOnOff = 0;      //off
        xSystem.Parameters.output420ma = 0;      //off
        xSystem.Parameters.input.value = 0;      //all input off
        xSystem.Parameters.input.name.pulse = 1; //on
        xSystem.Parameters.alarm = 0;            //alarm off
        xSystem.Parameters.kFactor = 1;
        xSystem.Parameters.input1Offset = 0;
        sprintf(xSystem.Parameters.phone_number, "%s", "000");
    }
    else
    {
        xSystem.Parameters.period_send_message_to_server_min = (*(__IO uint32_t *)(CONFIG_FREQ_SEND_ADDR)) & 0xFFFF;
        xSystem.Parameters.period_measure_peripheral = (*(__IO uint32_t *)(CONFIG_FREQ_MEASURE_ADDR)) & 0xFFFF;
        xSystem.Parameters.outputOnOff = (*(__IO uint32_t *)(CONFIG_OUTPUT1_ADDR)) & 0xFF;
        xSystem.Parameters.output420ma = (*(__IO uint32_t *)(CONFIG_OUTPUT2_ADDR)) & 0xFF;
        xSystem.Parameters.input.value = (*(__IO uint32_t *)(CONFIG_INPUT_ADDR)) & 0xFF;
        xSystem.Parameters.alarm = (*(__IO uint32_t *)(CONFIG_ALARM_ADDR)) & 0xFF;
        xSystem.Parameters.input1Offset = (*(__IO uint32_t *)(CONFIG_INPUT1_OFFSET)) & 0xFFFFFFFF;
        xSystem.Parameters.kFactor = (*(__IO uint32_t *)(CONFIG_INPUT1_K_FACTOR)) & 0xFFFFFFFF;;

        for (uint8_t i = 0; i < 15; i++)
        {
            char ch = *(__IO uint32_t *)(CONFIG_PHONE_ADDR + i * 4);
            if ((ch >= '0' && ch <= '9') || ch == '+')
                xSystem.Parameters.phone_number[i] = ch;
            else
                xSystem.Parameters.phone_number[i] = '0';
        }
    }
    DEBUG_PRINTF("Freq: %u - %u\r\n", xSystem.Parameters.period_send_message_to_server_min, xSystem.Parameters.period_measure_peripheral);
    DEBUG_PRINTF("Out1: %u, out2: %umA\r\n", xSystem.Parameters.outputOnOff, xSystem.Parameters.output420ma);
    DEBUG_PRINTF("Input: %02X at addr 0x%08X\r\n", xSystem.Parameters.input.value, CONFIG_INPUT_ADDR);
    DEBUG_PRINTF("Alarm: %u\r\n", xSystem.Parameters.alarm);
    DEBUG_PRINTF("Offset: %u, K %u\r\n", xSystem.Parameters.input1Offset, xSystem.Parameters.kFactor);
    DEBUG_PRINTF("Phone: %s\r\n", xSystem.Parameters.phone_number);
    DEBUG_PRINTF("Measure addr 0x%08X\r\n", MEASURE_PRESSURE_ADDR);

    //Doc thong so do
    if (*(__IO uint32_t *)(MEASURE_STORE_FLAG_ADDR) != MEASURE_STORE_FLAG_VALUE)
    {
        xSystem.MeasureStatus.PulseCounterInFlash = 0;
        for (uint8_t i = 0; i < 4; i++)
            xSystem.MeasureStatus.Pressure[i] = 0;
    }
    else
    {
        xSystem.MeasureStatus.PulseCounterInFlash = *(__IO uint32_t *)(MEASURE_PULSE_COUNT_ADDR);
        //pressure
        for (uint8_t i = 0; i < 4; i++)
        {
            xSystem.MeasureStatus.Pressure[i] = (*(__IO uint32_t *)(MEASURE_PRESSURE_ADDR + i * 4)) & 0xFFFF;
        }
    }
    DEBUG_PRINTF("Pulse in flash: %u\r\n", xSystem.MeasureStatus.PulseCounterInFlash);
    DEBUG_PRINTF("Pressure: %u-%u-%u-%u\r\n", xSystem.MeasureStatus.Pressure[0], xSystem.MeasureStatus.Pressure[1],
          xSystem.MeasureStatus.Pressure[2], xSystem.MeasureStatus.Pressure[3]);
}

/********************************* END OF FILE *******************************/

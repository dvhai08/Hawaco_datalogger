#ifndef __INTERNAL_FLASH_H__
#define __INTERNAL_FLASH_H__

#include <stdint.h>


/* Cau truc bo nho Flash GD32E103CB (128KB - Medium Density) */
/* Main memory: 128 Pages of 1KB size */
/*--------------------------------------------------------------------------------------
* 						Bootloader 8KB, page 0 - 7 									
*	--------------------------------------------------------------------------------------
* 						Main application 59KB, page 8 - 	66				
*	--------------------------------------------------------------------------------------
* 						UDFW memory 59KB, page 67 - 125 				
*	--------------------------------------------------------------------------------------
* 						Measure result memory 1KB, page 126		
*	--------------------------------------------------------------------------------------							
*							Params config memory 1KB, page 127
*	--------------------------------------------------------------------------------------
*/

#define		FLASH_PAGE_SIZE	0x400	/* 1024 Bytes */

/* Define dia chi bat dau cua Bootloader trong Flash */
/* BootLoader: Page 0 - 7  = 8 pages = 8 KB */
#define 	BOOTLOADER_ADDR	FLASH_BASE
#define		BOOT_LOADER_START_PAGE	0
#define		BOOT_LOADER_END_PAGE		7
#define 	BOOTLOADER_MAX_SIZE		0x2000 	/* 8KB */

/* Define dia chi bat dau cua Main Application */
/* Main Application: Page 8 - 66 = 59 Pages = 59KB */
#define		MAIN_FW_START_PAGE	8
#define		MAIN_FW_END_PAGE		66
#define 	MAIN_FW_ADDR		(FLASH_BASE + BOOTLOADER_MAX_SIZE)

/* Define dia chi vung nho luu Firmware Update */
/* FW Update Store: Page 67 - 125 = 59 pages = 59KB */
#define		FW_UPDATE_START_PAGE		67
#define		FW_UPDATE_END_PAGE		125
#define		FW_UPDATE_ADDR		(uint32_t)(FW_UPDATE_START_PAGE * FLASH_PAGE_SIZE)		//Page 67

#define		MEASURE_STORE_START_PAGE	126
#define		CONFIG_START_PAGE	127

#define		NewFWFlag_InFlash	FW_UPDATE_ADDR
#define 	NewFWFlag_Length		4

#define		FWLength_InFlash		(NewFWFlag_InFlash + NewFWFlag_Length)
#define 	FWLength_Length	4

#define		FWCRC_InFlash	(FWLength_InFlash + FWLength_Length)
#define 	FWCRC_Length	4

#define 	FWDATA_ADDR		(FW_UPDATE_ADDR + 20)
#define 	NEW_FW_FLAG_VALUE		0xAA55
#define		NEW_BLDER_FLAG_VALUE	0x66BB

/* =================== Dia chi vung nho luu thong so do */
#define	MEASURE_STORE_FLAG_VALUE		0x56ABCDEF
#define	MEASURE_STORE_ADDR	(FLASH_BASE +  FLASH_PAGE_SIZE*MEASURE_STORE_START_PAGE)		/* Page 126 */

#define MEASURE_STORE_FLAG_ADDR	(MEASURE_STORE_ADDR + 0x80)
#define MEASURE_STORE_FLAG_LENGTH	4

#define	MEASURE_PULSE_COUNT_ADDR		(uint32_t)(MEASURE_STORE_FLAG_ADDR + MEASURE_STORE_FLAG_LENGTH)
#define	MEASURE_PULSE_COUNT_LENGTH	4

//4 gia tri pressure gan nhat
#define	MEASURE_PRESSURE_ADDR		(uint32_t)(MEASURE_PULSE_COUNT_ADDR + MEASURE_PULSE_COUNT_LENGTH)
#define	MEASURE_PRESSURE_LENGTH	16

/* =================== Dia chi vung nho luu cau hinh thiet bi */
#define	CONFIG_FLAG_VALUE		0x56ABCDEF
#define	CONFIG_ADDR	(FLASH_BASE +  FLASH_PAGE_SIZE*CONFIG_START_PAGE)		/* Page 127 */

//Bo qua 128 byte dau
#define	CONFIG_FLAG_ADDR	(CONFIG_ADDR + 0x40)
#define	CONFIG_FLAG_LENGTH	4

//chu ky gui tin
#define	CONFIG_FREQ_SEND_ADDR	(uint32_t)(CONFIG_FLAG_ADDR + CONFIG_FLAG_LENGTH)
#define	CONFIG_FREQ_SEND_LENGTH	4	

//chu ky thuc day do dac
#define	CONFIG_FREQ_MEASURE_ADDR		(uint32_t)(CONFIG_FREQ_SEND_ADDR + CONFIG_FREQ_SEND_LENGTH)
#define	CONFIG_FREQ_MEASURE_LENGTH	4

//config dau vao input: to hop bit cua cac dau vao Input1, input2, RS485, 
#define	CONFIG_INPUT_ADDR		(uint32_t)(CONFIG_FREQ_MEASURE_ADDR + CONFIG_FREQ_MEASURE_LENGTH)
#define	CONFIG_INPUT_LENGTH		4

//dieu khien output1: logic 0/1
#define	CONFIG_OUTPUT1_ADDR		(uint32_t)(CONFIG_INPUT_ADDR + CONFIG_INPUT_LENGTH)
#define	CONFIG_OUTPUT1_LENGTH		4

//dieu khien output2: tin hieu 4-20mA
#define	CONFIG_OUTPUT2_ADDR		(uint32_t)(CONFIG_OUTPUT1_ADDR + CONFIG_OUTPUT1_LENGTH)
#define	CONFIG_OUTPUT2_LENGTH		4

//cau hinh canh bao
#define	CONFIG_ALARM_ADDR		(uint32_t)(CONFIG_OUTPUT2_ADDR + CONFIG_OUTPUT2_LENGTH)
#define	CONFIG_ALARM_LENGTH		4

#define	CONFIG_INPUT1_OFFSET		(uint32_t)(CONFIG_ALARM_ADDR + CONFIG_ALARM_LENGTH)
#define	CONFIG_INPUT1_OFFSET_LENGTH		4

#define	CONFIG_INPUT1_K_FACTOR		(uint32_t)(CONFIG_INPUT1_OFFSET + CONFIG_INPUT1_OFFSET_LENGTH)
#define	CONFIG_INPUT1_K_FACTOR_LENGTH		4

//cau hinh SDT
#define	CONFIG_PHONE_ADDR		(uint32_t)(CONFIG_INPUT1_K_FACTOR + CONFIG_INPUT1_K_FACTOR_LENGTH)
#define	CONFIG_PHONE_LENGTH		15




void InternalFlashWriteBytes(uint8_t *Buffer, uint16_t Length);
void CheckEOF(void);

uint8_t InternalFlash_Prepare(uint16_t StartPage, uint16_t EndPage);
uint8_t InternalFlash_WriteBytes(uint32_t Address, uint16_t *Buffer, uint16_t Length);
void InternalFlash_ReadBytes(uint32_t Address, uint8_t *pBuffer, uint32_t Length);
uint8_t InternalFlash_WriteInt(uint32_t Address, uint16_t Data);
uint8_t InternalFlash_WriteLong(uint32_t Address, uint32_t Data);
uint8_t InternalFlash_CopyData(uint32_t DesAddress,uint32_t SourceAddress, uint32_t Length);
uint8_t CheckFirmwareCRC(uint32_t FWCRC, uint32_t BeginAddr, uint32_t Length);

uint8_t InternalFlash_WriteConfig(void);
uint8_t InternalFlash_WriteMeasures(void);
void InternalFlash_ReadConfig(void);

#endif // __INTERNAL_FLASH_H__


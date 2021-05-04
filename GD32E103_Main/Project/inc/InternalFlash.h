#ifndef		__INTERNAL_FLASH_H__
#define	__INTERNAL_FLASH_H__

#include "DataDefine.h"

/*
*	The memory organization of STM32F030x4, STM32F030x6, STM32F070x6 and 
*	STM32F030x8 devices is based on a main Flash memory block containing up to 64 pages 
*	of 1 Kbyte or up to 16 sectors of 4 Kbytes (4 pages). 
*/
 #define PAGE_SIZE		((uint32_t)0x00000400)   /* FLASH Page Size */

/* Dinh nghia dia chi bat dau cua BootLoader */
#define	BOOTLOADER_FW_ADDR	FLASH_BASE
#define	BOOTLOADER_MAX_SIZE 0x4000 /* 16K */

/* Dinh nghia dia chi bat dau cua Main Application */
#define	APPLICATION_FW_ADDR (FLASH_BASE + BOOTLOADER_MAX_SIZE)	/*Sector 4-9*/
#define	APPLICATION_FW_SIZE	0x6000	/* 24K size */

/* Define dia chi vung nho luu Firmware Update: 24K size */
#define	FW_UPDATE_ADDR	(APPLICATION_FW_ADDR + APPLICATION_FW_SIZE)	/*Sector 10-15 */

#define	VEHICLE_COUNT_ADDR	(FLASH_BASE + 62*PAGE_SIZE)		/* Page 62 */
#define	VEHICLE_COUNT_NUM_ADDR		(uint32_t)(VEHICLE_COUNT_ADDR + 8)
#define	VEHICLE_COUNT_FLAG_VALUE		0x89ABCDEF

/* Dia chi vung nho luu cau hinh thiet bi */
#define	CONFIG_FLAG_VALUE		0x56ABCDEF
#define	CONFIG_ADDR	(FLASH_BASE +  PAGE_SIZE*127)		/* Page 127 */

#define	CONFIG_FLAG_ADDR	CONFIG_ADDR
#define	CONFIG_FLAG_LENGTH	4

#define	CONFIG_MODE_ADDR		(uint32_t)(CONFIG_FLAG_ADDR + CONFIG_FLAG_LENGTH)
#define	CONFIG_MODE_LENGTH		4

#define	CONFIG_FREQ_ADDR	(uint32_t)(CONFIG_MODE_ADDR + CONFIG_MODE_LENGTH)
#define	CONFIG_FREQ_LENGTH	20	//For max 5 Freq

#define	CONFIG_FREQ_INDEX_ADDR		(uint32_t)(CONFIG_FREQ_ADDR + CONFIG_FREQ_LENGTH)
#define	CONFIG_FREQ_INDEX_LENGTH	4

#define	CONFIG_VOLUME_ADDR		(uint32_t)(CONFIG_FREQ_INDEX_ADDR + CONFIG_FREQ_INDEX_LENGTH)
#define	CONFIG_VOLUME_LENGTH	4

#define	CONFIG_LEDBRIGHT_ADDR		(uint32_t)(CONFIG_VOLUME_ADDR + CONFIG_VOLUME_LENGTH)
#define	CONFIG_LEDBRIGHT_LENGTH	4

#define	CONFIG_LOCAL_MODE_ADDR		(uint32_t)(CONFIG_LEDBRIGHT_ADDR + CONFIG_LEDBRIGHT_LENGTH)
#define	CONFIG_LOCAL_MODE_LENGTH	4

#define	CONFIG_DISP_DELAY_ADDR		(uint32_t)(CONFIG_LOCAL_MODE_ADDR + CONFIG_LOCAL_MODE_LENGTH)
#define	CONFIG_DISP_DELAY_LENGTH	4

uint8_t InternalFlash_Prepare(uint32_t Address);
uint8_t InternalFlash_WriteConfig(void);
void InternalFlash_ReadConfig(void);

uint8_t InternalFlash_WriteBytes(uint32_t Address, uint8_t *pBuffer, uint16_t Length);
uint8_t InternalFlash_WriteLong(uint32_t Address, uint32_t Data);

#endif	/* __INTERNAL_FLASH_H__ */

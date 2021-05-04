/**
  ******************************************************************************
  * @file    I2CFM.h
  * @author  MCD Application Team
  * @version V1.1.1
  * @date    16-January-2014
  * @brief   This file contains all the functions prototypes for the 
  *          I2CFM.c firmware driver.
  ******************************************************************************
  */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __I2C_FM_H__
#define __I2C_FM_H__

#ifdef __cplusplus
 extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "gd32e10x.h"
#include "Hardware.h"
	 
typedef enum
{
  KT0935_OK = 0,
  KT0935_FAIL = 0xFFFF
} KT0935_Status_TypDef;

typedef enum {
	NORMAL_MODE = 0,
	STANDBY_MODE
} FM_OperateMode_t;

/* Exported constants --------------------------------------------------------*/
/* Uncomment the following line to use Timeout_User_Callback KT0935_TimeoutUserCallback(). 
   If This Callback is enabled, it should be implemented by user in main function .
   KT0935_TimeoutUserCallback() function is called whenever a timeout condition 
   occurs during communication (waiting on an event that doesn't occur, bus 
   errors, busy devices ...). */   
/* #define USE_TIMEOUT_USER_CALLBACK */    
    
/* Maximum Timeout values for flags and events waiting loops. These timeouts are
   not based on accurate values, they just guarantee that the application will 
   not remain stuck if the I2C communication is corrupted.
   You may modify these timeout values depending on CPU frequency and application
   conditions (interrupts routines ...). */   
#define KT0935_FLAG_TIMEOUT         ((uint32_t)0x10000)
#define KT0935_LONG_TIMEOUT         ((uint32_t)(10 * KT0935_FLAG_TIMEOUT))    
    
/**
  * @brief  Block Size
  */
#define FM_REG_DEVICEID0		0x00	/* Default: 0x82 */
#define FM_REG_DEVICEID1		0x01	/* Default: 0x06 */

#define FM_REG_KTMARK0		0x02	/* Default: 0x4B */
#define FM_REG_KTMARK1		0x03	/* Default: 0x54 */

#define FM_REG_PLLCFG0		0x04	/* Default: 0x00 */
#define FM_REG_PLLCFG1		0x05	/* Default: 0x01 */
#define FM_REG_PLLCFG2		0x06	/* Default: 0x02 */
#define FM_REG_PLLCFG3		0x07	/* Default: 0x9C */

#define FM_REG_SYSCLKCFG0		0x08	/* Default: 0x08 */
#define FM_REG_SYSCLKCFG1		0x09	/* Default: 0x00 */
#define FM_REG_SYSCLKCFG2		0x0A	/* Default: 0x00 */

/* Ref clock enable: bit4 = 0: Crystal, = 1:  External Ref clock */
#define FM_REG_XTALCFG		0x0D	/* Default: 0xC3 = Crystal */

/* Standby mode control: bit5 = 0: Normal mode, = 1:  Standby mode */
#define FM_REG_OPMODE		0x0E	/* Default: 0x00 = normal mode */

/* Volume control : bit [4:0] : 11111 = 0dB, 00000 = mute */
#define FM_REG_VOLCONTROL		0x0F	/* Default: 0x11111 = 0dB */

#define FM_REG_BANDCFG3		0x19	/* Default: 0x00 */

/* Mute config */
#define FM_REG_MUTECFG0		0x1A	/* Default: 0x00 */
#define FM_REG_MUTECFG0		0x1A	/* Default: 0x00 */

#define FM_REG_FMSNR		0xE2	/* Default: 0x00 */
#define FM_REG_FMRSSI		0xE6	/* Default: 0x00 */

#define KT0935_ADDR           0x6A   /* KT0935R address */
#define I2C_TIMEOUT         ((uint32_t)0x3FFFF) /* I2C Time out */
#define KT0935_I2C_TIMING     0x1045061D
   
#define	KT0935_MIN_FREQ		3200	//x10KHz = 32MHz
#define	KT0935_MAX_FREQ		11000	//x10KHz = 110MHz

#define KT_REG_LIST_MAX 30

typedef struct {
	uint8_t reg;
	uint8_t value;
	uint8_t isWrite;
	uint8_t isRead;
	uint8_t isReadDone;
} KT_RW_Reg_t;

typedef struct {
	KT_RW_Reg_t List[KT_REG_LIST_MAX];
	uint8_t total;
} KT_Write_List_t;

typedef struct {
	KT_RW_Reg_t List[KT_REG_LIST_MAX];
	uint8_t total;
} KT_Read_List_t;


/* Exported macro ------------------------------------------------------------*/
/* Exported functions ------------------------------------------------------- */ 
void FM_DeInit(void);
void FM_Init(void);
void FM_Tick(void);

ErrStatus FM_GetStatus(void);
void KT093x_User_FMTune(uint16_t Frequency);

uint16_t FM_ReadReg(uint8_t RegName);
uint16_t FM_WriteReg(uint8_t RegName, uint8_t RegValue);

uint16_t FM_SetFMChanFreq(uint32_t freq);
uint16_t FM_SetVolume(uint8_t volume);
uint16_t FM_Standby(FM_OperateMode_t newMode);

uint8_t FM_GetVolume(void);
uint32_t FM_GetFMChanFreq(void);
uint8_t FM_GetSNR(void);
uint8_t FM_GetRSSI(void);

/** 
  * @brief  Timeout user callback function. This function is called when a timeout
  *         condition occurs during communication with KT0935. Only protoype
  *         of this function is decalred in KT0935 driver. Its implementation
  *         may be done into user application. This function may typically stop
  *         current operations and reset the I2C peripheral and KT0935.
  *         To enable this function use uncomment the define USE_TIMEOUT_USER_CALLBACK
  *         at the top of this file.          
  */
#ifdef USE_TIMEOUT_USER_CALLBACK 
 uint8_t KT0935_TIMEOUT_UserCallback(void);
#else
 #define KT0935_TIMEOUT_UserCallback()  KT0935_FAIL
#endif /* USE_TIMEOUT_USER_CALLBACK */
 
#ifdef __cplusplus
}
#endif

#endif /* __I2C_FM_H__ */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/

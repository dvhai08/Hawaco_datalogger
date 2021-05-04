/*----------------------------------------------------------------------------
 *      RL-ARM - FlashFS
 *----------------------------------------------------------------------------
 *      Name:    SPI_SDCard.c
 *      Purpose: Serial Peripheral Interface Driver for ST STM32F205
 *      Rev.:    V4.22
 *----------------------------------------------------------------------------
 *      This code is part of the RealView Run-Time Library.
 *      Copyright (c) 2004-2011 KEIL - An ARM Company. All rights reserved.
 *---------------------------------------------------------------------------*/

#include <File_Config.h>
#include <stm32f2xx.h>
#include "stm32f2xx_spi.h"
#include "Hardware.h"


/*----------------------------------------------------------------------------
  SPI Driver instance definition
   spi0_drv: First SPI driver
   spi1_drv: Second SPI driver
 *---------------------------------------------------------------------------*/

#define __DRV_ID  spi0_drv

/* SPI Driver Interface functions */
static BOOL Init (void);
static BOOL UnInit (void);
static U8   Send (U8 outb);
static BOOL SendBuf (U8 *buf, U32 sz);
static BOOL RecBuf (U8 *buf, U32 sz);
static BOOL BusSpeed (U32 kbaud);
static BOOL SetSS (U32 ss);
static U32  CheckMedia (void);        /* Optional function for SD card check */

/* SPI Device Driver Control Block */
SPI_DRV __DRV_ID = {
    Init,
    UnInit,
    Send,
    SendBuf,
    RecBuf,
    BusSpeed,
    SetSS,
    CheckMedia                          /* Can be NULL if not existing         */
};

/*--------------------------- Init ------------------------------------------*/

static BOOL Init (void) 
{
    /* Initialize and enable the SSP Interface module. */
    GPIO_InitTypeDef GPIO_InitStructure;
    SPI_InitTypeDef SPI_InitStructure;
    
    /* SPI GPIO Configuration --------------------------------------------------*/
    GPIO_PinAFConfig(SD_CARD_SCK_PORT, SD_CARD_SCK_PIN_SOURCE, GPIO_AF_SPI1);
    GPIO_PinAFConfig(SD_CARD_MOSI_PORT, SD_CARD_MOSI_PIN_SOURCE, GPIO_AF_SPI1);
    GPIO_PinAFConfig(SD_CARD_MISO_PORT, SD_CARD_MISO_PIN_SOURCE, GPIO_AF_SPI1);

    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_DOWN;

    /* SPI SCK pin configuration */
    GPIO_InitStructure.GPIO_Pin = SD_CARD_SCK_PIN;
    GPIO_Init(SD_CARD_SCK_PORT, &GPIO_InitStructure);

    /* SPI  MOSI pin configuration */
    GPIO_InitStructure.GPIO_Pin =  SD_CARD_MOSI_PIN;
    GPIO_Init(SD_CARD_MOSI_PORT, &GPIO_InitStructure);
    
	/* SPI  MISO pin configuration */
    GPIO_InitStructure.GPIO_Pin =  SD_CARD_MISO_PIN;
    GPIO_Init(SD_CARD_MISO_PORT, &GPIO_InitStructure);
    
	/* SPI  CS pin configuration */
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_NOPULL;
    
    GPIO_InitStructure.GPIO_Pin =  SD_CARD_CS_PIN;
    GPIO_Init(SD_CARD_CS_PORT, &GPIO_InitStructure);
    
    /* SPI  CD pin configuration */
    GPIO_InitStructure.GPIO_Pin =  SD_CARD_CD_PIN;
    GPIO_Init(SD_CARD_CD_PORT, &GPIO_InitStructure);
    GPIO_ResetBits(SD_CARD_CD_PORT, SD_CARD_CD_PIN);

    /* SPI configuration -------------------------------------------------------*/    
    SPI_InitStructure.SPI_Direction = SPI_Direction_2Lines_FullDuplex;
    SPI_InitStructure.SPI_Mode = SPI_Mode_Master;
    SPI_InitStructure.SPI_DataSize = SPI_DataSize_8b;
    SPI_InitStructure.SPI_CPOL = SPI_CPOL_Low;
    SPI_InitStructure.SPI_CPHA = SPI_CPHA_1Edge;
    SPI_InitStructure.SPI_NSS = SPI_NSS_Soft;
    SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_2;

    SPI_InitStructure.SPI_FirstBit = SPI_FirstBit_MSB;
    SPI_InitStructure.SPI_CRCPolynomial = 7;
    SPI_Init(SD_CARD_SPI, &SPI_InitStructure);
    
    SPI_Cmd(SD_CARD_SPI, ENABLE);
    
    return (__TRUE);
}


/*--------------------------- UnInit ----------------------------------------*/

static BOOL UnInit (void) 
{
    /* Return SSP interface to default state. */ 
    SPI_I2S_DeInit(SD_CARD_SPI);


    return (__TRUE);
}


/*--------------------------- Send ------------------------------------------*/

static U8 Send (U8 outb) 
{    
    while(SPI_I2S_GetFlagStatus(SD_CARD_SPI, SPI_I2S_FLAG_TXE) == RESET)
	{
	}

    SPI_I2S_SendData(SD_CARD_SPI, outb);

    while(SPI_I2S_GetFlagStatus(SD_CARD_SPI, SPI_I2S_FLAG_RXNE) == RESET)
	{
	}
    
    return SPI_I2S_ReceiveData(SD_CARD_SPI);
  
}


/*--------------------------- SendBuf ---------------------------------------*/

static BOOL SendBuf (U8 *buf, U32 sz) 
{
    /* Send buffer to SPI interface. */
    U32 i;
    
    for (i = 0; i < sz; i++) 
    {
		Send(buf[i]);
    }
            
    return (__TRUE);
}


/*--------------------------- RecBuf ----------------------------------------*/

static BOOL RecBuf (U8 *buf, U32 sz) 
{
    /* Receive SPI data to buffer. */
    U32 i;
    
    for (i = 0; i < sz; i++) 
    {        
        buf[i] = Send(0xFF);        
    }
    
    return (__TRUE);
}


/*--------------------------- BusSpeed --------------------------------------*/
static BOOL BusSpeed (U32 kbaud) 
{
	
    return (__TRUE);
}       
/*--------------------------- SetSS -----------------------------------------*/

static BOOL SetSS (U32 ss) 
{	    
    if(ss)
    {
        GPIO_SetBits(SD_CARD_CS_PORT,SD_CARD_CS_PIN);
    }
    else
    {
        GPIO_ResetBits(SD_CARD_CS_PORT,SD_CARD_CS_PIN);
    }
	    
    return (__TRUE);
}


/*--------------------------- CheckMedia ------------------------------------*/

static U32 CheckMedia (void) 
{
    /* Read CardDetect and WriteProtect SD card socket pins. */
    return M_INSERTED;
}

/*----------------------------------------------------------------------------
 * end of file
 *---------------------------------------------------------------------------*/

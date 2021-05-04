/******************************************************************************
 * @file    	TransferMessageLayer.c
 * @author  	Phinht
 * @version 	V1.0.0
 * @date    	03/03/2016
 * @brief   	
 ******************************************************************************/

/******************************************************************************
                                   INCLUDES					    			 
 ******************************************************************************/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "DataDefine.h"
#include "Transfer.h"
#include "Utilities.h"

/******************************************************************************
                                   GLOBAL VARIABLES					    			 
 ******************************************************************************/
extern uint8_t BLB_TxBuffer[];
	
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
/**
 * @brief	:   BLK_ValidMessage
 * @param	:   RFID message 
 * @retval	:   0 if valid, others : invalid
 * @author	:	Phinht
 * @created	:	04/03/2016
 * @version	:
 * @reviewer:	
 */
uint8_t BLK_ValidMessage(uint8_t *BLBMessage, uint8_t Length) 
{ 
	Int_t DataLength;
	uint16_t i;
	uint16_t ChecksumCalc = 0;
	Int_t ChecksumReal;
    
	if(Length < 14) return 1;
	
	if(BLBMessage[0] != '$' || BLBMessage[1] != 'B' || BLBMessage[2] != 'A' || 
			BLBMessage[3] != 'P' || BLBMessage[4] != 'C')
	{
			return 2;
	}
	
	DataLength.bytes[1] = BLBMessage[12];
	DataLength.bytes[0] = BLBMessage[13];
			
	for(i = 0; i < DataLength.value + 14; i++)
	{
			ChecksumCalc += BLBMessage[i];
	}
    
	//Kiem tra checksum
	ChecksumReal.bytes[1] = BLBMessage[DataLength.value + 14];
	ChecksumReal.bytes[0] = BLBMessage[DataLength.value + 15];
		
	if(ChecksumCalc != ChecksumReal.value) return 3;
  if(BLBMessage[i + 2] != '#') return 4;
    
  return 0;
}

/*
* Byte 3 : (Byte 0) + (Byte 1) * 2 + (Byte 2)*3
* Byte 4 : (Byte 1) + (Byte 0) * 2 + (Byte 2)*3
* Byte 5 : (Byte 2) + (Byte 1) * 2 + (Byte 0)*3 
*/
uint8_t BLK_ValidSecureCode(uint8_t *Buffer)
{
	uint8_t ViTri = 6;
	
	if(Buffer[ViTri + 3] != ((Buffer[ViTri] + Buffer[ViTri+1]*2 + Buffer[ViTri+2]*3) & 0xFF))
		return 1;
	if(Buffer[ViTri + 4] != ((Buffer[ViTri+1] + Buffer[ViTri]*2 + Buffer[ViTri+2]*3) & 0xFF))
		return 2;
	if(Buffer[ViTri + 5] != ((Buffer[ViTri+2] + Buffer[ViTri+1]*2 + Buffer[ViTri]*3) & 0xFF))
		return 3;
	
	return 0;
}

uint8_t BAPC_AddHeader(void)
{
	uint8_t Index = 0;
	
	BLB_TxBuffer[Index++] = '$';
	BLB_TxBuffer[Index++] = 'B';
	BLB_TxBuffer[Index++] = 'A';
	BLB_TxBuffer[Index++] = 'P';
	BLB_TxBuffer[Index++] = 'C';
	
	return Index;
}


uint8_t BAPC_AddSecureCode(uint8_t Index)
{
	uint8_t i;
	uint8_t Rand[3] = {0};
	
	for(i = 0; i < 3; i++)
	{
		Rand[i] = (uint8_t)(rand());	//Random 0 - 255
	}
	
	i = Index;
	BLB_TxBuffer[i++] = Rand[0];
	BLB_TxBuffer[i++] = Rand[1];
	BLB_TxBuffer[i++] = Rand[2];
	BLB_TxBuffer[i++] = Rand[0] + 2*Rand[1] + 3*Rand[2];
	BLB_TxBuffer[i++] = Rand[1] + 2*Rand[0] + 3*Rand[2];
	BLB_TxBuffer[i++] = Rand[2] + 2*Rand[1] + 3*Rand[0];
	
	return i;
}

/********************************* END OF FILE *******************************/

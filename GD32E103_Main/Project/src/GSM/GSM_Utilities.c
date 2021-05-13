/******************************************************************************
 * @file    	GSM_Utilities.c
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
#include <stdlib.h>
#include "GSM.h"
#include "Utilities.h"
#include "DataDefine.h"

/******************************************************************************
                                   GLOBAL VARIABLES					    			 
 ******************************************************************************/
extern System_t xSystem;
extern GSM_Manager_t	GSM_Manager;

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
 * @brief	:  Lay thong tin IMEI
 * @param	:  
 * @retval	:
 * @author	:	
 * @created	:	15/01/2014
 * @version	:
 * @reviewer:	
 */
void GSM_GetIMEI(uint8_t LoaiIMEI, uint8_t *IMEI_Buffer)
{
    uint8_t Count = 0;
    uint8_t tmpCount = 0;

    for(Count = 0; Count < strlen((char*) IMEI_Buffer); Count++)
    {
        if(IMEI_Buffer[Count] >= '0' && IMEI_Buffer[Count] <= '9')
        {
            if(LoaiIMEI == GSMIMEI)
            {
                xSystem.Parameters.GSM_IMEI[tmpCount++] = IMEI_Buffer[Count];
            }
            else
            {
                xSystem.Parameters.SIM_IMEI[tmpCount++] = IMEI_Buffer[Count];
            }
        }
        if(tmpCount >= 20) break;
    }

    if(LoaiIMEI == GSMIMEI)
    {
        xSystem.Parameters.GSM_IMEI[tmpCount] = 0;
    }
    else
    {
        xSystem.Parameters.SIM_IMEI[tmpCount] = 0;
    }
}

/*****************************************************************************/
/**
 * @brief	:  
 * @param	:  
 * @retval	:
 * @author	:	
 * @created	:	15/10/2015
 * @version	:
 * @reviewer:	
 */
void GSM_GetSignalStrength(uint8_t *Buffer)
{	
	char *tempBuff = strstr((char *)Buffer, "+CSQ:");
	
	if(tempBuff == NULL) return;	
	xSystem.Status.CSQ = GetNumberFromString(6, tempBuff);
	GSM_Manager.GSMReady = 1;
}

#if __USE_APN_CONFIG__
/*
* Get APN name only, w/o username & password, ex: "v-internet"
*/
void GSM_GetShortAPN(char *ShortAPN)
{
	uint8_t i = 0;
	
	while(xSystem.Parameters.APNConfig[i] && i < 20)
	{
		if(xSystem.Parameters.APNConfig[i] == ',')
			break;
		i++;
	}
	
	if(i > 2)
	{
		memcpy(ShortAPN, xSystem.Parameters.APNConfig, i);
	}
}
#endif

/*
+CUSD: 1,"84353078550. TKG: 0d, dung den 0h ngay 18/02/2020. Bam chon dang ky:1. 15K=3GB/3ngay2. 30K=7GB/7ngayHoac bam goi *098#",15
OK
hoac
+CUSD: 4
+CME ERROR: unknown
*/
void GSM_ProcessCUSDMessage(char* buffer)
{
#if 0
	uint8_t sizeBuff = sizeof(xSystem.Status.GSMBalance);
	memset(xSystem.Status.GSMBalance, 0, sizeBuff);
	
	char *cusd = strstr(buffer, "+CUSD");
	if(cusd)
	{
		uint16_t cusdLeng = strlen(cusd);
		if(cusdLeng >= sizeBuff) cusdLeng = sizeBuff - 1;
	
		memcpy(xSystem.Status.GSMBalance, cusd, cusdLeng);
		GSM_Manager.GSMReady = 1;
	}
#endif
}

/*****************************************************************************/
/**
 * @brief	:  
 * @param	:  +CGREG: 2,1,"3279","487BD01",7
 * @retval	:
 * @author	:	Phinht
 * @created	:	15/10/2015
 * @version	:
 * @reviewer:	
 */
void GSM_GetNetworkStatus(char *Buffer)
{
	/**
	* +CGREG: 2,1,"3279","487BD01",7
	*
	* OK
	*/
	char *tempBuff = strstr(Buffer, "+CGREG:");
	if(tempBuff == NULL) return;	
	
	uint8_t commaIndex[10] = {0};
	uint8_t index = 0;
	for(uint8_t i = 0; i < strlen(tempBuff); i++)
	{
		if(tempBuff[i] == ',') commaIndex[index++] = i;
	}
	if(index >= 4)
	{
		GSM_Manager.AccessTechnology = GetNumberFromString(commaIndex[3] + 1, tempBuff);
		GSM_Manager.GSMReady = 1;
		
		if(GSM_Manager.AccessTechnology > 9) GSM_Manager.AccessTechnology = 9;
//		DEBUG ("\r\nNetwork status: %s - %u", tempBuff, GSM_Manager.AccessTechnology);
	}
}


/*****************************************************************************/
/**
 * @brief	:  
 * @param	:  +COPS: 0,0,"Viettel Viettel",7
 * @retval	:
 * @author	:	Phinht
 * @created	:	15/10/2015
 * @version	:
 * @reviewer:	
 */
void GSM_GetNetworkOperator(char *Buffer)
{
	/**
	* AT+COPS=? 
	* +COPS: (2,"Viettel","Viettel","45204",7),(1,"VIETTEL","VIETTEL","45204",2),(1,"VIETTEL","VIETTEL","45204",0),
	* (1,"Vietnamobile","VNMOBILE","45205",2),(1,"VN VINAPHONE","GPC","45202",2),(1,"Vietnamobile","VNMOBILE","45205",0),
	* (1,"VN Mobifone","Mobifone","45201",7),(1,"VN VINAPHONE","GPC","45202",0),(1,"VN Mobifone","Mobifone","45201",0),
	* (1,"VN VINAPHONE","GPC","45202",7),(1,"VN Mobifone","Mobifone","45201",2),,(0-4),(0-2)
	*
	* Query selected operator: 
	* AT+COPS?
	* +COPS: 0,0,"Viettel Viettel",7
	*
	* OK
	*/
#if 0
	char *tempBuff = strstr(Buffer, "+COPS:");
	if(tempBuff == NULL) return;	
	
	uint8_t commaIndex[5] = {0};
	uint8_t index = 0;
	for(uint8_t i = 0; i < strlen(tempBuff); i++)
	{
		if(tempBuff[i] == '"') commaIndex[index++] = i;
	}
	if(index >= 2)
	{
		uint8_t opLength = sizeof(xSystem.Status.GSMOperator);
		uint8_t length = commaIndex[1] - commaIndex[0];
		if(length > opLength) length = opLength;
		
		//Copy operator name
		memset(xSystem.Status.GSMOperator, 0, sizeof(xSystem.Status.GSMOperator));
		memcpy(xSystem.Status.GSMOperator, &tempBuff[commaIndex[0] + 1], length - 1);
		
		DEBUG ("\r\nOperator: %s", xSystem.Status.GSMOperator);
	}
#endif
}

/********************************* END OF FILE *******************************/

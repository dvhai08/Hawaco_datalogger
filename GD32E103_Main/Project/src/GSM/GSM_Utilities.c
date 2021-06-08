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
#include "gsm.h"
#include "gsm_utilities.h"
#include "DataDefine.h"

/******************************************************************************
                                   GLOBAL VARIABLES					    			 
 ******************************************************************************/
extern System_t xSystem;
extern GSM_Manager_t gsm_manager;

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
void gsm_get_imei(uint8_t LoaiIMEI, uint8_t *IMEI_buffer)
{
    uint8_t Count = 0;
    uint8_t tmp_cnt = 0;

    for (Count = 0; Count < strlen((char *)IMEI_buffer); Count++)
    {
        if (IMEI_buffer[Count] >= '0' && IMEI_buffer[Count] <= '9')
        {
            if (LoaiIMEI == GSMIMEI)
            {
                xSystem.Parameters.GSM_IMEI[tmp_cnt++] = IMEI_buffer[Count];
            }
            else
            {
                xSystem.Parameters.SIM_IMEI[tmp_cnt++] = IMEI_buffer[Count];
            }
        }
        if (tmp_cnt >= 20)
            break;
    }

    if (LoaiIMEI == GSMIMEI)
    {
        xSystem.Parameters.GSM_IMEI[tmp_cnt] = 0;
    }
    else
    {
        xSystem.Parameters.SIM_IMEI[tmp_cnt] = 0;
    }
}

bool gsm_utilities_get_signal_strength_from_buffer(uint8_t *buffer, uint8_t *csq)
{
    char *tmp_buff = strstr((char *)buffer, "+CSQ:");

    if (tmp_buff == NULL)
    {
        return false;
    }

    *csq = gsm_utilities_get_number_from_string(6, tmp_buff);
    return true;
}

#if __USE_APN_CONFIG__
/*
* Get APN name only, w/o username & password, ex: "v-internet"
*/
void gsm_get_short_apn(char *ShortAPN)
{
    uint8_t i = 0;

    while (xSystem.Parameters.APNConfig[i] && i < 20)
    {
        if (xSystem.Parameters.APNConfig[i] == ',')
            break;
        i++;
    }

    if (i > 2)
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
void gsm_process_cusd_message(char *buffer)
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
		gsm_manager.GSMReady = 1;
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
void gsm_get_network_status(char *buffer)
{
    /**
	* +CGREG: 2,1,"3279","487BD01",7
	*
	* OK
	*/
    char *tmp_buff = strstr(buffer, "+CGREG:");
    if (tmp_buff == NULL)
        return;

    uint8_t commaIndex[10] = {0};
    uint8_t index = 0;
    for (uint8_t i = 0; i < strlen(tmp_buff); i++)
    {
        if (tmp_buff[i] == ',')
            commaIndex[index++] = i;
    }
    if (index >= 4)
    {
        gsm_manager.AccessTechnology = gsm_utilities_get_number_from_string(commaIndex[3] + 1, tmp_buff);
        gsm_manager.GSMReady = 1;

        if (gsm_manager.AccessTechnology > 9)
            gsm_manager.AccessTechnology = 9;
        //		DEBUG ("\r\nNetwork status: %s - %u", tmp_buff, gsm_manager.AccessTechnology);
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
void gsm_get_network_operator(char *buffer)
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
	char *tmp_buff = strstr(buffer, "+COPS:");
	if(tmp_buff == NULL) return;	
	
	uint8_t commaIndex[5] = {0};
	uint8_t index = 0;
	for(uint8_t i = 0; i < strlen(tmp_buff); i++)
	{
		if(tmp_buff[i] == '"') commaIndex[index++] = i;
	}
	if(index >= 2)
	{
		uint8_t opLength = sizeof(xSystem.Status.GSMOperator);
		uint8_t length = commaIndex[1] - commaIndex[0];
		if(length > opLength) length = opLength;
		
		//Copy operator name
		memset(xSystem.Status.GSMOperator, 0, sizeof(xSystem.Status.GSMOperator));
		memcpy(xSystem.Status.GSMOperator, &tmp_buff[commaIndex[0] + 1], length - 1);
		
		DEBUG ("\r\nOperator: %s", xSystem.Status.GSMOperator);
	}
#endif
}

// +HTTPACTION: 0,200,12314\r\n
bool gsm_utilities_parse_http_action_response(char *response, uint32_t *error_code, uint32_t *content_length)
{
    bool retval = false;
    char tmp[32];
    char *p;
#if 0 // SIMCOM
    p = strstr(response, "+HTTPACTION: 0,200,");
    if (p)
    {
        p += strlen("+HTTPACTION: 0,200,");
        for (uint32_t i = 0; i < (sizeof(tmp) - 1); i++, p++)
        {
            if (*p != '\r')
            {
                tmp[i] = *p;
            }
            else
            {
                tmp[i] = '\0';
                break;
            }
        }

        *content_length = atoi(tmp);
        *error_code = 200;
        retval = true;
    }
    else
    {
        // TODO parse error code
        retval = false;
    }
#else // Quectel
    p = strstr(response, "+QHTTPGET: 0,200,");
    if (p)
    {
        p += strlen("+QHTTPGET: 0,200,");
        for (uint32_t i = 0; i < (sizeof(tmp) - 1); i++, p++)
        {
            if (*p != '\r')
            {
                tmp[i] = *p;
            }
            else
            {
                tmp[i] = '\0';
                break;
            }
        }

        *content_length = atoi(tmp);
        *error_code = 200;
        retval = true;
    }
    else
    {
        // TODO parse error code
        retval = false;
    }
#endif
    return retval;
}

int32_t gsm_utilities_parse_httpread_msg(char *buffer, uint8_t **begin_data_pointer)
{
#if 0 // SIMCOM
    // +HTTPREAD: 123\r\nData
    char tmp[32];
    char *p = strstr(buffer, "+HTTPREAD: ");
    if (p == NULL)
    {
        return -1;
    }

    p += strlen("+HTTPREAD: ");
    if (strstr(p, "\r\n") == NULL)
    {
        return -1;
    }

    for (uint32_t i = 0; i < (sizeof(tmp) - 1); i++, p++)
    {
        if (*p != '\r')
        {
            tmp[i] = *p;
        }
        else
        {
            tmp[i] = '\0';
            break;
        }
    }
    p += 2; // Skip \r\n
    *begin_data_pointer = (uint8_t*)p;

    return atoi(tmp);
#else // Quectel
    char tmp[32];
    char *p = strstr(buffer, "CONNECT\r\n");
    if (p == NULL)
    {
        return -1;
    }

    p += strlen("CONNECT\r\n");
    if (strstr(p, "OK\r\n") == NULL)
    {
        return -1;
    }

    for (uint32_t i = 0; i < (sizeof(tmp) - 1); i++, p++)
    {
        if (*p != '\r')
        {
            tmp[i] = *p;
        }
        else
        {
            tmp[i] = '\0';
            break;
        }
    }
    p += 2; // Skip \r\n
    *begin_data_pointer = (uint8_t *)p;

    return atoi(tmp);
#endif
}

/*
 * 	Ham doc mot so trong chuoi bat dau tu dia chi nao do.
 *	Buffer = abc124mff thi gsm_utilities_get_number_from_string(3,Buffer) = 123
 *
 */
uint32_t gsm_utilities_get_number_from_string(uint16_t begin_index, char *buffer)
{
    // assert(buffer);

    uint32_t value = 0;
    uint16_t tmp = begin_index;
    uint32_t len = strlen(buffer);
    while (buffer[tmp] && tmp < len)
    {
        if (buffer[tmp] >= '0' && buffer[tmp] <= '9')
        {
            value *= 10;
            value += buffer[tmp] - 48;
        }
        else
        {
            break;
        }
        tmp++;
    }

    return value;
}

int32_t find_index_of_char(char char_to_find, char *buffer_to_find)
{
    uint32_t tmp_cnt = 0;
    uint32_t max_length = 0;

    /* Do dai du lieu */
    max_length = strlen(buffer_to_find);

    for (tmp_cnt = 0; tmp_cnt < max_length; tmp_cnt++)
    {
        if (buffer_to_find[tmp_cnt] == char_to_find)
        {
            return tmp_cnt;
        }
    }
    return -1;
}

bool gsm_utilities_copy_parameters(char *src, char *dst, char comma_begin, char comma_end)
{
    int16_t begin_idx = find_index_of_char(comma_begin, src);
    int16_t end_idx = find_index_of_char(comma_end, src);
    int16_t tmp_cnt, i = 0;

    if (begin_idx == -1 || end_idx == -1)
    {
        return false;
    }

    if (end_idx - begin_idx <= 1)
    {
        return false;
    }

    for (tmp_cnt = begin_idx + 1; tmp_cnt < end_idx; tmp_cnt++)
    {
        dst[i++] = src[tmp_cnt];
    }

    dst[i] = 0;

    return true;
}

bool gsm_utilities_parse_timestamp_buffer(char *response_buffer, date_time_t *date_time)
{
    // Parse response buffer
    // "\r\n+CCLK : "yy/MM/dd,hh:mm:ss+zz"\r\n\r\nOK\r\n        zz : timezone
    // \r\n+CCLK : "10/05/06,00:01:52+08"\r\n\r\nOK\r\n
    bool val = false;
    uint8_t tmp[4];
    char *p_index = strstr(response_buffer, "+CCLK");
    if (p_index == NULL)
    {
        goto exit;
    }

    while (*p_index && ((*p_index) != '"'))
    {
        p_index++;
    }
    if (*p_index == '\0')
    {
        goto exit;
    }
    p_index++;
    response_buffer = p_index;

    memset(tmp, 0, sizeof(tmp));
    memcpy(tmp, response_buffer, 2);
    date_time->year = atoi((char*)tmp);
    if (date_time->year < 20) // 2020
    {
        // Invalid timestamp
        val = false;
    }
    else
    {
        // MM
        memset(tmp, 0, sizeof(tmp));
        memcpy(tmp, response_buffer + 3, 2);
        date_time->month = atoi((char*)tmp);

        // dd
        memset(tmp, 0, sizeof(tmp));
        memcpy(tmp, response_buffer + 6, 2);
        date_time->day = atoi((char*)tmp);

        // hh
        memset(tmp, 0, sizeof(tmp));
        memcpy(tmp, response_buffer + 9, 2);
        date_time->hour = atoi((char*)tmp);

        // mm
        memset(tmp, 0, sizeof(tmp));
        memcpy(tmp, response_buffer + 12, 2);
        date_time->minute = atoi((char*)tmp);

        // ss
        memset(tmp, 0, sizeof(tmp));
        memcpy(tmp, response_buffer + 15, 2);
        date_time->second = atoi((char*)tmp);

        val = true;
    }

exit:
    return val;
}

#include "gsm.h"
#include "gsm_utilities.h"
#include "hardware.h"
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include "app_debug.h"
#include "main.h"

static gsm_sms_msg_t m_sms_memory[1];

gsm_sms_msg_t *gsm_get_sms_memory_buffer(void)
{
    return m_sms_memory;
}

uint32_t gsm_get_max_sms_memory_buffer(void)
{
    return sizeof(m_sms_memory)/sizeof(gsm_sms_msg_t);
}


//static void upcase_data(char *buffer_data)
//{
//    uint16_t i;
//    i = 0;

//    while (buffer_data[i] && i < 1024)
//    {
//        if (buffer_data[i] >= 97 && buffer_data[i] <= 122)
//        {
//            buffer_data[i] = buffer_data[i] - 32;
//        }
//        i++;
//    }
//}

static bool split_phone_number_from_incomming_message(char *buffer, char *phone_number)
{
    uint16_t i = 0;
    uint16_t j = 0;
    int16_t idx = 0;

    /* Find index of phone number in buffer */
    i = 0;
    idx = -1;
    while (buffer[i] && i < 255)
    {
        if (++i > 3)
        {
            if (buffer[i] == '"' && buffer[i - 1] == ',' && buffer[i - 2] == '"')
            {
                idx = i + 1;
                break;
            }
        }
    }

    /* Copy phone number */
    j = 0;
    if (idx > -1)
    {
        for (i = 0; i < 20; i++)
        {
            if (buffer[idx + i] != '"')
            {
                phone_number[j++] = buffer[idx + i];
            }
            else
            {
                break;
            }
        }
    }

    if (strlen(phone_number) < 7)
    {
        return false;
    }

    return true;
}


static void copy_sms_content(char *buffer, char *content, uint32_t max_size)
{
    // +CMGR: "REC UNREAD","+84942018895","","21/01/19,15:46:46+28"<CF><LF>
    //#SIM#12345678#0942018895#<CR><LF>
    //OK
    // TODO check input params
    char *p_begin = strstr(buffer, "+CMGR");
    if (p_begin == NULL)
    {
        DEBUG_PRINTF("Invalid sms format\r\n");
        return;
    }
    p_begin += strlen("+CMGR");

    p_begin = strstr(p_begin, "\r\n");
    if (p_begin == NULL)
    {
        DEBUG_PRINTF("Invalid sms format\r\n");
        return;
    }
    p_begin += 2;  // 2 = len(CRLF)
    buffer = p_begin;       // 2 = len(CRLF)
    char *p_end = strstr(buffer, "\r\nOK");
    if (p_end == NULL)
    {
        DEBUG_PRINTF("Invalid sms format\r\n");
        return;
    }

    memcpy(content, p_begin, ((p_end-p_begin) <= max_size) ? (p_end-p_begin) : max_size);
    return;
}


bool gsm_send_sms(char *phone_number, char *message)
{
    uint8_t cnt = 0;

    /* Check message length */
    if (strlen(message) >= 160)
    {
        DEBUG_PRINTF("SMS message too long %d\r\n", strlen(message));
        return false;
    }

    if ((strlen(phone_number) >= 15) || (strlen(phone_number) == 0))
    {
        DEBUG_PRINTF("SMS phone number [%s] is invalid\r\n", phone_number);
        return false;
    }

    /* Find an empty buffer */
    for (cnt = 0; cnt < sizeof(m_sms_memory)/sizeof(gsm_sms_msg_t); cnt++)
    {
        if (m_sms_memory[cnt].need_to_send != 0)
            continue;

        memset(m_sms_memory[cnt].phone_number, 0, sizeof(((gsm_sms_msg_t*)0)->phone_number));
        memset(m_sms_memory[cnt].message, 0, sizeof(((gsm_sms_msg_t*)0)->message));

        strcpy(m_sms_memory[cnt].phone_number, phone_number);
        strcpy(m_sms_memory[cnt].message, message);

        m_sms_memory[cnt].need_to_send = 1;
        m_sms_memory[cnt].retry_count = 0;

        DEBUG_PRINTF("Add sms message into buffer %u : %s, phone : %s\r\n", cnt, message, phone_number);

        return true;
    }

    DEBUG_PRINTF("SMS buffer full\r\n");

    return false;
}

void gsm_sms_layer_process_cmd(char *buffer)
{
    DEBUG_PRINTF("SMS cmd %s\r\n", buffer);
    char sms_content[160];
    char phone_number[20];

    memset(sms_content, 0, sizeof(sms_content));
    memset(phone_number, 0, sizeof(phone_number));

    if (split_phone_number_from_incomming_message(buffer, phone_number) == 0)
    {
        DEBUG_PRINTF("Cannot get phone number from incomming msg\r\n");
        return;
    }

    memset(sms_content, 0, sizeof(sms_content));
    copy_sms_content(buffer, sms_content, sizeof(sms_content) - 1);     // 1 for null pointer

    //Gan lai noi dung
    buffer = sms_content;

//    // Send msg to server
//    gsm_message_send_sms_to_server(phone_number, sms_content);

    DEBUG_PRINTF("New sms from %s, content : %s\r\n", phone_number, sms_content);
}


//static bool send_sms(char *phone_number, char *message)
//{
//    uint8_t count = 0;

//    /* Kiem tra dieu kien ve do dai */
//    if (strlen(message) >= sizeof(((gsm_sms_msg_t*)0)->message))
//    {
//        DEBUG_PRINTF("Message content is too long\r\n");
//        return false;
//    }

//    if ((strlen(phone_number) >= 15) || (strlen(phone_number) == 0))
//    {
//        DEBUG_PRINTF("Phone number invalid\r\n");
//        return false;
//    }

//    /* Find empty buffer */
//    for (count = 0; count < sizeof(m_sms_memory)/sizeof(gsm_sms_msg_t); count++)
//    {
//        if (m_sms_memory[count].need_to_send == 0)
//        {
//            memset(m_sms_memory[count].phone_number, 0, sizeof(((gsm_sms_msg_t*)0)->phone_number));
//            memset(m_sms_memory[count].message, 0, sizeof(((gsm_sms_msg_t*)0)->message));

//            strcpy(m_sms_memory[count].phone_number, phone_number);
//            strcpy(m_sms_memory[count].message, message);

//            m_sms_memory[count].need_to_send = 1;
//            m_sms_memory[count].retry_count = 0;

//            DEBUG_PRINTF("Add new sms into buffer %u : %s, phone : %s\r\n", count, message, phone_number);

//            return true;
//        }
//    }

//    return false;
//}

static uint8_t gsm_read_sms_step = 0;
static uint8_t m_sms_read_at_cmd_buffer[64];
static void gsm_at_cb_read_sms(gsm_response_event_t event, void *resp_buffer)
{
    static uint8_t tmp_num = 0xFF;

    if ((resp_buffer && strstr(resp_buffer, "REC UNREAD")) 
        || strstr(resp_buffer, "REC READ"))
    {
        gsm_read_sms_step = 2;
    }

    switch (gsm_read_sms_step)
    {
    case 0:
    case 1:
        if (tmp_num == 0xFF)
            tmp_num = 1;
        else
            tmp_num++;

        if (tmp_num > 10)
        {
            DEBUG_PRINTF("Cannot read SMS\r\n");
            gsm_change_state(GSM_STATE_OK);
            tmp_num = 0xFF;
            return;
        }

        sprintf((char*)m_sms_read_at_cmd_buffer, "AT+CMGR=%u\r\n", tmp_num);
        gsm_hw_send_at_cmd((char*)m_sms_read_at_cmd_buffer, 
                            "REC UNREAD", 
                            "OK\r\n",
                            1000, 
                            1, 
                            gsm_at_cb_read_sms);
        break;

    case 2:
        if (event == GSM_EVENT_OK)
        {
            gsm_sms_layer_process_cmd(resp_buffer);
        }
        else
        {
            DEBUG_PRINTF("Cannot read sms from storage\r\n");
        }

        /* Delete all SMS */
        //gsm_hw_send_at_cmd("AT+CMGD=1,4\r\n", 
        //                    "OK\r\n", 
        //                    NULL,
        //                    3000, 
        //                    10, 
        //                    gsm_at_cb_read_sms);
        gsm_hw_send_at_cmd("AT\r\n", 
                            "OK\r\n", 
                            NULL,
                            3000, 
                            10, 
                            gsm_at_cb_read_sms);
        tmp_num = 0xFF;
        break;

    case 3:
        if (event == GSM_EVENT_OK)
        {
            DEBUG_PRINTF("Message deleted\r\n");
        }
        else
        {
            DEBUG_PRINTF("Cannot delete sms\r\n");
        }
        gsm_read_sms_step = 0;
        gsm_change_state(GSM_STATE_OK);
        return;

    default:
        DEBUG_PRINTF("[%s] Unhandled switch case\r\n", __FUNCTION__);
        break;
    }

    gsm_read_sms_step++;
}

void gsm_enter_read_sms(void)
{
    gsm_hw_send_at_cmd("ATV1\r\n", "OK\r\n", "", 1000, 1, gsm_at_cb_read_sms);
}

#include "gsm.h"
#include "gsm_http.h"
#include "gsm_utilities.h"
#include "app_debug.h"
#include <stdio.h>
#include <string.h>
#include "gsm.h"

#define DEBUG_HTTP                  1
#ifdef DEBUG_HTTP
static uint32_t m_debug_http_post_count = 0;
#define DEBUG_HTTP_POST_INC()           (m_debug_http_post_count++)
#define DEBUG_HTTP_POST_GET_COUNT()     (m_debug_http_post_count)
#else
#define DEBUG_HTTP_POST_INC()
#define DEBUG_HTTP_POST_GET_COUNT()     (0)
#endif 

#define OTA_FILE_NAME        "UFS:ota.bin"

static gsm_http_config_t m_http_cfg;
static uint8_t m_http_step = 0;
static uint32_t m_total_bytes_recv = 0;
static uint32_t m_content_length = 0;
static char m_http_cmd_buffer[256];
static gsm_http_data_t post_rx_data;
static bool m_renew_config_ssl = true;
static bool m_renew_apn = true;
static uint32_t m_start_download_timestamp;

static void gsm_http_query(gsm_response_event_t event, void *response_buffer);
static int32_t m_http_read_big_file_step = 0;
static int32_t m_ssl_step = -1;
static int32_t m_file_handle = -1;
static void setup_http_ssl(gsm_response_event_t event, void *response_buffer)
{
    if (!m_renew_config_ssl)
    {
        gsm_hw_send_at_cmd("AT\r\n", 
                    "OK\r\n", 
                    "", 
                    1000, 
                    1, 
                    gsm_http_query);
        return;
    }

    switch (m_ssl_step)
    {
        case 0: // Set ssl
        {
            DEBUG_PRINTF("Enter setup ssl\n", 
                        (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]", 
                        (char*)response_buffer);

            gsm_hw_send_at_cmd("AT+QHTTPCFG=\"sslctxid\",1\r\n", 
                                "OK\r\n", 
                                "", 
                                2000, 
                                1, 
                                setup_http_ssl);
        }
            break;
        
        case 1:
        {
            DEBUG_PRINTF("Set ssl config: %s, response %s\r\n", 
                        (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]", 
                        (char*)response_buffer);

            gsm_hw_send_at_cmd("AT+QSSLCFG=\"sslversion\",1,4\r\n", 
                                "OK\r\n", 
                                "", 
                                1000, 
                                1, 
                                setup_http_ssl);

            
        }
            break;

        case 2:
        {
            DEBUG_PRINTF("Set the SSL version: %s, response %s\r\n", 
                        (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]", 
                        (char*)response_buffer);

            gsm_hw_send_at_cmd("AT+QSSLCFG=\"ciphersuite\",1,0XFFFF\r\n", 
                                "OK\r\n", 
                                "", 
                                1000, 
                                1, 
                                setup_http_ssl);
        }
            break;


        case 3:
        {
            DEBUG_PRINTF("Set the SSL ciphersuite: %s, response %s\r\n", 
                        (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]", 
                        (char*)response_buffer);

            gsm_hw_send_at_cmd("AT+QSSLCFG=\"seclevel\",1,0\r\n", 
                                "OK\r\n", 
                                "", 
                                1000, 
                                1, 
                                setup_http_ssl);
        }
            break;
            
        case 4:
        {
            DEBUG_PRINTF("SSL level: %s, response %s\r\n", 
                        (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]", 
                        (char*)response_buffer);
            if (1)
            {
                gsm_hw_send_at_cmd("AT\r\n", 
                                    "OK\r\n", 
                                    "", 
                                    1000, 
                                    1, 
                                    gsm_http_query);
                m_renew_config_ssl = false;
            }
            else
            {

            }
            m_ssl_step = -1;
        }   
            break;

        default:
            break;
    }

    m_ssl_step++;
}


void gsm_http_download_big_file(gsm_response_event_t event, void *response_buffer)
{
    DEBUG_PRINTF("Download big file step %u, result %s\r\n", 
						m_http_read_big_file_step,
                        (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]", 
                        (char*)response_buffer);
    if (event != GSM_EVENT_OK)
    {
        if (m_http_cfg.on_event_cb)
        {
            m_total_bytes_recv = 0;
            m_http_read_big_file_step = 0;
			m_file_handle = -1;
            m_http_cfg.on_event_cb(GSM_HTTP_GET_EVENT_FINISH_FAILED, &m_total_bytes_recv);
            return;
        }
    }

    if (m_http_read_big_file_step == 0)
    {
        uint32_t download_time = gsm_get_current_tick() - m_start_download_timestamp;
        DEBUG_PRINTF("Speed %ukb/s, download time %ums\r\n", (m_content_length*1000/1024)/download_time, download_time);
        sprintf(m_http_cmd_buffer, "AT+QFOPEN=\"%s\",2\r\n", OTA_FILE_NAME);
        gsm_hw_send_at_cmd(m_http_cmd_buffer, 
                            "+QFOPEN:", 
                            "OK\r\n", 
                            2000, 
                            1, 
                            gsm_http_download_big_file);
    }
    else if (m_http_read_big_file_step == 1)     // Seek file
    {
		gsm_utilities_parse_file_handle((char*)response_buffer, &m_file_handle);
        DEBUG_PRINTF("File handle %s\r\n", (char*)response_buffer);
		
		if (m_file_handle != -1)
		{
			sprintf(m_http_cmd_buffer, "AT+QFSEEK=%u,%u,0\r\n", m_file_handle, m_total_bytes_recv);
			gsm_hw_send_at_cmd(m_http_cmd_buffer, 
								"OK\r\n",
								"",
								2000, 
								1, 
								gsm_http_download_big_file);
		}
		else
		{
			m_total_bytes_recv = 0;
            m_http_read_big_file_step = 0;
			m_file_handle = -1;
            if (m_http_cfg.on_event_cb) 
                m_http_cfg.on_event_cb(GSM_HTTP_GET_EVENT_FINISH_FAILED, &m_total_bytes_recv);
            return;
		}
    }
    else if (m_http_read_big_file_step == 2)    // Read file
    {
		sprintf(m_http_cmd_buffer, "AT+QFREAD=%u,%u\r\n", m_file_handle, 128);
        gsm_hw_send_at_cmd(m_http_cmd_buffer, 
                            "CONNECT ",
                            "OK\r\n",
                            2000, 
                            1, 
                            gsm_http_download_big_file);
    }
    else if(m_http_read_big_file_step == 3)
    {
		uint8_t *content;
		uint32_t size;
		gsm_utilities_get_qfile_content(response_buffer, &content, &size);
			
		DEBUG_PRINTF("Data size %u bytes\r\n", size);
        post_rx_data.action = m_http_cfg.action;
        post_rx_data.data_length = size;
        post_rx_data.data = (uint8_t*)content;

        m_http_cfg.on_event_cb(GSM_HTTP_GET_EVENT_DATA, &post_rx_data);
        
        m_total_bytes_recv += size;
        if (m_total_bytes_recv >= m_content_length)
        {
			DEBUG_VERBOSE("Closing file\r\n");
			sprintf(m_http_cmd_buffer, "AT+QFCLOSE=%u\r\n", m_file_handle);
			gsm_hw_send_at_cmd(m_http_cmd_buffer, 
								"OK\r\n",
								"",
								2000, 
								1, 
								gsm_http_download_big_file);
        }
		else
		{
			m_http_read_big_file_step = 2;
			sprintf(m_http_cmd_buffer, "AT+QFREAD=%u,%u\r\n", m_file_handle, 128);
			gsm_hw_send_at_cmd(m_http_cmd_buffer, 
								"CONNECT",
								"OK\r\n",
								2000, 
								1, 
								gsm_http_download_big_file);
    
		}
    }
	else if (m_http_read_big_file_step == 4)
	{
		DEBUG_VERBOSE("All data received\r\n");
		m_http_cfg.on_event_cb(GSM_HTTP_GET_EVENT_FINISH_SUCCESS, &m_total_bytes_recv);
		return;
	}

    m_http_read_big_file_step++;
}


void gsm_http_close_on_error(void)
{
    // Goto case close http session
    m_ssl_step = 0;
    m_total_bytes_recv = 0;
    m_renew_config_ssl = 0;
    if (m_http_cfg.on_event_cb)
    {
        m_http_cfg.on_event_cb(GSM_HTTP_GET_EVENT_FINISH_FAILED, &m_total_bytes_recv);
    }
}

void gsm_http_query(gsm_response_event_t event, void *response_buffer)
{
    //sys_ctx_t *ctx = sys_ctx();

    switch (m_http_step)
    {
        case 0:     // Config bearer profile
        {
            if (m_http_cfg.on_event_cb)
            {
                m_http_cfg.on_event_cb(GSM_HTTP_EVENT_START, NULL);
            }

//            DEBUG_VERBOSE("Set PDP context as 1\r\n");
            if (m_renew_apn)
            {
                gsm_hw_send_at_cmd("AT+QHTTPCFG=\"contextid\",1\r\n", 
                                    "OK\r\n", 
                                    "", 
                                    5000, 
                                    1, 
                                    gsm_http_query);
            }
            else
            {
                gsm_hw_send_at_cmd("AT\r\n", 
                                    "OK\r\n", 
                                    "", 
                                    5000, 
                                    2, 
                                    gsm_http_query);
            }
        }
        break;

        case 1: // Allow to output HTTP response header
        {
            DEBUG_PRINTF("Set PDP context as 1 : %s, response %s\r\n",
                         (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]",
                         (char*)response_buffer);
            if (m_http_cfg.action == GSM_HTTP_ACTION_GET)
            {
                gsm_hw_send_at_cmd("AT+QHTTPCFG=\"responseheader\",0\r\n", 
                                    "OK\r\n", 
                                    "", 
                                    5000, 
                                    1, 
                                    gsm_http_query);
            }
            else
            {
                DEBUG_HTTP_POST_INC();
//                DEBUG_VERBOSE("Post total %u msg\r\n", DEBUG_HTTP_POST_GET_COUNT());
#if GSM_HTTP_CUSTOM_HEADER
                gsm_hw_send_at_cmd("AT+QHTTPCFG=\"requestheader\",1\r\n", 
                                    "OK\r\n", 
                                    "", 
                                    5000, 
                                    2, 
                                    gsm_http_query);
#else
                gsm_hw_send_at_cmd("AT+QHTTPCFG=\"requestheader\",0\r\n", 
                                    "OK\r\n", 
                                    "", 
                                    5000, 
                                    2, 
                                    gsm_http_query);
#endif
            }
        }
        break;
        
        case 2:
        {
            DEBUG_PRINTF("request/response header : %s, response %s\r\n",
                         (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]",
                         (char*)response_buffer);
//            if (event != GSM_EVENT_OK)
//            {
//                if (strstr(response_buffer, "+CME ERROR: HTTP(S) busy"))
//                {
//                    sys_delay_ms(1000);
//                    if (m_http_cfg.action == GSM_HTTP_ACTION_GET)
//                    {
//                        gsm_hw_send_at_cmd("AT+QHTTPCFG=\"responseheader\",0\r\n", 
//                                            "OK\r\n", 
//                                            "", 
//                                            5000, 
//                                            1, 
//                                            gsm_http_query);
//                    }
//                    else
//                    {
//                        DEBUG_HTTP_POST_INC();
//                        DEBUG_PRINTF("Post total %u msg\r\n", DEBUG_HTTP_POST_GET_COUNT());
//        #if GSM_HTTP_CUSTOM_HEADER
//                        gsm_hw_send_at_cmd("AT+QHTTPCFG=\"requestheader\",1\r\n", 
//                                            "OK\r\n", 
//                                            "", 
//                                            5000, 
//                                            2, 
//                                            gsm_http_query);
//        #else
//                        gsm_hw_send_at_cmd("AT+QHTTPCFG=\"requestheader\",0\r\n", 
//                                            "OK\r\n", 
//                                            "", 
//                                            5000, 
//                                            2, 
//                                            gsm_http_query);
//        #endif
//                    }
//                    m_http_step--;
//                }
//            }
//            else
            {
                if (m_http_cfg.action == GSM_HTTP_ACTION_GET)
                {
                    gsm_hw_send_at_cmd("AT+QHTTPCFG=\"requestheader\",0\r\n", 
                                        "OK\r\n", 
                                        "", 
                                        5000, 
                                        1, 
                                        gsm_http_query);
                }
                else
                {
                    gsm_hw_send_at_cmd("AT+QHTTPCFG=\"contenttype\",1\r\n", 
                                        "OK\r\n", 
                                        "", 
                                        2000, 
                                        1, 
                                        gsm_http_query);
                }
            }
        }
            break;

        case 3:
        {
            DEBUG_PRINTF("Config http header : %s, response %s\r\n", 
                                                (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]", 
                                                (char*)response_buffer);
            if (m_renew_apn)
            {
                gsm_hw_send_at_cmd("AT+QICSGP=1,1,\"v-internet\",\"\",\"\",1\r\n", 
                                    "OK\r\n", 
                                    "", 
                                    5000, 
                                    1, 
                                    gsm_http_query);
            }
            else
            {
                gsm_hw_send_at_cmd("AT+QIACT?\r\n", 
                                    "QIACT", 
                                    "OK\r\n", 
                                    3000, 
                                    2, 
                                    gsm_http_query);
            }
        }
        break;

        case 4:     // Activate context 1
        {
            DEBUG_PRINTF("Active AT+QIACT %s, response %s\r\n", 
                        (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]",
                         (char*)response_buffer);

            m_renew_apn = false;
            if (strstr((char*)response_buffer, "+QIACT:"))
            {
                gsm_hw_send_at_cmd("AT\r\n", 
                                    "OK\r\n",
                                    "",
                                    1000, 
                                    1, 
                                    gsm_http_query);
            }
            else
            {
                gsm_hw_send_at_cmd("AT+QIACT=1\r\n", 
                                    "OK\r\n",
                                    "",
                                    5000, 
                                    2, 
                                    gsm_http_query);
            }

        }
        break;

        case 5:     // Query state of context
        {
            //DEBUG_PRINTF("AT+QIACT=1: %s, response %s\r\n", 
            //            (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]", 
            //            (char*)response_buffer);

            DEBUG_VERBOSE("AT: %s, response %s\r\n", 
                        (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]", 
                        (char*)response_buffer);
            gsm_hw_send_at_cmd("AT+QIACT?\r\n", 
                                "QIACT: ", 
                                "OK\r\n", 
                               3000, 
                                2, 
                                gsm_http_query);
        }
        break;
        
        case 6: // Set ssl
        {
            DEBUG_PRINTF("AT+QIACT: %s, response %s\r\n", 
                        (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]", 
                        (char*)response_buffer);
            m_ssl_step = 0;
            if (strstr(m_http_cfg.url, "https://"))
            {
                gsm_hw_send_at_cmd("AT\r\n", 
                                    "OK\r\n", 
                                    "", 
                                    200, 
                                    2, 
                                    setup_http_ssl);
            }
            else
            {
                m_renew_config_ssl = true;
                gsm_hw_send_at_cmd("AT\r\n", 
                                    "OK\r\n", 
                                    "", 
                                    200, 
                                    2, 
                                    gsm_http_query);
            }
        }
            break;

        case 7: // Set url
        {
            DEBUG_PRINTF("SSL : %s, response %s\r\n", 
                        (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]", 
                        (char*)response_buffer);
            DEBUG_PRINTF("URL %s\r\n", m_http_cfg.url);
            if (event == GSM_EVENT_OK)
            {
                sprintf(m_http_cmd_buffer, "AT+QHTTPURL=%u,1000\r\n", strlen(m_http_cfg.url));
                gsm_hw_send_at_cmd(m_http_cmd_buffer, 
                                    "CONNECT\r\n", 
                                    "", 
                                    2000, 
                                    1, 
                                    gsm_http_query);
            }
            else
            {
                gsm_http_close_on_error();
                return;
            }
        }
        break;

        case 8:
        {
            DEBUG_PRINTF("URL : %s, response %s\r\n", 
                        (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]", 
                        (char*)response_buffer);

            gsm_hw_send_at_cmd(m_http_cfg.url, 
                            "", 
                            "OK\r\n", 
                            5000, 
                            1, 
                            gsm_http_query);
        }
            break;
        
        case 9: // Start HTTP download
        {
            
            DEBUG_PRINTF("\r\nHTTP setting url : %s, response %s\r\n", 
                        (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]", 
                        (char*)response_buffer);
            if (m_http_cfg.action == GSM_HTTP_ACTION_GET)       // GET
            {
                if (!m_http_cfg.big_file_for_ota)
                {
                    gsm_hw_send_at_cmd("AT+QHTTPGET=12\r\n", 
                                    "+QHTTPGET: 0,", 
                                    "\r\n", 
                                    20000, 
                                    1, 
                                    gsm_http_query);
                }
                else
                {
                    gsm_hw_send_at_cmd("AT+QHTTPGET=80\r\n", 
                                    "+QHTTPGET: 0,", 
                                    "\r\n", 
                                    80000, 
                                    1, 
                                    gsm_http_query);
                }
            }
            else        // POST
            {
                post_rx_data.action = m_http_cfg.action;
                if (m_http_cfg.on_event_cb)
                {
                    m_http_cfg.on_event_cb(GSM_HTTP_POST_EVENT_DATA, &post_rx_data);
                }
//                DEBUG_PRINTF("Send post data\r\n"); 
                sprintf(m_http_cmd_buffer, "AT+QHTTPPOST=%u,3,7\r\n", 
                                            post_rx_data.data_length); 
                gsm_hw_send_at_cmd(m_http_cmd_buffer, 
                                    "CONNECT", 
                                    "", 
                                    11000, 
                                    1, 
                                    gsm_http_query);
            }
        }
        break;

        case 10:
        {
            DEBUG_PRINTF("HTTP action : %s, response %s\r\n", 
                        (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]", 
                        (char*)response_buffer);

            if (m_http_cfg.action == GSM_HTTP_ACTION_GET)       // GET
            {
                if (event == GSM_EVENT_OK)
                {
                    uint32_t http_response_code;

                    if (gsm_utilities_parse_http_action_response((char*)response_buffer, 
                                                                &http_response_code, 
                                                                &m_content_length)
                        && m_content_length)
                    {
                        if (m_http_cfg.on_event_cb)
                        {
                            m_http_cfg.on_event_cb(GSM_HTTP_EVENT_CONNTECTED, &m_content_length);
                        }

                        DEBUG_PRINTF("Content length %u\r\n", m_content_length);
                        if (!m_http_cfg.big_file_for_ota)
                        {
                            sprintf(m_http_cmd_buffer, "%s", "AT+QHTTPREAD=11\r\n");
                            //sprintf(m_http_cmd_buffer, "%s", "AT+QHTTPREADFILE=\"RAM:1.txt\",80\r\n");
                            gsm_hw_send_at_cmd(m_http_cmd_buffer, 
                                                "QHTTPREAD: 0", 
                                                "\r\n", 
                                                12000, 
                                                1, 
                                                gsm_http_query); // Close a GPRS context.
                            m_total_bytes_recv = m_content_length;
                        }
                        else
                        {
                            //sprintf(m_http_cmd_buffer, "%s", "AT+QHTTPREAD=75\r\n");
                            sprintf(m_http_cmd_buffer, "AT+QHTTPREADFILE=\"%s\",79\r\n", OTA_FILE_NAME);
                            gsm_hw_send_at_cmd(m_http_cmd_buffer, 
                                                "OK\r\n", 
                                                "+QHTTPREADFILE: 0\r\n", 
                                                81000, 
                                                1, 
                                                gsm_http_download_big_file); // Close a GPRS context.
                            m_http_read_big_file_step = 0;
                            m_start_download_timestamp = gsm_get_current_tick();
                            m_total_bytes_recv = 0;
                        }
                    }
                    else
                    {
                        DEBUG_PRINTF("HTTP error\r\n");
                        gsm_http_close_on_error();
                        return;
                    }
                }
                else
                {
                    DEBUG_PRINTF("Cannot download file\r\n");
                    gsm_http_close_on_error();
                    return;
                }
            }
            else        // POST
            {
//                DEBUG_PRINTF("Input http post, header size %u, data size %u\r\n", 
//								strlen((char*)post_rx_data.header), strlen((char*)post_rx_data.data));
#if GSM_HTTP_CUSTOM_HEADER
				DEBUG_RAW("%s", post_rx_data.header);
                gsm_hw_uart_send_raw(post_rx_data.header, strlen((char*)post_rx_data.header));
#endif
                gsm_hw_send_at_cmd((char*)post_rx_data.data, 
                                    "QHTTPPOST: ", 
                                    "\r\n", 
                                    11000, 
                                    1, 
                                    gsm_http_query); // Close a GPRS context.
            }
        }
        break;
        
        case 11:     // Proccess data
        {
            DEBUG_VERBOSE("HTTP response : %s, data %s\r\n", 
                            (event == GSM_EVENT_OK) ? "OK" : "FAIL",
                             (char*)response_buffer);

            if (m_http_cfg.action == GSM_HTTP_ACTION_GET)       // GET
            {
                if (event == GSM_EVENT_OK)
                {
                    char *p_begin = strstr((char*)response_buffer, "CONNECT\r\n");
                    p_begin += strlen("CONNECT\r\n");
                    char *p_end = strstr(p_begin, "+QHTTPREAD: 0");
                    
                    post_rx_data.action = m_http_cfg.action;
                    post_rx_data.data_length = p_end - p_begin;
                    post_rx_data.data = (uint8_t*)p_begin;
                    
                    if (m_http_cfg.on_event_cb)
                    {
                        m_http_cfg.on_event_cb(GSM_HTTP_GET_EVENT_DATA, &post_rx_data);
                        m_http_cfg.on_event_cb(GSM_HTTP_GET_EVENT_FINISH_SUCCESS, &m_total_bytes_recv);
                    }
                }
                else
                {
                    gsm_http_close_on_error();
                    return;
                }
            }
            else
            {
                uint32_t http_response_code, m_content_length;
                bool success = false;

                if (gsm_utilities_parse_http_action_response((char*)response_buffer, 
                                                                &http_response_code, 
                                                                &m_content_length))
                {
                    if (http_response_code != 200)
                    {
                        success = false;
//                        DEBUG_ERROR("HTTP error code %u\r\n", http_response_code);
                    }
                    else
                    {
                        success = true;
                    }
                }
                
                if (success)
                {
                    sprintf(m_http_cmd_buffer, "%s", "AT+QHTTPREAD=12\r\n");
                    gsm_hw_send_at_cmd(m_http_cmd_buffer, 
                                        "QHTTPREAD: 0", 
                                        "", 
                                        12000, 
                                        1, 
                                        gsm_http_query); // Close a GPRS context.
                }
                else
                {
                    if (m_http_cfg.on_event_cb)
                    {
                        m_http_cfg.on_event_cb(GSM_HTTP_POST_EVENT_FINISH_FAILED, NULL);
                    }
                }
            }
        }
            break;
        
        case 12:
            DEBUG_VERBOSE("HTTP post response : %s, data %s\r\n", 
                            (event == GSM_EVENT_OK) ? "OK" : "FAIL",
                             (char*)response_buffer);
            if (m_http_cfg.on_event_cb)
            {
                m_http_cfg.on_event_cb(GSM_HTTP_POST_EVENT_FINISH_SUCCESS, NULL);
            }
            m_http_step = 0;
            //return;
            break;
        
        default:
            DEBUG_ERROR("[%s] Unhandle step %d\r\n", __FUNCTION__, m_http_step);
            break;
    }
    m_http_step++;
}


gsm_http_config_t *gsm_http_get_config(void)
{
    return &m_http_cfg;
}

void gsm_http_cleanup(void)
{
    m_renew_config_ssl = true;
    m_renew_apn = true;
    m_http_step = 0;
    m_total_bytes_recv = 0;
    m_content_length = 0;
    memset(&m_http_cmd_buffer, 0, sizeof(m_http_cmd_buffer));
    memset(&m_http_cfg, 0, sizeof(m_http_cfg));
}

bool gsm_http_start(gsm_http_config_t *config)
{
    m_http_step = 0;
    m_total_bytes_recv = 0;
    m_content_length = 0;
    memset(&m_http_cmd_buffer, 0, sizeof(m_http_cmd_buffer));
    memset(&m_http_cfg, 0, sizeof(m_http_cfg));
    memcpy(&m_http_cfg, config, sizeof(gsm_http_config_t));
    gsm_http_query(GSM_EVENT_OK, NULL);
    return true;
}

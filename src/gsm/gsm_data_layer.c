/******************************************************************************
 * @file    	GSM_DataLayer.c
 * @author  	
 * @version 	V1.0.0
 * @date    	10/03/2016
 * @brief   	
 ******************************************************************************/

/******************************************************************************
                                   INCLUDES					    			 
 ******************************************************************************/
#include <stdio.h>
#include <string.h>
#include "gsm.h"
#include "DataDefine.h"
#include "gsm_utilities.h"
#include "main.h"
//#include "Parameters.h"
#include "hardware.h"
#include "hardware_manager.h"
#include "gsm_http.h"
#include "server_msg.h"
#include "app_queue.h"
#include "umm_malloc.h"
#include "app_bkup.h"
#include "app_eeprom.h"
#include "measure_input.h"
#include "version_control.h"
#include "sys_ctx.h"
#include "ota_update.h"
#include "app_rtc.h"

#ifdef STM32L083xx
#include "usart.h"
#include "app_rtc.h"
#endif

#define CUSD_ENABLE     0
#define MAX_TIMEOUT_TO_SLEEP_S 60
#define GSM_NEED_ENTER_HTTP_GET() (m_enter_http_get)
#define GSM_DONT_NEED_HTTP_GET() (m_enter_http_get = false)
#define GSM_NEED_ENTER_HTTP_POST() (m_enter_http_post)
#define GSM_DONT_NEED_HTTP_POST() (m_enter_http_post = false)


extern gsm_manager_t gsm_manager;


#define GET_BTS_INFOR_TIMEOUT 300
/******************************************************************************
                                   PRIVATE VARIABLES					    			 
 ******************************************************************************/
#if __GSM_SMS_ENABLE__
static char tmpBuffer[50] = {0};
#endif

static char m_at_cmd_buffer[128];
uint8_t in_sleep_mode_tick = 0;
uint8_t m_timeout_to_sleep = 0;
static app_queue_t m_http_msq;

void gsm_at_cb_power_on_gsm(gsm_response_event_t event, void *resp_buffer);
void gsm_at_cb_read_sms(gsm_response_event_t event, void *resp_buffer);
void gsm_at_cb_send_sms(gsm_response_event_t event, void *resp_buffer);
void gsm_data_layer_switch_mode_at_cmd(gsm_response_event_t event, void *resp_buffer);
void gsm_at_cb_exit_sleep(gsm_response_event_t event, void *resp_buffer);
void gsm_hard_reset(void);
uint8_t convert_csq_to_percent(uint8_t csq);
uint8_t gsm_check_ready_status(void);
static void gsm_http_event_cb(gsm_http_event_t event, void *data);

static bool m_enter_http_get = false;
static bool m_enter_http_post = false;
static gsm_internet_mode_t m_internet_mode;
static uint32_t m_malloc_count = 0;

static app_queue_data_t m_last_http_msg =
{
    .pointer = NULL,
};

bool m_do_read_sms = false;

void gsm_set_flag_prepare_enter_read_sms_mode(void)
{
    m_do_read_sms = true;
}

void gsm_wakeup_periodically(void)
{
	sys_ctx_t *ctx = sys_ctx();
	app_eeprom_config_data_t *cfg = app_eeprom_read_config_data();
	
    DEBUG_PRINTF("Sleep time %usecond, periodic send msg %u, remaining %us\r\n",
                 ctx->status.sleep_time_s,
                 cfg->send_to_server_interval_ms,
                 cfg->send_to_server_interval_ms - ctx->status.sleep_time_s * 1000);

    if (ctx->status.sleep_time_s*1000 >= cfg->send_to_server_interval_ms)
    {
        ctx->status.sleep_time_s = 0;
        DEBUG_PRINTF("GSM: wakeup to send msg\r\n");
        gsm_change_state(GSM_STATE_WAKEUP);
    }
}

void gsm_set_wakeup_now(void)
{
	sys_ctx_t *ctx = sys_ctx();
	ctx->status.sleep_time_s = 0;
	gsm_change_state(GSM_STATE_WAKEUP);
}


uint8_t m_send_at_cmd_in_idle_mode = 0;
static void gsm_query_sms_buffer(void)
{
    uint8_t cnt = 0;
    gsm_sms_msg_t *sms = gsm_get_sms_memory_buffer();
    uint32_t max_sms = gsm_get_max_sms_memory_buffer();

    for (cnt = 0; cnt < max_sms; cnt++)
    {
        if (sms[cnt].need_to_send == 1 
            || sms[cnt].need_to_send == 2)
        {
            sms[cnt].retry_count++;
            DEBUG_PRINTF("Send sms in buffer index %d\r\n", cnt);

            /* Neu gui 3 lan khong thanh cong thi xoa khoi queue */
            if (sms[cnt].retry_count < 3)
            {
                sms[cnt].need_to_send = 2;
                DEBUG_PRINTF("Change gsm state to send sms\r\n");
                gsm_change_state(GSM_STATE_SEND_SMS);
            }
            else
            {
                DEBUG_PRINTF("SMS buffer %u send FAIL %u times. Cancle!\r\n", 
                                cnt, 
                                sms[cnt].retry_count);
                sms[cnt].need_to_send = 0;
            }
            return;
        }
    }
}

void gsm_manager_tick(void)
{
	sys_ctx_t *ctx = sys_ctx();
    if (gsm_manager.is_gsm_power_off == 1)
        return;

    /* Cac trang thai lam viec module GSM */
    switch (gsm_manager.state)
    {
    case GSM_STATE_POWER_ON:
        if (gsm_manager.step == 0)
        {
            DEBUG_PRINTF("GSM power on, query ATV1\r\n");
            gsm_manager.step = 1;
            gsm_hw_send_at_cmd("ATV1\r\n", "OK\r\n", "", 1000, 30, gsm_at_cb_power_on_gsm);
        }
        //m_do_read_sms = true;
        break;

    case GSM_STATE_OK: /* PPP data mode */
    {
        if (m_do_read_sms)
        {
            m_do_read_sms = false;
            gsm_change_state(GSM_STATE_READ_SMS);
        }
        
        if (m_timeout_to_sleep++ >= MAX_TIMEOUT_TO_SLEEP_S)
        {
            DEBUG_PRINTF("GSM in at mode : need to sleep\r\n");
        }

        gsm_wakeup_periodically();

        if (gsm_manager.state == GSM_STATE_OK)
        {
            gsm_query_sms_buffer();
        }
        
        if (gsm_manager.state == GSM_STATE_OK
            && m_internet_mode == GSM_INTERNET_MODE_AT_STACK)      // gsm state maybe changed in gsm_query_sms_buffer task
        {
            bool enter_sleep_in_http = true;
            if (!app_queue_is_empty(&m_http_msq))
            {
                DEBUG_PRINTF("Post http data\r\n");
                m_enter_http_post = true;
                enter_sleep_in_http = false;
                gsm_change_state(GSM_STATE_HTTP_POST);
            }
            else
            {
                DEBUG_PRINTF("Queue empty\r\n");
                if (gsm_manager.state == GSM_STATE_OK)
                {
                    if (GSM_NEED_ENTER_HTTP_GET())
                    {
                        gsm_change_state(GSM_STATE_HTTP_GET);
                        enter_sleep_in_http = false;
                    }
                }
            }
        
			//#warning "Sleep in http mode is not enabled"
			if (enter_sleep_in_http)
			//if (0)
			{
				gsm_change_state(GSM_STATE_SLEEP);
			}
			else
			{
				if (m_timeout_to_sleep > 15)
				{
					m_timeout_to_sleep -= 15;
				}
			}
        }
    }
    break;

    case GSM_STATE_RESET: /* Hard Reset */
        gsm_manager.gsm_ready = 0;
        gsm_hard_reset();
        break;

    case GSM_STATE_READ_SMS: /* Read SMS */
        if (gsm_manager.step == 0)
        {
            gsm_enter_read_sms();
        }
        break;

    case GSM_STATE_SEND_SMS: /* Send SMS */

        if (!gsm_manager.gsm_ready)
            break;

        if (gsm_manager.step == 0)
        {
            DEBUG_PRINTF("Enter send sms cb\r\n");
            gsm_manager.step = 1;
            gsm_hw_send_at_cmd("ATV1\r\n", "OK\r\n", "", 100, 1, gsm_at_cb_send_sms);
        }
        break;

    case GSM_STATE_HTTP_POST:
    {
        if (GSM_NEED_ENTER_HTTP_POST())
        {
            GSM_DONT_NEED_HTTP_POST();
            static gsm_http_config_t cfg;
            sprintf(cfg.url, "https://iot.wilad.vn/api/v1/%s/telemetry",
                    gsm_get_module_imei());
            //sprintf(cfg.url, "%s", "https://iot.wilad.vn");
            cfg.on_event_cb = gsm_http_event_cb;
            cfg.action = GSM_HTTP_ACTION_POST;
            cfg.port = 443;
            gsm_http_start(&cfg);
            m_enter_http_get = true;
        }
    }
    break;

    case GSM_STATE_HTTP_GET:
    {
        if (GSM_NEED_ENTER_HTTP_GET())
        {
            static gsm_http_config_t cfg;
            if (!ota_update_is_running())
            {
                sprintf(cfg.url, "https://iot.wilad.vn/api/v1/%s/attributes",
                        gsm_get_module_imei());
                cfg.on_event_cb = gsm_http_event_cb;
                cfg.action = GSM_HTTP_ACTION_GET;
//                cfg.port = 443;
                cfg.big_file_for_ota = 0;
                gsm_http_start(&cfg);
                GSM_DONT_NEED_HTTP_GET();
            }
            else
            {
                sprintf(cfg.url, "%s", ctx->status.ota_url);
                cfg.on_event_cb = gsm_http_event_cb;
                cfg.action = GSM_HTTP_ACTION_GET;
//                cfg.port = 443;
                cfg.big_file_for_ota = 1;
                gsm_http_start(&cfg);
                GSM_DONT_NEED_HTTP_GET();
            }
        }
    }
    break;

    case GSM_STATE_WAKEUP: /* Thoat che do sleep */
    {
        gsm_data_layer_initialize();
        gsm_change_state(GSM_STATE_RESET);
    }
        break;

    case GSM_STATE_SLEEP: /* Dang trong che do Sleep */
        if (in_sleep_mode_tick % 10 == 0)
        {
            DEBUG_PRINTF("GSM is sleeping...\r\n");
        }
        in_sleep_mode_tick++;
        m_timeout_to_sleep = 0;
#ifdef GD32E10X
        driver_uart_deinitialize(GSM_UART);
        usart_interrupt_flag_clear(GSM_UART, USART_INT_FLAG_RBNE);
        usart_interrupt_flag_clear(GSM_UART, USART_INT_FLAG_TBE);
#else
        usart1_control(false);
#endif
        GSM_PWR_EN(0);
        GSM_PWR_RESET(0);
        GSM_PWR_KEY(0);
        /* Thuc day gui tin dinh ky */
        gsm_wakeup_periodically();

        break;

    default:
        DEBUG_PRINTF("Unhandled case %u\r\n",gsm_manager.state);
        break;
    }
}


static bool m_is_the_first_time = true;
static void init_http_msq(void)
{

    if (m_internet_mode == GSM_INTERNET_MODE_AT_STACK)
    {
        if (m_is_the_first_time)
        {
            m_is_the_first_time = false;
            umm_init();
            DEBUG_PRINTF("HTTP: init buffer\r\n");
            app_queue_reset(&m_http_msq);
        }
    }
}

void gsm_data_layer_initialize(void)
{
    gsm_manager.ri_signal = 0;

    gsm_http_cleanup();
    m_internet_mode = GSM_INTERNET_MODE_AT_STACK;
    init_http_msq();
}

bool gsm_data_layer_is_module_sleeping(void)
{
    if (gsm_manager.state == GSM_STATE_SLEEP)
    {
        return 1;
    }
    return 0;
}

void gsm_change_state(gsm_state_t new_state)
{
    if (new_state == GSM_STATE_OK) //Command state -> Data state trong PPP mode
    {
        gsm_manager.gsm_ready = 2;
        m_timeout_to_sleep = 0;
    }

    DEBUG_PRINTF("Change GSM state to: ");
    switch ((uint8_t)new_state)
    {
    case GSM_STATE_OK:
        DEBUG_RAW("OK\r\n");
        break;
    case GSM_STATE_RESET:
        DEBUG_RAW("RESET\r\n");
        break;
    case GSM_STATE_SEND_SMS:
        DEBUG_RAW("SEND SMS\r\n");
        break;
    case GSM_STATE_READ_SMS:
        DEBUG_RAW("READ SMS\r\n");
        break;
    case GSM_STATE_POWER_ON:
        DEBUG_RAW("POWERON\r\n");
        break;
    case GSM_STATE_REOPEN_PPP:
        DEBUG_RAW("REOPENPPP\r\n");
        break;
    case GSM_STATE_GET_BTS_INFO:
        DEBUG_RAW("GETSIGNAL\r\n");
        break;
    case GSM_STATE_SEND_ATC:
        DEBUG_RAW("Quit PPP and send AT command\r\n");
        break;
    case GSM_STATE_GOTO_SLEEP:
        DEBUG_RAW("Prepare sleep\r\n");
        break;
    case GSM_STATE_WAKEUP:
        DEBUG_RAW("WAKEUP\r\n");
        break;
    case GSM_STATE_AT_MODE_IDLE:
        DEBUG_RAW("IDLE\r\n");
        break;
    case GSM_STATE_SLEEP:
        DEBUG_RAW("SLEEP\r\n");
        break;
    case GSM_STATE_HTTP_GET:
        DEBUG_RAW("HTTP GET\r\n");
        break;
    case GSM_STATE_HTTP_POST:
        DEBUG_RAW("HTTP POST\r\n");
        break;
    default:
        break;
    }
    gsm_manager.state = new_state;
    gsm_manager.step = 0;
}

void gsm_at_cb_power_on_gsm(gsm_response_event_t event, void *resp_buffer)
{
    //DEBUG_PRINTF("%s\r\n", __FUNCTION__);
    switch (gsm_manager.step)
    {
    case 1:
        if (event == GSM_EVENT_OK)
        {
            DEBUG_PRINTF("Connect modem OK\r\n");
        }
        else
        {
            DEBUG_PRINTF("Connect modem ERR!\r\n");
        }
        gsm_hw_send_at_cmd("ATE0\r\n", "OK\r\n", "", 1000, 10, gsm_at_cb_power_on_gsm);
        break;

    case 2: /* Use AT+CMEE=2 to enable result code and use verbose values */
        DEBUG_PRINTF("Disable AT echo : %s\r\n", (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]");
        gsm_hw_send_at_cmd("AT+CMEE=2\r\n", "OK\r\n", "", 1000, 10, gsm_at_cb_power_on_gsm);
        break;

    case 3:
        DEBUG_PRINTF("Set CMEE report: %s\r\n", (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]");
        gsm_hw_send_at_cmd("ATI\r\n", "OK\r\n", "", 1000, 10, gsm_at_cb_power_on_gsm);
        break;

    case 4:
        DEBUG_PRINTF("Get module info: %s\r\n", resp_buffer);
        gsm_hw_send_at_cmd("AT+QURCCFG=\"URCPORT\",\"uart1\"\r\n", "OK\r\n", "", 1000, 5, gsm_at_cb_power_on_gsm);
        break;

    case 5:
        DEBUG_PRINTF("Set URC port: %s\r\n", (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]");
        gsm_hw_send_at_cmd("AT+QCFG=\"urc/ri/smsincoming\",\"pulse\",2000\r\n", "OK\r\n", "", 1000, 10, gsm_at_cb_power_on_gsm);
        break;

    case 6:
        DEBUG_PRINTF("Set URC ringtype: %s\r\n", (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]");
        gsm_hw_send_at_cmd("AT+CNMI=2,1,0,0,0\r\n", "", "OK\r\n", 1000, 10, gsm_at_cb_power_on_gsm);
        break;
    case 7:
        DEBUG_PRINTF("Config SMS event report: %s\r\n", (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]");
        gsm_hw_send_at_cmd("AT+CMGF=1\r\n", "", "OK\r\n", 1000, 10, gsm_at_cb_power_on_gsm);
        break;

    case 8:
        DEBUG_PRINTF("Set SMS text mode: %s\r\n", (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]");
        //gsm_hw_send_at_cmd("AT+CMGD=1,4\r\n", "OK\r\n", "", 2000, 5, gsm_at_cb_power_on_gsm);
        gsm_hw_send_at_cmd("AT\r\n", "", "OK\r\n", 2000, 5, gsm_at_cb_power_on_gsm);
        break;

    case 9:
        DEBUG_PRINTF("Delete all SMS: %s\r\n", (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]");
        gsm_hw_send_at_cmd("AT+CGSN\r\n", "", "OK\r\n", 1000, 5, gsm_at_cb_power_on_gsm);
        break;

    case 10:
	{
		uint8_t *imei_buffer = (uint8_t*)gsm_get_module_imei();
        gsm_utilities_get_imei(resp_buffer, (uint8_t *)imei_buffer, 16);
        DEBUG_PRINTF("Get GSM IMEI: %s\r\n", imei_buffer);
        if (strlen(gsm_get_module_imei()) < 15)
        {
            DEBUG_PRINTF("IMEI's invalid!\r\n");
            gsm_change_state(GSM_STATE_RESET); //Khong doc dung IMEI -> reset module GSM!
            return;
        }
        gsm_hw_send_at_cmd("AT+CIMI\r\n", "OK\r\n", "", 1000, 10, gsm_at_cb_power_on_gsm);
	}
        break;

    case 11:
	{
		uint8_t *imei_buffer = (uint8_t*)gsm_get_sim_imei();
        gsm_utilities_get_imei(resp_buffer, imei_buffer, 16);
        DEBUG_PRINTF("Get SIM IMSI: %s\r\n", gsm_get_sim_imei());
        if (strlen(gsm_get_sim_imei()) < 15)
        {
            DEBUG_PRINTF("SIM's not inserted!\r\n");
            gsm_change_state(GSM_STATE_RESET); //Neu ko nhan SIM -> reset module GSM!
            return;
        }
        gsm_hw_send_at_cmd("AT+QCCID\r\n", "QCCID", "OK\r\n", 1000, 3, gsm_at_cb_power_on_gsm);
	}
        break;

    case 12:
    {
        DEBUG_PRINTF("Get SIM IMEI: %s\r\n", (char *)resp_buffer);
        gsm_hw_send_at_cmd("AT+CPIN?\r\n", "READY\r\n", "", 3000, 3, gsm_at_cb_power_on_gsm); 
    }
        break;

    case 13:
        DEBUG_PRINTF("CPIN: %s\r\n", (char *)resp_buffer);
        gsm_hw_send_at_cmd("AT+QIDEACT=1\r\n", "OK\r\n", "", 3000, 1, gsm_at_cb_power_on_gsm);
        break;
     
    case 14:
        DEBUG_PRINTF("De-activate PDP: %s\r\n", (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]");
        gsm_hw_send_at_cmd("AT+QCFG=\"nwscanmode\",0\r\n", "OK\r\n", "", 5000, 2, gsm_at_cb_power_on_gsm); // Select mode AUTO
        break;

    case 15:
        DEBUG_PRINTF("Network search mode AUTO: %s\r\n", (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]");
        gsm_hw_send_at_cmd("AT+CGDCONT=1,\"IP\",\"v-internet\"\r\n", "", "OK\r\n", 1000, 2, gsm_at_cb_power_on_gsm); /** <cid> = 1-24 */
        break;

    case 16:
        DEBUG_PRINTF("Define PDP context: %s\r\n", (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]");
        //			SendATCommand ("AT+QIACT=1\r\n", "OK\r\n", "", 5000, 5, PowerOnModuleGSM);	/** Bật QIACT lỗi gửi tin với 1 số SIM dùng gói cước trả sau! */
        gsm_hw_send_at_cmd("AT+CSCS=\"GSM\"\r\n", "OK\r\n", "", 1000, 1, gsm_at_cb_power_on_gsm);
        break;

    case 17:
        DEBUG_PRINTF("CSCS=GSM: %s\r\n", (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]");
        gsm_hw_send_at_cmd("AT+CGREG=2\r\n", "OK\r\n", "", 1000, 3, gsm_at_cb_power_on_gsm); /** Query CGREG? => +CGREG: <n>,<stat>[,<lac>,<ci>[,<Act>]] */
        break;

    case 18:
        DEBUG_PRINTF("Network registration status: %s\r\n", (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]");
        gsm_hw_send_at_cmd("AT+CGREG?\r\n", "OK\r\n", "", 1000, 5, gsm_at_cb_power_on_gsm);
        break;

    case 19:
        DEBUG_PRINTF("Query network status: %s\r\n", (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]"); /** +CGREG: 2,1,"3279","487BD01",7 */
        if (event == GSM_EVENT_OK)
        {
            bool retval;
            retval = gsm_utilities_get_network_access_tech(resp_buffer, &gsm_manager.access_tech);
            if (retval == false)
            {
                gsm_hw_send_at_cmd("AT+CGREG?\r\n", "OK\r\n", "", 1000, 5, gsm_at_cb_power_on_gsm);
                gsm_change_hw_polling_interval(1000);
                return;
            }
        }
        gsm_hw_send_at_cmd("AT+COPS?\r\n", "OK\r\n", "", 2000, 5, gsm_at_cb_power_on_gsm);
        break;

    case 20:
        DEBUG_PRINTF("Query network operator: %s\r\n", (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]"); /** +COPS: 0,0,"Viettel Viettel",7 */
        if (event == GSM_EVENT_OK)
        {
            gsm_utilities_get_network_operator(resp_buffer, 
                                                gsm_get_network_operator(), 
                                                32);
            if (strlen(gsm_get_network_operator()) < 5)
            {
                gsm_hw_send_at_cmd("AT+COPS?\r\n", "OK\r\n", "", 1000, 5, gsm_at_cb_power_on_gsm);
                gsm_change_hw_polling_interval(1000);
                return;
            }
            DEBUG_PRINTF("Network operator: %s\r\n", gsm_get_network_operator());
        }
        gsm_change_hw_polling_interval(5);
        gsm_hw_send_at_cmd("AT\r\n", "OK\r\n", "", 1000, 5, gsm_at_cb_power_on_gsm);
        break;

    case 21:
    {
        DEBUG_PRINTF("Select QSCLK: %s\r\n", (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]");
        gsm_hw_send_at_cmd("AT+CCLK?\r\n", "+CCLK:", "OK\r\n", 1000, 5, gsm_at_cb_power_on_gsm);
    }
    break;

    case 22:
    {
        DEBUG_PRINTF("Query CCLK: %s,%s\r\n",
                     (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]",
                     (char *)resp_buffer);
        rtc_date_time_t time;
        gsm_utilities_parse_timestamp_buffer((char *)resp_buffer, &time);
        time.hour += 7;     // GMT + 7
        if (time.year > 20 && time.year < 40)
        {
            app_rtc_set_counter(&time);
        }
        gsm_hw_send_at_cmd("AT+CSQ\r\n", "", "OK\r\n", 1000, 5, gsm_at_cb_power_on_gsm);
    }
    break;

    case 23:
        if (event != GSM_EVENT_OK)
        {
            DEBUG_PRINTF("GSM: init fail, reset modem...\r\n");
            gsm_manager.step = 0;
            gsm_change_state(GSM_STATE_RESET);
            return;
        }
		
		uint8_t csq;
        gsm_set_csq(0);
        gsm_utilities_get_signal_strength_from_buffer(resp_buffer, &csq);
        DEBUG_PRINTF("CSQ: %d\r\n", csq);

        if (csq == 99)
        {
            DEBUG_PRINTF("Invalid csq\r\n");
            gsm_manager.step = 21;
            gsm_hw_send_at_cmd("AT+CSQ\r\n", "OK\r\n", "", 1000, 5, gsm_at_cb_power_on_gsm);
            gsm_change_hw_polling_interval(500);
        }
        else
        {
#if CUSD_ENABLE
            gsm_change_hw_polling_interval(100);
            gsm_hw_send_at_cmd("AT+CUSD=1,\"*101#\"\r\n", "+CUSD: ", "\r\n", 10000, 1, gsm_at_cb_power_on_gsm);
#else
			gsm_set_csq(csq);
            gsm_hw_send_at_cmd("AT\r\n", "OK\r\n", "", 2000, 1, gsm_at_cb_power_on_gsm);
#endif
        }
        break;

        case 24:
#if CUSD_ENABLE
            if (event != GSM_EVENT_OK)
            {
                DEBUG_PRINTF("GSM: CUSD query failed\r\n");
            }
            else
            {
                char *p = strstr((char*)resp_buffer, "+CUSD: ");
                p += 5;
                DEBUG_PRINTF("CUSD %s\r\n", p);
                //Delayms(5000);
            }
#endif
            gsm_change_hw_polling_interval(5);
            gsm_manager.gsm_ready = 1;
            gsm_manager.step = 0;
            gsm_build_http_post_msg();
            gsm_change_state(GSM_STATE_OK);
        break;

        default:
            DEBUG_PRINTF("GSM unhandled step %u\r\n", gsm_manager.step);
            break;

    }
   
    gsm_manager.step++;
}

void gsm_at_cb_exit_sleep(gsm_response_event_t event, void *resp_buffer)
{
    DEBUG_PRINTF("%s\r\n", __FUNCTION__);
    switch (gsm_manager.step)
    {
    case 1:
        gsm_hw_send_at_cmd("ATV1\r\n", "OK\r\n", "", 1000, 5, gsm_at_cb_exit_sleep);
        break;
    case 2:
        gsm_hw_send_at_cmd("AT+QSCLK=1\r\n", "OK\r\n", "", 1000, 10, gsm_at_cb_exit_sleep);
        break;
    case 3:
        if (event == GSM_EVENT_OK)
        {
            DEBUG_PRINTF("Exit sleep!");
            gsm_change_state(GSM_STATE_OK);
        }
        else
        {
            DEBUG_PRINTF("Khong phan hoi lenh, reset module...");
            gsm_change_state(GSM_STATE_RESET);
        }
        break;

    default:
        break;
    }
    gsm_manager.step++;
}

/*
* Reset module GSM
*/
void gsm_hard_reset(void)
{
    static uint8_t step = 0;
    DEBUG_PRINTF("GSM hard reset step %d\r\n", step);

    switch (step)
    {
    case 0: // Power off
        gsm_manager.gsm_ready = 0;
        GSM_PWR_EN(0);
        GSM_PWR_RESET(1);
        GSM_PWR_KEY(0);
        step++;
        break;

    case 1:
        GSM_PWR_RESET(0);
        GSM_PWR_EN(0);
        step++;
        break;

    case 2:
        GSM_PWR_RESET(0);
        DEBUG_PRINTF("Gsm power on\r\n");
#ifdef GD32E10X
        nvic_irq_disable(GSM_UART_IRQ);
#endif
        GSM_PWR_EN(1);
        step++;
        break;

    case 3: // Delayms for Vbat stable
        step++;
        break;

    case 4: // Delayms for Vbat stable
        step++;
        break;

    case 5:
        /* Tao xung |_| de Power On module, min 1s  */
        GSM_PWR_KEY(1);
        step++;
        break;

    case 6:
        GSM_PWR_KEY(0);
        GSM_PWR_RESET(0);
		usart1_control(true);
        gsm_manager.timeout_after_reset = 90;
        step++;
        break;

    case 7:
    case 8:
    case 9:
    case 10:
        step++;
        break;
    case 11:
        step = 0;
        gsm_change_state(GSM_STATE_POWER_ON);
        break;
    default:
        break;
    }
}





void gsm_at_cb_send_sms(gsm_response_event_t event, void *resp_buffer)
{
    uint8_t count;
    static uint8_t retry_count = 0;
    gsm_sms_msg_t *sms = gsm_get_sms_memory_buffer();
    uint32_t max_sms = gsm_get_max_sms_memory_buffer();

    DEBUG_PRINTF("Debug SEND SMS : %u %u,%s\r\n", gsm_manager.step, event, resp_buffer);

    switch (gsm_manager.step)
    {
    case 1:
        for (count = 0; count < max_sms; count++)
        {
            if (sms[count].need_to_send == 2)
            {
                DEBUG_PRINTF("Sms to %s. Content : %s\r\n",
                           sms[count].phone_number, sms[count].message);

                sprintf(m_at_cmd_buffer, "AT+CMGS=\"%s\"\r", sms[count].phone_number);

                gsm_hw_send_at_cmd(m_at_cmd_buffer, 
                                    ">", NULL, 
                                    15000, 
                                    1, 
                                    gsm_at_cb_send_sms);
                break;
            }
        }
        break;

    case 2:
        if (event == GSM_EVENT_OK)
        {
            for (count = 0; count < max_sms; count++)
            {
                if (sms[count].need_to_send == 2)
                {
                    sms[count].message[strlen(sms[count].message)] = 26;        // 26 = ctrl Z
                    gsm_hw_send_at_cmd(sms[count].message, "+CMGS", 
                                        NULL, 
                                        30000, 
                                        1, 
                                        gsm_at_cb_send_sms);
                    DEBUG_PRINTF("Sending sms in buffer %u\r\n", count);
                    break;
                }
            }
        }
        else
        {
            retry_count++;
            if (retry_count < 3)
            {
                gsm_change_state(GSM_STATE_SEND_SMS);
                return;
            }
            else
            {
                goto SEND_SMS_FAIL;
            }
        }
        break;

    case 3:
        if (event == GSM_EVENT_OK)
        {
            DEBUG_PRINTF("SMS : Send sms success\r\n");

            for (count = 0; count < max_sms; count++)
            {
                if (sms[count].need_to_send == 2)
                {
                    sms[count].need_to_send = 0;
                }
            }
            retry_count = 0;
            gsm_change_state(GSM_STATE_OK);
        }
        else
        {
            DEBUG_PRINTF("SMS : Send sms failed\r\n");
            retry_count++;
            if (retry_count < 3)
            {
                gsm_change_state(GSM_STATE_SEND_SMS);
                return;
            }
            else
            {
                DEBUG_PRINTF("Send sms failed many times, cancle\r\n");
                goto SEND_SMS_FAIL;
            }
        }
        return;

    default:
        DEBUG_PRINTF("Unknown outgoing sms step %d\r\n", gsm_manager.step);
        gsm_change_state(GSM_STATE_OK);
        break;
    }

    gsm_manager.step++;

    return;

SEND_SMS_FAIL:
    for (count = 0; count < gsm_get_max_sms_memory_buffer(); count++)
    {
        if (sms[count].need_to_send == 2)
        {
            sms[count].need_to_send = 1;
            gsm_change_state(GSM_STATE_OK);
        }
    }

    retry_count = 0;
}


void gsm_set_timeout_to_sleep(uint32_t sec)
{
    if (sec <= MAX_TIMEOUT_TO_SLEEP_S && m_timeout_to_sleep < MAX_TIMEOUT_TO_SLEEP_S)
    {
        DEBUG_PRINTF("GSM sleep in next %u second\r\n", sec);
        m_timeout_to_sleep = MAX_TIMEOUT_TO_SLEEP_S - sec;
    }
}

//static LargeBuffer_t m_http_buffer;
/* {
	"Timestamp":"1623849775","ID":"860262050129720","PhoneNum":"000","Money":"0","Input1":"5558",
	"Input2":"0.0","Output1":"0","Output2":"0","SignalStrength":"100","WarningLevel":"0",
	"BatteryLevel":"77","BatteryDebug":"4086","K":"1","Offset":"5480"}
 */
static uint32_t m_malloc_failed_count = 0;
uint16_t gsm_build_http_post_msg(void)
{
    app_queue_data_t new_msq;
    app_eeprom_config_data_t *cfg = app_eeprom_read_config_data();
	measure_input_perpheral_data_t *measure_input = measure_input_current_data();
	
	char alarm_str[32];
    char *p = alarm_str;

    // bat,temp,4glost,sensor_err,sensor_overflow, sensor break
    if (measure_input->vbat_percent < 10)
    {
        p += sprintf(p, "%u,", 1);
    }
    else
    {
        p += sprintf(p, "%u,", 0);
    }
    
    //4glost,sensor_err,sensor_overflow
    p += sprintf(p, "%u,", 0);
    p += sprintf(p, "%u,", 0);
    p += sprintf(p, "%u,", 0);
	bool found_break_pulse_input = false;
	for (uint32_t i = 0; i < MEASURE_NUMBER_OF_WATER_METER_INPUT; i++)
	{
		if (measure_input->water_pulse_counter[i].line_break_detect)
		{
			found_break_pulse_input = true;
			break;
		}
	}
	
	p += sprintf(p, "%u", found_break_pulse_input ? 1 : 0);


    if (app_queue_is_full(&m_http_msq))
    {
        DEBUG_PRINTF("HTTP msq full\r\n");
        return 0;
    }

    new_msq.pointer = umm_malloc(256+128);
    if (new_msq.pointer == NULL)
    {
        DEBUG_PRINTF("[%s-%d] No memory\r\n", __FUNCTION__, __LINE__);
		m_malloc_failed_count = 0;
		if (m_malloc_failed_count == 0)
		{
			NVIC_SystemReset();
		}
        return 0;
    }
    else
    {
        m_malloc_count++;
		m_malloc_failed_count = 0;
        DEBUG_PRINTF("[%s-%d] Malloc success, nb of times malloc %u\r\n", __FUNCTION__, __LINE__, m_malloc_count);
    }
	
	#warning "Please store default input 2 offset"
	uint32_t counter0_f, counter1_f, counter0_r, counter1_r;
	app_bkup_read_pulse_counter(&counter0_f, &counter1_f, &counter0_r, &counter1_r);
	
	counter0_f = counter0_f / cfg->k0 + cfg->offset0;
	counter1_f = counter1_f / cfg->k1 + cfg->offset1;
#if 1
    new_msq.length = sprintf((char *)new_msq.pointer, "{\"Timestamp\":\"%u\",", app_rtc_get_counter()); //second since 1970
#ifdef DTG02
    new_msq.length += sprintf((char *)(new_msq.pointer + new_msq.length), "\"ID\":\"DTG2-%s\",", gsm_get_module_imei());
#else
    new_msq.length += sprintf((char *)(new_msq.pointer + new_msq.length), "\"ID\":\"DTG1-%s\",", gsm_get_module_imei());
#endif
    new_msq.length += sprintf((char *)(new_msq.pointer + new_msq.length), "\"PhoneNum\":\"%s\",", cfg->phone);
    new_msq.length += sprintf((char *)(new_msq.pointer + new_msq.length), "\"Money\":\"%d\",", 0);
    new_msq.length += sprintf((char *)(new_msq.pointer + new_msq.length), "\"Input1_J1\":\"%u\",\"Input1_J2\":\"%u\",",
                              counter0_f, counter1_f); //so xung
	
	if (cfg->meter_mode[0] == APP_EEPROM_METER_MODE_PWM_F_PWM_R)
	{
		new_msq.length += sprintf((char *)(new_msq.pointer + new_msq.length), "\"Input1_J1_R\":\"%u\",",
									counter0_r);
	}
	
	if (cfg->meter_mode[1] == APP_EEPROM_METER_MODE_PWM_F_PWM_R)
	{
		new_msq.length += sprintf((char *)(new_msq.pointer + new_msq.length), "\"Input1_J2_R\":\"%u\",",
									counter1_r);
	}

	for (uint32_t i = 0; i < NUMBER_OF_INPUT_4_20MA; i++)
	{
		new_msq.length += sprintf((char *)(new_msq.pointer + new_msq.length), "\"Input1_J3_%u\":\"%u.%u\",", 
																			i,
																			measure_input->input_4_20mA[i]/10, 
																			measure_input->input_4_20mA[i]%10); // dau vao 4-20mA 0
	}

#ifdef DTG02
	for (uint32_t i = 0; i < NUMBER_OF_INPUT_ON_OFF; i++)
	{
		new_msq.length += sprintf((char *)(new_msq.pointer + new_msq.length), "\"Input1_J9_%u\":\"%u\",", 
																			i,
																			measure_input->input_on_off[i]); // dau vao 4-20mA 0
	}	

	for (uint32_t i = 0; i < NUMBER_OF_OUT_ON_OFF; i++)
	{
		new_msq.length += sprintf((char *)(new_msq.pointer + new_msq.length), "\"Output%u\":\"%u\",", 
																			i,
																			measure_input->output_on_off[i]); // dau vao 4-20mA 0
	}	
#endif
    new_msq.length += sprintf((char *)(new_msq.pointer + new_msq.length), "\"Output4\":\"%d\",", measure_input->output_4_20mA);    //dau ra on/off
    new_msq.length += sprintf((char *)(new_msq.pointer + new_msq.length), "\"SignalStrength\":\"%d\",", gsm_get_csq_in_percent());
    new_msq.length += sprintf((char *)(new_msq.pointer + new_msq.length), "\"WarningLevel\":\"%s\",", alarm_str);

    new_msq.length += sprintf((char *)(new_msq.pointer + new_msq.length), "\"BatteryLevel\":\"%d\",", measure_input->vbat_percent);

    new_msq.length += sprintf((char *)(new_msq.pointer + new_msq.length), "\"Db\":\"%umV,rst-%u,k-%u-%u,os-%u-%u,m-%u,%u,%s-%s\"}", 
                                                                            measure_input->vbat_raw,
                                                                            hardware_manager_get_reset_reason()->value,
                                                                            cfg->k0, cfg->k1,
                                                                            cfg->offset0, cfg->offset1,
																			cfg->meter_mode[0], cfg->meter_mode[1],
																			VERSION_CONTROL_FW, VERSION_CONTROL_HW);
#else
	new_msq.length = sprintf((char *)new_msq.pointer, "{\"Timestamp\":\"%u\"}", app_rtc_get_counter());
#endif
    hardware_manager_get_reset_reason()->value = 0;

    if (app_queue_put(&m_http_msq, &new_msq) == false)
    {
        DEBUG_PRINTF("Put to http msg failed\r\n");
        m_malloc_count--;
        umm_free(new_msq.pointer);
        return 0;
    }
    else
    {
        DEBUG_RAW("%s\r\n", (char*)new_msq.pointer);
        return new_msq.length;
    }
}

#if GSM_HTTP_CUSTOM_HEADER
static char m_build_http_post_header[255];
static char *build_http_header(uint32_t length)
{
    sprintf(m_build_http_post_header, "POST /api/v1/%s/telemetry HTTP/1.1\r\n"
                                      "Host: iot.wilad.vn\r\nContent-Type: text/plain\r\n"
                                      "Content-Length: %u\r\n\r\n",
            gsm_get_module_imei(),
            length);
    return m_build_http_post_header;
}
#endif

static void gsm_http_event_cb(gsm_http_event_t event, void *data)
{
	sys_ctx_t *ctx = sys_ctx();
    switch (event)
    {
    case GSM_HTTP_EVENT_START:
        DEBUG_PRINTF("HTTP task started\r\n");
        break;

    case GSM_HTTP_EVENT_CONNTECTED:
        DEBUG_PRINTF("HTTP connected, data size %u\r\n", *((uint32_t *)data));
        break;

    case GSM_HTTP_GET_EVENT_DATA:
    {
        gsm_http_data_t *get_data = (gsm_http_data_t *)data;
        DEBUG_PRINTF("DATA %u bytes: %s\r\n", get_data->data_length, get_data->data);
        uint8_t new_cfg = 0;
        server_msg_process_cmd((char *)get_data->data, &new_cfg);
        if (new_cfg)
        {
            gsm_build_http_post_msg();
        }
    }
    break;

    case GSM_HTTP_POST_EVENT_DATA:
    {
        if (m_last_http_msg.pointer == NULL)
        {
            DEBUG_PRINTF("Get http post data from queue\r\n");
            if (!app_queue_get(&m_http_msq, &m_last_http_msg))
            {
                DEBUG_PRINTF("Get msg queue failed\r\n");
                hardware_manager_sys_reset(2);
            }
            else
            {
                ((gsm_http_data_t *)data)->data = (uint8_t *)m_last_http_msg.pointer;
                ((gsm_http_data_t *)data)->data_length = m_last_http_msg.length;
#if GSM_HTTP_CUSTOM_HEADER
                ((gsm_http_data_t *)data)->header = (uint8_t *)build_http_header(m_last_http_msg.length);
                DEBUG_PRINTF("Header len %u\r\n", strlen(build_http_header(m_last_http_msg.length)));
#else
            ((gsm_http_data_t *)data)->header = (uint8_t *)"";
#endif
            }
        }
    }
    break;

    case GSM_HTTP_GET_EVENT_FINISH_SUCCESS:
    {
        DEBUG_PRINTF("HTTP get : event success\r\n");
        ctx->status.disconnect_timeout_s = 0;
        gsm_change_state(GSM_STATE_OK);
        DEBUG_PRINTF("Free um memory, malloc count[%u]\r\n", m_malloc_count);
        LED1(0);
    }
    break;

    case GSM_HTTP_POST_EVENT_FINISH_SUCCESS:
    {
        DEBUG_PRINTF("HTTP post : event success\r\n");
        ctx->status.disconnect_timeout_s = 0;
        if (m_last_http_msg.pointer)
        {
            m_malloc_count--;
            umm_free(m_last_http_msg.pointer);
            DEBUG_PRINTF("Free um memory, malloc count[%u]\r\n", m_malloc_count);
            m_last_http_msg.pointer = NULL;
        }
        LED1(0);
        gsm_change_state(GSM_STATE_OK);
    }
    break;

    case GSM_HTTP_POST_EVENT_FINISH_FAILED:
    case GSM_HTTP_EVENT_FINISH_FAILED:
    {
        DEBUG_PRINTF("HTTP event failed\r\n");
        if (m_last_http_msg.pointer)
        {
            m_malloc_count--;
            umm_free(m_last_http_msg.pointer);
            DEBUG_PRINTF("Free um memory, malloc count[%u]\r\n", m_malloc_count);
            m_last_http_msg.pointer = NULL;
        }
        gsm_change_state(GSM_STATE_OK);
    }
    break;

    default:
        DEBUG_PRINTF("Unknown http event %d\r\n", (int)event);
        gsm_change_state(GSM_STATE_OK);
        break;
    }
}

gsm_internet_mode_t *gsm_get_internet_mode(void)
{
    return &m_internet_mode;
}

uint32_t gsm_get_current_tick(void)
{
    return sys_get_ms();
}


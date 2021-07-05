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
#include "app_debug.h"
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
#include "app_spi_flash.h"
#include "spi.h"

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
#define GSM_ENTER_HTTP_POST()       (m_enter_http_post = true)

#define POST_URL        "%s/api/v1/%s/telemetry"
#define GET_URL         "%s/api/v1/%s/attributes"
      

extern gsm_manager_t gsm_manager;
static char m_at_cmd_buffer[128];

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
static uint32_t m_malloc_count = 0;

static char *m_last_http_msg = NULL;
static app_spi_flash_data_t *m_retransmision_data_in_flash;

#if GSM_READ_SMS_ENABLE
bool m_do_read_sms = false;

void gsm_set_flag_prepare_enter_read_sms_mode(void)
{
    m_do_read_sms = true;
}
#endif

void gsm_wakeup_periodically(void)
{
	sys_ctx_t *ctx = sys_ctx();
	app_eeprom_config_data_t *cfg = app_eeprom_read_config_data();
    DEBUG_PRINTF("Sleep time %usecond, periodic send msg %us, remaining %us\r\n",
                 ctx->status.sleep_time_s,
                 cfg->send_to_server_interval_ms/1000,
                 cfg->send_to_server_interval_ms/1000 - ctx->status.sleep_time_s);

    if (ctx->status.sleep_time_s*1000 >= cfg->send_to_server_interval_ms)
    {
        ctx->status.sleep_time_s = 0;
        gsm_change_state(GSM_STATE_WAKEUP);
    }
}

void gsm_set_wakeup_now(void)
{
	sys_ctx_t *ctx = sys_ctx();
	ctx->status.sleep_time_s = 0;
	gsm_change_state(GSM_STATE_WAKEUP);
}

#if GSM_READ_SMS_ENABLE
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

            /* If retry > 3 =>> delete from queue */
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
#endif

volatile uint32_t m_delay_wait_for_measurement_again_s = 0;
void gsm_manager_tick(void)
{
	sys_ctx_t *ctx = sys_ctx();

    /* GSM state machine */
    switch (gsm_manager.state)
    {
    case GSM_STATE_POWER_ON:
        if (gsm_manager.step == 0)
        {
            gsm_manager.step = 1;
            gsm_hw_send_at_cmd("ATV1\r\n", "OK\r\n", "", 1000, 30, gsm_at_cb_power_on_gsm);
        }
        break;

    case GSM_STATE_OK:
    {
#if GSM_READ_SMS_ENABLE
        if (m_do_read_sms)
        {
            m_do_read_sms = false;
            gsm_change_state(GSM_STATE_READ_SMS);
        }
#endif
        gsm_wakeup_periodically();
#if GSM_READ_SMS_ENABLE
        if (gsm_manager.state == GSM_STATE_OK)
        {
            gsm_query_sms_buffer();
        }
#endif
        if (gsm_manager.state == GSM_STATE_OK)      // gsm state maybe changed in gsm_query_sms_buffer task
        {
            bool enter_sleep_in_http = true;
            bool enter_post = false;
            if ((measure_input_sensor_data_availble()
                || m_retransmision_data_in_flash)
                && !ctx->status.enter_ota_update)
            {
                enter_post = true;
            }
            if (enter_post)
            {
                DEBUG_PRINTF("Post http data\r\n");
                GSM_ENTER_HTTP_POST();
                enter_sleep_in_http = false;
                gsm_change_state(GSM_STATE_HTTP_POST);
            }
            else
            {
                DEBUG_PRINTF("Queue empty\r\n");
                if (gsm_manager.state == GSM_STATE_OK)
                {
                    if (GSM_NEED_ENTER_HTTP_GET() || ctx->status.enter_ota_update)
                    {
                        ctx->status.delay_ota_update = 0;
                        gsm_change_state(GSM_STATE_HTTP_GET);
                        enter_sleep_in_http = false;
                        m_enter_http_get = true;
                    }
                }
            }
            if (m_delay_wait_for_measurement_again_s > 0)
            {
                m_delay_wait_for_measurement_again_s--;
                if (m_delay_wait_for_measurement_again_s == 1)
                {
                    if (measure_input_sensor_data_availble())
                        m_delay_wait_for_measurement_again_s = 0;       
                }
                enter_sleep_in_http = false;
            }
			//#warning "Sleep in http mode is not enabled"
			if (enter_sleep_in_http)
			{
                gsm_hw_layer_reset_rx_buffer();
				gsm_change_state(GSM_STATE_SLEEP);
			}
        }
    }
    break;

    case GSM_STATE_RESET: /* Hard Reset */
        gsm_manager.gsm_ready = 0;
        gsm_hard_reset();
        break;
    
#if GSM_READ_SMS_ENABLE
    case GSM_STATE_READ_SMS: /* Read SMS */
        if (gsm_manager.step == 0)
        {
            gsm_enter_read_sms();
        }
        break;
#endif
    
    case GSM_STATE_SEND_SMS: /* Send SMS */
    {
        if (!gsm_manager.gsm_ready)
            break;

        if (gsm_manager.step == 0)
        {
            DEBUG_PRINTF("Enter send sms cb\r\n");
            gsm_manager.step = 1;
            gsm_hw_send_at_cmd("ATV1\r\n", "OK\r\n", "", 100, 1, gsm_at_cb_send_sms);
        }
    }
        break;

    case GSM_STATE_HTTP_POST:
    {
        if (GSM_NEED_ENTER_HTTP_POST())
        {
            GSM_DONT_NEED_HTTP_POST();
            static gsm_http_config_t cfg;
            snprintf(cfg.url, GSM_HTTP_MAX_URL_SIZE, POST_URL,
                    (char*)app_eeprom_read_config_data()->server_addr,
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
            if (!sys_ctx()->status.enter_ota_update)
            {
                snprintf(cfg.url, GSM_HTTP_MAX_URL_SIZE, 
                        GET_URL, 
                        (char*)app_eeprom_read_config_data()->server_addr,
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
                snprintf(cfg.url, GSM_HTTP_MAX_URL_SIZE, "%s", ctx->status.ota_url);
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
        usart1_control(false);
        GSM_PWR_EN(0);
        GSM_PWR_RESET(0);
        GSM_PWR_KEY(0);
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
    if (m_is_the_first_time)
    {
        m_is_the_first_time = false;
        umm_init();
        DEBUG_PRINTF("HTTP: init buffer\r\n");
    }
}

void gsm_data_layer_initialize(void)
{
    gsm_manager.ri_signal = 0;

    gsm_http_cleanup();
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
    }
    DEBUG_PRINTF("Change GSM state to: ");
    switch ((uint8_t)new_state)
    {
    case GSM_STATE_OK:
        DEBUG_RAW("OK\r\n");
        break;
    case GSM_STATE_RESET:
        DEBUG_RAW("RESET\r\n");
        gsm_hw_layer_reset_rx_buffer();
        break;
    case GSM_STATE_SEND_SMS:
        DEBUG_RAW("SEND SMS\r\n");
        break;
    case GSM_STATE_READ_SMS:
        DEBUG_RAW("READ SMS\r\n");
        break;
    case GSM_STATE_POWER_ON:
        DEBUG_RAW("POWERON\r\n");
        gsm_hw_layer_reset_rx_buffer();
        break;
    case GSM_STATE_REOPEN_PPP:
        DEBUG_RAW("REOPENPPP\r\n");
        break;
    case GSM_STATE_GET_BTS_INFO:
        DEBUG_RAW("GETSIGNAL\r\n");
        break;
//    case GSM_STATE_SEND_ATC:
//        DEBUG_RAW("Quit PPP and send AT command\r\n");
//        break;
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
    {
        DEBUG_RAW("SLEEP\r\n");
        sys_ctx_t *ctx = sys_ctx();
        ctx->peripheral_running.name.gsm_running = 0;
    }
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
        if (event != GSM_EVENT_OK)
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

                sprintf(m_at_cmd_buffer, "AT+CMGS=\"%s\"\r\n", sms[count].phone_number);

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


//static LargeBuffer_t m_http_buffer;
/* {
	"Timestamp":"1623849775","ID":"860262050129720","PhoneNum":"000","Money":"0","Input1":"5558",
	"Input2":"0.0","Output1":"0","Output2":"0","SignalStrength":"100","WarningLevel":"0",
	"BatteryLevel":"77","BatteryDebug":"4086","K":"1","Offset":"5480"}
 */
static uint16_t gsm_build_sensor_msq(char *ptr, measurement_msg_queue_t *msg)
{
    app_eeprom_config_data_t *cfg = app_eeprom_read_config_data();
	measure_input_perpheral_data_t *measure_input = measure_input_current_data();
	
	char alarm_str[32];
    char *p = alarm_str;
    uint16_t total_length = 0;

    // bat,temp,4glost,sensor_err,sensor_overflow, sensor break, vtemp is valid or not
    if (msg->vbat_percent < 10)
    {
        p += sprintf(p, "%u,", 1);
    }
    else
    {
        p += sprintf(p, "%u,", 0);
    }
    
    //4glost,sensor_err,sensor_overflow,sensor_break,flash_err, vtemp is valid or not
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
	
	p += sprintf(p, "%u,", found_break_pulse_input ? 1 : 0);
    p += sprintf(p, "%u,", app_spi_flash_is_ok() ? 0 : 1);
    p += sprintf(p, "%u", measure_input->temperature_error ? 0 : 1);
    
    total_length += sprintf((char *)ptr, "{\"Timestamp\":\"%u\",", msg->measure_timestamp); //second since 1970
    
    uint32_t temp_counter;

#ifdef DTG02
	msg->counter1_f = msg->counter1_f / cfg->k1 + cfg->offset1;
    total_length += sprintf((char *)(ptr + total_length), "\"ID\":\"DTG2-%s\",", gsm_get_module_imei());
#else
    total_length += sprintf((char *)(ptr + total_length), "\"ID\":\"DTG1-%s\",", gsm_get_module_imei());
#endif
    total_length += sprintf((char *)(ptr + total_length), "\"PhoneNum\":\"%s\",", cfg->phone);
    total_length += sprintf((char *)(ptr + total_length), "\"Money\":\"%d\",", 0);
       
#ifdef DTG02
    temp_counter = msg->counter0_f / cfg->k0 + cfg->offset0;
    total_length += sprintf((char *)(ptr + total_length), "\"Input1_J1\":\"%u\",",
                              temp_counter);
    
    temp_counter = msg->counter1_f / cfg->k1 + cfg->offset1;
    total_length += sprintf((char *)(ptr + total_length), "\"Input1_J2\":\"%u\",",
                              temp_counter); //so xung
	
	if (cfg->meter_mode[0] == APP_EEPROM_METER_MODE_PWM_F_PWM_R)
	{
        temp_counter = msg->counter0_r / cfg->k0 + cfg->offset0;
		total_length += sprintf((char *)(ptr + total_length), "\"Input1_J1_DIR\":\"%u\",",
									temp_counter);
	}
	
    temp_counter = msg->counter1_r / cfg->k1 + cfg->offset1;
	if (cfg->meter_mode[1] == APP_EEPROM_METER_MODE_PWM_F_PWM_R)
	{
		total_length += sprintf((char *)(ptr + total_length), "\"Input1_J2_DIR\":\"%u\",",
									temp_counter);
	}

	for (uint32_t i = 0; i < NUMBER_OF_INPUT_4_20MA; i++)
	{
		total_length += sprintf((char *)(ptr + total_length), "\"Input1_J3_%u\":\"%u\",", 
																			i,
																			msg->input_4_20ma[i]); // dau vao 4-20mA 0
	}
	for (uint32_t i = 0; i < NUMBER_OF_INPUT_ON_OFF; i++)
	{
		total_length += sprintf((char *)(ptr + total_length), "\"Input1_J9_%u\":\"%u\",", 
																			i,
																			measure_input->input_on_off[i]); // dau vao 4-20mA 0
	}	

	for (uint32_t i = 0; i < NUMBER_OF_OUT_ON_OFF; i++)
	{
		total_length += sprintf((char *)(ptr + total_length), "\"Output%u\":\"%u\",", 
																			i,
																			measure_input->output_on_off[i]); // dau vao 4-20mA 0
	}
    total_length += sprintf((char *)(ptr + total_length), "\"Output4_20mA\":\"%d\",", measure_input->output_4_20mA);    //dau ra on/off
#else	
    temp_counter = msg->counter0_f / cfg->k0 + cfg->offset0;
    total_length += sprintf((char *)(ptr + total_length), "\"Input1\":%u,",
                              temp_counter); //so xung
    
    if (cfg->meter_mode[0] == APP_EEPROM_METER_MODE_PWM_F_PWM_R)
	{
        temp_counter = msg->counter0_r / cfg->k0 + cfg->offset0;
		total_length += sprintf((char *)(ptr + total_length), "\"Input1_J1_DIR\":%u,",
									temp_counter);
	}
    
    total_length += sprintf((char *)(ptr + total_length), "\"Output1\":%u,", 
                                                                        msg->input_4_20ma[0]); // dau vao 4-20mA 0

    total_length += sprintf((char *)(ptr + total_length), "\"Output2\":%u,", 
                                                                        measure_input->output_4_20mA); // dau ra 4-20mA 0
#endif
    if (msg->csq_percent)
    {
        total_length += sprintf((char *)(ptr + total_length), "\"SignalStrength\":%d,", msg->csq_percent);
    }
    total_length += sprintf((char *)(ptr + total_length), "\"WarningLevel\":\"%s\",", alarm_str);

    total_length += sprintf((char *)(ptr + total_length), "\"BatteryLevel\":%d,", msg->vbat_percent);
    
    if (!measure_input->temperature_error)
    {
        total_length += sprintf((char *)(ptr + total_length), "\"Temperature\":%d,", measure_input->temperature);
    }
    
    total_length += sprintf((char *)(ptr + total_length), "\"Vbat_mv\":%u,", msg->vbat_mv);
    total_length += sprintf((char *)(ptr + total_length), "\"RST\":%u,", hardware_manager_get_reset_reason()->value);
    total_length += sprintf((char *)(ptr + total_length), "\"K0\":%u,", cfg->k0);
    total_length += sprintf((char *)(ptr + total_length), "\"Offset0\":%u,", cfg->offset0);
    total_length += sprintf((char *)(ptr + total_length), "\"Mode0\":%u,", cfg->meter_mode[0]);
    
#ifdef DTG02
    total_length += sprintf((char *)(ptr + total_length), "\"K1\":\"%u\",", cfg->k1);
    total_length += sprintf((char *)(ptr + total_length), "\"Offset1\":\"%u\",", cfg->offset1);
    total_length += sprintf((char *)(ptr + total_length), "\"Mode1\":\"%u\",", cfg->meter_mode[1]);
#endif
    
    total_length += sprintf((char *)(ptr + total_length), "\"FW\":\"%s\",", VERSION_CONTROL_FW);
    total_length += sprintf((char *)(ptr + total_length), "\"HW\":\"%s\"}", VERSION_CONTROL_HW);
        
//    hardware_manager_get_reset_reason()->value = 0;

    DEBUG_RAW("%s\r\n", (char*)ptr);
    return total_length;
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
static measurement_msg_queue_t *m_sensor_msq;

static void gsm_http_event_cb(gsm_http_event_t event, void *data)
{
	sys_ctx_t *ctx = sys_ctx();
    sys_turn_on_led(3);
    switch (event)
    {
    case GSM_HTTP_EVENT_START:
        DEBUG_PRINTF("HTTP task started\r\n");
        break;

    case GSM_HTTP_EVENT_CONNTECTED:
        DEBUG_PRINTF("HTTP connected, data size %u\r\n", *((uint32_t *)data));
        ctx->status.disconnect_timeout_s = 0;
        if (ctx->status.enter_ota_update)
        {
            ota_update_start(*((uint32_t*)data));
        }
        break;
    
    case GSM_HTTP_GET_EVENT_DATA:
    {
        gsm_http_data_t *get_data = (gsm_http_data_t *)data;
        if (!ctx->status.enter_ota_update)
        {
            DEBUG_PRINTF("DATA %u bytes: %s\r\n", get_data->data_length, get_data->data);
            uint8_t new_cfg = 0;
            server_msg_process_cmd((char *)get_data->data, &new_cfg);
            if (new_cfg)
            {
                measure_input_measure_wakeup_to_get_data();
                m_delay_wait_for_measurement_again_s = 10;
            }
        }
        else
        {
            ota_update_write_next((uint8_t*)get_data->data, get_data->data_length);
        }
    }
    break;

    case GSM_HTTP_POST_EVENT_DATA:
    {
        bool build_msg = false;
        DEBUG_PRINTF("Get http post data from queue\r\n");
        m_sensor_msq = measure_input_get_data_in_queue();
        if (!m_sensor_msq)
        {
            DEBUG_PRINTF("No more sensor data\r\n");
            if (!m_retransmision_data_in_flash)
            {
                gsm_change_state(GSM_STATE_OK);
            }
            else
            {
                static measurement_msg_queue_t tmp;
                tmp.counter0_f = m_retransmision_data_in_flash->meter_input[0].pwm_f;
                tmp.counter0_r = m_retransmision_data_in_flash->meter_input[0].dir_r;
                
#ifdef DTG02
                tmp.counter0_f = m_retransmision_data_in_flash->meter_input[1].pwm_f;
                tmp.counter0_r = m_retransmision_data_in_flash->meter_input[1].dir_r;  
#endif
                for (uint32_t i = 0; i < NUMBER_OF_INPUT_4_20MA; i++)
                {
                    tmp.input_4_20ma[i] = m_retransmision_data_in_flash->input_4_20mA[i];
                }
                tmp.csq_percent = 0;
                tmp.measure_timestamp = m_retransmision_data_in_flash->timestamp;
                tmp.temperature = m_retransmision_data_in_flash->temp;
                tmp.vbat_mv = m_retransmision_data_in_flash->vbat_mv;
                tmp.vbat_mv = m_retransmision_data_in_flash->vbat_precent;
                m_sensor_msq = &tmp;
                build_msg = true;
                DEBUG_PRINTF("Build retransmision data\r\n");
            }
        }
        else
        {
            build_msg = true;
        }
        
        if (build_msg)
        {
            if (m_last_http_msg)
            {
                umm_free(m_last_http_msg);
                m_malloc_count--;
            }
            m_last_http_msg = (char*)umm_malloc(384);
            if (m_last_http_msg == NULL)
            {
                DEBUG_ERROR("Malloc error\r\n");
                NVIC_SystemReset();
            }
            else
            {
                ++m_malloc_count;
                m_sensor_msq->state = MEASUREMENT_QUEUE_STATE_PROCESSING;
                gsm_build_sensor_msq(m_last_http_msg, m_sensor_msq);
                ((gsm_http_data_t *)data)->data = (uint8_t *)m_last_http_msg;
                ((gsm_http_data_t *)data)->data_length = strlen(m_last_http_msg);
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
        if (ctx->status.enter_ota_update 
            && ctx->status.delay_ota_update == 0)
        {
            GSM_PWR_EN(0);
            GSM_PWR_RESET(0);
            GSM_PWR_KEY(0);
            ota_update_finish(true);
        }
        gsm_change_state(GSM_STATE_OK);
        DEBUG_PRINTF("Free um memory, malloc count[%u]\r\n", m_malloc_count);
        LED1(0);
    }
    break;

    case GSM_HTTP_POST_EVENT_FINISH_SUCCESS:
    {
        DEBUG_PRINTF("HTTP post : event success\r\n");
        ctx->status.disconnect_timeout_s = 0;
        if (m_last_http_msg)
        {
            m_malloc_count--;
            umm_free(m_last_http_msg);
            DEBUG_PRINTF("Free um memory, malloc count[%u]\r\n", m_malloc_count);
            m_last_http_msg = NULL;
        }
        LED1(0);
        app_spi_flash_data_t wr_data;
        wr_data.header_overlap_detect = APP_FLASH_DATA_HEADER_KEY;
        wr_data.resend_to_server_flag = APP_FLASH_DONT_NEED_TO_SEND_TO_SERVER_FLAG;
        for (uint32_t i = 0; i < APP_FLASH_NB_OFF_4_20MA_INPUT; i++)
        {
            wr_data.input_4_20mA[i] = m_sensor_msq->input_4_20ma[i];
        }
        wr_data.meter_input[0].pwm_f = m_sensor_msq->counter0_f;
        wr_data.meter_input[0].dir_r = m_sensor_msq->counter0_r;
        
#ifdef DTG02
        wr_data.meter_input[1].pwm_f = m_sensor_msq->counter1_f;
        wr_data.meter_input[1].dir_r = m_sensor_msq->counter1_r;
#endif        
        wr_data.timestamp = m_sensor_msq->measure_timestamp;
        wr_data.valid_flag = APP_FLASH_VALID_DATA_KEY;
        wr_data.vbat_mv = m_sensor_msq->vbat_mv;
        wr_data.vbat_precent = m_sensor_msq->vbat_percent;
        wr_data.temp = m_sensor_msq->temperature;
        
        if (!ctx->peripheral_running.name.flash_running)
        {
            DEBUG_PRINTF("Wakup flash\r\n");
            spi_init();
            app_spi_flash_wakeup();
            ctx->peripheral_running.name.flash_running = 1;
        }
        app_spi_flash_write_data(&wr_data);
        
        #warning "Please save RS485 to flash\r\n";
//        data.rs485;
        
        m_sensor_msq->state = MEASUREMENT_QUEUE_STATE_IDLE;
        m_sensor_msq = NULL;
        
        bool retransmition;
        static app_spi_flash_data_t rd_data;
        if (!ctx->peripheral_running.name.flash_running)
        {
            spi_init();
            app_spi_flash_wakeup();
            ctx->peripheral_running.name.flash_running = 1;
        }
        uint32_t addr = app_spi_flash_estimate_current_read_addr(&retransmition);
        if (retransmition)
        {
            if (!app_spi_flash_get_retransmission_data(addr, &rd_data))
            {
                gsm_change_state(GSM_STATE_OK);
                m_retransmision_data_in_flash = NULL;
            }
            else
            {
                m_retransmision_data_in_flash = &rd_data;
                GSM_ENTER_HTTP_POST();
            }
        }
        else
        {
            gsm_change_state(GSM_STATE_OK);
            DEBUG_PRINTF("No more data need to re-send to server\r\n");
            m_retransmision_data_in_flash = NULL;
        }
    }
    break;

    case GSM_HTTP_POST_EVENT_FINISH_FAILED:
    {
        app_spi_flash_data_t wr_data;
        wr_data.header_overlap_detect = APP_FLASH_DATA_HEADER_KEY;
        wr_data.resend_to_server_flag = 0;
        for (uint32_t i = 0; i < APP_FLASH_NB_OFF_4_20MA_INPUT; i++)
        {
            wr_data.input_4_20mA[i] = m_sensor_msq->input_4_20ma[i];
        }
        wr_data.meter_input[0].pwm_f = m_sensor_msq->counter0_f;
        wr_data.meter_input[0].dir_r = m_sensor_msq->counter0_r;
        
#ifdef DTG02
        wr_data.meter_input[1].pwm_f = m_sensor_msq->counter1_f;
        wr_data.meter_input[1].dir_r = m_sensor_msq->counter1_r;
#endif        
        wr_data.timestamp = m_sensor_msq->measure_timestamp;
        wr_data.valid_flag = APP_FLASH_VALID_DATA_KEY;
        wr_data.vbat_mv = m_sensor_msq->vbat_mv;
        wr_data.vbat_precent = m_sensor_msq->vbat_percent;
        wr_data.temp = m_sensor_msq->temperature;
        
        if (!ctx->peripheral_running.name.flash_running)
        {
            spi_init();
            app_spi_flash_wakeup();
            ctx->peripheral_running.name.flash_running = 1;
        }
        app_spi_flash_write_data(&wr_data);
        
        #warning "Please save RS485 to flash\r\n";
//        data.rs485;
        
        m_sensor_msq->state = MEASUREMENT_QUEUE_STATE_IDLE;
        m_sensor_msq = NULL;
    }
    case GSM_HTTP_GET_EVENT_FINISH_FAILED:
    {
        DEBUG_PRINTF("HTTP event failed\r\n");
        if (ctx->status.enter_ota_update 
            && ctx->status.delay_ota_update == 0)
        {
            ota_update_finish(false);
            GSM_PWR_EN(0);
            GSM_PWR_RESET(0);
            GSM_PWR_KEY(0);
        }
        if (m_last_http_msg)
        {
            m_malloc_count--;
            umm_free(m_last_http_msg);
            DEBUG_PRINTF("Free um memory, malloc count[%u]\r\n", m_malloc_count);
            m_last_http_msg = NULL;
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

uint32_t gsm_get_current_tick(void)
{
    return sys_get_ms();
}


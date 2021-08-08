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
#include "umm_malloc_cfg.h"

#ifdef STM32L083xx
#include "usart.h"
#include "app_rtc.h"
#endif

#define UNLOCK_BAND     1
#define CUSD_ENABLE     0
#define MAX_TIMEOUT_TO_SLEEP_S 60
#define GSM_NEED_ENTER_HTTP_GET() (m_enter_http_get)
#define GSM_DONT_NEED_HTTP_GET() (m_enter_http_get = false)
#define GSM_NEED_ENTER_HTTP_POST() (m_enter_http_post)
#define GSM_DONT_NEED_HTTP_POST() (m_enter_http_post = false)
#define GSM_ENTER_HTTP_POST()       (m_enter_http_post = true)

#define POST_URL        		"%s/api/v1/%s/telemetry"
#define GET_URL         		"%s/api/v1/%s/attributes"
#define POLL_CONFIG_URL      	"%s/api/v1/default_imei/attributes"

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
uint32_t m_wake_time = 0;

#if GSM_READ_SMS_ENABLE
bool m_do_read_sms = false;

void gsm_set_flag_prepare_enter_read_sms_mode(void)
{
    m_do_read_sms = true;
}
#endif

uint32_t estimate_wakeup_time = 0;
void gsm_wakeup_periodically(void)
{
	sys_ctx_t *ctx = sys_ctx();
	app_eeprom_config_data_t *cfg = app_eeprom_read_config_data();
    uint32_t current_sec = app_rtc_get_counter();
    uint32_t send_interval = cfg->send_to_server_interval_ms/1000;
    if (send_interval == 0)
    {
        send_interval = 1;
    }
    
    if (estimate_wakeup_time == 0)
    {
        estimate_wakeup_time = (send_interval*(current_sec/send_interval + 1) + cfg->send_to_server_delay_s);
    }
//    rtc_date_time_t time;
//    if (app_rtc_get_time(&time))
//    {
////		uint32_t remain = estimate_wakeup_time - current_sec;
////        DEBUG_VERBOSE("[%02u:%02u:%02u] Send to server in %umin, %u sec\r\n",
////                    time.hour,
////                    time.minute,
////                    time.second,
////                    remain/60,
////					remain % 60);
//    }
//    else
//    {
//        DEBUG_WARN("Cannot get date time\r\n");
//    }   

    
//    DEBUG_VERBOSE("Current sec %us\r\n", current_sec);
    if (current_sec >= estimate_wakeup_time)
    {
        if (gsm_data_layer_is_module_sleeping())
        {
            gsm_change_state(GSM_STATE_WAKEUP);
//            ctx->status.sleep_time_s = 0;
        }
    }
}

void gsm_set_wakeup_now(void)
{
	sys_ctx_t *ctx = sys_ctx();
//	ctx->status.sleep_time_s = 0;
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
            bool ready_to_sleep = true;
            bool enter_post = false;
			
			// If sensor data avaible =>> Enter http post
            if ((measure_input_sensor_data_availble()
                || m_retransmision_data_in_flash)
                && !ctx->status.enter_ota_update)
            {
                enter_post = true;
            }
			
            if (enter_post)
            {
                GSM_ENTER_HTTP_POST();
                ready_to_sleep = false;
                gsm_change_state(GSM_STATE_HTTP_POST);
            }
            else	// Enter http get
            {
                DEBUG_INFO("Queue empty\r\n");
                if (gsm_manager.state == GSM_STATE_OK)
                {
					// If device need to get data to server, or need to update firmware, or need to try to new server =>> Enter http get
                    if (GSM_NEED_ENTER_HTTP_GET() 
						|| ctx->status.enter_ota_update
						|| (ctx->status.new_server && ctx->status.try_new_server))
                    {
                        ctx->status.delay_ota_update = 0;
                        gsm_change_state(GSM_STATE_HTTP_GET);
                        ready_to_sleep = false;
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
                ready_to_sleep = false;
            }
			
			
			if (ready_to_sleep)
			{
				uint32_t current_sec = app_rtc_get_counter();
				if (current_sec >= ctx->status.next_time_get_data_from_server)
				{
					ctx->status.next_time_get_data_from_server = current_sec + app_eeprom_read_config_data()->poll_config_interval_hour*3600;
					ctx->status.poll_broadcast_msg_from_server = 1;
					DEBUG_INFO("Poll server config\r\n");
					gsm_change_state(GSM_STATE_HTTP_GET);
					m_enter_http_get = 1;
				}
				else
				{
					ctx->status.poll_broadcast_msg_from_server = 0;
					gsm_hw_layer_reset_rx_buffer();
					gsm_change_state(GSM_STATE_SLEEP);
				}
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
//            DEBUG_VERBOSE("Enter send sms cb\r\n");
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
			char *server_addr = (char*)app_eeprom_read_config_data()->server_addr[APP_EEPROM_MAIN_SERVER_ADDR_INDEX];
			// If main server addr is not valid =>> switch to new server addr
			// TODO check new server
			// But device has limited memory size =>>
			if (strlen(server_addr) == 0)
			{
				server_addr = (char*)app_eeprom_read_config_data()->server_addr[APP_EEPROM_ALTERNATIVE_SERVER_ADDR_INDEX];
			}
			
            snprintf(cfg.url, GSM_HTTP_MAX_URL_SIZE, POST_URL,
                    server_addr,
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
		app_eeprom_config_data_t *eeprom_cfg = app_eeprom_read_config_data();
        if (GSM_NEED_ENTER_HTTP_GET())
        {
            static gsm_http_config_t cfg;
			char *server_addr = (char*)eeprom_cfg->server_addr[APP_EEPROM_MAIN_SERVER_ADDR_INDEX];
			if (strlen(server_addr) == 0)
			{
				DEBUG_INFO("We use alternative server\r\n");
				server_addr = (char*)eeprom_cfg->server_addr[APP_EEPROM_ALTERNATIVE_SERVER_ADDR_INDEX];
			}
			// Peridic update config data from server
			if (sys_ctx()->status.poll_broadcast_msg_from_server)
			{	
                snprintf(cfg.url, GSM_HTTP_MAX_URL_SIZE, 
                        POLL_CONFIG_URL, 
                        server_addr);
                cfg.on_event_cb = gsm_http_event_cb;
                cfg.action = GSM_HTTP_ACTION_GET;
                cfg.big_file_for_ota = 0;
                gsm_http_start(&cfg);
                GSM_DONT_NEED_HTTP_GET();
			}
			// If device not need to ota update =>> Enter mode get data from server
			else if (!sys_ctx()->status.enter_ota_update
				&& !ctx->status.try_new_server)
            {
                snprintf(cfg.url, GSM_HTTP_MAX_URL_SIZE, 
                        GET_URL, 
                        server_addr,
                        gsm_get_module_imei());
                cfg.on_event_cb = gsm_http_event_cb;
                cfg.action = GSM_HTTP_ACTION_GET;
                cfg.big_file_for_ota = 0;
                gsm_http_start(&cfg);
                GSM_DONT_NEED_HTTP_GET();
            }
			else if (ctx->status.new_server && ctx->status.try_new_server)		// Try new server address
			{
				snprintf(cfg.url, GSM_HTTP_MAX_URL_SIZE, 
						GET_URL, 
						ctx->status.new_server,
						gsm_get_module_imei());
                cfg.on_event_cb = gsm_http_event_cb;
                cfg.action = GSM_HTTP_ACTION_GET;
                cfg.big_file_for_ota = 0;
                gsm_http_start(&cfg);
                GSM_DONT_NEED_HTTP_GET();
			}
			// Device is in ota update
            else if (sys_ctx()->status.enter_ota_update)
            {
                snprintf(cfg.url, GSM_HTTP_MAX_URL_SIZE, "%s", ctx->status.ota_url);
                cfg.on_event_cb = gsm_http_event_cb;
                cfg.action = GSM_HTTP_ACTION_GET;
//                cfg.port = 443;
                cfg.big_file_for_ota = 1;
                gsm_http_start(&cfg);
                GSM_DONT_NEED_HTTP_GET();
            }
			else
			{
				gsm_change_state(GSM_STATE_OK);
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
    {
        gsm_wakeup_periodically();      // Must be called before estimate wakeup time
        usart1_control(false);
        GSM_PWR_EN(0);
        GSM_PWR_RESET(0);
        GSM_PWR_KEY(0);
    }
        break;

    default:
        DEBUG_ERROR("Unhandled case %u\r\n", gsm_manager.state);
        break;
    }
}


static bool m_is_the_first_time = true;
static void init_http_msq(void)
{
    if (m_is_the_first_time)
    {
        m_is_the_first_time = false;
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
    DEBUG_INFO("Change GSM state to: ");
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
		m_wake_time = app_bkup_read_nb_of_wakeup_time();
		m_wake_time++;
		app_bkup_write_nb_of_wakeup_time(m_wake_time);
        break;
//    case GSM_STATE_REOPEN_PPP:
//        DEBUG_VERBOSE("REOPENPPP\r\n");
//        break;
//    case GSM_STATE_GET_BTS_INFO:
//        DEBUG_VERBOSE("GETSIGNAL\r\n");
//        break;
//    case GSM_STATE_SEND_ATC:
//        DEBUG_RAW("Quit PPP and send AT command\r\n");
//        break;
    case GSM_STATE_GOTO_SLEEP:
        DEBUG_RAW("Prepare sleep\r\n");
        break;
    case GSM_STATE_WAKEUP:
        DEBUG_RAW("WAKEUP\r\n");
        break;
//    case GSM_STATE_AT_MODE_IDLE:
//        DEBUG_VERBOSE("IDLE\r\n");
//        break;
    case GSM_STATE_SLEEP:
    {
        DEBUG_RAW("SLEEP\r\n");
        sys_ctx_t *ctx = sys_ctx();
        ctx->peripheral_running.name.gsm_running = 0;
        gsm_hw_layer_reset_rx_buffer();
        app_eeprom_config_data_t *cfg = app_eeprom_read_config_data();
        uint32_t current_sec = app_rtc_get_counter();
        uint32_t send_interval = cfg->send_to_server_interval_ms/1000 + cfg->send_to_server_delay_s;
        if (send_interval == 0)
        {
            send_interval = 1;
        }
        estimate_wakeup_time = send_interval*(current_sec/send_interval + 1) + cfg->send_to_server_delay_s;
//        DEBUG_VERBOSE("Estimate next wakeup time %us\r\n", estimate_wakeup_time);
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

#if UNLOCK_BAND
uint8_t m_unlock_band_step = 0;
void do_unlock_band(gsm_response_event_t event, void *resp_buffer)
{
    switch(m_unlock_band_step)
    {
        case 0:
        {
//            if(xSystem.Parameters.GSM_Mode == GSM_MODE_2G_ONLY)
//              SendATCommand ("AT+QCFG=\"nwscanseq\",1\r", "OK", 1000, 10, PowerOnModuleGSM);
//            else if(xSystem.Parameters.GSM_Mode == GSM_MODE_4G_ONLY)
//              SendATCommand ("AT+QCFG=\"nwscanseq\",3,1\r", "OK", 1000, 10, PowerOnModuleGSM);
//            else
                gsm_hw_send_at_cmd("AT+QCFG=\"nwscanseq\",3,1\r\n", "OK\r\n", "", 1000, 10, do_unlock_band);
        }
            break;
        
        case 1:
        {
//            DEBUG_INFO("Setup network scan sequence: %s\r\n",(event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]");
//            if(xSystem.Parameters.GSM_Mode == GSM_MODE_2G_ONLY)
//              SendATCommand ("AT+QCFG=\"nwscanmode\",1\r", "OK", 1000, 10, PowerOnModuleGSM);	
//            else if(xSystem.Parameters.GSM_Mode == GSM_MODE_4G_ONLY)
//              SendATCommand ("AT+QCFG=\"nwscanmode\",3,1\r", "OK", 1000, 10, PowerOnModuleGSM);	
//            else
                gsm_hw_send_at_cmd("AT+QCFG=\"nwscanmode\",3,1\r\n", "OK\r\n", "", 1000, 10, do_unlock_band);			
        }
                break;
        
        case 2:
        {
//            DEBUG_INFO("Setup network scan mode: %s\r\n",(event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]");
            gsm_hw_send_at_cmd("AT+QCFG=\"band\",00,45\r\n", "OK\r\n", "", 1000, 10, do_unlock_band);
        }
            break;
        
        case 3:
        {
            DEBUG_INFO("Unlock band: %s\r\n",(event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]");
            m_unlock_band_step = 0;
            gsm_hw_send_at_cmd("AT+CPIN?\r\n", "+CPIN: READY\r\n", "", 1000, 10, gsm_at_cb_power_on_gsm);
            break;
        }
        
        default:
            break;
    }
    m_unlock_band_step++;
}
#endif /* UNLOCK_BAND */

void gsm_at_cb_power_on_gsm(gsm_response_event_t event, void *resp_buffer)
{
    //DEBUG_PRINTF("%s\r\n", __FUNCTION__);
    switch (gsm_manager.step)
    {
    case 1:
        if (event != GSM_EVENT_OK)
        {
            DEBUG_ERROR("Connect modem ERR!\r\n");
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
        DEBUG_INFO("Set URC port: %s\r\n", (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]");
        gsm_hw_send_at_cmd("AT+QCFG=\"urc/ri/smsincoming\",\"pulse\",2000\r\n", "OK\r\n", "", 1000, 10, gsm_at_cb_power_on_gsm);
        break;

    case 6:
        DEBUG_INFO("Set URC ringtype: %s\r\n", (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]");
        gsm_hw_send_at_cmd("AT+CNMI=2,1,0,0,0\r\n", "", "OK\r\n", 1000, 10, gsm_at_cb_power_on_gsm);
        break;
    
    case 7:
        DEBUG_INFO("Config SMS event report: %s\r\n", (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]");
        gsm_hw_send_at_cmd("AT+CMGF=1\r\n", "", "OK\r\n", 1000, 10, gsm_at_cb_power_on_gsm);
        break;

    case 8:
        DEBUG_INFO("Set SMS text mode: %s\r\n", (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]");
        //gsm_hw_send_at_cmd("AT+CMGD=1,4\r\n", "OK\r\n", "", 2000, 5, gsm_at_cb_power_on_gsm);
        gsm_hw_send_at_cmd("AT\r\n", "", "OK\r\n", 2000, 5, gsm_at_cb_power_on_gsm);
        break;

    case 9:
        DEBUG_INFO("AT cmd: %s\r\n", (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]");
//        DEBUG_PRINTF("Delete all SMS: %s\r\n", (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]");
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
            DEBUG_ERROR("SIM's not inserted!\r\n");
            gsm_manager.step = 10;
//            gsm_change_hw_polling_interval(10);
            gsm_hw_send_at_cmd("AT+CGSN\r\n", "", "OK\r\n", 1000, 5, gsm_at_cb_power_on_gsm);
//            return;
        }
        else
        {
            gsm_change_hw_polling_interval(5);
            gsm_hw_send_at_cmd("AT+QCCID\r\n", "QCCID", "OK\r\n", 1000, 3, gsm_at_cb_power_on_gsm);
        }
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
#if UNLOCK_BAND == 0        // o lan dau tien active sim, voi module ec200 thi phai active band, ec20 thi ko can
                            // neu ko active thi se ko reg dc vao nha mang
    case 14:
        DEBUG_PRINTF("De-activate PDP: %s\r\n", (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]");
        gsm_hw_send_at_cmd("AT+QCFG=\"nwscanmode\",0\r\n", "OK\r\n", "", 5000, 2, gsm_at_cb_power_on_gsm); // Select mode AUTO
        break;
#else
    case 14:
        DEBUG_INFO("De-activate PDP: %s\r\n", (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]");
        m_unlock_band_step = 0;
        gsm_hw_send_at_cmd("AT\r\n", "OK\r\n", "", 1000, 10, do_unlock_band);
        break;
#endif
    case 15:
#if UNLOCK_BAND == 0
        DEBUG_PRINTF("Network search mode AUTO: %s\r\n", (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]");
#else
        DEBUG_PRINTF("Unlock band: %s\r\n", (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]");
#endif
        gsm_hw_send_at_cmd("AT+CGDCONT=1,\"IP\",\"v-internet\"\r\n", "", "OK\r\n", 1000, 2, gsm_at_cb_power_on_gsm); /** <cid> = 1-24 */
        break;

    case 16:
        DEBUG_PRINTF("Define PDP context: %s\r\n", (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]");
        //			gsm_hw_send_at_cmd("AT+QIACT=1\r\n", "OK\r\n", "", 5000, 5, gsm_at_cb_power_on_gsm);	/** Bật QIACT lỗi gửi tin với 1 số SIM dùng gói cước trả sau! */
        gsm_hw_send_at_cmd("AT+CSCS=\"GSM\"\r\n", "OK\r\n", "", 1000, 1, gsm_at_cb_power_on_gsm);
        break;

    case 17:
        DEBUG_INFO("CSCS=GSM: %s\r\n", (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]");
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
            DEBUG_INFO("Network operator: %s\r\n", gsm_get_network_operator());
        }
        gsm_change_hw_polling_interval(5);
        gsm_hw_send_at_cmd("AT\r\n", "OK\r\n", "", 1000, 5, gsm_at_cb_power_on_gsm);
        break;

    case 21:
    {
//        DEBUG_PRINTF("Select QSCLK: %s\r\n", (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]");
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
        if (time.year > 20 && time.year < 40 && time.hour < 24) // if 23h40 =>> time.hour += 7 = 30h =>> invalid
                                                                // Lazy solution : do not update time from 17h
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
            DEBUG_WARN("GSM unhandled step %u\r\n", gsm_manager.step);
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
//            DEBUG_PRINTF("Exit sleep!");
            gsm_change_state(GSM_STATE_OK);
        }
        else
        {
//            DEBUG_PRINTF("Khong phan hoi lenh, reset module...");
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
    DEBUG_INFO("GSM hard reset step %d\r\n", step);

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

//    DEBUG_PRINTF("Debug SEND SMS : %u %u,%s\r\n", gsm_manager.step, event, resp_buffer);

    switch (gsm_manager.step)
    {
    case 1:
        for (count = 0; count < max_sms; count++)
        {
            if (sms[count].need_to_send == 2)
            {
                DEBUG_ERROR("Sms to %s. Content : %s\r\n",
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
//                    DEBUG_PRINTF("Sending sms in buffer %u\r\n", count);
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
            DEBUG_ERROR("SMS : Send sms failed\r\n");
            retry_count++;
            if (retry_count < 3)
            {
                gsm_change_state(GSM_STATE_SEND_SMS);
                return;
            }
            else
            {
                DEBUG_ERROR("Send sms failed many times, cancle\r\n");
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


/* DTG02
{
	"Timestamp": "1628275184",
	"ID": "G2-860262050125777",
	"Phone": "0916883454",
	"Money": 7649,
	"Inputl_J1": 7649,
	"Inputl_J3_1": 0.01,
	"Input1_J3_2": 0.00,
	"Input1_J3_3": 0.01,
	"Input1_J3_4 ": 0.01,
	"Input1_J9_1 ": 1,
	"Input1_g9_2 ": 1,
	"Inputl_J9_3 ": 1,
	"Input_J9_4 ": 71,
	"Output1 ": 0,
	"Output2": 0,
	"Output3": 0,
	"Output4": 0,
	"Output4_20": 0.0,
	"WarningLevel ": "0,0,0,0,1,0,1",
	"BatteryLevel ": 100,
	"Vin": 23.15,
	"Temperature ": 29,
	"Registerl_1 ": 64,
	"Unitl_1 ": "m3/s",
	"Register1_2 ": 339,
	"Unit1_2 ": "jun ",
	"Register2_1": 0.0000,
	"Unit2_1": "kg",
	"Register2_2": 0,
	"Unit2_2": "1it",
	"SIM": "452040700052210",
	"Uptime": "3",
	"FW": "0.0.5",
	"HW": "0.0.1"
}
 */
static uint16_t gsm_build_sensor_msq(char *ptr, measure_input_perpheral_data_t *msg)
{
    app_eeprom_config_data_t *eeprom_cfg = app_eeprom_read_config_data();
	measure_input_perpheral_data_t *measure_input = measure_input_current_data();
//    DEBUG_VERBOSE("Free mem %u bytes\r\n", umm_free_heap_size());
	sys_ctx_t *ctx = sys_ctx();
	bool found_break_pulse_input = false;
	char alarm_str[128];
    char *p = alarm_str;
    uint16_t total_length = 0;
	
	for (uint32_t i = 0; i < MEASURE_NUMBER_OF_WATER_METER_INPUT; i++)
	{
		if (measure_input->water_pulse_counter[i].line_break_detect)
		{
			found_break_pulse_input = true;
			break;
		}
	}

	total_length += sprintf((char *)(ptr + total_length), "%s", "{\"Error\":\"");

	if (found_break_pulse_input)
	{
		ctx->error_not_critical.detail.circuit_break = 1;
		total_length += sprintf((char *)(ptr + total_length), "%s", "cam_bien_xung_dut,");
	}
	else
	{
		ctx->error_not_critical.detail.circuit_break = 0;
	}
	

    // Build error msg
    if (msg->vbat_percent < 10)
    {
        total_length += sprintf((char *)(ptr + total_length), "%s", "pin_yeu,");
    }
    
	// Flash
	if (ctx->error_not_critical.detail.flash)
	{
		total_length += sprintf((char *)(ptr + total_length), "%s", "flash,");
	}

	// Nhiet do
	if (measure_input->temperature > 70)
	{
		total_length += sprintf((char *)(ptr + total_length), "%s", "nhiet_do_cao,");
	}

	
	// RS485
	if (ctx->error_not_critical.detail.rs485_err)
	{
		total_length += sprintf((char *)(ptr + total_length), "%s", "rs485,");
	}
	
	// Cam bien
	if (ctx->error_critical.detail.sensor_out_of_range)
	{
		total_length += sprintf((char *)(ptr + total_length), "%s", "qua_nguong,");
	}
	
	p[total_length - 1] = 0;
	total_length--;		// remove char ','
	
	total_length += sprintf((char *)(ptr + total_length), "%s", "\",");
	
    // Build timestamp
    total_length += sprintf((char *)(ptr + total_length), "\"Timestamp\":\"%u\",", msg->measure_timestamp); //second since 1970
    
    uint32_t temp_counter;

    // Build ID, phone and id
#ifdef DTG02
    total_length += sprintf((char *)(ptr + total_length), "\"ID\":\"G2-%s\",", gsm_get_module_imei());
#else
    total_length += sprintf((char *)(ptr + total_length), "\"ID\":\"G1-%s\",", gsm_get_module_imei());
#endif
    total_length += sprintf((char *)(ptr + total_length), "\"Phone\":\"%s\",", eeprom_cfg->phone);
    total_length += sprintf((char *)(ptr + total_length), "\"Money\":%d,", 0);
       
#ifdef DTG02
	for (uint32_t i = 0; i < MEASURE_NUMBER_OF_WATER_METER_INPUT; i++)
	{
		// Build input pulse counter
		temp_counter = msg->counter[i].forward / eeprom_cfg->k[i] + eeprom_cfg->offset[i];
		total_length += sprintf((char *)(ptr + total_length), "\"Input1_J%u\":%u,",
									i,
								  temp_counter);
		if (eeprom_cfg->meter_mode[i] == APP_EEPROM_METER_MODE_PWM_F_PWM_R)
		{
			temp_counter = msg->counter[i].reserve / eeprom_cfg->k[i] + eeprom_cfg->offset[i];
			total_length += sprintf((char *)(ptr + total_length), "\"Input1_J%u_D\":%u,",
										i,
											temp_counter);
			// K : he so chia cua dong ho nuoc, input 1
			// Offset: Gia tri offset cua dong ho nuoc
			// Mode : che do hoat dong
			total_length += sprintf((char *)(ptr + total_length), "\"K%u\":%u,", i, eeprom_cfg->k[i]);
			total_length += sprintf((char *)(ptr + total_length), "\"Offset%u\":%u,", i, eeprom_cfg->offset[i]);
			total_length += sprintf((char *)(ptr + total_length), "\"Mode%u\":%u,", i, eeprom_cfg->meter_mode[i]);
		}
	}
	
    // Build input 4-20ma
	if (eeprom_cfg->io_enable.name.input_4_20ma_0_enable)
	{
		total_length += sprintf((char *)(ptr + total_length), "\"Input1_J3_1\":%.3f,", 
																			msg->input_4_20mA[0]); // dau vao 4-20mA 0
	}
	if (eeprom_cfg->io_enable.name.input_4_20ma_1_enable)
	{
		total_length += sprintf((char *)(ptr + total_length), "\"Input1_J3_2\":%.3f,", 
																			msg->input_4_20mA[1]); // dau vao 4-20mA 0
	}
	if (eeprom_cfg->io_enable.name.input_4_20ma_2_enable)
	{
		total_length += sprintf((char *)(ptr + total_length), "\"Input1_J3_3\":%.3f,", 
																			msg->input_4_20mA[2]); // dau vao 4-20mA 0
	}
		if (eeprom_cfg->io_enable.name.input_4_20ma_3_enable)
	{
		total_length += sprintf((char *)(ptr + total_length), "\"Input1_J3_4\":%.3f,", 
																			msg->input_4_20mA[3]); // dau vao 4-20mA 0
	}
    
    // Build input on/off
	for (uint32_t i = 0; i < NUMBER_OF_INPUT_ON_OFF; i++)
	{
		total_length += sprintf((char *)(ptr + total_length), "\"Input1_J9_%u\":%u,", 
																			i+1,
																			measure_input->input_on_off[i]); // dau vao 4-20mA 0
	}	

    // Build output on/off
	for (uint32_t i = 0; i < NUMBER_OF_OUTPUT_ON_OFF; i++)
	{
		total_length += sprintf((char *)(ptr + total_length), "\"Output%u\":%u,", 
																			i+1,
																			measure_input->output_on_off[i]);  //dau ra on/off 
	}
    
    // Build output 4-20ma
	if (eeprom_cfg->io_enable.name.output_4_20ma_enable)
	{
		total_length += sprintf((char *)(ptr + total_length), "\"Output4_20\":%.3f,", measure_input->output_4_20mA);   // dau ra 4-20mA 0
	}
	
		
#else	
    // Build pulse counter
    temp_counter = msg->counter[0].forward / cfg->k[0]+ cfg->offset[0];
    total_length += sprintf((char *)(ptr + total_length), "\"Input1\":%u,",
                              temp_counter); //so xung
    
    if (cfg->meter_mode[0] == APP_EEPROM_METER_MODE_PWM_F_PWM_R)
	{
        temp_counter = msg->counter[0].reserve / cfg->k[0]+ cfg->offset[0];
		total_length += sprintf((char *)(ptr + total_length), "\"Input1_J1_D\":%u,",
									temp_counter);
	}
	
    // Build input on/off
    total_length += sprintf((char *)(ptr + total_length), "\"Output1\":%u,", 
                                                                        measure_input->output_on_off[0]); // dau vao on/off
	if (eeprom_cfg->io_enable.name.input_4_20ma_0_enable)
	{
		// Build input 4-20ma
		total_length += sprintf((char *)(ptr + total_length), "\"Input2\":%.2f,", 
																			msg->input_4_20mA[0]); // dau vao 4-20mA 0
	}

    // Build ouput 4-20ma
    total_length += sprintf((char *)(ptr + total_length), "\"Output2\":%.2f,", 
                                                                        measure_input->output_4_20mA); // dau ra 4-20mA
#endif

    // CSQ in percent 
    if (msg->csq_percent)
    {
        total_length += sprintf((char *)(ptr + total_length), "\"SignalStrength\":%d,", msg->csq_percent);
    }
    
    // Warning level
    total_length += sprintf((char *)(ptr + total_length), "\"WarningLevel\":\"%s\",", alarm_str);
    total_length += sprintf((char *)(ptr + total_length), "\"BatteryLevel\":%d,", msg->vbat_percent);
#ifdef DTG01    // Battery
    total_length += sprintf((char *)(ptr + total_length), "\"Vbat\":%.2f,", msg->vbat_mv/1000);
#else   // DTG02 : Vinput 24V
    total_length += sprintf((char *)(ptr + total_length), "\"Vin\":%.2f,", msg->vin_mv/1000);
#endif    
    // Temperature
    if (!measure_input->temperature_error)
    {
        total_length += sprintf((char *)(ptr + total_length), "\"Temperature\":%d,", measure_input->temperature);
    }
    
//    // Reset reason
//    total_length += sprintf((char *)(ptr + total_length), "\"RST\":%u,", hardware_manager_get_reset_reason()->value);
    
	// Register0_1:1234, Register0_2:4567, Register1_0:12345
	for (uint32_t index = 0; index < RS485_MAX_SLAVE_ON_BUS; index++)
	{
		// 485
		if (eeprom_cfg->io_enable.name.rs485_en)
		{	
			for (uint32_t sub_idx = 0; sub_idx < RS485_MAX_SUB_REGISTER; sub_idx++)
			{
				if (msg->rs485[index].sub_register[sub_idx].read_ok
					&& msg->rs485[index].sub_register[sub_idx].data_type.name.valid)
				{
					total_length += sprintf((char *)(ptr + total_length), "\"Register%u_%u\":", index+1, sub_idx+1);
					if (msg->rs485[index].sub_register[sub_idx].data_type.name.type == RS485_DATA_TYPE_INT16 
						|| msg->rs485[index].sub_register[sub_idx].data_type.name.type == RS485_DATA_TYPE_INT32)
					{
						total_length += sprintf((char *)(ptr + total_length), "%u,", msg->rs485[index].sub_register[sub_idx].value);
					}
					else
					{
						total_length += sprintf((char *)(ptr + total_length), "%.4f,", (float)msg->rs485[index].sub_register[sub_idx].value);
					}
					if (strlen((char*)msg->rs485[index].sub_register[sub_idx].unit))
					{
						total_length += sprintf((char *)(ptr + total_length), "\"Unit%u_%u\":\"%s\",", index+1, sub_idx+1, msg->rs485[index].sub_register[sub_idx].unit);
					}
				}
				else if (msg->rs485[index].sub_register[sub_idx].data_type.name.valid)
				{
					total_length += sprintf((char *)(ptr + total_length), "\"Register%u_%u\":%s,", index+1, sub_idx+1, "FFFF");
				}	
			}
		}
	}
    
    // Sim imei
    total_length += sprintf((char *)(ptr + total_length), "\"SIM\":%s,", gsm_get_sim_imei());
    
    // Uptime
    total_length += sprintf((char *)(ptr + total_length), "\"Uptime\":%u,", m_wake_time);
    
    // Firmware and hardware
    total_length += sprintf((char *)(ptr + total_length), "\"FW\":\"%s\",", VERSION_CONTROL_FW);
    total_length += sprintf((char *)(ptr + total_length), "\"HW\":\"%s\"}", VERSION_CONTROL_HW);
        
//    hardware_manager_get_reset_reason()->value = 0;

    DEBUG_VERBOSE("%s\r\n", (char*)ptr);
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
static measure_input_perpheral_data_t *m_sensor_msq;

static void gsm_http_event_cb(gsm_http_event_t event, void *data)
{
	sys_ctx_t *ctx = sys_ctx();
    sys_turn_on_led(3);
    switch (event)
    {
    case GSM_HTTP_EVENT_START:
        DEBUG_VERBOSE("HTTP task started\r\n");
        break;

    case GSM_HTTP_EVENT_CONNTECTED:
        LED1(1);
        DEBUG_INFO("HTTP connected, data size %u\r\n", *((uint32_t *)data));
        ctx->status.disconnect_timeout_s = 0;
        if (ctx->status.enter_ota_update)
        {
            ota_update_start(*((uint32_t*)data));
        }
        break;
    
    case GSM_HTTP_GET_EVENT_DATA:
    {
        LED1(1);
        sys_delay_ms(4);
        gsm_http_data_t *get_data = (gsm_http_data_t *)data;
		app_eeprom_config_data_t *eeprom_cfg = app_eeprom_read_config_data();
        if (!ctx->status.enter_ota_update)
        {
			if (sys_ctx()->status.poll_broadcast_msg_from_server)
			{
				sys_ctx()->status.poll_broadcast_msg_from_server = 0;
				server_msg_process_boardcast_cmd((char *)get_data->data);
			}
			else
			{
				uint8_t new_cfg = 0;
				server_msg_process_cmd((char *)get_data->data, &new_cfg);
				if (new_cfg)
				{
					measure_input_measure_wakeup_to_get_data();
					app_eeprom_save_config();
					m_delay_wait_for_measurement_again_s = 10;
				}
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
            DEBUG_VERBOSE("No more sensor data\r\n");
            if (!m_retransmision_data_in_flash)
            {
                gsm_change_state(GSM_STATE_OK);
            }
            else
            {
                static measure_input_perpheral_data_t tmp;
				for (uint32_t i = 0; i < MEASURE_NUMBER_OF_WATER_METER_INPUT; i++)
				{
					tmp.counter[i].forward = m_retransmision_data_in_flash->meter_input[i].forward;
					tmp.counter[i].reserve = m_retransmision_data_in_flash->meter_input[i].reserve;
				}
				
               
                for (uint32_t i = 0; i < NUMBER_OF_INPUT_4_20MA; i++)
                {
                    tmp.input_4_20mA[i] = m_retransmision_data_in_flash->input_4_20mA[i];
                }
                tmp.csq_percent = 0;
                tmp.measure_timestamp = m_retransmision_data_in_flash->timestamp;
                tmp.temperature = m_retransmision_data_in_flash->temp;
                tmp.vbat_mv = m_retransmision_data_in_flash->vbat_mv;
                tmp.vbat_mv = m_retransmision_data_in_flash->vbat_precent;
				
				// Modbus
				for (uint32_t i = 0; i < RS485_MAX_SLAVE_ON_BUS; i++)
				{
					memcpy(&tmp.rs485[i], &m_retransmision_data_in_flash->rs485[i], sizeof(measure_input_modbus_register_t));
				}
                
                m_sensor_msq = &tmp;
                build_msg = true;
                DEBUG_VERBOSE("Build retransmision data\r\n");
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
                m_last_http_msg = NULL;
                DEBUG_VERBOSE("Umm free\r\n");
                m_malloc_count--;
            }
            
            // Malloc data to http post
            DEBUG_VERBOSE("Malloc data\r\n");
#ifdef DTG01
            m_last_http_msg = (char*)umm_malloc(512+128);
#else
            m_last_http_msg = (char*)umm_malloc(736);
#endif
            if (m_last_http_msg == NULL)
            {
                DEBUG_ERROR("Malloc error\r\n");
                NVIC_SystemReset();
            }
            else
            {
                ++m_malloc_count;
                
                // Build sensor message
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
        app_eeprom_config_data_t *eeprom_cfg = app_eeprom_read_config_data();
        ctx->status.disconnect_timeout_s = 0;
        if (ctx->status.enter_ota_update            // Neu dang trong tien trinh ota update vaf download xong file =>> turn off gsm
            && ctx->status.delay_ota_update == 0)
        {
            GSM_PWR_EN(0);
            GSM_PWR_RESET(0);
            GSM_PWR_KEY(0);
            ota_update_finish(true);
        }
		
		// If device in mode test connection with new server =>> Store new server to flash
		if (ctx->status.try_new_server && ctx->status.new_server)
		{
			// Delete old server and copy new server addr
			memset(eeprom_cfg->server_addr[APP_EEPROM_MAIN_SERVER_ADDR_INDEX], 0, APP_EEPROM_MAX_SERVER_ADDR_LENGTH);
			sprintf((char*)eeprom_cfg->server_addr[APP_EEPROM_ALTERNATIVE_SERVER_ADDR_INDEX], "%s", ctx->status.new_server);
			ctx->status.try_new_server = 0;
			
			umm_free(ctx->status.new_server);
			ctx->status.new_server = NULL;
			
			app_eeprom_save_config();		// Store current config into eeprom
			DEBUG_INFO("Set new server addr success\r\n");
		}
        
        gsm_change_state(GSM_STATE_OK);
//        DEBUG_VERBOSE("Free um memory, malloc count[%u]\r\n", m_malloc_count);
        LED1(0);
		
#ifdef WDT_ENABLE
    LL_IWDG_ReloadCounter(IWDG);
#endif
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
            DEBUG_VERBOSE("Free um memory, malloc count[%u]\r\n", m_malloc_count);
            m_last_http_msg = NULL;
        }
        LED1(0);
        app_spi_flash_data_t wr_data;
        wr_data.resend_to_server_flag = APP_FLASH_DONT_NEED_TO_SEND_TO_SERVER_FLAG;
        for (uint32_t i = 0; i < APP_FLASH_NB_OFF_4_20MA_INPUT; i++)
        {
            wr_data.input_4_20mA[i] = m_sensor_msq->input_4_20mA[i];
        }
		for (uint32_t i = 0; i < MEASURE_NUMBER_OF_WATER_METER_INPUT; i++)
		{
			memcpy(&wr_data.meter_input[i], &m_sensor_msq->counter[i], sizeof(measure_input_counter_t));
		}
              
        wr_data.timestamp = m_sensor_msq->measure_timestamp;
        wr_data.valid_flag = APP_FLASH_VALID_DATA_KEY;
        wr_data.vbat_mv = m_sensor_msq->vbat_mv;
        wr_data.vbat_precent = m_sensor_msq->vbat_percent;
        wr_data.temp = m_sensor_msq->temperature;
        
		for (uint32_t i = 0; i < RS485_MAX_SLAVE_ON_BUS; i++)
		{
			memcpy(&wr_data.rs485[i], &m_sensor_msq->rs485[i], sizeof(measure_input_modbus_register_t));
		}
        
        if (!ctx->peripheral_running.name.flash_running)
        {
            DEBUG_VERBOSE("Wakup flash\r\n");
            spi_init();
            app_spi_flash_wakeup();
            ctx->peripheral_running.name.flash_running = 1;
        }
        app_spi_flash_write_data(&wr_data);
        
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
        uint32_t addr = app_spi_flash_estimate_current_read_addr(&retransmition, false);
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
		else if (sys_ctx()->status.poll_broadcast_msg_from_server)
		{
			sys_ctx()->status.poll_broadcast_msg_from_server = 0;
			gsm_change_state(GSM_STATE_OK);
		}
        else
        {
            gsm_change_state(GSM_STATE_OK);
            DEBUG_PRINTF("No more data need to re-send to server\r\n");
            m_retransmision_data_in_flash = NULL;
        }
		
		
        
#ifdef WDT_ENABLE
    LL_IWDG_ReloadCounter(IWDG);
#endif
    }
    break;

    case GSM_HTTP_POST_EVENT_FINISH_FAILED:
    {
        app_spi_flash_data_t wr_data;
        wr_data.resend_to_server_flag = 0;
        for (uint32_t i = 0; i < APP_FLASH_NB_OFF_4_20MA_INPUT; i++)
        {
            wr_data.input_4_20mA[i] = m_sensor_msq->input_4_20mA[i];
        }
		
		for (uint32_t i = 0; i < MEASURE_NUMBER_OF_WATER_METER_INPUT; i++)
        {
			memcpy(&wr_data.meter_input[i], &m_sensor_msq->counter[i], sizeof(measure_input_counter_t));
        }
      
        wr_data.timestamp = m_sensor_msq->measure_timestamp;
        wr_data.valid_flag = APP_FLASH_VALID_DATA_KEY;
        wr_data.vbat_mv = m_sensor_msq->vbat_mv;
        wr_data.vbat_precent = m_sensor_msq->vbat_percent;
        wr_data.temp = m_sensor_msq->temperature;
		
		for (uint32_t i = 0; i < RS485_MAX_SLAVE_ON_BUS; i++)
		{
			memcpy(&wr_data.rs485[i], &m_sensor_msq->rs485[i], sizeof(measure_input_modbus_register_t));
		}
        
        if (!ctx->peripheral_running.name.flash_running)
        {
            spi_init();
            app_spi_flash_wakeup();
            ctx->peripheral_running.name.flash_running = 1;
        }
        app_spi_flash_write_data(&wr_data);

        
        m_sensor_msq->state = MEASUREMENT_QUEUE_STATE_IDLE;
        m_sensor_msq = NULL;
    }
    case GSM_HTTP_GET_EVENT_FINISH_FAILED:
    {
        DEBUG_WARN("HTTP event failed\r\n");
		
		// If in in ota update mode
        if (ctx->status.enter_ota_update 
            && ctx->status.delay_ota_update == 0)
        {
            ota_update_finish(false);
            GSM_PWR_EN(0);
            GSM_PWR_RESET(0);
            GSM_PWR_KEY(0);
        }
		// If device in mode test new server =>> Store new server to flash
		if (ctx->status.try_new_server && ctx->status.new_server)
		{
			ctx->status.try_new_server = 0;
			umm_free(ctx->status.new_server);
			ctx->status.new_server = NULL;
			DEBUG_ERROR("Try new server failed\r\n");
		}
		
        if (m_last_http_msg)
        {
            m_malloc_count--;
            umm_free(m_last_http_msg);
//            DEBUG_ERROR("Free um memory, malloc count[%u]\r\n", m_malloc_count);
            m_last_http_msg = NULL;
        }
        gsm_change_state(GSM_STATE_OK);
    }
    break;

    default:
        DEBUG_WARN("Unknown http event %d\r\n", (int)event);
        gsm_change_state(GSM_STATE_OK);
        break;
    }
}

uint32_t gsm_get_current_tick(void)
{
    return sys_get_ms();
}


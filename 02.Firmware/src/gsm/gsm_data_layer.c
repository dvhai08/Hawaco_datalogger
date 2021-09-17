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

#include "usart.h"
#include "app_rtc.h"
#define TIMEZONE        0

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
static uint32_t m_send_time = 0;

#if GSM_SMS_ENABLE
bool m_do_read_sms = false;

void gsm_set_flag_prepare_enter_read_sms_mode(void)
{
    m_do_read_sms = true;
}
#endif

uint32_t estimate_wakeup_time = 0;

uint32_t gsm_data_layer_get_estimate_wakeup_time_stamp(void)
{
    return estimate_wakeup_time;
}

static float convert_input_4_20ma_to_pressure(float current)
{
    // Pa = a*current + b
    // 0 = 4*a + b
    // 10 = 20*a + b
    // =>> Pa = 0.625*current - 2.5f
    if (current < 4.0f)
    {
        return 0.0f;
    }
    
    if (current > 20.0f)
    {
        return 10.0f;       // 10PA
    }
    
    return (0.625f*current - 2.5f);
}

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
        measure_input_monitor_min_max_in_cycle_send_web();
        if (gsm_data_layer_is_module_sleeping())
        {
            gsm_change_state(GSM_STATE_WAKEUP);
//            ctx->status.sleep_time_s = 0;
        }
    }
}

void gsm_wakeup_now(void)
{
	sys_ctx_t *ctx = sys_ctx();
//	ctx->status.sleep_time_s = 0;
    estimate_wakeup_time = 1;
	gsm_change_state(GSM_STATE_WAKEUP);
}

#if GSM_SMS_ENABLE
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
            DEBUG_INFO("Send sms in buffer index %d\r\n", cnt);

            /* If retry > 3 =>> delete from queue */
            if (sms[cnt].retry_count < 3)
            {
                sms[cnt].need_to_send = 2;
                DEBUG_INFO("Change gsm state to send sms\r\n");
                gsm_change_state(GSM_STATE_SEND_SMS);
            }
            else
            {
                DEBUG_INFO("SMS buffer %u send FAIL %u times. Cancle!\r\n", 
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
	app_eeprom_config_data_t *eeprom_cfg = app_eeprom_read_config_data();

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
#if GSM_SMS_ENABLE
        if (m_do_read_sms)
        {
            m_do_read_sms = false;
            gsm_change_state(GSM_STATE_READ_SMS);
        }
				
		if (eeprom_cfg->io_enable.name.register_sim_status == 0
			&& strlen((char*)eeprom_cfg->phone) > 9
			&& eeprom_cfg->io_enable.name.warning
			&& (ctx->status.total_sms_in_24_hour < eeprom_cfg->max_sms_1_day))
		{				
			char msg[128];
			char *p = msg;
			rtc_date_time_t time;
			app_rtc_get_time(&time);
			p += sprintf(p, "%04u/%02u/%02u %02u:%02u: ",
							time.year + 2000,
							time.month,
							time.day,
							time.hour,
							time.minute);
			p += sprintf(p, "Thiet bi %s %s %s", VERSION_CONTROL_DEVICE, gsm_get_module_imei(), "dang ki hoa mang");
			ctx->status.total_sms_in_24_hour++;
			gsm_send_sms((char*)eeprom_cfg->phone, msg);
			eeprom_cfg->io_enable.name.register_sim_status = 1;
			app_eeprom_save_config();
		}
				
#endif
        gsm_wakeup_periodically();
		
#if GSM_SMS_ENABLE
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
                DEBUG_INFO("Enter http post\r\n");
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
                DEBUG_INFO("Poll default config\r\n");
				uint32_t current_sec = app_rtc_get_counter();
				if (current_sec >= ctx->status.next_time_get_data_from_server)
				{
                    // Estimate next time polling server config
					ctx->status.next_time_get_data_from_server = current_sec/ 3600 + app_eeprom_read_config_data()->poll_config_interval_hour;
                    ctx->status.next_time_get_data_from_server *= 3600;
                    
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
				server_addr = (char*)eeprom_cfg->server_addr[APP_EEPROM_ALTERNATIVE_SERVER_ADDR_INDEX];
                DEBUG_INFO("We use alternative server %s\r\n", server_addr);
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
				DEBUG_INFO("Try new server\r\n");
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
        GSM_PWR_EN(0);
        GSM_PWR_RESET(1);
        GSM_PWR_KEY(0);
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
#ifdef DTG01
		m_wake_time = app_bkup_read_nb_of_wakeup_time();
		m_wake_time++;
		app_bkup_write_nb_of_wakeup_time(m_wake_time);
#else
		m_wake_time++;
#endif
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
    //DEBUG_INFO("%s\r\n", __FUNCTION__);
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
        DEBUG_INFO("Disable AT echo : %s\r\n", (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]");
        gsm_hw_send_at_cmd("AT+CMEE=2\r\n", "OK\r\n", "", 1000, 10, gsm_at_cb_power_on_gsm);
        break;

    case 3:
        DEBUG_INFO("Set CMEE report: %s\r\n", (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]");
        gsm_hw_send_at_cmd("ATI\r\n", "OK\r\n", "", 1000, 10, gsm_at_cb_power_on_gsm);
        break;

    case 4:
        DEBUG_INFO("Get module info: %s\r\n", resp_buffer);
        gsm_hw_send_at_cmd("AT\r\n", "OK\r\n", "", 1000, 10, gsm_at_cb_power_on_gsm);
//        gsm_hw_send_at_cmd("AT+QURCCFG=\"URCPORT\",\"uart1\"\r\n", "OK\r\n", "", 1000, 5, gsm_at_cb_power_on_gsm);
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
        gsm_hw_send_at_cmd("AT\r\n", "OK\r\n", "", 1000, 5, gsm_at_cb_power_on_gsm);
        // gsm_hw_send_at_cmd("AT+CMGD=1,4\r\n", "	if (eeprom_cfg->io_enable.name.input_4_20ma_0_enable)OK\r\n", "", 2000, 5, gsm_at_cb_power_on_gsm);
        //gsm_hw_send_at_cmd("AT+CNUM\r\n", "", "OK\r\n", 2000, 5, gsm_at_cb_power_on_gsm);
        break;

    case 9:
        DEBUG_INFO("AT CNUM: %s, %s\r\n", (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]",
										(char*)resp_buffer);
//        DEBUG_INFO("Delete all SMS: %s\r\n", (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]");
        gsm_hw_send_at_cmd("AT+CGSN\r\n", "", "OK\r\n", 1000, 5, gsm_at_cb_power_on_gsm);
        break;

    case 10:
	{
        DEBUG_INFO("CSGN resp: %s, Data %s\r\n", (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]", (char*)resp_buffer);
		uint8_t *imei_buffer = (uint8_t*)gsm_get_module_imei();
        if (strlen((char*)imei_buffer) < 14)
        {
            gsm_utilities_get_imei(resp_buffer, (uint8_t *)imei_buffer, 16);
            DEBUG_INFO("Get GSM IMEI: %s\r\n", imei_buffer);
            imei_buffer = (uint8_t*)gsm_get_module_imei();
            if (strlen((char*)imei_buffer) < 15)
            {
                DEBUG_INFO("IMEI's invalid!\r\n");
                gsm_change_state(GSM_STATE_RESET); //Khong doc dung IMEI -> reset module GSM!
                return;
            }
            if (strcmp((char*)imei_buffer, "860262050129480") == 0)
            {
                sprintf((char*)imei_buffer, "%s", "860262050127815");
            }
        }
        gsm_hw_send_at_cmd("AT+CIMI\r\n", "OK\r\n", "", 1000, 10, gsm_at_cb_power_on_gsm);
	}
        break;

    case 11:
	{
		uint8_t *imei_buffer = (uint8_t*)gsm_get_sim_imei();
        gsm_utilities_get_imei(resp_buffer, imei_buffer, 16);
        DEBUG_INFO("Get SIM IMSI: %s\r\n", gsm_get_sim_imei());
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
        DEBUG_INFO("Get SIM IMEI: %s\r\n", (char *)resp_buffer);
		uint8_t *ccid_buffer = (uint8_t*)gsm_get_sim_ccid();
        gsm_utilities_get_sim_ccid(resp_buffer, ccid_buffer, 20);
        DEBUG_INFO("SIM CCID: %s\r\n", ccid_buffer);
        if (strlen((char*)ccid_buffer) < 10)
        {
            gsm_hw_send_at_cmd("AT+QCCID\r\n", "QCCID", "OK\r\n", 1000, 3, gsm_at_cb_power_on_gsm); 
            gsm_manager.step = 11;
            return;
        }
        gsm_hw_send_at_cmd("AT+CPIN?\r\n", "READY\r\n", "", 3000, 3, gsm_at_cb_power_on_gsm); 
    }
        break;

    case 13:
        DEBUG_INFO("CPIN: %s\r\n", (char *)resp_buffer);
        gsm_hw_send_at_cmd("AT+QIDEACT=1\r\n", "OK\r\n", "", 3000, 1, gsm_at_cb_power_on_gsm);
        break;
#if UNLOCK_BAND == 0        // o lan dau tien active sim, voi module ec200 thi phai active band, ec20 thi ko can
                            // neu ko active thi se ko reg dc vao nha mang
    case 14:
        DEBUG_INFO("De-activate PDP: %s\r\n", (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]");
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
        DEBUG_INFO("Network search mode AUTO: %s\r\n", (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]");
#else
        DEBUG_INFO("Unlock band: %s\r\n", (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]");
#endif
        gsm_hw_send_at_cmd("AT+CGDCONT=1,\"IP\",\"v-internet\"\r\n", "", "OK\r\n", 1000, 2, gsm_at_cb_power_on_gsm); /** <cid> = 1-24 */
        break;

    case 16:
        DEBUG_INFO("Define PDP context: %s\r\n", (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]");
        //			gsm_hw_send_at_cmd("AT+QIACT=1\r\n", "OK\r\n", "", 5000, 5, gsm_at_cb_power_on_gsm);	/** Bật QIACT lỗi gửi tin với 1 số SIM dùng gói cước trả sau! */
        gsm_hw_send_at_cmd("AT+CSCS=\"GSM\"\r\n", "OK\r\n", "", 1000, 1, gsm_at_cb_power_on_gsm);
        break;

    case 17:
        DEBUG_INFO("CSCS=GSM: %s\r\n", (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]");
        gsm_hw_send_at_cmd("AT+CGREG=2\r\n", "OK\r\n", "", 1000, 3, gsm_at_cb_power_on_gsm); /** Query CGREG? => +CGREG: <n>,<stat>[,<lac>,<ci>[,<Act>]] */
        break;

    case 18:
        DEBUG_INFO("Network registration status: %s, data %s\r\n", (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]", (char*)resp_buffer);
        gsm_hw_send_at_cmd("AT+CGREG?\r\n", "OK\r\n", "", 1000, 5, gsm_at_cb_power_on_gsm);
        break;

    case 19:
        DEBUG_INFO("Query network status: %s, data %s\r\n", (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]", (char*)resp_buffer); /** +CGREG: 2,1,"3279","487BD01",7 */
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
        DEBUG_INFO("Query network operator: %s\r\n", (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]"); /** +COPS: 0,0,"Viettel Viettel",7 */
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
//        DEBUG_INFO("Select QSCLK: %s\r\n", (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]");
        gsm_hw_send_at_cmd("AT+CCLK?\r\n", "+CCLK:", "OK\r\n", 1000, 5, gsm_at_cb_power_on_gsm);
    }
    break;

    case 22:
    {
        DEBUG_INFO("Query CCLK: %s,%s\r\n",
                     (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]",
                     (char *)resp_buffer);
        rtc_date_time_t time;
        gsm_utilities_parse_timestamp_buffer((char *)resp_buffer, &time);
        time.hour += TIMEZONE;
        if (time.year > 20 
			&& time.year < 40 
			&& time.hour < 24) // if 23h40 =>> time.hour += 7 = 30h =>> invalid
                                                                // Lazy solution : do not update time from 17h
        {
            static uint32_t update_counter = 0;
            if ((update_counter % 24) == 0)
            {
                app_rtc_set_counter(&time);
            }
            update_counter++;
        }
        gsm_hw_send_at_cmd("AT+CSQ\r\n", "", "OK\r\n", 1000, 5, gsm_at_cb_power_on_gsm);
    }
    break;

    case 23:
        if (event != GSM_EVENT_OK)
        {
            DEBUG_INFO("GSM: init fail, reset modem...\r\n");
            gsm_manager.step = 0;
            gsm_change_state(GSM_STATE_RESET);
            return;
        }
		
		uint8_t csq;
        gsm_set_csq(0);
        gsm_utilities_get_signal_strength_from_buffer(resp_buffer, &csq);
        DEBUG_INFO("CSQ: %d\r\n", csq);

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
                DEBUG_INFO("GSM: CUSD query failed\r\n");
            }
            else
            {
                char *p = strstr((char*)resp_buffer, "+CUSD: ");
                p += 5;
                DEBUG_INFO("CUSD %s\r\n", p);
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
//            DEBUG_INFO("Exit sleep!");
            gsm_change_state(GSM_STATE_OK);
        }
        else
        {
//            DEBUG_INFO("Khong phan hoi lenh, reset module...");
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
    case 0: // Power off // add a lot of delay in power off state, due to reserve time for measurement sensor data
            // If vbat is low, sensor data will be incorrect
        gsm_manager.gsm_ready = 0;
        GSM_PWR_EN(0);
        GSM_PWR_RESET(1);
        GSM_PWR_KEY(0);
        step++;
        break;
    case 1: 
        gsm_manager.gsm_ready = 0;
        GSM_PWR_EN(0);
        GSM_PWR_RESET(1);
        GSM_PWR_KEY(0);
        step++;
        break;
    
    case 2:
        GSM_PWR_RESET(0);
        DEBUG_INFO("Gsm power on\r\n");
        GSM_PWR_EN(1);
        step++;
        break;

    case 3: // Delayms for Vbat stable
    case 4:
        step++;
        break;

    case 5:
        usart1_control(true);
        DEBUG_INFO("Pulse power key\r\n");
        /* Tao xung |_| de Power On module, min 1s  */
        GSM_PWR_KEY(1);
        step++;
        break;

    case 6:
        GSM_PWR_KEY(0);
        GSM_PWR_RESET(0);
        gsm_manager.timeout_after_reset = 90;
        step++;
        break;

    case 7:
    case 8:
        step++;
        break;
    
    case 9:
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

//    DEBUG_INFO("Debug SEND SMS : %u %u,%s\r\n", gsm_manager.step, event, resp_buffer);

    switch (gsm_manager.step)
    {
    case 1:
        for (count = 0; count < max_sms; count++)
        {
            if (sms[count].need_to_send == 2)
            {
                DEBUG_INFO("Sms to %s. Content : %s\r\n",
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
//                    DEBUG_INFO("Sending sms in buffer %u\r\n", count);
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
            DEBUG_INFO("SMS : Send sms success\r\n");

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
        DEBUG_INFO("Unknown outgoing sms step %d\r\n", gsm_manager.step);
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

/* DTG01
{
	"Error": "cam_bien_xung_dut",
	"Timestamp": 1629200614,
	"ID": "G1-860262050125363",
	"Input1": 124511,
	"Outl": 0,
	"Out2": 0.00,
	"BatteryLevel": 80,
	"Vbat": 4101,
	"Temperature": 26,
	"SIM": 452018100001935,
	"Uptime": 7,
	"FW": "0.0.5",
	"HW": "0.0.1"
}
*/

/* DTG02
{
	"Timestamp": "1628275184",
	"ID": "G2-860262050125777",
	"Phone": "0916883454",
	"Money": 7649,
	"Inputl_J1": 7649,
	"Ipl_J3_1": 0.01,
	"Ip1_J3_2": 0.00,
	"Ip1_J3_3": 0.01,
	"Ip1_J3_4 ": 0.01,
	"Ip1_J9_1 ": 1,
	"Ip1_g9_2 ": 1,
	"Ipl_J9_3 ": 1,
	"Ip_J9_4 ": 71,
	"Out1 ": 0,
	"Out2": 0,
	"Out3": 0,
	"Out4": 0,
	"Out4_20": 0.0,
	"WarningLevel ": "0,0,0,0,1,0,1",
	"BatteryLevel ": 100,
	"Vin": 23.15,
	"Temperature ": 29,
	"Regl_1 ": 64,
	"Unitl_1 ": "m3/s",
	"Reg1_2 ": 339,
	"Unit1_2 ": "jun ",
	"Reg2_1": 0.0000,
	"Unit2_1": "kg",
	"Reg2_2": 0,
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
//	measure_input_perpheral_data_t *measure_input = measure_input_current_data();
//    DEBUG_VERBOSE("Free mem %u bytes\r\n", umm_free_heap_size());
	sys_ctx_t *ctx = sys_ctx();
	bool found_break_pulse_input = false;
//	char alarm_str[128];
//    char *p = alarm_str;
    volatile uint16_t total_length = 0;
	
    uint64_t ts = msg->measure_timestamp;
    ts *= 1000;
    
    total_length += sprintf((char *)(ptr + total_length), "{\"ts\":%llu,\"values\":", ts);
	total_length += sprintf((char *)(ptr + total_length), "%s", "{\"Err\":\"");

	for (uint32_t i = 0; i < MEASURE_NUMBER_OF_WATER_METER_INPUT; i++)
	{
		if (msg->counter[i].cir_break)
		{
			found_break_pulse_input = true;
			total_length += sprintf((char *)(ptr + total_length), "cir %u,", i+1);
		}
	}
	
	if (found_break_pulse_input)
	{
		ctx->error_not_critical.detail.circuit_break = 1;
	}
	else
	{
		ctx->error_not_critical.detail.circuit_break = 0;
	}
	

    // Build error msg
    if (msg->vbat_percent < eeprom_cfg->battery_low_percent)
    {
        total_length += sprintf((char *)(ptr + total_length), "%s", "pin yeu,");
    }
    
	// Flash
	if (ctx->error_not_critical.detail.flash)
	{
		total_length += sprintf((char *)(ptr + total_length), "%s", "flash,");
	}

	// Nhiet do
	if (msg->temperature > 70)
	{
		total_length += sprintf((char *)(ptr + total_length), "%s", "nhiet cao,");
	}

	
	// RS485
#if 0
	if (ctx->error_not_critical.detail.rs485_err)
	{
		total_length += sprintf((char *)(ptr + total_length), "%s", "rs485,");
	}
#else
    bool found_rs485_err = false;
    for (uint32_t slave_idx = 0; slave_idx < RS485_MAX_SLAVE_ON_BUS; slave_idx++)
    {
        for (uint32_t sub_register_index = 0; sub_register_index < RS485_MAX_SUB_REGISTER; sub_register_index++)
        {
            if (eeprom_cfg->rs485[slave_idx].sub_register[sub_register_index].data_type.name.valid == 0)
            {
                continue;
            }
            if (msg->rs485[slave_idx].sub_register[sub_register_index].read_ok == 0)
            {
                found_rs485_err = true;
                break;
            }
        }
    }
    
    if (found_rs485_err)
    {
        total_length += sprintf((char *)(ptr + total_length), "%s", "rs485,");
    }
#endif
	// Cam bien
	if (ctx->error_critical.detail.sensor_out_of_range)
	{
		total_length += sprintf((char *)(ptr + total_length), "%s", "qua nguong,");
	}
	if (ptr[total_length - 1] == ',')
	{
		ptr[total_length - 1] = 0;
		total_length--;		// remove char ','
	}
//	else	// no error
//	{
//		total_length += sprintf((char *)(ptr + total_length), "%s", "\",");
//	}
	
	total_length += sprintf((char *)(ptr + total_length), "%s", "\",");
	
    // Build timestamp
    total_length += sprintf((char *)(ptr + total_length), "\"Timestamp\":%u,", msg->measure_timestamp); //second since 1970
    
    int32_t temp_counter;

    // Build ID, phone and id
#ifdef DTG02
    total_length += sprintf((char *)(ptr + total_length), "\"ID\":\"G2-%s\",", gsm_get_module_imei());
#endif
	
#ifdef DTG02V2
	total_length += sprintf((char *)(ptr + total_length), "\"ID\":\"G2V2-%s\",", gsm_get_module_imei());
#endif
	
#ifdef DTG01
    total_length += sprintf((char *)(ptr + total_length), "\"ID\":\"G1-%s\",", gsm_get_module_imei());
	// K : he so chia cua dong ho nuoc, input 1
	// Offset: Gia tri offset cua dong ho nuoc
	// Mode : che do hoat dong
    if (msg->counter[0].k == 0)
    {
        DEBUG_ERROR("K is zero\r\n");
        msg->counter[0].k = 1;
    }
	total_length += sprintf((char *)(ptr + total_length), "\"K%u\":%u,", 0, msg->counter[0].k);
	//total_length += sprintf((char *)(ptr + total_length), "\"Offset%u\":%u,", i+1, eeprom_cfg->offset[i]);
	total_length += sprintf((char *)(ptr + total_length), "\"M%u\":%u,", 0, eeprom_cfg->meter_mode[0]);
#endif

//    total_length += sprintf((char *)(ptr + total_length), "\"Phone\":\"%s\",", eeprom_cfg->phone);
//    total_length += sprintf((char *)(ptr + total_length), "\"Money\":%d,", 0);
//	total_length += sprintf((char *)(ptr + total_length), "\"Dir\":%u,", eeprom_cfg->dir_level);
#ifndef DTG01
	for (uint32_t i = 0; i < MEASURE_NUMBER_OF_WATER_METER_INPUT; i++)
	{
		// Build input pulse counter
		if (msg->counter[i].k == 0)
		{
			msg->counter[i].k = 1;
		}
		temp_counter = msg->counter[i].real_counter/msg->counter[i].k + msg->counter[i].indicator;
		total_length += sprintf((char *)(ptr + total_length), "\"Ip1_J%u\":%d,",
									i+1,
									temp_counter);
        
        temp_counter = msg->counter[i].reserve_counter / msg->counter[i].k /* + msg->counter[i].indicator */;
        total_length += sprintf((char *)(ptr + total_length), "\"Ip1_J%u_D\":%u,",
                                        i+1,
                                        temp_counter);
        
        // Total forward flow
        total_length += sprintf((char *)(ptr + total_length), "\"FwFlw%u\":%u,",
											i+1,
											msg->counter[i].total_forward);
        total_length += sprintf((char *)(ptr + total_length), "\"RsvFlw%u\":%u,",
											i+1,
											msg->counter[i].total_reserve);
        
        // Total forward/reserve index : tong luu luong tich luy thuan/nguoc
        total_length += sprintf((char *)(ptr + total_length), "\"FwIdx%u\":%u,",
											i+1,
											msg->counter[i].total_forward_index);
        total_length += sprintf((char *)(ptr + total_length), "\"RsvIdx%u\":%u,",
											i+1,
											msg->counter[i].total_reserve_index);
        
        if (msg->counter[i].flow_avg_cycle_send_web.valid)
        {
                    
            // min max forward flow
            total_length += sprintf((char *)(ptr + total_length), "\"MinFwFlw%u\":%.2f,",
											i+1,
											msg->counter[i].flow_avg_cycle_send_web.forward_flow_min);
        
            total_length += sprintf((char *)(ptr + total_length), "\"MaxFwFlw%u\":%.2f,",
											i+1,
											msg->counter[i].flow_avg_cycle_send_web.forward_flow_max);
        
            // min max reserve flow
            total_length += sprintf((char *)(ptr + total_length), "\"MinRsvFlw%u\":%.2f,",
                                                i+1,
                                                msg->counter[i].flow_avg_cycle_send_web.reserve_flow_min);
            
            total_length += sprintf((char *)(ptr + total_length), "\"MaxRsvFlw%u\":%.2f,",
                                                i+1,
                                                msg->counter[i].flow_avg_cycle_send_web.reserve_flow_max);
        }

        
               
        
//		// K : he so chia cua dong ho nuoc, input 1
//		// Offset: Gia tri offset cua dong ho nuoc
//		// Mode : che do hoat dong
//		total_length += sprintf((char *)(ptr + total_length), "\"K%u\":%u,", i+1, msg->counter[i].k);
//		//total_length += sprintf((char *)(ptr + total_length), "\"Offset%u\":%u,", i+1, eeprom_cfg->offset[i]);
//		total_length += sprintf((char *)(ptr + total_length), "\"M%u\":%u,", i+1, msg->counter[i].mode);
	}		
    // Build input 4-20ma
    total_length += sprintf((char *)(ptr + total_length), "\"Ip1_J3_1\":%.2f,", 
                                                                        msg->input_4_20mA[0]); // dau vao 4-20mA 0

    total_length += sprintf((char *)(ptr + total_length), "\"Ip1_J3_2\":%.2f,", 
                                                                        msg->input_4_20mA[1]); // dau vao 4-20mA 0

    total_length += sprintf((char *)(ptr + total_length), "\"Ip1_J3_3\":%.2f,", 
                                                                        msg->input_4_20mA[2]); // dau vao 4-20mA 0

    total_length += sprintf((char *)(ptr + total_length), "\"Ip1_J3_4\":%.2f,", 
                                                                        msg->input_4_20mA[3]); // dau vao 4-20mA 0
    
    // Min max 4-20mA
#if 0   // test            
    // 4-20mA  
    for (uint32_t i = 0; i < NUMBER_OF_INPUT_4_20MA; i++)
    {
        if (msg->input_4_20ma_cycle_send_web[i].valid)
        {
            total_length += sprintf((char *)(ptr + total_length), "\"MinPA%u\":%.2f,", i+1,
                                                                        convert_input_4_20ma_to_pressure(msg->input_4_20ma_cycle_send_web[i].input4_20ma_min));
        }
        total_length += sprintf((char *)(ptr + total_length), "\"PA%u\":%.2f,", i+1,
                                                                        convert_input_4_20ma_to_pressure(msg->input_4_20mA[i]));
    }      
    
    // Build input on/off
	for (uint32_t i = 0; i < NUMBER_OF_INPUT_ON_OFF; i++)
	{
		total_length += sprintf((char *)(ptr + total_length), "\"Ip1_J9_%u\":%u,", 
																			i+1,
																			msg->input_on_off[i]); // dau vao 4-20mA 0
	}	

    // Build output on/off
	for (uint32_t i = 0; i < NUMBER_OF_OUTPUT_ON_OFF; i++)
	{
		total_length += sprintf((char *)(ptr + total_length), "\"Out%u\":%u,", 
																			i+1,
																			msg->output_on_off[i]);  //dau ra on/off 
	}
    
    // Build output 4-20ma
	if (eeprom_cfg->io_enable.name.output_4_20ma_enable)
	{
		total_length += sprintf((char *)(ptr + total_length), "\"Out4_20\":%.2f,", msg->output_4_20mA[0]);   // dau ra 4-20mA
	}
#endif	
		
#else	
    // Build pulse counter
    temp_counter = msg->counter[0].real_counter / msg->counter[0].k + msg->counter[0].indicator;
    total_length += sprintf((char *)(ptr + total_length), "\"Ip1\":%u,",
                              temp_counter); //so xung
    
    if (eeprom_cfg->meter_mode[0] == APP_EEPROM_METER_MODE_PWM_F_PWM_R)
	{
        temp_counter = msg->counter[0].reserve / msg->counter[0].k + msg->counter[0].indicator;
		total_length += sprintf((char *)(ptr + total_length), "\"Ip1_J1_D\":%u,",
									temp_counter);
	}
	
    // Build input on/off
    total_length += sprintf((char *)(ptr + total_length), "\"Out1\":%u,", 
                                                                        msg->output_on_off[0]); // dau vao on/off
	if (eeprom_cfg->io_enable.name.input_4_20ma_0_enable)
	{
		// Build input 4-20ma
		total_length += sprintf((char *)(ptr + total_length), "\"Ip2\":%.2f,", 
																			msg->input_4_20mA[0]); // dau vao 4-20mA 0
	}

    // Build ouput 4-20ma
    total_length += sprintf((char *)(ptr + total_length), "\"Out2\":%.2f,", 
																			msg->output_4_20mA[0]); // dau ra 4-20mA
#endif

    // CSQ in percent 
    if (msg->csq_percent)
    {
        total_length += sprintf((char *)(ptr + total_length), "\"SignalStrength\":%d,", msg->csq_percent);
    }
    
    // Warning level
//    total_length += sprintf((char *)(ptr + total_length), "\"WarningLevel\":\"%s\",", alarm_str);
    total_length += sprintf((char *)(ptr + total_length), "\"BatteryLevel\":%d,", msg->vbat_percent);
#ifdef DTG01    // Battery
    total_length += sprintf((char *)(ptr + total_length), "\"Vbat\":%u,", msg->vbat_mv);
#else   // DTG02 : Vinput 24V
	total_length += sprintf((char *)(ptr + total_length), "\"Vbat\":%u,", msg->vbat_mv);
    total_length += sprintf((char *)(ptr + total_length), "\"Vin\":%.1f,", msg->vin_mv/1000);
#endif    
    // Temperature
	total_length += sprintf((char *)(ptr + total_length), "\"T\":%d,", msg->temperature);
    
//    // Reset reason
//    total_length += sprintf((char *)(ptr + total_length), "\"RST\":%u,", hardware_manager_get_reset_reason()->value);
	// Reg0_1:1234, Reg0_2:4567, Reg1_0:12345
#if 0   // test
	for (uint32_t index = 0; index < RS485_MAX_SLAVE_ON_BUS; index++)
	{
        // Min max
        if (msg->rs485[index].min_max.valid)
        {
            if (msg->rs485[index].min_max.min_forward_flow.type_int != INPUT_485_INVALID_INT_VALUE)
            {
                total_length += sprintf((char *)(ptr + total_length), "\"MbMinFwFlw%u\":%d,", 
                                                                index+1, msg->rs485[index].min_max.min_forward_flow.type_int);
            }
            
            if (msg->rs485[index].min_max.max_forward_flow.type_int != INPUT_485_INVALID_INT_VALUE)
            {
                total_length += sprintf((char *)(ptr + total_length), "\"MbMaxFwFlw%u\":%d,", 
                                                                index+1, msg->rs485[index].min_max.max_forward_flow.type_int);
            }
            
            if (msg->rs485[index].min_max.min_reserve_flow.type_int != INPUT_485_INVALID_INT_VALUE)
            {
                total_length += sprintf((char *)(ptr + total_length), "\"MbinRsvFlw%u\":%d,", 
                                                                index+1, msg->rs485[index].min_max.min_reserve_flow.type_int);
            }
            
            if (msg->rs485[index].min_max.max_reserve_flow.type_int != INPUT_485_INVALID_INT_VALUE)
            {
                total_length += sprintf((char *)(ptr + total_length), "\"MbMaxRsvFlw%u\":%u,", 
                                                                index+1, msg->rs485[index].min_max.max_reserve_flow.type_int);
            }
        }

        // Value
        total_length += sprintf((char *)(ptr + total_length), "\"SlID%u\":%u,", index+1, msg->rs485[index].slave_addr);
        for (uint32_t sub_idx = 0; sub_idx < RS485_MAX_SUB_REGISTER; sub_idx++)
        {
            if (msg->rs485[index].sub_register[sub_idx].read_ok
                && msg->rs485[index].sub_register[sub_idx].data_type.name.valid)
            {
                total_length += sprintf((char *)(ptr + total_length), "\"Rg%u_%u\":", index+1, sub_idx+1);
                if (msg->rs485[index].sub_register[sub_idx].data_type.name.type == RS485_DATA_TYPE_INT16 
                    || msg->rs485[index].sub_register[sub_idx].data_type.name.type == RS485_DATA_TYPE_INT32)
                {
                    total_length += sprintf((char *)(ptr + total_length), "%u,", msg->rs485[index].sub_register[sub_idx].value);
                }
                else
                {
                    total_length += sprintf((char *)(ptr + total_length), "%.2f,", (float)msg->rs485[index].sub_register[sub_idx].value);
                }
                if (strlen((char*)msg->rs485[index].sub_register[sub_idx].unit))
                {
                    total_length += sprintf((char *)(ptr + total_length), "\"U%u_%u\":\"%s\",", index+1, sub_idx+1, msg->rs485[index].sub_register[sub_idx].unit);
                }
            }
            else if (msg->rs485[index].sub_register[sub_idx].data_type.name.valid)
            {
                if (strlen((char*)msg->rs485[index].sub_register[sub_idx].unit))
                {
                    total_length += sprintf((char *)(ptr + total_length), "\"U%u_%u\":\"%s\",", index+1, sub_idx+1, msg->rs485[index].sub_register[sub_idx].unit);
                }
                total_length += sprintf((char *)(ptr + total_length), "\"Rg%u_%u\":\"%s\",", index+1, sub_idx+1, "FFFF");
            }	
        }
	}
#endif    
    // Sim imei
    total_length += sprintf((char *)(ptr + total_length), "\"SIM\":%s,", gsm_get_sim_imei());
	total_length += sprintf((char *)(ptr + total_length), "\"CCID\":%s,", gsm_get_sim_ccid());
    
    // Uptime
//    total_length += sprintf((char *)(ptr + total_length), "\"Uptime\":%u,", m_wake_time);
    total_length += sprintf((char *)(ptr + total_length), "\"Sendtime\":%u,", ++m_send_time);
	
//	// Release date
//	total_length += sprintf((char *)(ptr + total_length), "\"Build\":\"%s %s\",", __DATE__, __TIME__);
	
//    total_length += sprintf((char *)(ptr + total_length), "\"FacSVR\":\"%s\",", app_eeprom_read_factory_data()->server);
	
    // Firmware and hardware
    total_length += sprintf((char *)(ptr + total_length), "\"FW\":\"%s\",", VERSION_CONTROL_FW);
    total_length += sprintf((char *)(ptr + total_length), "\"HW\":\"%s\"}}", VERSION_CONTROL_HW);
    
//    hardware_manager_get_reset_reason()->value = 0;
    //DEBUG_INFO("Size %u, data %s\r\n", total_length, (char*)ptr);
//    usart_lpusart_485_control(true);
//    sys_delay_ms(500);
//    usart_lpusart_485_send((uint8_t*)ptr, total_length);
    
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
//        DEBUG_VERBOSE("HTTP task started\r\n");
        break;

    case GSM_HTTP_EVENT_CONNTECTED:
        LED1(1);
        DEBUG_INFO("HTTP connected, data size %u\r\n", *((uint32_t *)data));
        ctx->status.disconnect_timeout_s = 0;
        if (ctx->status.enter_ota_update)
        {
            #if OTA_VERSION == 0
            if (!ota_update_start(*((uint32_t*)data)))
			{
				DEBUG_WARN("OTA update failed\r\n");
				sys_delay_ms(10);
				NVIC_SystemReset();
			}
            #endif
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
            #if OTA_VERSION == 0
            ota_update_write_next((uint8_t*)get_data->data, get_data->data_length);
            #endif
        }
    }
    break;

    case GSM_HTTP_POST_EVENT_DATA:
    {
        bool build_msg = false;
        DEBUG_INFO("Get http post data from queue\r\n");
        m_sensor_msq = measure_input_get_data_in_queue();
        if (!m_sensor_msq)
        {       
            DEBUG_INFO("No data in RAM queue, scan message in flash\r\n");
            if (!m_retransmision_data_in_flash)
            {
                DEBUG_INFO("No more retransmission data\r\n");
                gsm_change_state(GSM_STATE_OK);
            }
            else
            {
                DEBUG_INFO("Found retransmission data\r\n");
                
                // Copy old pulse counter from spi flash to temp variable
                static measure_input_perpheral_data_t tmp;
				for (uint32_t i = 0; i < MEASURE_NUMBER_OF_WATER_METER_INPUT; i++)
				{
                    memcpy(&tmp.counter[i], 
                            &m_retransmision_data_in_flash->counter[i], 
                            sizeof(measure_input_counter_t));
				}
				
				// Input 4-20mA
                for (uint32_t i = 0; i < NUMBER_OF_INPUT_4_20MA; i++)
                {
                    tmp.input_4_20mA[i] = m_retransmision_data_in_flash->input_4_20mA[i];
                    memcpy(&tmp.input_4_20ma_cycle_send_web[i], 
                            &m_retransmision_data_in_flash->input_4_20ma_cycle_send_web[i], 
                            sizeof(input_4_20ma_min_max_hour_t));
                }
				
				// Output 4-20mA
				for (uint32_t i = 0; i < NUMBER_OF_OUTPUT_4_20MA; i++)
                {
                    tmp.output_4_20mA[i] = m_retransmision_data_in_flash->output_4_20mA[i];
                }
				
//                tmp.csq_percent = 0;
                tmp.csq_percent = m_retransmision_data_in_flash->csq_percent;
                tmp.measure_timestamp = m_retransmision_data_in_flash->timestamp;
                tmp.temperature = m_retransmision_data_in_flash->temp;
                tmp.vbat_mv = m_retransmision_data_in_flash->vbat_mv;
                tmp.vbat_percent = m_retransmision_data_in_flash->vbat_precent;
				
				// on/off
				tmp.output_on_off[0] = m_retransmision_data_in_flash->on_off.name.output_on_off_0;
#if defined (DTG02) || defined (DTG02)
				tmp.input_on_off[0] = m_retransmision_data_in_flash->on_off.name.input_on_off_0;
				tmp.input_on_off[1] = m_retransmision_data_in_flash->on_off.name.input_on_off_1;
				tmp.output_on_off[1] = m_retransmision_data_in_flash->on_off.name.output_on_off_1;
				tmp.input_on_off[2] = m_retransmision_data_in_flash->on_off.name.input_on_off_2;
				tmp.output_on_off[2] = m_retransmision_data_in_flash->on_off.name.output_on_off_2;
				tmp.input_on_off[3] = m_retransmision_data_in_flash->on_off.name.input_on_off_3;
				tmp.output_on_off[3] = m_retransmision_data_in_flash->on_off.name.output_on_off_3;
#endif
				
				// Modbus
				for (uint32_t i = 0; i < RS485_MAX_SLAVE_ON_BUS; i++)
				{
					memcpy(&tmp.rs485[i], &m_retransmision_data_in_flash->rs485[i], sizeof(measure_input_modbus_register_t));
				}
                
                m_sensor_msq = &tmp;
                build_msg = true;
//                DEBUG_VERBOSE("Build retransmision data\r\n");
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
//                DEBUG_VERBOSE("Umm free\r\n");
                m_malloc_count--;
            }
            
            // Malloc data to http post
//            DEBUG_VERBOSE("Malloc data\r\n");
#ifdef DTG01
            m_last_http_msg = (char*)umm_malloc(512+128);
#else
            m_last_http_msg = (char*)umm_malloc(1024+128);
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
                DEBUG_INFO("Header len %u\r\n", strlen(build_http_header(m_last_http_msg.length)));
#else
                ((gsm_http_data_t *)data)->header = (uint8_t *)"";
#endif
                }
        }
    }
    break;

    case GSM_HTTP_GET_EVENT_FINISH_SUCCESS:
    {
        DEBUG_INFO("HTTP get : event success\r\n");
        app_eeprom_config_data_t *eeprom_cfg = app_eeprom_read_config_data();
        ctx->status.disconnect_timeout_s = 0;
        if (ctx->status.enter_ota_update            // Neu dang trong tien trinh ota update vaf download xong file =>> turn off gsm
            && ctx->status.delay_ota_update == 0)
        {
            GSM_PWR_EN(0);
            GSM_PWR_RESET(0);
            GSM_PWR_KEY(0);
            #if OTA_VERSION == 0
            ota_update_finish(true);
            #endif
        }
		
		// If device in mode test connection with new server =>> Store new server to flash
		// Step 1 : device trying to connect with new server, step jump from 2 to 1
		// Step 2 : step == 1 =>> try success
		if (ctx->status.try_new_server == 1 && ctx->status.new_server)
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
        if (ctx->status.try_new_server)
		{
			ctx->status.try_new_server--;
		}
        
        if (ctx->status.last_state_is_disconnect)
		{				
			ctx->status.last_state_is_disconnect = 0;
			if (strlen((char*)eeprom_cfg->phone) > 9
				&& eeprom_cfg->io_enable.name.warning
				&& (ctx->status.total_sms_in_24_hour < eeprom_cfg->max_sms_1_day))
			{
				char msg[128];
				char *p = msg;
				rtc_date_time_t time;
				app_rtc_get_time(&time);
				
				p += sprintf(p, "%04u/%02u/%02u %02u:%02u: ",
								time.year + 2000,
								time.month,
								time.day,
								time.hour,
								time.minute);
				p += sprintf(p, "Thiet bi %s, %s %s", VERSION_CONTROL_DEVICE, gsm_get_module_imei(), "da ket noi tro lai");
				ctx->status.total_sms_in_24_hour++;
				gsm_send_sms((char*)eeprom_cfg->phone, msg);
			}
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
        DEBUG_INFO("HTTP post : event success\r\n");
        ctx->status.disconnect_timeout_s = 0;
        if (m_last_http_msg)
        {
            m_malloc_count--;
            umm_free(m_last_http_msg);
            m_last_http_msg = NULL;
        }
        LED1(0);
        app_spi_flash_data_t *wr_data = (app_spi_flash_data_t*)umm_malloc(sizeof(app_spi_flash_data_t));
        // Check malloc return value
        if (wr_data == NULL)
        {
            DEBUG_INFO("Malloc failed\r\n");
            NVIC_SystemReset();
        }
        wr_data->resend_to_server_flag = APP_FLASH_DONT_NEED_TO_SEND_TO_SERVER_FLAG;
        for (uint32_t i = 0; i < APP_FLASH_NB_OFF_4_20MA_INPUT; i++)
        {
            wr_data->input_4_20mA[i] = m_sensor_msq->input_4_20mA[i];
            memcpy(&wr_data->input_4_20ma_cycle_send_web[i], 
                    &m_sensor_msq->input_4_20ma_cycle_send_web[i], 
                    sizeof(input_4_20ma_min_max_hour_t));
        }
        
		for (uint32_t i = 0; i < MEASURE_NUMBER_OF_WATER_METER_INPUT; i++)
		{
			memcpy(&wr_data->counter[i], &m_sensor_msq->counter[i], sizeof(measure_input_counter_t));
		}
        
		// on/off
		wr_data->on_off.name.output_on_off_0 = m_sensor_msq->output_on_off[0];
#if defined(DTG02) || defined(DTG02V2)
		wr_data->on_off.name.input_on_off_0 = m_sensor_msq->input_on_off[0];
		wr_data->on_off.name.input_on_off_1 = m_sensor_msq->input_on_off[1];
		wr_data->on_off.name.output_on_off_1 = m_sensor_msq->output_on_off[1];
		wr_data->on_off.name.input_on_off_2 = m_sensor_msq->input_on_off[2];
		wr_data->on_off.name.output_on_off_2 = m_sensor_msq->output_on_off[2];
		wr_data->on_off.name.input_on_off_3 = m_sensor_msq->input_on_off[3];
		wr_data->on_off.name.output_on_off_3 = m_sensor_msq->output_on_off[3];
#endif		
		// 4-20mA output
		for (uint32_t i = 0; i < NUMBER_OF_OUTPUT_4_20MA; i++)
		{
			wr_data->output_4_20mA[i] = m_sensor_msq->output_4_20mA[i];
		}
		
        wr_data->csq_percent = m_sensor_msq->csq_percent;
        wr_data->timestamp = m_sensor_msq->measure_timestamp;
        wr_data->valid_flag = APP_FLASH_VALID_DATA_KEY;
        wr_data->vbat_mv = m_sensor_msq->vbat_mv;
        wr_data->vbat_precent = m_sensor_msq->vbat_percent;
        wr_data->temp = m_sensor_msq->temperature;
        
		for (uint32_t i = 0; i < RS485_MAX_SLAVE_ON_BUS; i++)
		{
			memcpy(&wr_data->rs485[i], &m_sensor_msq->rs485[i], sizeof(measure_input_modbus_register_t));
		}
        
        if (!ctx->peripheral_running.name.flash_running)
        {
//            DEBUG_VERBOSE("Wakup flash\r\n");
            spi_init();
            app_spi_flash_wakeup();
            ctx->peripheral_running.name.flash_running = 1;
        }
        app_spi_flash_write_data(wr_data);
        umm_free(wr_data);
        
        m_sensor_msq->state = MEASUREMENT_QUEUE_STATE_IDLE;
        m_sensor_msq = NULL;
        
        bool retransmition;
        if (measure_input_sensor_data_availble() == 0)
        {
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
                DEBUG_INFO("Need re-transission at addr 0x%08X\r\n", addr);
                if (!app_spi_flash_get_stored_data(addr, &rd_data, true))
                {
                    DEBUG_ERROR("Read failed\r\n");
                    gsm_change_state(GSM_STATE_OK);
                    m_retransmision_data_in_flash = NULL;
                }
                else
                {
                    m_retransmision_data_in_flash = &rd_data;
                    DEBUG_INFO("Enter http post\r\n");
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
                DEBUG_INFO("No more data need to re-send to server\r\n");
                m_retransmision_data_in_flash = NULL;
            }
        }
        else
        {
            gsm_change_state(GSM_STATE_OK);
        }    
#ifdef WDT_ENABLE
    LL_IWDG_ReloadCounter(IWDG);
#endif
    }
    break;

    case GSM_HTTP_POST_EVENT_FINISH_FAILED:
    {
        DEBUG_WARN("Http post event failed\r\n");

        if (m_last_http_msg)
        {
            m_malloc_count--;
            umm_free(m_last_http_msg);
            m_last_http_msg = NULL;
        }
        
        app_spi_flash_data_t *wr_data = (app_spi_flash_data_t*)umm_malloc(sizeof(app_spi_flash_data_t));
        if (wr_data == NULL)
        {
            NVIC_SystemReset();
        }
        wr_data->resend_to_server_flag = 0;
		
		// Input 4-20mA
        for (uint32_t i = 0; i < APP_FLASH_NB_OFF_4_20MA_INPUT; i++)
        {
            wr_data->input_4_20mA[i] = m_sensor_msq->input_4_20mA[i];
            memcpy(&wr_data->input_4_20ma_cycle_send_web[i], 
                    &m_sensor_msq->input_4_20ma_cycle_send_web[i], 
                    sizeof(input_4_20ma_min_max_hour_t));
        }
		
		for (uint32_t i = 0; i < MEASURE_NUMBER_OF_WATER_METER_INPUT; i++)
        {
			memcpy(&wr_data->counter[i], &m_sensor_msq->counter[i], sizeof(measure_input_counter_t));
        }
      
        wr_data->timestamp = m_sensor_msq->measure_timestamp;
        wr_data->valid_flag = APP_FLASH_VALID_DATA_KEY;
        wr_data->vbat_mv = m_sensor_msq->vbat_mv;
        wr_data->vbat_precent = m_sensor_msq->vbat_percent;
        wr_data->temp = m_sensor_msq->temperature;
        wr_data->csq_percent = m_sensor_msq->csq_percent;
		
		// on/off
		wr_data->on_off.name.output_on_off_0 = m_sensor_msq->output_on_off[0];
#if defined(DTG02) || defined(DTG02V2)
		wr_data->on_off.name.input_on_off_0 = m_sensor_msq->input_on_off[0];
		wr_data->on_off.name.input_on_off_1 = m_sensor_msq->input_on_off[1];
		wr_data->on_off.name.output_on_off_1 = m_sensor_msq->output_on_off[1];
		wr_data->on_off.name.input_on_off_2 = m_sensor_msq->input_on_off[2];
		wr_data->on_off.name.output_on_off_2 = m_sensor_msq->output_on_off[2];
		wr_data->on_off.name.input_on_off_3 = m_sensor_msq->input_on_off[3];
		wr_data->on_off.name.output_on_off_3 = m_sensor_msq->output_on_off[3];
#endif
		
		// Output 4-20mA
		for (uint32_t i = 0; i < NUMBER_OF_OUTPUT_4_20MA; i++)
		{
			wr_data->output_4_20mA[i] = m_sensor_msq->output_4_20mA[i];
		}
		
		// 485
		for (uint32_t i = 0; i < RS485_MAX_SLAVE_ON_BUS; i++)
		{
			memcpy(&wr_data->rs485[i], &m_sensor_msq->rs485[i], sizeof(measure_input_modbus_register_t));
		}
        
        if (!ctx->peripheral_running.name.flash_running)
        {
            spi_init();
            app_spi_flash_wakeup();
            ctx->peripheral_running.name.flash_running = 1;
        }
        if (m_sensor_msq->state != MEASUREMENT_QUEUE_STATE_IDLE)
        {
            app_spi_flash_write_data(wr_data);
            m_sensor_msq->state = MEASUREMENT_QUEUE_STATE_IDLE;
        }
        
        umm_free(wr_data);
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

void gsm_mnr_task(void *arg)
{
	gsm_manager_tick();
    sys_ctx_t *ctx = sys_ctx();
    if (gsm_data_layer_is_module_sleeping())
    {
//        ctx->status.sleep_time_s++;
        ctx->status.disconnect_timeout_s = 0;
    }
    else
    {
        if (ctx->status.disconnect_timeout_s++ > MAX_DISCONNECTED_TIMEOUT_S)
        {
            DEBUG_ERROR("GSM disconnected for a long time\r\n");
            app_eeprom_config_data_t *eeprom_cfg = app_eeprom_read_config_data();
            measure_input_save_all_data_to_flash();
            gsm_http_cleanup();
            gsm_hw_layer_reset_rx_buffer();
            ctx->status.disconnect_timeout_s = 0;
            if (ctx->status.disconnected_count++ > 23)
            {				
                ctx->status.disconnected_count = 0;
				ctx->status.last_state_is_disconnect = 1;
                if (strlen((char*)eeprom_cfg->phone) > 9
                    && eeprom_cfg->io_enable.name.warning
					&& (ctx->status.total_sms_in_24_hour < eeprom_cfg->max_sms_1_day))
                {
					char msg[156];
					char *p = msg;
					rtc_date_time_t time;
					app_rtc_get_time(&time);
					
					p += sprintf(p, "%04u/%02u/%02u %02u:%02u: ",
									time.year + 2000,
									time.month,
									time.day,
									time.hour,
									time.minute);
					char *server = (char*)eeprom_cfg->server_addr[APP_EEPROM_MAIN_SERVER_ADDR_INDEX];
					if (strlen((char*)eeprom_cfg->server_addr[APP_EEPROM_ALTERNATIVE_SERVER_ADDR_INDEX]) > 8)
					{
						server = (char*)eeprom_cfg->server_addr[APP_EEPROM_ALTERNATIVE_SERVER_ADDR_INDEX];
					}
					p += snprintf(p, 155, "TB %s %s,%s %s", VERSION_CONTROL_DEVICE, gsm_get_module_imei(), "Mat ket noi toi", server);
					ctx->status.total_sms_in_24_hour++;
					gsm_send_sms((char*)eeprom_cfg->phone, msg);
                }
                else
                {
                    gsm_change_state(GSM_STATE_SLEEP);
                }
            }
            else
            {
                gsm_change_state(GSM_STATE_SLEEP);
            }
        }
    }
}

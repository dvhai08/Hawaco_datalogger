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
//#include "umm_malloc.h"
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
static uint8_t hard_reset_step = 0;
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
static app_spi_flash_data_t m_flash_rd_data;
        
static char *m_last_http_msg = NULL;
static app_spi_flash_data_t *m_retransmision_data_in_flash;
uint32_t m_wake_time = 0;
static uint32_t m_send_time = 0;
static bool m_post_failed = false;

#if GSM_SMS_ENABLE
bool m_do_read_sms = false;

void gsm_set_flag_prepare_enter_read_sms_mode(void)
{
    m_do_read_sms = true;
}
#endif

static inline uint32_t build_device_id(uint32_t offset, char *ptr, char *imei)
{
    uint32_t len = 0;
#ifdef DTG01
    len = sprintf((char *)(ptr + offset), "\"ID\":\"G1-%s\",", imei);
#endif
    
#ifdef DTG02    
    len = sprintf((char *)(ptr + offset), "\"ID\":\"G2-%s\",", imei); 
#endif  

#ifdef DTG02V2  
    len = sprintf((char *)(ptr + offset), "\"ID\":\"G2V2-%s\",", imei); 
#endif  

#ifdef DTG02V3  
    len = sprintf((char *)(ptr + offset), "\"ID\":\"G2V3-%s\",", imei);
#endif  
    return len;
}    

static inline uint32_t build_error_code(char *ptr, measure_input_perpheral_data_t *msg)
{
    bool found_break_pulse_input = false;
    char *tmp_ptr = ptr;
    sys_ctx_t *ctx = sys_ctx();
    app_eeprom_config_data_t *eeprom_cfg = app_eeprom_read_config_data();
    
	for (uint32_t i = 0; i < MEASURE_NUMBER_OF_WATER_METER_INPUT; i++)
	{
		if (msg->counter[i].cir_break)
		{
			found_break_pulse_input = true;
			tmp_ptr += sprintf(tmp_ptr, "cir %u,", i+1);
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
        tmp_ptr += sprintf(tmp_ptr, "%s", "pin yeu,");
    }
    
	// Flash
	if (ctx->error_not_critical.detail.flash)
	{
		tmp_ptr += sprintf(tmp_ptr, "%s", "flash,");
	}

	// Nhiet do
	if (msg->temperature > 60)
	{
		tmp_ptr += sprintf(tmp_ptr, "%s", "nhiet cao,");
	}

	
	// RS485
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
        tmp_ptr += sprintf(tmp_ptr, "rs485-%u,", measure_input_485_error_code);
    }

	// Cam bien
	if (ctx->error_critical.detail.sensor_out_of_range)
	{
		tmp_ptr += sprintf(tmp_ptr, "%s", "qua nguong,");
	}
    
	if (ptr[tmp_ptr - ptr-1] == ',')
	{
		ptr[tmp_ptr - ptr-1] = 0;
		tmp_ptr--;		// remove char ','
	}
	
	tmp_ptr += sprintf(tmp_ptr, "%s", "\",");
    
    return tmp_ptr - ptr;
}


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
        return 10.0f;       // 10Pressure
    }
    
    // x10 bar
    return (6.25f*current - 25.0f);
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
    m_post_failed = false;
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
            m_post_failed = false;
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
			p += sprintf(p, "%04u/%02u/%02u %02u:%02u GMT%d: ",
							time.year + 2000,
							time.month,
							time.day,
							time.hour,
							time.minute,
                            TIMEZONE);
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
                && !ctx->status.enter_ota_update
                && m_post_failed == false)
            {
                enter_post = true;
            }
			
            if (enter_post == false 
                && m_post_failed == false 
                && m_retransmision_data_in_flash == NULL)
            {
                // Wakeup flash if needed
                if (!ctx->peripheral_running.name.flash_running)
                {
        //            DEBUG_VERBOSE("Wakup flash\r\n");
                    spi_init();
                    app_spi_flash_wakeup();
                    ctx->peripheral_running.name.flash_running = 1;
                }
        
                bool re_send;
                uint32_t addr = app_spi_flash_estimate_current_read_addr(&re_send, false);
                if (re_send)
                {
                    DEBUG_INFO("Need re-transission at addr 0x%08X\r\n", addr);
                    if (!app_spi_flash_get_stored_data(addr, &m_flash_rd_data, true))
                    {
                        DEBUG_ERROR("Read failed\r\n");
                    }
                    else
                    {
                        m_retransmision_data_in_flash = &m_flash_rd_data;
                        enter_post = true;
                    }
                } 
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
				uint32_t current_sec = app_rtc_get_counter();
				if (current_sec >= ctx->status.next_time_get_data_from_server)
				{
//                    DEBUG_INFO("Poll default config\r\n");
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
			if (!sys_ctx()->status.enter_ota_update
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
            else if (sys_ctx()->status.poll_broadcast_msg_from_server)
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
        hard_reset_step = 0;
        m_post_failed = false;
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

uint32_t make_counter(rtc_date_time_t *time)
{
    uint32_t counter;
    counter = rtc_struct_to_counter(time);
    counter += (946681200 + 3600);
    return counter;
}

bool need_update_time(uint32_t new_counter, uint32_t current_counter)
{
    DEBUG_WARN("New counter %u, current counter %u\r\n", new_counter, current_counter);
    static uint32_t m_last_time_update = 0;
    if (m_last_time_update == 0
        || (current_counter - m_last_time_update >= (uint32_t)(3600*12))
        || ((new_counter >= current_counter) && (new_counter - current_counter >= (uint32_t)60))
        || ((new_counter < current_counter) && (current_counter - new_counter >= (uint32_t)60)))
    {
        DEBUG_WARN("Update time\r\n");
        m_last_time_update = new_counter;
        return true;
    }
    return false;
}

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
        DEBUG_VERBOSE("Set URC port: %s\r\n", (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]");
        gsm_hw_send_at_cmd("AT+QCFG=\"urc/ri/smsincoming\",\"pulse\",2000\r\n", "OK\r\n", "", 1000, 10, gsm_at_cb_power_on_gsm);
        break;

    case 6:
        DEBUG_VERBOSE("Set URC ringtype: %s\r\n", (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]");
        gsm_hw_send_at_cmd("AT+CNMI=2,1,0,0,0\r\n", "", "OK\r\n", 1000, 10, gsm_at_cb_power_on_gsm);
        break;
    
    case 7:
        DEBUG_VERBOSE("Config SMS event report: %s\r\n", (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]");
        gsm_hw_send_at_cmd("AT+CMGF=1\r\n", "", "OK\r\n", 1000, 10, gsm_at_cb_power_on_gsm);
        break;

    case 8:
        DEBUG_VERBOSE("Set SMS text mode: %s\r\n", (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]");
        gsm_hw_send_at_cmd("AT\r\n", "OK\r\n", "", 1000, 5, gsm_at_cb_power_on_gsm);
        // gsm_hw_send_at_cmd("AT+CMGD=1,4\r\n", "	if (eeprom_cfg->io_enable.name.input_4_20ma_0_enable)OK\r\n", "", 2000, 5, gsm_at_cb_power_on_gsm);
        //gsm_hw_send_at_cmd("AT+CNUM\r\n", "", "OK\r\n", 2000, 5, gsm_at_cb_power_on_gsm);
        break;

    case 9:
        DEBUG_VERBOSE("AT CNUM: %s, %s\r\n", (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]",
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
            DEBUG_WARN("Get GSM IMEI: %s\r\n", imei_buffer);
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
        gsm_hw_send_at_cmd("AT+CIMI\r\n", "OK\r\n", "", 1000, 5, gsm_at_cb_power_on_gsm);
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
            gsm_hw_send_at_cmd("AT+QCCID\r\n", "QCCID", "OK\r\n", 1000, 3, gsm_at_cb_power_on_gsm);
        }
	}
        break;

    case 12:
    {
        DEBUG_INFO("Get SIM IMEI: %s\r\n", (char *)resp_buffer);
        gsm_change_hw_polling_interval(5);
		uint8_t *ccid_buffer = (uint8_t*)gsm_get_sim_ccid();
        if (strlen((char*)ccid_buffer) < 10)
        {
            gsm_utilities_get_sim_ccid(resp_buffer, ccid_buffer, 20);
        }
        DEBUG_INFO("SIM CCID: %s\r\n", ccid_buffer);
        static uint32_t retry = 0;
        if (strlen((char*)ccid_buffer) < 10 && retry < 2)
        {
            retry++;
            gsm_hw_send_at_cmd("AT+QCCID\r\n", "QCCID", "OK\r\n", 1000, 3, gsm_at_cb_power_on_gsm); 
            gsm_manager.step = 11;
        }
        else
        {
            gsm_hw_send_at_cmd("AT+CPIN?\r\n", "READY\r\n", "", 3000, 3, gsm_at_cb_power_on_gsm); 
        }
    }
        break;

    case 13:
        DEBUG_INFO("CPIN: %s\r\n", (char *)resp_buffer);
        gsm_change_hw_polling_interval(5);
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
        DEBUG_WARN("Query CCLK: %s,%s\r\n",
                     (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]",
                     (char *)resp_buffer);
        rtc_date_time_t time;
        gsm_utilities_parse_timestamp_buffer((char *)resp_buffer, &time);
        time.hour += TIMEZONE;
        static uint32_t retry = 0;
        if (time.year > 20 
			&& time.year < 40 
			&& time.hour < 24) // if 23h40 =>> time.hour += 7 = 30h =>> invalid
                                                                // Lazy solution : do not update time from 17h
        {
            retry = 0;
            uint32_t new_counter = make_counter(&time);
            uint32_t current_counter = app_rtc_get_counter();
            if (need_update_time(new_counter, current_counter))
            {
                app_rtc_set_counter(&time);
            }
        }
        else
        {
            retry++;
        }
        if (retry == 0 || retry > 5)
        {
            gsm_change_hw_polling_interval(5);
            gsm_hw_send_at_cmd("AT+CSQ\r\n", "", "OK\r\n", 1000, 5, gsm_at_cb_power_on_gsm);
        }
        else
        {
            DEBUG_WARN("Re-sync clock\r\n");
            gsm_hw_send_at_cmd("AT+CCLK?\r\n", "+CCLK:", "OK\r\n", 1200, 5, gsm_at_cb_power_on_gsm);
            gsm_manager.step = 21;
            gsm_change_hw_polling_interval(1000);
        }
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
    DEBUG_INFO("GSM hard reset step %d\r\n", hard_reset_step);

    switch (hard_reset_step)
    {
    case 0: // Power off // add a lot of delay in power off state, due to reserve time for measurement sensor data
            // If vbat is low, sensor data will be incorrect
        gsm_manager.gsm_ready = 0;
        GSM_PWR_EN(0);
        GSM_PWR_RESET(1);
        GSM_PWR_KEY(0);
        hard_reset_step++;
        break;
    
    case 1:
        GSM_PWR_RESET(0);
        DEBUG_INFO("Gsm power on\r\n");
        GSM_PWR_EN(1);
        hard_reset_step++;
        break;

    case 2:
        hard_reset_step++;
        break;

    case 3:
        usart1_control(true);
        DEBUG_INFO("Pulse power key\r\n");
        /* Tao xung |_| de Power On module, min 1s  */
        GSM_PWR_KEY(1);
        hard_reset_step++;
        break;

    case 4:
        GSM_PWR_KEY(0);
        GSM_PWR_RESET(0);
        gsm_manager.timeout_after_reset = 90;
        hard_reset_step++;
        break;

    case 5:
    case 6:
        hard_reset_step++;
        break;
    
    case 7:
        hard_reset_step = 0;
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
	"Inputl_J3_1": 0.01,
	"Input1_J3_2": 0.00,
	"Input1_J3_3": 0.01,
	"Input1_J3_4 ": 0.01,
	"Input1_J9_1 ": 1,
	"Input1_g9_2 ": 1,
	"Inputl_J9_3 ": 1,
	"Input_J9_4 ": 71,
	"Output1 ": 0,
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

//	char alarm_str[128];
//    char *p = alarm_str;
    volatile uint16_t total_length = 0;
	
    uint64_t ts = msg->measure_timestamp;
    ts *= 1000;
    
    total_length += sprintf((char *)(ptr + total_length), "{\"ts\":%llu,\"values\":", ts);
	total_length += sprintf((char *)(ptr + total_length), "%s", "{\"Err\":\"");
    
    total_length += build_error_code(ptr + total_length, msg);

	
    // Build timestamp
    total_length += sprintf((char *)(ptr + total_length), "\"Timestamp\":%u,", msg->measure_timestamp); //second since 1970
    
    int32_t temp_counter;
    
    total_length += build_device_id(total_length, ptr, gsm_get_module_imei());
   
	

/*
    total_length += sprintf((char *)(ptr + total_length), "\"Phone\":\"%s\",", eeprom_cfg->phone);
    total_length += sprintf((char *)(ptr + total_length), "\"Money\":%d,", 0);
    total_length += sprintf((char *)(ptr + total_length), "\"Dir\":%u,", eeprom_cfg->dir_level);
*/

#ifndef DTG01       // G2, G2V2, G2V3
    
    // Build all meter input data
	for (uint32_t i = 0; i < MEASURE_NUMBER_OF_WATER_METER_INPUT; i++)
	{
		// Build input pulse counter
		if (msg->counter[i].k == 0)
		{
			msg->counter[i].k = 1;
		}
		temp_counter = msg->counter[i].real_counter/msg->counter[i].k + msg->counter[i].indicator;
		total_length += sprintf((char *)(ptr + total_length), "\"Input1_J%u\":%d,",
									i+1,
									temp_counter);
        
        temp_counter = msg->counter[i].reverse_counter / msg->counter[i].k /* + msg->counter[i].indicator */;
        total_length += sprintf((char *)(ptr + total_length), "\"Input1_J%u_D\":%u,",
                                        i+1,
                                        temp_counter);
        
        // Total forward flow
        if (i)
        {
            total_length += sprintf((char *)(ptr + total_length), "\"ForwardFlow%u\":%.1f,",
                                                i+1,
                                                msg->counter[i].flow_speed_forward_agv_cycle_wakeup);
            
            total_length += sprintf((char *)(ptr + total_length), "\"ReverseFlow%u\":%.1f,",
                                                i+1,
                                                msg->counter[i].flow_speed_reserve_agv_cycle_wakeup);
            
//            float tmp = msg->counter[i].fw_flow;
//            tmp /= msg->counter[i].k;
//            total_length += sprintf((char *)(ptr + total_length), "\"ForwardFlow%u\":%.2f,",
//                                                i+1,
//                                                tmp);
//            
//            tmp = msg->counter[i].reverse_flow;
//            tmp /= msg->counter[i].k;
//            total_length += sprintf((char *)(ptr + total_length), "\"ReverseFlow%u\":%.2f,",
//                                                i+1,
//                                                tmp);
        }
        
        else
        {
//            float tmp = msg->counter[i].fw_flow;
//            tmp /= msg->counter[i].k;
//            total_length += sprintf((char *)(ptr + total_length), "\"ForwardFlow\":%.2f,",
//                                                tmp);
//            
//            tmp = msg->counter[i].reverse_flow;
//            tmp /= msg->counter[i].k;
//            total_length += sprintf((char *)(ptr + total_length), "\"ReverseFlow\":%.2f,",
//                                                tmp);
            total_length += sprintf((char *)(ptr + total_length), "\"ForwardFlow\":%.1f,",
                                                msg->counter[i].flow_speed_forward_agv_cycle_wakeup);
            
            total_length += sprintf((char *)(ptr + total_length), "\"ReverseFlow\":%.1f,",
                                                msg->counter[i].flow_speed_reserve_agv_cycle_wakeup);

        }
            
        // Total forward/reserve index : tong luu luong tich luy thuan/nguoc
        if (i)
        {
            total_length += sprintf((char *)(ptr + total_length), "\"ForwardIndex%u\":%u,",
                                                i+1,
                                                msg->counter[i].total_forward_index);
            total_length += sprintf((char *)(ptr + total_length), "\"ReverseIndex%u\":%u,",
                                                i+1,
                                                msg->counter[i].total_reverse_index);
        }
        else
        {
            total_length += sprintf((char *)(ptr + total_length), "\"ForwardIndex\":%u,",
                                                msg->counter[i].total_forward_index);
            total_length += sprintf((char *)(ptr + total_length), "\"ReverseIndex\":%u,",
                                                msg->counter[i].total_reverse_index);
        }
        
        if (msg->counter[i].flow_avg_cycle_send_web.valid)
        {
            if (i)
            {
                total_length += sprintf((char *)(ptr + total_length), "\"FwIdxHour%u\":%u,",
                                                    i+1,
                                                    msg->counter[i].total_forward_index);
                total_length += sprintf((char *)(ptr + total_length), "\"RvsIdxHour%u\":%u,",
                                                    i+1,
                                                    msg->counter[i].total_reverse_index);
            }
            else
            {
                total_length += sprintf((char *)(ptr + total_length), "\"FwIdxHour\":%u,",
                                                    msg->counter[i].total_forward_index);
                total_length += sprintf((char *)(ptr + total_length), "\"RvsIdxHour\":%u,",
                                                    msg->counter[i].total_reverse_index);
            }
        
            // Min max   
            if (i)
            {
                // min max forward flow
                total_length += sprintf((char *)(ptr + total_length), "\"MinForwardFlow%u\":%.1f,",
                                                i+1,
                                                msg->counter[i].flow_avg_cycle_send_web.fw_flow_min);
            
                total_length += sprintf((char *)(ptr + total_length), "\"MaxForwardFlow%u\":%.1f,",
                                                i+1,
                                                msg->counter[i].flow_avg_cycle_send_web.fw_flow_max);
            
                // min max reserve flow
                total_length += sprintf((char *)(ptr + total_length), "\"MinReverseFlow%u\":%.1f,",
                                                    i+1,
                                                    msg->counter[i].flow_avg_cycle_send_web.reserve_flow_min);
                
                total_length += sprintf((char *)(ptr + total_length), "\"MaxReverseFlow%u\":%.1f,",
                                                    i+1,
                                                    msg->counter[i].flow_avg_cycle_send_web.reserve_flow_max);
                
            }
            else
            {
                // min max forward flow
                total_length += sprintf((char *)(ptr + total_length), "\"MinForwardFlow\":%.1f,",
                                                msg->counter[i].flow_avg_cycle_send_web.fw_flow_min);
            
                total_length += sprintf((char *)(ptr + total_length), "\"MaxForwardFlow\":%.1f,",
                                                msg->counter[i].flow_avg_cycle_send_web.fw_flow_max);
            
                // min max reserve flow
                total_length += sprintf((char *)(ptr + total_length), "\"MinReverseFlow\":%.1f,",
                                                msg->counter[i].flow_avg_cycle_send_web.reserve_flow_min);
                
                total_length += sprintf((char *)(ptr + total_length), "\"MaxReverseFlow\":%.1f,",
                                                msg->counter[i].flow_avg_cycle_send_web.reserve_flow_max);

            }
            
            // Hour
            if (i)
            {
                total_length += sprintf((char *)(ptr + total_length), "\"ForwardFlowSum%u\":%.1f,",
                                                    i+1,
                                                    msg->counter[i].flow_avg_cycle_send_web.fw_flow_sum);
                total_length += sprintf((char *)(ptr + total_length), "\"ReverseFlowSum%u\":%.1f,",
                                                    i+1,
                                                    msg->counter[i].flow_avg_cycle_send_web.reverse_flow_sum);
            }
            
            else
            {
                total_length += sprintf((char *)(ptr + total_length), "\"ForwardFlowSum\":%.1f,",
                                                    msg->counter[i].flow_avg_cycle_send_web.fw_flow_sum);
                total_length += sprintf((char *)(ptr + total_length), "\"ReverseFlowSum\":%.1f,",
                                                    msg->counter[i].flow_avg_cycle_send_web.reverse_flow_sum);

            }
//            DEBUG_WARN("Send to server Min-max fw flow%u %.2f %.2f\r\n", i+1, msg->counter[i].flow_avg_cycle_send_web.fw_flow_min,
//                                                        msg->counter[i].flow_avg_cycle_send_web.fw_flow_max);
//                
//            DEBUG_WARN("Send to server Min-max resv flow%u %.2f %.2f\r\n", i+1, msg->counter[i].flow_avg_cycle_send_web.reserve_flow_min,
//                                                        msg->counter[i].flow_avg_cycle_send_web.reserve_flow_max);
        }
          
        
//		// K : he so chia cua dong ho nuoc, input 1
//		// Offset: Gia tri offset cua dong ho nuoc
//		// Mode : che do hoat dong
//		total_length += sprintf((char *)(ptr + total_length), "\"K%u\":%u,", i+1, msg->counter[i].k);
//		//total_length += sprintf((char *)(ptr + total_length), "\"Offset%u\":%u,", i+1, eeprom_cfg->offset[i]);
//		total_length += sprintf((char *)(ptr + total_length), "\"M%u\":%u,", i+1, msg->counter[i].mode);
	}		
    
    // Build input 4-20ma
    total_length += sprintf((char *)(ptr + total_length), "\"Input1_J3_1\":%.1f,", 
                                                                        msg->input_4_20mA[0]); // dau vao 4-20mA 0
    total_length += sprintf((char *)(ptr + total_length), "\"Input1_J3_2\":%.1f,", 
                                                                        msg->input_4_20mA[1]); // dau vao 4-20mA 0

    total_length += sprintf((char *)(ptr + total_length), "\"Input1_J3_3\":%.1f,", 
                                                                        msg->input_4_20mA[2]); // dau vao 4-20mA 0
    total_length += sprintf((char *)(ptr + total_length), "\"Input1_J3_4\":%.1f,", 
                                                                        msg->input_4_20mA[3]); // dau vao 4-20mA 0
    
    // Min max 4-20mA         
    // 4-20mA  
    for (uint32_t i = 0; i < NUMBER_OF_INPUT_4_20MA; i++)
    {
        if (msg->input_4_20ma_cycle_send_web[i].valid)
        {
            if (i)
            {
                total_length += sprintf((char *)(ptr + total_length), "\"MinPressure%u\":%.1f,", i+1,
                                                                            convert_input_4_20ma_to_pressure(msg->input_4_20ma_cycle_send_web[i].input4_20ma_min));
                total_length += sprintf((char *)(ptr + total_length), "\"MaxPressure%u\":%.1f,", i+1,
                                                                            convert_input_4_20ma_to_pressure(msg->input_4_20ma_cycle_send_web[i].input4_20ma_max));
            }
            else
            {
                total_length += sprintf((char *)(ptr + total_length), "\"MinPressure\":%.1f,",
                                                                            convert_input_4_20ma_to_pressure(msg->input_4_20ma_cycle_send_web[i].input4_20ma_min));
                total_length += sprintf((char *)(ptr + total_length), "\"MaxPressure\":%.1f,",
                                                                            convert_input_4_20ma_to_pressure(msg->input_4_20ma_cycle_send_web[i].input4_20ma_max));
            }
            
            total_length += sprintf((char *)(ptr + total_length), "\"PressureHour%u\":%.1f,", i+1,
                                                                        convert_input_4_20ma_to_pressure(msg->input_4_20mA[i]));
        }
        
        total_length += sprintf((char *)(ptr + total_length), "\"Pressure%u\":%.1f,", i+1,
                                                                        convert_input_4_20ma_to_pressure(msg->input_4_20mA[i]));
    }      
    
    // Build input on/off
	for (uint32_t i = 0; i < NUMBER_OF_INPUT_ON_OFF; i++)
	{
		total_length += sprintf((char *)(ptr + total_length), "\"Input1_J9_%u\":%u,", 
																			i+1,
																			msg->input_on_off[i]); // dau vao 4-20mA 0
	}	

//    // Build output on/off
//	for (uint32_t i = 0; i < NUMBER_OF_OUTPUT_ON_OFF; i++)
//	{
//		total_length += sprintf((char *)(ptr + total_length), "\"Output%u\":%u,", 
//																			i+1,
//																			msg->output_on_off[i]);  //dau ra on/off 
//	}
    
    // Build output 4-20ma
    #ifndef DTG02V3
        if (eeprom_cfg->io_enable.name.output_4_20ma_enable)
        {
            total_length += sprintf((char *)(ptr + total_length), "\"Output4_20\":%.1f,", msg->output_4_20mA[0]);   // dau ra 4-20mA
        }
    #else       // G2V3 moi cho phan fix bug nay
        if (msg->output_4_20ma_enable && (msg->output_4_20mA[0] < 4.0f))
        {
            total_length += sprintf((char *)(ptr + total_length), "\"Output4_20\":%.1f,", msg->output_4_20mA[0]);   // dau ra 4-20mA
        }
    #endif
#else	    // DTG01

    // K : he so chia cua dong ho nuoc, input 1
	// Offset: Gia tri offset cua dong ho nuoc
	// Mode : che do hoat dong
    if (msg->counter[0].k == 0)
    {
        DEBUG_ERROR("K is zero\r\n");
        msg->counter[0].k = 1;
    }
	total_length += sprintf((char *)(ptr + total_length), "\"K%u\":%u,", 0, msg->counter[0].k);
	total_length += sprintf((char *)(ptr + total_length), "\"M%u\":%u,", 0, eeprom_cfg->meter_mode[0]);
    
    // Build pulse counter
    temp_counter = msg->counter[0].real_counter / msg->counter[0].k + msg->counter[0].indicator;
    total_length += sprintf((char *)(ptr + total_length), "\"Input1\":%u,",
                              temp_counter); //so xung
    
    temp_counter = msg->counter[0].reverse_counter / msg->counter[0].k + msg->counter[0].indicator;
    total_length += sprintf((char *)(ptr + total_length), "\"Input1_J1_D\":%u,",
                                temp_counter);
	
    // Total forward flow
    total_length += sprintf((char *)(ptr + total_length), "\"ForwardFlow\":%u,",                                    
                                        msg->counter[0].total_forward);
    total_length += sprintf((char *)(ptr + total_length), "\"ReverseFlow\":%u,",                                        
                                        msg->counter[0].total_reverse);
    
    // Total forward/reserve index : tong luu luong tich luy thuan/nguoc
    total_length += sprintf((char *)(ptr + total_length), "\"ForwardIndex\":%u,",                                        
                                        msg->counter[0].total_forward_index);
    total_length += sprintf((char *)(ptr + total_length), "\"ReverseIndex\":%u,",                                       
                                        msg->counter[0].total_reverse_index);
    
    if (msg->counter[0].flow_avg_cycle_send_web.valid)
    {
        // min max forward flow
        total_length += sprintf((char *)(ptr + total_length), "\"MinForwardFlow\":%.2f,",
                                        msg->counter[0].flow_avg_cycle_send_web.fw_flow_min);
    
        total_length += sprintf((char *)(ptr + total_length), "\"MaxForwardFlow\":%.2f,",
                                        
                                        msg->counter[0].flow_avg_cycle_send_web.fw_flow_max);
    
        // min max reserve flow
        total_length += sprintf((char *)(ptr + total_length), "\"MinReverseFlow\":%.2f,",                                           
                                            msg->counter[0].flow_avg_cycle_send_web.reserve_flow_min);
        
        total_length += sprintf((char *)(ptr + total_length), "\"MaxReverseFlow\":%.2f,",                                           
                                            msg->counter[0].flow_avg_cycle_send_web.reserve_flow_max);
    }


    
    // Build input on/off
    total_length += sprintf((char *)(ptr + total_length), "\"Output1\":%u,", 
                                                            msg->output_on_off[0]); // dau vao on/off
    // Build input 4-20ma
    total_length += sprintf((char *)(ptr + total_length), "\"Input2\":%.2f,", 
                                                        msg->input_4_20mA[0]); // dau vao 4-20mA 0
    // Min max 4-20mA     
    if (msg->input_4_20ma_cycle_send_web[0].valid)
    {
        total_length += sprintf((char *)(ptr + total_length), "\"MinPressure1\":%.2f,",
                                                                convert_input_4_20ma_to_pressure(msg->input_4_20ma_cycle_send_web[0].input4_20ma_min));
        total_length += sprintf((char *)(ptr + total_length), "\"MaxPressure1\":%.2f,",
                                                                convert_input_4_20ma_to_pressure(msg->input_4_20ma_cycle_send_web[0].input4_20ma_max));
    }
    total_length += sprintf((char *)(ptr + total_length), "\"Pressure1\":%.2f,",
                                                            convert_input_4_20ma_to_pressure(msg->input_4_20mA[0]));

    // Build ouput 4-20ma
    total_length += sprintf((char *)(ptr + total_length), "\"Output2\":%.2f,", 
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
    total_length += sprintf((char *)(ptr + total_length), "\"Vbat\":%u,", msg->vbat_mv);
//#ifndef DTG01    // DTG02 : Vinput 24V
//    total_length += sprintf((char *)(ptr + total_length), "\"Vin\":%.1f,", msg->vin_mv/1000);
//#endif    
    // Temperature
	total_length += sprintf((char *)(ptr + total_length), "\"Temperature\":%d,", msg->temperature);
    
//    // Reset reason
//    total_length += sprintf((char *)(ptr + total_length), "\"RST\":%u,", hardware_manager_get_reset_reason()->value);
	// Reg0_1:1234, Reg0_2:4567, Reg1_0:12345
#if 1
	for (uint32_t index = 0; index < RS485_MAX_SLAVE_ON_BUS; index++)
	{
        // Min max
        
                    // Min forward flow
        if (msg->rs485[index].min_max.forward_flow.type_float != INPUT_485_INVALID_FLOAT_VALUE)
        {
            uint32_t tmp_len = total_length;
            DEBUG_WARN("%s\r\n", (ptr+tmp_len));
            total_length += sprintf((char *)(ptr + total_length), "\"MbFwFl%u\":%.1f,", 
                                                            index+1, msg->rs485[index].min_max.forward_flow.type_float);
            DEBUG_WARN("%s\r\n", (ptr+tmp_len));
        }
        
        if (msg->rs485[index].min_max.reverse_flow.type_float != INPUT_485_INVALID_FLOAT_VALUE)
        {
            uint32_t tmp_len = total_length;
            total_length += sprintf((char *)(ptr + total_length), "\"MbRvsFlw%u\":%.1f,", 
                                                            index+1, msg->rs485[index].min_max.reverse_flow.type_float);
            DEBUG_WARN("%s\r\n", (ptr+tmp_len));
        }
        
        
        if (msg->rs485[index].min_max.valid)
        {
            // Min forward flow
            if (msg->rs485[index].min_max.min_forward_flow.type_float != INPUT_485_INVALID_FLOAT_VALUE)
            {
                uint32_t tmp_len = total_length;
                total_length += sprintf((char *)(ptr + total_length), "\"MbMinFwFl%u\":%.1f,", 
                                                                index+1, msg->rs485[index].min_max.min_forward_flow.type_float);
                DEBUG_WARN("%s\r\n", (ptr+tmp_len));
            }
            
            // Max forward flow
            if (msg->rs485[index].min_max.max_forward_flow.type_float != INPUT_485_INVALID_FLOAT_VALUE)
            {
                uint32_t tmp_len = total_length;
                total_length += sprintf((char *)(ptr + total_length), "\"MbMaxFwFl%u\":%.1f,", 
                                                                index+1, msg->rs485[index].min_max.max_forward_flow.type_float);
                DEBUG_WARN("%s\r\n", (ptr+tmp_len));
            }
            
            // Min reserve flow
            if (msg->rs485[index].min_max.min_reverse_flow.type_float != INPUT_485_INVALID_FLOAT_VALUE)
            {
                uint32_t tmp_len = total_length;
                total_length += sprintf((char *)(ptr + total_length), "\"MbMinRvsFl%u\":%.1f,", 
                                                                index+1, msg->rs485[index].min_max.min_reverse_flow.type_float);
                DEBUG_WARN("%s\r\n", (ptr+tmp_len));
            }
            
            // Max reserve flow
            if (msg->rs485[index].min_max.max_reverse_flow.type_float != INPUT_485_INVALID_FLOAT_VALUE)
            {
                uint32_t tmp_len = total_length;
                total_length += sprintf((char *)(ptr + total_length), "\"MbMaxRvsFl%u\":%.1f,", 
                                                                index+1, msg->rs485[index].min_max.max_reverse_flow.type_float);
                DEBUG_WARN("%s\r\n", (ptr+tmp_len));
            }
#if MEASURE_SUM_MB_FLOW           
            // Forward flow sum
            total_length += sprintf((char *)(ptr + total_length), "\"MbFwFlSum%u\":%.2f,", 
                                                                index+1, msg->rs485[index].min_max.forward_flow_sum.type_float);
            // Reverse flow sum
            total_length += sprintf((char *)(ptr + total_length), "\"MbRvsFwSum%u\":%.2f,", 
                                                                index+1, msg->rs485[index].min_max.reverse_flow_sum.type_float);
#endif
            if (msg->rs485[index].min_max.net_speed.type_float != INPUT_485_INVALID_FLOAT_VALUE)
            {
                total_length += sprintf((char *)(ptr + total_length), "\"MbNetFlSum%u\":%.1f,", 
                                                                index+1, msg->rs485[index].min_max.net_speed.type_float);
            }
            
            if (msg->rs485[index].min_max.net_volume_fw.type_float != INPUT_485_INVALID_FLOAT_VALUE)
            {
                total_length += sprintf((char *)(ptr + total_length), "\"MbNetFwFlSum%u\":%.1f,", 
                                                                index+1, msg->rs485[index].min_max.net_volume_fw.type_float);
            }
            
            if (msg->rs485[index].min_max.net_volume_reverse.type_float != INPUT_485_INVALID_FLOAT_VALUE)
            {
                total_length += sprintf((char *)(ptr + total_length), "\"MbNetRvsFlSum%u\":%.1f,", 
                                                                index+1, msg->rs485[index].min_max.net_volume_reverse.type_float);
            }
           
            if (msg->rs485[index].min_max.net_index.type_float != INPUT_485_INVALID_FLOAT_VALUE)
            {
                total_length += sprintf((char *)(ptr + total_length), "\"Mbidxhour%u\":%.1f,", 
                                                                index+1, msg->rs485[index].min_max.net_index.type_float);
            } 
        }

        // Value
//        total_length += sprintf((char *)(ptr + total_length), "\"SlID%u\":%u,", index+1, msg->rs485[index].slave_addr);
        for (uint32_t sub_idx = 0; sub_idx < RS485_MAX_SUB_REGISTER; sub_idx++)
        {
            if (msg->rs485[index].sub_register[sub_idx].read_ok
                && msg->rs485[index].sub_register[sub_idx].data_type.name.valid)
            {
                uint32_t tmp_len = total_length;
                total_length += sprintf((char *)(ptr + total_length), "\"Rg%u_%u\":", index+1, sub_idx+1);
                if (msg->rs485[index].sub_register[sub_idx].data_type.name.type == RS485_DATA_TYPE_INT16 
                    || msg->rs485[index].sub_register[sub_idx].data_type.name.type == RS485_DATA_TYPE_INT32)
                {
                    total_length += sprintf((char *)(ptr + total_length), "%u,", msg->rs485[index].sub_register[sub_idx].value.int32_val);
                }
                else
                {
                    // dump hexa
                    total_length += sprintf((char *)(ptr + total_length), "%.1f,", msg->rs485[index].sub_register[sub_idx].value.float_val);
//                    total_length += sprintf((char *)(ptr + total_length), "\"%.1f - 0x%02X%02X%02X%02X\",", 
//                                                                                    msg->rs485[index].sub_register[sub_idx].value.float_val,
//                                                                                    msg->rs485[index].sub_register[sub_idx].value.raw[0],
//                                                                                    msg->rs485[index].sub_register[sub_idx].value.raw[1],
//                                                                                    msg->rs485[index].sub_register[sub_idx].value.raw[2],
//                                                                                    msg->rs485[index].sub_register[sub_idx].value.raw[3]);
                }
//                if (strlen((char*)msg->rs485[index].sub_register[sub_idx].unit))
//                {
//                    total_length += sprintf((char *)(ptr + total_length), "\"U%u_%u\":\"%s\",", index+1, sub_idx+1, msg->rs485[index].sub_register[sub_idx].unit);
//                }
//                DEBUG_WARN("%s\r\n", (ptr+tmp_len));
            }
            else if (msg->rs485[index].sub_register[sub_idx].data_type.name.valid)
            {
                uint32_t tmp_len = total_length;
//                if (strlen((char*)msg->rs485[index].sub_register[sub_idx].unit))
//                {
//                    total_length += sprintf((char *)(ptr + total_length), "\"U%u_%u\":\"%s\",", index+1, sub_idx+1, msg->rs485[index].sub_register[sub_idx].unit);
//                }
                total_length += sprintf((char *)(ptr + total_length), "\"Rg%u_%u\":\"%s\",", index+1, sub_idx+1, "FFFF");
//                DEBUG_WARN("%s\r\n", (ptr+tmp_len));
            }	
        }
	}
#endif    
    // Sim imei
    total_length += sprintf((char *)(ptr + total_length), "\"SIM\":%s,", gsm_get_sim_imei());
	// total_length += sprintf((char *)(ptr + total_length), "\"CCID\":\"%s\",", gsm_get_sim_ccid());
    
    // Uptime
//    total_length += sprintf((char *)(ptr + total_length), "\"Uptime\":%u,", m_wake_time);
    total_length += sprintf((char *)(ptr + total_length), "\"Sendtime\":%u,", ++m_send_time);
    DEBUG_INFO("Send time %u, ts %u\r\n", m_send_time, msg->measure_timestamp);

#if defined(DTG02V2)
    total_length += sprintf((char *)(ptr + total_length), "\"charge\":%u,", LL_GPIO_IsOutputPinSet(CHARGE_EN_GPIO_Port, CHARGE_EN_Pin) ? 1 : 0);
#endif	
#if defined(DTG02V3)
    uint32_t charge = 0;
    if (LL_GPIO_IsOutputPinSet(CHARGE_EN_GPIO_Port, CHARGE_EN_Pin) && LL_GPIO_IsOutputPinSet(SYS_5V_EN_GPIO_Port, SYS_5V_EN_Pin))
    {
        charge = 1;
    }
    total_length += sprintf((char *)(ptr + total_length), "\"charge\":%u,", charge);
#endif	
    
    app_eeprom_factory_data_t *factory = app_eeprom_read_factory_data();
//    if (factory->baudrate.baudrate_valid_key == EEPROM_BAUD_VALID)
//    {
//        total_length += sprintf((char *)(ptr + total_length), "\"baud\":%u,", factory->baudrate.value);
//    }
//    else
//    {
//        total_length += sprintf((char *)(ptr + total_length), "\"baud\":%u,", APP_EEPROM_DEFAULT_BAUD);
//    }

//    total_length += sprintf((char *)(ptr + total_length), "\"bytes\":%u,", factory->byte_order);
//    total_length += sprintf((char *)(ptr + total_length), "\"pulse\":%u,", factory->pulse_ms);
    
//	// Release date
//	total_length += sprintf((char *)(ptr + total_length), "\"Build\":\"%s %s\",", __DATE__, __TIME__);
	
//    total_length += sprintf((char *)(ptr + total_length), "\"FacSVR\":\"%s\",", app_eeprom_read_factory_data()->server);
	
//    // Firmware and hardware
//    total_length += sprintf((char *)(ptr + total_length), "\"RstReason\":%u,", hardware_manager_get_reset_reason()->value);
    total_length += sprintf((char *)(ptr + total_length), "\"FW\":\"%s\",", VERSION_CONTROL_FW);
    total_length += sprintf((char *)(ptr + total_length), "\"HW\":\"%s\"}}", VERSION_CONTROL_HW);
    
//    hardware_manager_get_reset_reason()->value = 0;
    //DEBUG_INFO("Size %u, data %s\r\n", total_length, (char*)ptr);
//    usart_lpusart_485_control(true);
//    sys_delay_ms(500);
//    usart_lpusart_485_send((uint8_t*)ptr, total_length);
    
    return total_length;
}

static measure_input_perpheral_data_t *m_sensor_msq;

static void gsm_http_event_cb(gsm_http_event_t event, void *data)
{
	sys_ctx_t *ctx = sys_ctx();
    sys_turn_on_led(3);
    switch (event)
    {
    case GSM_HTTP_EVENT_START:
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
        
        // If we have data need to send to server from ext flash
        // Else we scan measure input message queeu
        if (m_retransmision_data_in_flash)
        {
            DEBUG_INFO("Found retransmission data\r\n");                
            // Copy old pulse counter from spi flash to temp variable
            static measure_input_perpheral_data_t tmp;
            memset(&tmp, 0, sizeof(measure_input_perpheral_data_t));
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
            
            tmp.csq_percent = m_retransmision_data_in_flash->csq_percent;
            tmp.measure_timestamp = m_retransmision_data_in_flash->timestamp;
            tmp.temperature = m_retransmision_data_in_flash->temp;
            tmp.vbat_mv = m_retransmision_data_in_flash->vbat_mv;
            tmp.vbat_percent = m_retransmision_data_in_flash->vbat_precent;
            
            // on/off
            tmp.output_on_off[0] = m_retransmision_data_in_flash->on_off.name.output_on_off_0;
#if defined (DTG02) || defined (DTG02V2) || defined (DTG02V3)
            tmp.input_on_off[0] = m_retransmision_data_in_flash->on_off.name.input_on_off_0;
            tmp.input_on_off[1] = m_retransmision_data_in_flash->on_off.name.input_on_off_1;
            tmp.output_on_off[1] = m_retransmision_data_in_flash->on_off.name.output_on_off_1;
            #ifndef DTG02V3     // Chi co G2, G2V2 moi co them 2 input on/off 3-4
                tmp.input_on_off[2] = m_retransmision_data_in_flash->on_off.name.input_on_off_2;
                tmp.input_on_off[3] = m_retransmision_data_in_flash->on_off.name.input_on_off_3;
            #endif
            tmp.output_on_off[2] = m_retransmision_data_in_flash->on_off.name.output_on_off_2;
            tmp.output_on_off[3] = m_retransmision_data_in_flash->on_off.name.output_on_off_3;
#endif
            
            // Modbus
            for (uint32_t i = 0; i < RS485_MAX_SLAVE_ON_BUS; i++)
            {
                memcpy(&tmp.rs485[i], &m_retransmision_data_in_flash->rs485[i], sizeof(measure_input_modbus_register_t));
            }
            
            m_sensor_msq = &tmp;
            build_msg = true;
        }
        else        // Get data from measure input queue
        {
            DEBUG_INFO("Get http post data from queue\r\n");
            m_sensor_msq = measure_input_get_data_in_queue();
            if (!m_sensor_msq)
            {       
                DEBUG_INFO("No data in RAM queue, scan message in flash\r\n");
                gsm_change_state(GSM_STATE_OK);
            }
            else
            {
                build_msg = true;
            }
        }
        
        if (build_msg)
        {
            if (m_last_http_msg)
            {
                m_last_http_msg = NULL;
                m_malloc_count--;
            }
            
#ifdef DTG01
            static char m_http_mem[1024];
            m_last_http_msg = m_http_mem;
#else
            static char m_http_mem[1024+512];
            m_last_http_msg = m_http_mem;
#endif
            if (m_last_http_msg)
            {
                ++m_malloc_count;
                
                // Build sensor message
                m_sensor_msq->state = MEASUREMENT_QUEUE_STATE_PROCESSING;
                gsm_build_sensor_msq(m_last_http_msg, m_sensor_msq);
                
                ((gsm_http_data_t *)data)->data = (uint8_t *)m_last_http_msg;
                ((gsm_http_data_t *)data)->data_length = strlen(m_last_http_msg);
                ((gsm_http_data_t *)data)->header = (uint8_t *)"";
            }
            else
            {
                DEBUG_ERROR("Malloc error\r\n");
                NVIC_SystemReset();
            }
        }
    }
    break;

    case GSM_HTTP_GET_EVENT_FINISH_SUCCESS:
    {
        DEBUG_INFO("HTTP get : event success\r\n");
        app_eeprom_config_data_t *eeprom_cfg = app_eeprom_read_config_data();
        ctx->status.disconnect_timeout_s = 0;
        if (ctx->status.enter_ota_update            // If ota update finish and all file downloaded =>> turn off gsm
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
			
//			umm_free(ctx->status.new_server);
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
        LED1(0);
		
#ifdef WDT_ENABLE
    LL_IWDG_ReloadCounter(IWDG);
#endif
    }
    break;

    case GSM_HTTP_POST_EVENT_FINISH_SUCCESS:
    {
        DEBUG_INFO("HTTP post : event success\r\n");
        m_post_failed = false;
        ctx->status.disconnect_timeout_s = 0;
        
        // Release old memory from send buffer
        if (m_last_http_msg)
        {
            m_malloc_count--;
//            umm_free(m_last_http_msg);
            m_last_http_msg = NULL;
        }
        LED1(0);
        
        // Read data from ext flash
        static app_spi_flash_data_t wr_data;
        
        // Mark flag we dont need to send data to server
        wr_data.resend_to_server_flag = APP_FLASH_DONT_NEED_TO_SEND_TO_SERVER_FLAG;
        
        // Copy 4-20mA input
        for (uint32_t i = 0; i < APP_FLASH_NB_OFF_4_20MA_INPUT; i++)
        {
            wr_data.input_4_20mA[i] = m_sensor_msq->input_4_20mA[i];
            memcpy(&wr_data.input_4_20ma_cycle_send_web[i], 
                    &m_sensor_msq->input_4_20ma_cycle_send_web[i], 
                    sizeof(input_4_20ma_min_max_hour_t));
        }
        
		for (uint32_t i = 0; i < MEASURE_NUMBER_OF_WATER_METER_INPUT; i++)
		{
			memcpy(&wr_data.counter[i], &m_sensor_msq->counter[i], sizeof(measure_input_counter_t));
		}
        
		// on/off
		wr_data.on_off.name.output_on_off_0 = m_sensor_msq->output_on_off[0];
#if defined(DTG02) || defined(DTG02V2) || defined(DTG02V3)
		wr_data.on_off.name.input_on_off_0 = m_sensor_msq->input_on_off[0];
		wr_data.on_off.name.input_on_off_1 = m_sensor_msq->input_on_off[1];
		wr_data.on_off.name.output_on_off_1 = m_sensor_msq->output_on_off[1];
        wr_data.on_off.name.output_on_off_2 = m_sensor_msq->output_on_off[2];
    #ifndef DTG02V3     // Chi co G2, G2V2 moi co them 2 input on/off 3-4
		wr_data.on_off.name.input_on_off_2 = m_sensor_msq->input_on_off[2];
		wr_data.on_off.name.input_on_off_3 = m_sensor_msq->input_on_off[3];
    #endif //not define DTG02V3
		wr_data.on_off.name.output_on_off_3 = m_sensor_msq->output_on_off[3];
#endif		
		// 4-20mA output
		for (uint32_t i = 0; i < NUMBER_OF_OUTPUT_4_20MA; i++)
		{
			wr_data.output_4_20mA[i] = m_sensor_msq->output_4_20mA[i];
		}
		
        wr_data.csq_percent = m_sensor_msq->csq_percent;
        wr_data.timestamp = m_sensor_msq->measure_timestamp;
        wr_data.valid_flag = APP_FLASH_VALID_DATA_KEY;
        wr_data.vbat_mv = m_sensor_msq->vbat_mv;
        wr_data.vbat_precent = m_sensor_msq->vbat_percent;
        wr_data.temp = m_sensor_msq->temperature;
        
        // Copy 485 data
		for (uint32_t i = 0; i < RS485_MAX_SLAVE_ON_BUS; i++)
		{
			memcpy(&wr_data.rs485[i], &m_sensor_msq->rs485[i], sizeof(measure_input_modbus_register_t));
		}
        
        // Wakeup flash if needed
        if (!ctx->peripheral_running.name.flash_running)
        {
//            DEBUG_VERBOSE("Wakup flash\r\n");
            spi_init();
            app_spi_flash_wakeup();
            ctx->peripheral_running.name.flash_running = 1;
        }
        
        // Commit data and release memory
        app_spi_flash_write_data(&wr_data);
        
        m_sensor_msq->state = MEASUREMENT_QUEUE_STATE_IDLE;
        m_sensor_msq = NULL;
        
        // Scan for retransmission data
        bool re_send = false;
        
        if (!ctx->peripheral_running.name.flash_running)
        {
            spi_init();
            app_spi_flash_wakeup();
            ctx->peripheral_running.name.flash_running = 1;
        }
        uint32_t addr = app_spi_flash_estimate_current_read_addr(&re_send, false);
        if (re_send)
        {
            DEBUG_INFO("Need re-transission at addr 0x%08X\r\n", addr);
            if (!app_spi_flash_get_stored_data(addr, &m_flash_rd_data, true))
            {
                DEBUG_ERROR("Read failed\r\n");
                m_retransmision_data_in_flash = NULL;
            }
            else
            {
                m_retransmision_data_in_flash = &m_flash_rd_data;
                DEBUG_INFO("Enter http post\r\n");
                GSM_ENTER_HTTP_POST();
            }
        }       // no need retransmisson
        else if (measure_input_sensor_data_availble() == 0)
        {
            if (sys_ctx()->status.poll_broadcast_msg_from_server)
            {
                sys_ctx()->status.poll_broadcast_msg_from_server = 0;
            }
            else
            {
                DEBUG_INFO("No more data need to re-send to server\r\n");
                m_retransmision_data_in_flash = NULL;
            }
        }
        
        gsm_change_state(GSM_STATE_OK);
        
#ifdef WDT_ENABLE
    LL_IWDG_ReloadCounter(IWDG);
#endif
    }
    break;

    case GSM_HTTP_POST_EVENT_FINISH_FAILED:
    {
        DEBUG_WARN("Http post event failed\r\n");
        m_post_failed = true;
        if (m_last_http_msg)
        {
            m_malloc_count--;
//            umm_free(m_last_http_msg);
            m_last_http_msg = NULL;
        }
        
        static app_spi_flash_data_t wr_data;;

        wr_data.resend_to_server_flag = 0;
		
		// Input 4-20mA
        for (uint32_t i = 0; i < APP_FLASH_NB_OFF_4_20MA_INPUT; i++)
        {
            wr_data.input_4_20mA[i] = m_sensor_msq->input_4_20mA[i];
            memcpy(&wr_data.input_4_20ma_cycle_send_web[i], 
                    &m_sensor_msq->input_4_20ma_cycle_send_web[i], 
                    sizeof(input_4_20ma_min_max_hour_t));
        }
		
		for (uint32_t i = 0; i < MEASURE_NUMBER_OF_WATER_METER_INPUT; i++)
        {
			memcpy(&wr_data.counter[i], &m_sensor_msq->counter[i], sizeof(measure_input_counter_t));
        }
      
        wr_data.timestamp = m_sensor_msq->measure_timestamp;
        wr_data.valid_flag = APP_FLASH_VALID_DATA_KEY;
        wr_data.vbat_mv = m_sensor_msq->vbat_mv;
        wr_data.vbat_precent = m_sensor_msq->vbat_percent;
        wr_data.temp = m_sensor_msq->temperature;
        wr_data.csq_percent = m_sensor_msq->csq_percent;
		
		// on/off
		wr_data.on_off.name.output_on_off_0 = m_sensor_msq->output_on_off[0];
#if defined(DTG02) || defined(DTG02V2) || defined(DTG02V3)
		wr_data.on_off.name.input_on_off_0 = m_sensor_msq->input_on_off[0];
		wr_data.on_off.name.input_on_off_1 = m_sensor_msq->input_on_off[1];
		wr_data.on_off.name.output_on_off_1 = m_sensor_msq->output_on_off[1];
    #ifndef DTG02V3     // Chi co G2, G2V2 moi co them 2 input on/off 3-4
		wr_data.on_off.name.input_on_off_2 = m_sensor_msq->input_on_off[2];
        wr_data.on_off.name.input_on_off_3 = m_sensor_msq->input_on_off[3];
    #endif
		wr_data.on_off.name.output_on_off_2 = m_sensor_msq->output_on_off[2];
		wr_data.on_off.name.output_on_off_3 = m_sensor_msq->output_on_off[3];
#endif
		
		// Output 4-20mA
		for (uint32_t i = 0; i < NUMBER_OF_OUTPUT_4_20MA; i++)
		{
			wr_data.output_4_20mA[i] = m_sensor_msq->output_4_20mA[i];
		}
		
		// 485
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
        if (m_sensor_msq->state != MEASUREMENT_QUEUE_STATE_IDLE)
        {
            app_spi_flash_write_data(&wr_data);
            m_sensor_msq->state = MEASUREMENT_QUEUE_STATE_IDLE;
        }
        if (m_retransmision_data_in_flash)
        {
            m_retransmision_data_in_flash = NULL;
        }

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
//			umm_free(ctx->status.new_server);
			ctx->status.new_server = NULL;
			DEBUG_ERROR("Try new server failed\r\n");
		}
		
        if (m_last_http_msg)
        {
            m_malloc_count--;
//            umm_free(m_last_http_msg);
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
            
            if (m_last_http_msg)
            {
                m_malloc_count--;
//                umm_free(m_last_http_msg);
                m_last_http_msg = NULL;
            }
        
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

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
#include "Main.h"
//#include "Parameters.h"
#include "hardware.h"
#include "hardware_manager.h"
#include "gsm_http.h"
#include "server_msg.h"
#include "app_queue.h"
#include "umm_malloc.h"

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

/******************************************************************************
                                   GLOBAL VARIABLES					    			 
 ******************************************************************************/
extern System_t xSystem;
//extern SMSStruct_t SMSMemory[3];
extern GSM_Manager_t gsm_manager;


#define GET_BTS_INFOR_TIMEOUT 300
/******************************************************************************
                                   PRIVATE VARIABLES					    			 
 ******************************************************************************/
#if __GSM_SMS_ENABLE__
static char tmpBuffer[50] = {0};
#endif

static char m_at_cmd_buffer[128];
static bool m_request_to_send_at_cmd = false;
static uint8_t m_timeout_switch_at_mode = 0;
uint8_t InSleepModeTick = 0;
uint8_t TimeoutToSleep = 0;
static app_queue_t m_http_msq;

void gsm_at_cb_power_on_gsm(gsm_response_event_t event, void *resp_buffer);
void gsm_at_cb_read_sms(gsm_response_event_t event, void *resp_buffer);
void gsm_at_cb_send_sms(gsm_response_event_t event, void *resp_buffer);
void gsm_data_layer_switch_mode_at_cmd(gsm_response_event_t event, void *resp_buffer);
void gsm_at_cb_exit_sleep(gsm_response_event_t event, void *resp_buffer);
void gsm_hard_reset(void);
uint8_t convert_csq_to_percent(uint8_t csq);
uint8_t gsm_check_ready_status(void);
uint8_t gsm_check_idle(void);
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

static uint32_t rtc_struct_to_counter(date_time_t *t)
{
    static const uint8_t days_in_month[] = {31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    uint16_t i;
    uint32_t result = 0;
    uint16_t idx, year;

    year = t->year + 2000;

    /* Calculate days of years before */
    result = (uint32_t)year * 365;
    if (t->year >= 1)
    {
        result += (year + 3) / 4;
        result -= (year - 1) / 100;
        result += (year - 1) / 400;
    }

    /* Start with 2000 a.d. */
    result -= 730485UL;

    /* Make month an array index */
    idx = t->month - 1;

    /* Loop thru each month, adding the days */
    for (i = 0; i < idx; i++)
    {
        result += days_in_month[i];
    }

    /* Leap year? adjust February */
    if (year % 400 == 0 || (year % 4 == 0 && year % 100 != 0))
    {
    }
    else
    {
        if (t->month > 2)
        {
            result--;
        }
    }

    /* Add remaining days */
    result += t->day;

    /* Convert to seconds, add all the other stuff */
    result = (result - 1) * 86400L + (uint32_t)t->hour * 3600 +
             (uint32_t)t->minute * 60 + t->second;

    return result;
}

void gsm_set_flag_prepare_enter_read_sms_mode(void)
{
    m_do_read_sms = true;
}

void gsm_wakeup_periodically(void)
{
    DEBUG_PRINTF("Sleep time %u, periodic send msg %u, remaining %uS\r\n",
                 xSystem.Status.gsm_sleep_time_s,
                 xSystem.Parameters.period_send_message_to_server_min * 60,
                 xSystem.Parameters.period_send_message_to_server_min * 60 - xSystem.Status.gsm_sleep_time_s);

    if (xSystem.Status.gsm_sleep_time_s >= xSystem.Parameters.period_send_message_to_server_min * 60)
    {
        xSystem.Status.gsm_sleep_time_s = 0;
        DEBUG_PRINTF("GSM: wakeup to send msg\r\n");
        xSystem.Status.YeuCauGuiTin = 2;
        gsm_change_state(GSM_STATE_WAKEUP);
    }
}


uint8_t m_send_at_cmd_in_idle_mode = 0;

void GSM_ManagerTestSleep(void)
{
    gsm_manager.state = GSM_STATE_SLEEP;
}

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
    if (xSystem.Status.InitSystemDone != 1)
        return;
    if (gsm_manager.isGSMOff == 1)
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
        
        if (TimeoutToSleep++ >= MAX_TIMEOUT_TO_SLEEP_S)
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
        
            if (m_internet_mode == GSM_INTERNET_MODE_AT_STACK)
            {
                //#warning "Sleep in http mode is not enabled"
                if (enter_sleep_in_http)
                //if (0)
                {
                    gsm_change_state(GSM_STATE_SLEEP);
                }
                else
                {
                    if (TimeoutToSleep > 15)
                    {
                        TimeoutToSleep -= 15;
                    }
                }
            }
        }
    }
    break;

    case GSM_STATE_RESET: /* Hard Reset */
        gsm_manager.GSMReady = 0;
        gsm_hard_reset();
        break;

    case GSM_STATE_READ_SMS: /* Read SMS */
        if (gsm_manager.step == 0)
        {
            gsm_enter_read_sms();
        }
        break;

    case GSM_STATE_SEND_SMS: /* Send SMS */

        if (!gsm_manager.GSMReady)
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
                    xSystem.Parameters.gsm_imei);
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
            if (!xSystem.file_transfer.ota_is_running)
            {
                sprintf(cfg.url, "https://iot.wilad.vn/api/v1/%s/attributes",
                        xSystem.Parameters.gsm_imei);
                cfg.on_event_cb = gsm_http_event_cb;
                cfg.action = GSM_HTTP_ACTION_GET;
                cfg.port = 443;
                cfg.big_file_for_ota = 0;
                gsm_http_start(&cfg);
                GSM_DONT_NEED_HTTP_GET();
            }
            else
            {
                sprintf(cfg.url, "%s", xSystem.file_transfer.url);
                cfg.on_event_cb = gsm_http_event_cb;
                cfg.action = GSM_HTTP_ACTION_GET;
                cfg.port = 443;
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
        if (InSleepModeTick % 10 == 0)
        {
            DEBUG_PRINTF("GSM is sleeping...\r\n");
        }
        InSleepModeTick++;
        TimeoutToSleep = 0;
#ifdef GD32E10X
        driver_uart_deinitialize(GSM_UART);
        usart_interrupt_flag_clear(GSM_UART, USART_INT_FLAG_RBNE);
        usart_interrupt_flag_clear(GSM_UART, USART_INT_FLAG_TBE);
#else
        uart1_control(false);
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

    //Khong co SIM
    if (strlen(xSystem.Parameters.sim_imei) < 15)
        return;

    /* Yeu cau gui lenh AT */
    if (gsm_manager.GSMReady == 1 && m_request_to_send_at_cmd == 1)
    {
        if (m_timeout_switch_at_mode && (m_timeout_switch_at_mode % 5 == 0)) //check lai sau 5s
        {
            if (gsm_check_idle())
            {
                //Du dieu kien thi gui lenh AT
                gsm_manager.step = 0;
                gsm_manager.state = GSM_STATE_SEND_ATC;
            }
        }
        if (m_timeout_switch_at_mode)
        {
            m_timeout_switch_at_mode--;
            if (m_timeout_switch_at_mode == 0)
            {
                m_request_to_send_at_cmd = false;
                DEBUG_PRINTF("Timeout switch to AT mode is over!\r\n");
            }
        }
    }
}


uint8_t gsm_check_idle(void)
{
    if (!gsm_manager.GSMReady)
        return 0;
    if (gsm_manager.RISignal)
        return 0;
    if (gsm_manager.state != GSM_STATE_OK)
        return 0;
    if (xSystem.file_transfer.State != FT_NO_TRANSER)
        return 0;

    //Dang gui TCP -> busy
    if (xSystem.Status.SendGPRSTimeout)
        return 0;

    return 1;
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
    gsm_manager.RISignal = 0;
    gsm_manager.Dial = 0;
    gsm_manager.TimeOutConnection = 0;

    gsm_http_cleanup();
    m_internet_mode = GSM_INTERNET_MODE_AT_STACK;
    init_http_msq();
}

bool gsm_data_layer_is_module_sleeping(void)
{
    //	return GPIO_ReadOutputDataBit(GSM_DTR_PORT, GSM_DTR_PIN);
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
        gsm_manager.GSMReady = 2;
        gsm_manager.ppp_cmd_state = 0;

        TimeoutToSleep = 0;
    }
    else
    {
        if (new_state == GSM_STATE_RESET)
        {
            gsm_manager.FirstTimePower = 1;
        }
    }

    DEBUG_PRINTF("Change GSM state to:\r\n");
    switch ((uint8_t)new_state)
    {
    case 0:
        DEBUG_PRINTF("OK\r\n");
        break;
    case 1:
        DEBUG_PRINTF("RESET\r\n");
        break;
    case 2:
        DEBUG_PRINTF("SENSMS\r\n");
        break;
    case 3:
        DEBUG_PRINTF("READSMS\r\n");
        break;
    case 4:
        DEBUG_PRINTF("POWERON\r\n");
        break;
    case 5:
        DEBUG_PRINTF("REOPENPPP\r\n");
        break;
    case 6:
        DEBUG_PRINTF("GETSIGNAL\r\n");
        break;
    case 7:
        DEBUG_PRINTF("SENDATC\r\n");
        break;
    case 8:
        DEBUG_PRINTF("GOTOSLEEP\r\n");
        break;
    case 9:
        DEBUG_PRINTF("WAKEUP\r\n");
        break;
    case 10:
        DEBUG_PRINTF("IDLE\r\n");
        break;
    case 11:
        DEBUG_PRINTF("SLEEP\r\n");
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
        gsm_hw_send_at_cmd("AT+CNMI=2,1,0,0,0\r\n", "OK\r\n", "", 1000, 10, gsm_at_cb_power_on_gsm);
        break;
    case 7:
        DEBUG_PRINTF("Config SMS event report: %s\r\n", (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]");
        gsm_hw_send_at_cmd("AT+CMGF=1\r\n", "OK\r\n", "", 1000, 10, gsm_at_cb_power_on_gsm);
        break;

    case 8:
        DEBUG_PRINTF("Set SMS text mode: %s\r\n", (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]");
        //gsm_hw_send_at_cmd("AT+CMGD=1,4\r\n", "OK\r\n", "", 2000, 5, gsm_at_cb_power_on_gsm);
        gsm_hw_send_at_cmd("AT\r\n", "OK\r\n", "", 2000, 5, gsm_at_cb_power_on_gsm);
        break;

    case 9:
        DEBUG_PRINTF("Delete all SMS: %s\r\n", (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]");
        gsm_hw_send_at_cmd("AT+CGSN\r\n", "OK\r\n", "", 1000, 5, gsm_at_cb_power_on_gsm);
        break;

    case 10:
        gsm_utilities_get_imei(resp_buffer, (uint8_t *)xSystem.Parameters.gsm_imei);
        DEBUG_PRINTF("Get GSM IMEI: %s\r\n", xSystem.Parameters.gsm_imei);
        if (strlen(xSystem.Parameters.gsm_imei) < 15)
        {
            DEBUG_PRINTF("IMEI's invalid!r\n");
            gsm_change_state(GSM_STATE_RESET); //Khong doc dung IMEI -> reset module GSM!
            return;
        }
        else
        {
            xSystem.Parameters.gsm_imei[15] = 0;
        }
        gsm_hw_send_at_cmd("AT+CIMI\r\n", "OK\r\n", "", 1000, 10, gsm_at_cb_power_on_gsm);
        break;

    case 11:
        gsm_utilities_get_imei(resp_buffer, (uint8_t *)xSystem.Parameters.sim_imei);
        DEBUG_PRINTF("Get SIM IMSI: %s\r\n", xSystem.Parameters.sim_imei);
        if (strlen(xSystem.Parameters.sim_imei) < 15)
        {
            DEBUG_PRINTF("SIM's not inserted!\r\n");
            gsm_change_state(GSM_STATE_RESET); //Neu ko nhan SIM -> reset module GSM!
            return;
        }
        gsm_hw_send_at_cmd("AT+QCCID\r\n", "OK\r\n", "", 1000, 3, gsm_at_cb_power_on_gsm);
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
        gsm_hw_send_at_cmd("AT+CGDCONT=1,\"IP\",\"v-internet\"\r\n", "OK\r\n", "", 1000, 2, gsm_at_cb_power_on_gsm); /** <cid> = 1-24 */
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
            gsm_utilities_get_network_access_tech(resp_buffer, &gsm_manager.access_tech);
        }
        gsm_hw_send_at_cmd("AT+COPS?\r\n", "OK\r\n", "", 2000, 5, gsm_at_cb_power_on_gsm);
        break;

    case 20:
        DEBUG_PRINTF("Query network operator: %s\r\n", (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]"); /** +COPS: 0,0,"Viettel Viettel",7 */
        if (event == GSM_EVENT_OK)
        {
            gsm_utilities_get_network_operator(resp_buffer, 
                                                (char*)xSystem.Status.network_operator, 
                                                sizeof(xSystem.Status.network_operator));
            if (strlen((char*)xSystem.Status.network_operator) < 5)
            {
                gsm_hw_send_at_cmd("AT+COPS?\r\n", "OK\r\n", "", 1000, 5, gsm_at_cb_power_on_gsm);
                gsm_change_hw_polling_interval(1000);
                return;
            }
            DEBUG_PRINTF("Network operator: %s\r\n", (char *)xSystem.Status.network_operator);
        }
        gsm_change_hw_polling_interval(5);
        gsm_hw_send_at_cmd("AT\r\n", "OK\r\n", "", 1000, 5, gsm_at_cb_power_on_gsm);
        break;

    case 21:
    {
        DEBUG_PRINTF("Select QSCLK: %s\r\n", (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]");
        gsm_hw_send_at_cmd("AT+CCLK?\r\n", "OK\r\n", "", 1000, 5, gsm_at_cb_power_on_gsm);
    }
    break;

    case 22:
    {
        DEBUG_PRINTF("Query CCLK: %s,%s\r\n",
                     (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]",
                     (char *)resp_buffer);
        date_time_t time;
        gsm_utilities_parse_timestamp_buffer((char *)resp_buffer, &time);
        uint32_t timestamp = rtc_struct_to_counter(&time);
        __disable_irq();
        xSystem.Status.TimeStamp = timestamp + 946681200 + 25200 + 3600; // 1970 to 2000 + GMT=7
        rtc_counter_set(xSystem.Status.TimeStamp);
        __enable_irq();
        gsm_hw_send_at_cmd("AT+CSQ\r\n", "OK\r\n", "", 1000, 5, gsm_at_cb_power_on_gsm);
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

        xSystem.Status.CSQ = 0;
        gsm_utilities_get_signal_strength_from_buffer(resp_buffer, &xSystem.Status.CSQ);
        DEBUG_PRINTF("CSQ: %d\r\n", xSystem.Status.CSQ);

        if (xSystem.Status.CSQ == 99)
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
            gsm_manager.GSMReady = 1;
            xSystem.Status.CSQPercent = convert_csq_to_percent(xSystem.Status.CSQ);
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

uint8_t convert_csq_to_percent(uint8_t csq)
{
    if (csq == 99)      // 99 is invalid CSQ
    {
        csq = 0;
    }

    if (csq > 31)
    {
        csq = 31;
    }

    if (csq < 10)
    {
        csq = 10;
    }

    return ((csq - 10) * 100) / (31 - 10);
}


/**
* Check RI pin every 10ms
*/
#if __GSM_SMS_ENABLE__
static uint8_t isNewSMSComing = 0;
static uint8_t isRetryReadSMS = 0;
static uint8_t RISignalTimeCount = 0;
static uint8_t RILowLogicTime = 0;
#endif
void gsm_query_sms_tick(void)
{
#if __GSM_SMS_ENABLE__
    if (gsm_manager.RISignal == 1)
    {
        uint8_t RIState = GPIO_ReadInputDataBit(GSM_RI_PORT, GSM_RI_PIN);
        if (RIState == 0)
            RILowLogicTime++;

        RISignalTimeCount++;
        if (RISignalTimeCount >= 100) //50 - 500ms, 100 - 1000ms
        {
            if (RIState == 0)
            {
                DEBUG_PRINTF("New SMS coming!");

#if (__GSM_SLEEP_MODE__)
                /* Neu dang sleep thi wake truoc */
//				if(GPIO_ReadOutputDataBit(GSM_DTR_PORT, GSM_DTR_PIN)) {
//					DEBUG ("GSM is sleeping, wake up...");
//					ChangeGSMState(GSM_WAKEUP);
//				}
#endif //__GSM_SLEEP_MODE__

                gsm_manager.RISignal = 0;
                RISignalTimeCount = 0;
                isNewSMSComing = 1;
            }
            else
            {
                DEBUG_PRINTF("RI by other URC: %ums", RILowLogicTime * 10);
                gsm_manager.RISignal = 0;
                RISignalTimeCount = 0;
                isNewSMSComing = 0;
            }
            RILowLogicTime = 0;
        }
    }
#endif
}

#if __RI_WITHOUT_INTERRUPT__
/*****************************************************************************/
/**
 * @brief	:  	Check RI pin to get new SMS, call every 10ms
 * @param	:  
 * @retval	:
 * @author	:	phinht
 * @created	:	15/01/2014
 * @version	:
 * @reviewer:	
 */
void gsm_check_sms_tick(void)
{
    static uint16_t RILowCount = 0xFFFF;

    if (gsm_manager.GSMReady == 0 || isNewSMSComing)
        return;

    if (GPIO_ReadInputDataBit(GSM_RI_PORT, GSM_RI_PIN) == 0)
    {
        if (RILowCount == 0xFFFF)
            RILowCount = 0;
        else
            RILowCount++;
    }
    else
    {
        if (RILowCount >= 50 && RILowCount < 250) // 500ms - 2500ms
        {
            DEBUG_PRINTF("New SMS coming...");
            isNewSMSComing = 1;
        }
        else
        {
            //DEBUG ("RI caused by other URCs: %ums", RILowCount*10);
        }
        RILowCount = 0xFFFF;
    }
}
#endif //__RI_WITHOUT_INTERRUPT__



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

void gsm_test_read_sms()
{
#if __GSM_SMS_ENABLE__
    if (gsm_manager.state == GSM_SLEEP)
    {
        DEBUG_PRINTF("Wakeup GSM to read SMS...");
        ChangeGSMState(GSM_WAKEUP);
    }
    isNewSMSComing = 1;
#endif
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
        gsm_manager.GSMReady = 0;
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
#ifdef GD32E10X
        nvic_irq_enable(GSM_UART_IRQ, 1, 0);
        driver_uart_initialize(GSM_UART, 115200);
#else
        uart1_control(true);
#endif
        gsm_manager.TimeOutOffAfterReset = 90;
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

void gsm_process_at_cmd(char *at_cmd)
{
    uint8_t i = 0;
    memset(m_at_cmd_buffer, 0, sizeof(m_at_cmd_buffer));
    while (m_at_cmd_buffer[i] && i < sizeof(m_at_cmd_buffer))
    {
        m_at_cmd_buffer[i] = at_cmd[i];
        i++;
    }
    if (m_at_cmd_buffer[i] != '\r')
        m_at_cmd_buffer[i] = '\r';

    m_request_to_send_at_cmd = true;
    m_timeout_switch_at_mode = 60;
}

void gsm_set_timeout_to_sleep(uint32_t sec)
{
    if (sec <= MAX_TIMEOUT_TO_SLEEP_S && TimeoutToSleep < MAX_TIMEOUT_TO_SLEEP_S)
    {
        DEBUG_PRINTF("GSM sleep in next %u second\r\n", sec);
        TimeoutToSleep = MAX_TIMEOUT_TO_SLEEP_S - sec;
    }
}

//static LargeBuffer_t m_http_buffer;
uint16_t gsm_build_http_post_msg(void)
{
    app_queue_data_t new_msq;
    char alarm_str[32];
    char *p = alarm_str;

    // bat,temp,4glost,sensor_err,sensor_overflow
    if (xSystem.MeasureStatus.batteryPercent < 20)
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
    p += sprintf(p, "%u", 0);


    if (app_queue_is_full(&m_http_msq))
    {
        DEBUG_PRINTF("HTTP msq full\r\n");
        return 0;
        //app_queue_data_t tmp;
        //app_queue_get(&m_http_msq, tmp);
        //m_malloc_count--;
        //umm_free(tmp.pointer);
    }

    new_msq.pointer = umm_malloc(256);
    if (new_msq.pointer == NULL)
    {
        DEBUG_PRINTF("[%s-%d] No memory\r\n", __FUNCTION__, __LINE__);
        return 0;
    }
    else
    {
        m_malloc_count++;
        DEBUG_PRINTF("[%s-%d] Malloc success, nb of times malloc %u\r\n", __FUNCTION__, __LINE__, m_malloc_count);
    }

    new_msq.length = sprintf((char *)new_msq.pointer, "{\"Timestamp\":\"%u\",", xSystem.Status.TimeStamp); //second since 1970
    new_msq.length += sprintf((char *)(new_msq.pointer + new_msq.length), "\"ID\":\"%s\",", xSystem.Parameters.gsm_imei);
    new_msq.length += sprintf((char *)(new_msq.pointer + new_msq.length), "\"PhoneNum\":\"%s\",", xSystem.Parameters.phone_number);
    new_msq.length += sprintf((char *)(new_msq.pointer + new_msq.length), "\"Money\":\"%d\",", 0);
    new_msq.length += sprintf((char *)(new_msq.pointer + new_msq.length), "\"Input1\":\"%d\",",
                              xSystem.MeasureStatus.PulseCounterInBkup / xSystem.Parameters.kFactor + xSystem.Parameters.input1Offset); //so xung

    new_msq.length += sprintf((char *)(new_msq.pointer + new_msq.length), "\"Input2\":\"%.1f\",", xSystem.MeasureStatus.Input420mA); //dau vao 4-20mA
    new_msq.length += sprintf((char *)(new_msq.pointer + new_msq.length), "\"Output1\":\"%d\",", xSystem.Parameters.outputOnOff);    //dau ra on/off
    new_msq.length += sprintf((char *)(new_msq.pointer + new_msq.length), "\"Output2\":\"%d\",", xSystem.Parameters.output420ma);    //dau ra 4-20mA
    new_msq.length += sprintf((char *)(new_msq.pointer + new_msq.length), "\"SignalStrength\":\"%d\",", xSystem.Status.CSQPercent);
    new_msq.length += sprintf((char *)(new_msq.pointer + new_msq.length), "\"WarningLevel\":\"%s\",", alarm_str);

    new_msq.length += sprintf((char *)(new_msq.pointer + new_msq.length), "\"BatteryLevel\":\"%d\",", xSystem.MeasureStatus.batteryPercent);
    new_msq.length += sprintf((char *)(new_msq.pointer + new_msq.length), "\"Db\":\"%umV,rst-%u,k-%u,os-%u\"}", 
                                                                            xSystem.MeasureStatus.Vin,
                                                                            hardware_manager_get_reset_reason()->value,
                                                                             xSystem.Parameters.kFactor,
                                                                             xSystem.Parameters.input1Offset);
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

static char m_build_http_post_header[255];
static char *build_http_header(uint32_t length)
{
    sprintf(m_build_http_post_header, "POST /api/v1/%s/telemetry HTTP/1.1\r\n"
                                      "Host: iot.wilad.vn\r\nContent-Type: application/json\r\n"
                                      "Content-Length: %u\r\n\r\n",
            xSystem.Parameters.gsm_imei,
            length);
    return m_build_http_post_header;
}

static void gsm_http_event_cb(gsm_http_event_t event, void *data)
{
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
                ((gsm_http_data_t *)data)->header = (uint8_t *)build_http_header(m_last_http_msg.length);
            }
        }
    }
    break;

    case GSM_HTTP_GET_EVENT_FINISH_SUCCESS:
    {
        DEBUG_PRINTF("HTTP get : event success\r\n");
        xSystem.Status.DisconnectTimeout = 0;
        gsm_change_state(GSM_STATE_OK);
        DEBUG_PRINTF("Free um memory, malloc count[%u]\r\n", m_malloc_count);
        LED1(0);
    }
    break;

    case GSM_HTTP_POST_EVENT_FINISH_SUCCESS:
    {
        DEBUG_PRINTF("HTTP post : event success\r\n");
        xSystem.Status.DisconnectTimeout = 0;
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


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
#include "HardwareManager.h"
#include "gsm.h"
#include "MQTTUser.h"
#include "gsm_http.h"
#include "server_msg.h"
#include "app_queue/app_queue.h"
#include "umm_malloc.h"

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

/******************************************************************************
                                   GLOBAL FUNCTIONS					    			 
 ******************************************************************************/

/******************************************************************************
                                   DATA TYPE DEFINE					    			 
 ******************************************************************************/
#define GET_BTS_INFOR_TIMEOUT 300
/******************************************************************************
                                   PRIVATE VARIABLES					    			 
 ******************************************************************************/
#if __GSM_SMS_ENABLE__
static char tmpBuffer[50] = {0};
#endif

static char m_at_cmd[32] = {0};
static bool m_request_to_send_at_cmd = false;
static uint8_t m_timeout_switch_at_mode = 0;

uint8_t SendATOPeriod = 0;
uint8_t ModuleNotMounted = 0;
uint8_t InSleepModeTick = 0;
uint8_t TimeoutToSleep = 0;
static uint32_t m_get_csq_timeout_s = 0;

static app_queue_t m_http_msq;

void gsm_at_cb_power_on_gsm(gsm_response_event_t event, void *resp_buffer);
void gsm_at_cb_open_ppp_stack(gsm_response_event_t event, void *resp_buffer);
void gsm_query_sms(void);
void gsm_at_cb_read_sms(gsm_response_event_t event, void *resp_buffer);
void gsm_at_cb_send_sms(gsm_response_event_t event, void *resp_buffer);
void gsm_at_cb_get_bts_info(gsm_response_event_t event, void *resp_buffer);
void gsm_data_layer_switch_mode_at_cmd(gsm_response_event_t event, void *resp_buffer);
void gsm_at_cb_goto_sleep(gsm_response_event_t event, void *resp_buffer);
void gsm_at_cb_exit_sleep(gsm_response_event_t event, void *resp_buffer);
void gsm_hard_reset(void);
uint8_t convert_csq_to_percent(uint8_t csq);
uint8_t gsm_check_ready_status(void);
uint8_t gsm_check_idle(void);
static void gsm_http_event_cb(gsm_http_event_t event, void *data);

static bool m_enter_http_get = false;
static bool m_enter_http_post = false;
static gsm_internet_mode_t m_internet_mode;
static uint32_t malloc_count = 0;

static app_queue_data_t m_last_http_msg =
{
    .pointer = NULL,
};

void gsm_data_layter_set_flag_switch_mode_http(void)
{
    m_enter_http_get = true;
}

void gsm_data_layter_exit_mode_http(void)
{
    m_enter_http_get = false;
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

void gsm_change_state_sleep(void)
{
    gsm_change_state(GSM_STATE_GOTO_SLEEP);
}

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

/******************************************************************************/
/**
 * @brief	:  GSM thức dậy gửi tin định kỳ
 * @param	:  
 * @retval	:
 * @author	:	
 * @created	:	15/01/2016
 * @version	:
 * @reviewer:	
 */
void gsm_wakeup_periodically(void)
{
    DEBUG_PRINTF("Sleep time %u, periodic send msg %u, remaining %uS\r\n",
                 xSystem.Status.gsm_sleep_time_s,
                 xSystem.Parameters.period_send_message_to_server_min * 60,
                 xSystem.Parameters.period_send_message_to_server_min * 60 - xSystem.Status.gsm_sleep_time_s);

    if (xSystem.Status.gsm_sleep_time_s >= xSystem.Parameters.period_send_message_to_server_min * 60)
    {
        xSystem.Status.gsm_sleep_time_s = 0;
        if (m_internet_mode == GSM_INTERNET_MODE_PPP_STACK)
        {
            MqttClientSendFirstMessageWhenWakeup();
        }
        else
        {
            gsm_build_http_post_msg();
        }

        DEBUG_PRINTF("GSM: wakeup to send msg\r\n");
        xSystem.Status.YeuCauGuiTin = 2;
        gsm_change_state(GSM_STATE_WAKEUP);
    }

    //	//Lưu lại biến đếm vào RTC Reg sau mỗi 10s
    //	if(xSystem.Status.gsm_sleep_time_s % 10 == 0)
    //	{
    //		RTC_WriteBackupRegister(GSM_SLEEP_TIME_ADDR, xSystem.Status.gsm_sleep_time_s);
    //	}
}

/******************************************************************************/
/**
 * @brief	:  called 1000ms
 * @param	:  
 * @retval	:
 * @author	:	
 * @created	:	15/01/2016
 * @version	:
 * @reviewer:	
 */
uint8_t m_send_at_cmd_in_idle_mode = 0;

void GSM_ManagerTestSleep(void)
{
    gsm_manager.state = GSM_STATE_SLEEP;
}

void gsm_manager_tick(void)
{
    if (xSystem.Status.InitSystemDone != 1)
        return;
    if (gsm_manager.isGSMOff == 1)
        return;

    //	ResetWatchdog();

    //	if(gsm_manager.FirstTimePower == 0)
    //	{
    //		gsm_manager.FirstTimePower = 1;
    //		InitGSM_DataLayer();
    //	}

    /* Cac trang thai lam viec module GSM */
    switch (gsm_manager.state)
    {
    case GSM_STATE_POWER_ON:
        if (gsm_manager.step == 0)
        {
            gsm_manager.step = 1;
            gsm_hw_send_at_cmd("ATV1\r\n", "OK\r\n", "", 1000, 30, gsm_at_cb_power_on_gsm);
        }
        break;

    case GSM_STATE_OK: /* PPP data mode */
    {
        if (m_internet_mode == GSM_INTERNET_MODE_PPP_STACK)
        {
            if (gsm_manager.GSMReady == 2)
            {
                SendATOPeriod++;
                if (SendATOPeriod > 2)
                {
                    DEBUG_PRINTF("Send ATO\r\n");
                    SendATOPeriod = 0;
                    gsm_hw_send_at_cmd("ATO\r\n", "CONNECT", "", 1000, 10, NULL);
                }
            }
            else
            {
                SendATOPeriod = 0;
            }
        }
        //			QuerySMS();

        //			/* Nếu không thực hiện công việc gì khác -> vào sleep sau 60s */
        if (TimeoutToSleep++ >= MAX_TIMEOUT_TO_SLEEP_S)
        {
            if (m_internet_mode == GSM_INTERNET_MODE_PPP_STACK)
            {
                if (xSystem.Status.TCPNeedToClose == 0)
                {
                    DEBUG_PRINTF("TCP need to close\r\n");
                    xSystem.Status.TCPNeedToClose = 1;
                }

                if (xSystem.Status.TCPCloseDone)
                {
                    TimeoutToSleep = 81;
                    xSystem.Status.TCPCloseDone = 0;
                }

                if (TimeoutToSleep > 80)
                {
                    TimeoutToSleep = 0;
                    /* Chỉ sleep khi :
                                                        - Đang không UDFW 
                                                        - Đang không trong thời gian gửi tin
                                                */
                    if (xSystem.FileTransfer.Retry == 0 && xSystem.Status.SendGPRSTimeout == 0)
                    {
                        DEBUG_PRINTF("GSM: Het viec roi, ngu thoi em...\r\n");
                        gsm_manager.step = 0;
                        gsm_manager.state = GSM_STATE_GOTO_SLEEP;
                    }
                }
            }
            else
            {
                DEBUG_PRINTF("GSM in at mode : need to sleep\r\n");
            }
        }

        gsm_wakeup_periodically();

        bool enter_sleep_in_http = true;
        if (!app_queue_is_empty(&m_http_msq))
        {
            DEBUG_PRINTF("Post http data\r\n");
            m_enter_http_post = true;
            gsm_change_state(GSM_STATE_HTTP_POST);
            enter_sleep_in_http = false;
        }
        else
        {
            if (gsm_manager.state == GSM_STATE_OK)
            {
                if (GSM_NEED_ENTER_HTTP_GET())
                {
                    gsm_change_state(GSM_STATE_HTTP_GET);
                    enter_sleep_in_http = false;
                }
            }
        }

        if (enter_sleep_in_http && m_internet_mode == GSM_INTERNET_MODE_AT_STACK)
        {
            gsm_change_state(GSM_STATE_SLEEP);
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
            gsm_manager.step = 1;
            if (gsm_manager.ppp_cmd_state == 0)
            {
                gsm_hw_send_at_cmd("+++", "OK\r\n", "", 2200, 5, gsm_at_cb_read_sms);
            }
            else
            {
                gsm_hw_send_at_cmd("ATV1\r\n", "OK\r\n", "", 1000, 1, gsm_at_cb_read_sms);
            }
        }
        break;

    case GSM_STATE_SEND_SMS: /* Send SMS */
        if (!gsm_manager.GSMReady)
            break;
        if (gsm_manager.step == 0)
        {
            gsm_manager.step = 1;
            gsm_hw_send_at_cmd("ATV1\r\n", "OK\r\n", "", 100, 1, gsm_at_cb_send_sms);
        }
        break;

    case GSM_STATE_REOPEN_PPP: /* Reopen PPP if lost */
        if (gsm_manager.step == 0)
        {
            gsm_manager.step = 1;
            gsm_hw_send_at_cmd("ATV1\r\n", "OK\r\n", "", 1000, 1, gsm_at_cb_open_ppp_stack);
        }
        break;

    case GSM_STATE_GET_BTS_INFO: /* Get GSM Signel level */
        if (gsm_manager.step == 0)
        {
            gsm_manager.step = 1;
            gsm_hw_send_at_cmd("ATV1\r\n", "OK\r\n", "", 100, 1, gsm_at_cb_get_bts_info);
        }
        break;

    case GSM_STATE_SEND_ATC: /* Send AT command */
        if (gsm_manager.step == 0)
        {
            gsm_manager.step = 1;
            gsm_hw_send_at_cmd("ATV1\r\n", "OK\r\n", "", 100, 1, gsm_data_layer_switch_mode_at_cmd);
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
            sprintf(cfg.url, "https://iot.wilad.vn/api/v1/%s/attributes",
                    xSystem.Parameters.gsm_imei);
            cfg.on_event_cb = gsm_http_event_cb;
            cfg.action = GSM_HTTP_ACTION_GET;
            cfg.port = 443;
            gsm_http_start(&cfg);
            GSM_DONT_NEED_HTTP_GET();
        }
    }
    break;

    case GSM_STATE_GOTO_SLEEP: /* Vao che do sleep */
        if (gsm_manager.step == 0)
        {
            gsm_manager.step = 1;
            gsm_hw_send_at_cmd("ATV1\r\n", "OK\r\n", "", 100, 1, gsm_at_cb_goto_sleep);
        }
        break;

    case GSM_STATE_WAKEUP: /* Thoat che do sleep */
    {
        if (m_internet_mode == GSM_INTERNET_MODE_PPP_STACK)
        {
            init_TcpNet();      // When gsm wakeup from sleep,  we must reinit tcpnet,
                                // If we dont reinit tcpNet =>> ppp stack never up
                                // I dont know why
        }
        gsm_data_layer_initialize();
        gsm_change_state(GSM_STATE_RESET);
    }
        break;

    case GSM_STATE_SLEEP: /* Dang trong che do Sleep */
        if (InSleepModeTick % 10 == 0)
        {
            DEBUG_PRINTF("GSM is sleeping...PPP is %s\r\n", ppp_is_up() ? "up" : "down");
        }
        InSleepModeTick++;
        TimeoutToSleep = 0;
        UART_DeInit(GSM_UART);
        GSM_PWR_EN(0);
        GSM_PWR_RESET(0);
        GSM_PWR_KEY(0);
        /* Thuc day gui tin dinh ky */
        gsm_wakeup_periodically();
        break;

    case GSM_STATE_AT_MODE_IDLE: /* AT mode - Hop quy GSM */
#if 0
			m_send_at_cmd_in_idle_mode++;
			if(m_send_at_cmd_in_idle_mode == 10)
			{
				//Lấy thông tin mạng
				DEBUG ("Get network info ...");
				com_put_at_string("AT+COPS?\r\n");
				m_send_at_cmd_in_idle_mode = 0;
			}
			else if(m_send_at_cmd_in_idle_mode == 15) {
				//Lấy thông tin CSQ
				DEBUG ("Get network CSQ...");
				com_put_at_string("AT+CSQ\r");
				m_send_at_cmd_in_idle_mode = 1;
			}
			if(m_send_at_cmd_in_idle_mode > 10) m_send_at_cmd_in_idle_mode = 0;
#endif
        break;
    }

    //Khong co SIM
    if (strlen(xSystem.Parameters.sim_imei) < 15)
        return;

    //	/* Giám sát thời gian gửi bản tin, 3 lần thức dậy ko gửi được phát nào -> reset GSM, reset mạch */
    //	xSystem.Status.GSMSendFailedTimeout++;
    //	if(xSystem.Status.GSMSendFailedTimeout > 3*xSystem.Parameters.period_send_message_to_server_min*60)
    //	{
    //		xSystem.Status.GSMSendFailedTimeout = 0;
    //
    //		DEBUG ("GSM: Send Failed timeout...");
    //		gsm_change_state(GSM_RESET);
    //		return;
    //	}
    //
    /* Lay cuong do song sau moi 5 phut  */
    if (gsm_manager.GSMReady == 1)
    {
        m_get_csq_timeout_s++;
        if (m_get_csq_timeout_s >= GET_BTS_INFOR_TIMEOUT) //300
        {
            if (gsm_check_idle() && gsm_manager.Dial == 0)
            {
                m_get_csq_timeout_s = 0;
                gsm_manager.step = 0;
                gsm_manager.GetBTSInfor = 0;
                gsm_manager.state = GSM_STATE_GET_BTS_INFO;
            }
            else
            {
                m_get_csq_timeout_s = GET_BTS_INFOR_TIMEOUT - 30; //check lai sau 30s
            }
        }
    }

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

/*
* Kiem tra trang thai chan STATUS module GSM
*/
#if 0
uint8_t CheckReadyStatus(void)
{
	return (GPIO_ReadInputDataBit(GSM_STATUS_PORT, GSM_STATUS_PIN));
}
#endif

uint8_t gsm_check_idle(void)
{
    if (!gsm_manager.GSMReady)
        return 0;
    if (gsm_manager.RISignal)
        return 0;
    if (gsm_manager.state != GSM_STATE_OK)
        return 0;
    if (xSystem.FileTransfer.State != FT_NO_TRANSER)
        return 0;

    //Dang gui TCP -> busy
    if (xSystem.Status.GuiGoiTinTCP)
        return 0;
    if (xSystem.Status.SendGPRSTimeout)
        return 0;

    return 1;
}

static void init_http_msq(void)
{

    if (m_internet_mode == GSM_INTERNET_MODE_AT_STACK)
    {
        DEBUG_PRINTF("HTTP: init buffer\r\n");

        static bool m_is_the_first_time = true;
        if (m_is_the_first_time)
        {
            m_is_the_first_time = false;
            umm_init();
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
    init_http_msq();

    m_internet_mode = GSM_INTERNET_MODE_AT_STACK;
}

/*****************************************************************************/
/**
 * @brief	:  
 * @param	:  
 * @retval	:
 * @author	:	
 * @created	:	15/01/2016
 * @version	:
 * @reviewer:	
 */
void gsm_change_state(gsm_state_t new_state)
{
    if (new_state == GSM_STATE_OK) //Command state -> Data state trong PPP mode
    {
        gsm_hw_send_at_cmd("ATO\r\n", "CONNECT", "", 1000, 10, NULL);
        gsm_manager.GSMReady = 2;
        gsm_manager.Mode = GSM_PPP_MODE;
        gsm_manager.ppp_cmd_state = 0;

        TimeoutToSleep = 0;
    }
    else
    {
        gsm_manager.Mode = GSM_AT_MODE;

        if (new_state == GSM_STATE_RESET)
        {
            ModuleNotMounted = 0;
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
        gsm_hw_send_at_cmd("AT+CMGD=1,4\r\n", "OK\r\n", "", 2000, 5, gsm_at_cb_power_on_gsm);
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
        DEBUG_PRINTF("Get SIM IMEI: %s\r\n", (char *)resp_buffer);
        gsm_hw_send_at_cmd("AT+QIDEACT=1\r\n", "OK\r\n", "", 3000, 1, gsm_at_cb_power_on_gsm);
        break;
    case 13:
        DEBUG_PRINTF("De-activate PDP: %s\r\n", (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]");
        gsm_hw_send_at_cmd("AT+QCFG=\"nwscanmode\",0\r\n", "OK\r\n", "", 5000, 2, gsm_at_cb_power_on_gsm); // Select mode AUTO
        break;
    case 14:
        DEBUG_PRINTF("Network search mode AUTO: %s\r\n", (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]");
        gsm_hw_send_at_cmd("AT+CGDCONT=1,\"IP\",\"v-internet\"\r\n", "OK\r\n", "", 1000, 2, gsm_at_cb_power_on_gsm); /** <cid> = 1-24 */
        break;
    case 15:
        DEBUG_PRINTF("Define PDP context: %s\r\n", (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]");
        //			SendATCommand ("AT+QIACT=1\r\n", "OK\r\n", "", 5000, 5, PowerOnModuleGSM);	/** Bật QIACT lỗi gửi tin với 1 số SIM dùng gói cước trả sau! */
        gsm_hw_send_at_cmd("AT+CSQ\r\n", "OK\r\n", "", 1000, 5, gsm_at_cb_power_on_gsm);
        break;
    case 16:
        DEBUG_PRINTF("Activate PDP: %s\r\n", (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]");
        gsm_hw_send_at_cmd("AT+CGREG=2\r\n", "OK\r\n", "", 1000, 3, gsm_at_cb_power_on_gsm); /** Query CGREG? => +CGREG: <n>,<stat>[,<lac>,<ci>[,<Act>]] */
        break;
    case 17:
        DEBUG_PRINTF("Network Registration Status: %s\r\n", (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]");
        gsm_hw_send_at_cmd("AT+CGREG?\r\n", "OK\r\n", "", 1000, 5, gsm_at_cb_power_on_gsm);
        break;
    case 18:
        DEBUG_PRINTF("Query Network Status: %s\r\n", (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]"); /** +CGREG: 2,1,"3279","487BD01",7 */
        if (event == GSM_EVENT_OK)
        {
            gsm_get_network_status(resp_buffer);
        }
        gsm_hw_send_at_cmd("AT+COPS?\r\n", "OK\r\n", "", 1000, 5, gsm_at_cb_power_on_gsm);
        break;
    case 19:
        DEBUG_PRINTF("Query Network Operator: %s\r\n", (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]"); /** +COPS: 0,0,"Viettel Viettel",7 */
        if (event == GSM_EVENT_OK)
        {
            DEBUG_PRINTF("Network operator: %s\r\n", (char *)resp_buffer);
            gsm_get_network_operator(resp_buffer);
        }
#if 0
#if (__GSM_SLEEP_MODE__)
//			SendATCommand ("AT+QSCLK=1\r\n", "OK\r\n", "", 1000, 5, PowerOnModuleGSM);
			SendATCommand ("AT+QSCLK=0\r\n", "OK\r\n", "", 1000, 5, PowerOnModuleGSM);
#else
			SendATCommand ("AT+QSCLK=0\r\n", "OK\r\n", "", 1000, 5, PowerOnModuleGSM);
#endif
#else
        gsm_hw_send_at_cmd("AT\r\n", "OK\r\n", "", 1000, 5, gsm_at_cb_power_on_gsm);
#endif
        break;

    case 20:
    {
        DEBUG_PRINTF("Select QSCLK: %s\r\n", (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]");
        gsm_hw_send_at_cmd("AT+CCLK?\r\n", "OK\r\n", "", 1000, 5, gsm_at_cb_power_on_gsm);
    }
    break;

    case 21:
    {
        DEBUG_PRINTF("Query CCLK: %s,%s\r\n",
                     (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]",
                     (char *)resp_buffer);
        date_time_t time;
        gsm_utilities_parse_timestamp_buffer((char *)resp_buffer, &time);
        uint32_t timestamp = rtc_struct_to_counter(&time);
        __disable_irq();
        xSystem.Status.TimeStamp = timestamp + 946681200 + 25200; // 1970 to 2000 + GMT=7
        rtc_counter_set(xSystem.Status.TimeStamp);
        __enable_irq();
        gsm_hw_send_at_cmd("AT+CSQ\r\n", "OK\r\n", "", 1000, 5, gsm_at_cb_power_on_gsm);
    }
    break;

    case 22:
        gsm_manager.step = 0;
        if (event != GSM_EVENT_OK)
        {
            DEBUG_PRINTF("GSM: init fail, reset modem...\r\n");
            gsm_change_state(GSM_STATE_RESET);
            return;
        }

        xSystem.Status.CSQ = 0;
        gsm_utilities_get_signal_strength_from_buffer(resp_buffer, &xSystem.Status.CSQ);
        DEBUG_PRINTF("CSQ: %d\r\n", xSystem.Status.CSQ);

        if (xSystem.Status.CSQ == 99)
        {
            DEBUG_PRINTF("Invalid csq\r\n");
            gsm_manager.step = 20;
            gsm_hw_send_at_cmd("AT+CSQ\r\n", "OK\r\n", "", 1000, 5, gsm_at_cb_power_on_gsm);
        }
        else
        {
            gsm_manager.GSMReady = 1;
            xSystem.Status.CSQPercent = convert_csq_to_percent(xSystem.Status.CSQ);

            if (m_internet_mode == GSM_INTERNET_MODE_PPP_STACK)
            {
                gsm_manager.step = 0;
                gsm_hw_send_at_cmd("ATV1\r\n", "OK\r\n", "", 1000, 3, gsm_at_cb_open_ppp_stack);
            }
            else
            {
                gsm_manager.step = 0;
                gsm_build_http_post_msg();
                gsm_change_state(GSM_STATE_OK);
            }
        }
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

void gsm_at_cb_open_ppp_stack(gsm_response_event_t event, void *resp_buffer)
{
    static uint8_t retry_count = 0;

    switch (gsm_manager.step)
    {
    case 1:
        gsm_hw_send_at_cmd("AT+CSQ\r\n", "OK\r\n", "", 1000, 5, gsm_at_cb_open_ppp_stack);
        gsm_manager.step = 2;
        break;
    case 2:
        if (event == GSM_EVENT_OK)
        {
            xSystem.Status.CSQ = 0;
            gsm_utilities_get_signal_strength_from_buffer(resp_buffer, &xSystem.Status.CSQ);
            gsm_manager.GSMReady = 1;
            xSystem.Status.CSQPercent = convert_csq_to_percent(xSystem.Status.CSQ);
            DEBUG_PRINTF("CSQ: %d\r\n", xSystem.Status.CSQ);
        }
        ppp_connect("*99#", "", "");
        gsm_hw_send_at_cmd("ATD*99***1#\r\n", "CONNECT", "", 1000, 10, gsm_at_cb_open_ppp_stack);
        gsm_manager.step = 3;
        break;
    case 3:
        DEBUG_PRINTF("Open PPP stack : %s\r\n", (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]");

        if (event != GSM_EVENT_OK)
        {
            retry_count++;
            if (retry_count > 5)
            {
                retry_count = 0;

                DEBUG_PRINTF("PPP open so many times error. Reset GSM!\r\n");
                if (!gsm_data_layer_is_module_sleeping())
                {
                    /* Reset GSM */
                    gsm_change_state(GSM_STATE_RESET);
                }
                else
                {
                    gsm_manager.step = 1;
                    ppp_close();
                    gsm_hw_send_at_cmd("ATV1\r\n", "OK\r\n", "", 1000, 5, gsm_at_cb_open_ppp_stack);
                }
            }
            else
            {
                gsm_manager.step = 1;
                ppp_close();
                gsm_hw_send_at_cmd("ATV1\r\n", "OK\r\n", "", 1000, 5, gsm_at_cb_open_ppp_stack);
            }
        }
        else //Truong hop AT->PPP, khong ChangeState de gui lenh ATO
        {
            retry_count = 0;
            gsm_manager.step = 0;
            gsm_manager.state = GSM_STATE_OK;
            gsm_manager.Mode = GSM_PPP_MODE; //Response "CONNECT" chua chac ppp_is_up() = 1!
            gsm_manager.GSMReady = 1;
            gsm_manager.ppp_cmd_state = 0;
        }
        break;
    default:
        break;
    }
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

/*****************************************************************************/
/**
 * @brief	:  Ham lay GSM signal level
 * @author	:	
 * @created	:	10/03/2016
 * @version	:
 * @reviewer:	
 */
void gsm_at_cb_get_bts_info(gsm_response_event_t event, void *resp_buffer)
{
    switch (gsm_manager.step)
    {
    case 1:
        gsm_hw_send_at_cmd("+++", "OK\r\n", "", 2000, 5, gsm_at_cb_get_bts_info);
        break;
    case 2:
        if (event == GSM_EVENT_OK)
        {
            gsm_manager.ppp_cmd_state = 1;
            gsm_hw_send_at_cmd("AT+CSQ\r\n", "OK\r\n", "", 1000, 3, gsm_at_cb_get_bts_info);
        }
        else
        {
            gsm_manager.step = 0;
            gsm_manager.state = GSM_STATE_OK;
            gsm_manager.ppp_cmd_state = 0;
            return;
        }
        break;
    case 3:
        if (event == GSM_EVENT_OK)
        {
            xSystem.Status.CSQ = 0;
            gsm_utilities_get_signal_strength_from_buffer(resp_buffer, &xSystem.Status.CSQ);
            gsm_manager.GSMReady = 1;
            gsm_manager.TimeOutCSQ = 0;
            xSystem.Status.CSQPercent = convert_csq_to_percent(xSystem.Status.CSQ);
            DEBUG_PRINTF("CSQ: %d\r\n", xSystem.Status.CSQ);

            /* Lay thong tin Network access selected */
            gsm_hw_send_at_cmd("AT+CGREG?\r\n", "OK\r\n", "", 1000, 3, gsm_at_cb_get_bts_info);
        }
        break;
    case 4:
        if (event == GSM_EVENT_OK)
        {
            DEBUG_PRINTF("%s", resp_buffer);
            gsm_get_network_status(resp_buffer);
        }
        gsm_change_state(GSM_STATE_OK);

        /* Gửi tin khi thức dậy định kỳ */
        if (xSystem.Status.YeuCauGuiTin == 2)
            xSystem.Status.YeuCauGuiTin = 3;
        break;

    default:
        break;
    }
    gsm_manager.step++;
}

/*****************************************************************************/
/**
 * @brief	:  Ham thuc hien lenh AT
 * @param	:  
 * @retval	:
 * @author	:	
 * @created	:	29/03/2016
 * @version	:
 * @reviewer:	
 */
void gsm_data_layer_switch_mode_at_cmd(gsm_response_event_t event, void *resp_buffer)
{
    switch (gsm_manager.step)
    {
    case 1:
        gsm_hw_send_at_cmd("+++", "OK\r\n", "", 2000, 5, gsm_data_layer_switch_mode_at_cmd);
        break;

    case 2:
        if (event == GSM_EVENT_OK)
        {
            gsm_manager.ppp_cmd_state = 1;
            if (strstr(m_at_cmd, "+CUSD"))
            {
                gsm_hw_send_at_cmd(m_at_cmd, "+CUSD", "\r\n", 5000, 5, gsm_data_layer_switch_mode_at_cmd);
            }
            else
            {
                gsm_hw_send_at_cmd(m_at_cmd, "OK\r\n", "", 2000, 5, gsm_data_layer_switch_mode_at_cmd);
            }
        }
        else
        {
            m_request_to_send_at_cmd = false;
            gsm_manager.step = 0;
            gsm_manager.state = GSM_STATE_OK;
            gsm_manager.ppp_cmd_state = 0;
            return;
        }
        break;

    case 3:
        if (event == GSM_EVENT_OK)
        {
            memset(m_at_cmd, 0, 20);
            DEBUG_PRINTF("Phan hoi: %s\r\n", resp_buffer);
        }
        m_request_to_send_at_cmd = false;
        m_timeout_switch_at_mode = 0;
        gsm_change_state(GSM_STATE_OK);
        break;

    default:
        break;
    }
    gsm_manager.step++;
}

/*****************************************************************************/
/**
 * @brief	:  Vao che do sleep
 * @param	:  
 * @retval	:
 * @author	:	
 * @created	:	29/03/2016
 * @version	:
 * @reviewer:	
 */
void gsm_at_cb_goto_sleep(gsm_response_event_t event, void *resp_buffer)
{
    switch (gsm_manager.step)
    {
    case 1:
        ppp_close();
        gsm_hw_send_at_cmd("+++", "OK\r\n", "", 2000, 5, gsm_at_cb_goto_sleep);
        break;
    case 2:
        if (event == GSM_EVENT_OK)
        {
            //				SendATCommand ("AT+QSCLK=1\r\n", "OK\r\n", "", 1000, 5, GSM_GotoSleepMode);
            gsm_hw_send_at_cmd("AT\r\n", "OK\r\n", "", 1000, 5, gsm_at_cb_goto_sleep);
        }
        else
        {
            DEBUG_PRINTF("Khong phan hoi lenh, bat buoc sleep!\r\n");
            //Dieu khien chan DTR vao sleep
            gsm_change_state_sleep();
            gsm_manager.state = GSM_STATE_SLEEP;
            return;
        }
        break;
    case 3:
        if (event == GSM_EVENT_OK)
        {
            DEBUG_PRINTF("Entry Sleep OK!\r\n");
        }
        //Dieu khien chan DTR vao sleep
        gsm_change_state_sleep();
        gsm_manager.state = GSM_STATE_SLEEP;

        // Tat nguon
        UART_DeInit(GSM_UART);
        usart_interrupt_disable(GSM_UART, (usart_interrupt_enum)(USART_INT_RBNE | USART_INT_FLAG_TBE));
        usart_interrupt_flag_clear(GSM_UART, USART_INT_FLAG_RBNE);
        usart_interrupt_flag_clear(GSM_UART, USART_INT_FLAG_TBE);
        GSM_PWR_EN(0);
        GSM_PWR_RESET(1);
        GSM_PWR_KEY(0);
        break;

    default:
        break;
    }
    gsm_manager.step++;
}

/*****************************************************************************/
/**
 * @brief	:  Thoat che do sleep
 * @param	:  
 * @retval	:
 * @author	:	
 * @created	:	29/03/2016
 * @version	:
 * @reviewer:	
 */
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
            DEBUG_PRINTF("Exit Sleep!");
            m_get_csq_timeout_s = GET_BTS_INFOR_TIMEOUT - 3;
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
        nvic_irq_disable(GSM_UART_IRQ);
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
        gsm_manager.Mode = GSM_AT_MODE;
        nvic_irq_enable(GSM_UART_IRQ, 1, 0);
        UART_Init(GSM_UART, 115200);
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

/*****************************************************************************/
/**
 * @brief	:  Kiem tra tin nhan den khi dang o mode PPP
 * @param	:  
 * @retval	:
 * @author	:	
 * @created	:	10/03/2016
 * @version	:
 * @reviewer:	
 */
uint8_t m_last_ri_state = 0xFF, m_call_counter = 0xFF;
void gsm_query_sms(void)
{
    //	uint8_t iTemp;
    //
    //	/* New SMS */
    //	if(isNewSMSComing || isRetryReadSMS) {
    //		ChangeGSMState(GSM_READSMS);
    //		return;
    //	}
    //
    //    /* Kiem tra cac buffer SMS de gui SMS */
    //    for(iTemp = 0; iTemp < 3; iTemp++)
    //    {
    //        if(SMSMemory[iTemp].NeedToSent == 1 || SMSMemory[iTemp].NeedToSent == 2)
    //        {
    //            SMSMemory[iTemp].retry_count++;
    //            DEBUG ("Gui SMS tai buffer %u", iTemp);

    //            /* Neu gui thanh cong thi xoa di */
    //            if(SMSMemory[iTemp].retry_count < 5)
    //            {
    //					SMSMemory[iTemp].NeedToSent = 2;
    //					ChangeGSMState(GSM_SENSMS);
    //            }
    //            else
    //            {
    //                DEBUG ("Buffer SMS %u da gui qua %u lan, huy gui",iTemp,SMSMemory[iTemp].retry_count);
    //                SMSMemory[iTemp].NeedToSent = 0;

    //					//Add: 05/05/17
    //					gsm_manager.SendSMSAfterRead = 0;
    //					ChangeGSMState(GSM_OK);
    //            }
    //            break;
    //        }
    //    }
}

/*****************************************************************************/
/**
 * @brief	:  
 * @param	:  
 * @retval	:
 * @author	:	
 * @created	:	10/11/2014
 * @version	:
 * @reviewer:	
 */
void gsm_at_cb_read_sms(gsm_response_event_t event, void *resp_buffer)
{
#if 0
	static uint8_t SMSIndex = 0xFF;
	static uint8_t RetryRead = 0;
	
	if(SMSIndex == 0xFF)
	{
		SMSIndex = 0;	/* Start at index 0 */
		if(RetryRead == 0) RetryRead = 6;
		
//		//Test
//		if(RetryRead == 0) {
//				RetryRead = 3;
//				isRetryReadSMS = 1;
//				SMSIndex = 0xFF;
//				DEBUG ("Read SMS failed, reset modem...");
//				ChangeGSMState(GSM_RESET);
//				return;
//		}
	}
	else if(SMSIndex == 0xAA)   //Da xu ly xong SMS moi
	{
		SMSIndex = 0xFF;
		
		/* Kiem tra xem co SMS nao can gui khong */
		gsm_manager.SendSMSAfterRead = 1;
		QuerySMS(); 
		
		/* Neu ko co SMS nao can gui thi chuyen trang thai ve GSM_OK */
		if(gsm_manager.state != GSM_SENSMS)
		{
			gsm_manager.SendSMSAfterRead = 0;
			ChangeGSMState(GSM_OK);
		}
		return;
	}
	else
	{
		if(SMSIndex < 10) SMSIndex++;
		if(SMSIndex == 10) SMSIndex = 0x55;
	}
			
	DEBUG ("Read SMS resp: %s", (char*)resp_buffer);
	if(strstr(resp_buffer,"UNREAD") || strstr(resp_buffer,"READ")) //REC UNREAD | REC READ
	{
		SMSIndex = 0xAA;
		gsm_process_cmd_from_sms(resp_buffer);
		SendATCommand ("AT+CMGD=1,4\r\n", "OK\r\n", "", 1000, 10, GSM_ReadSMS);
		isNewSMSComing = 0;
		isRetryReadSMS = 0;
		RetryRead = 0;
	}
	else
	{
		if(SMSIndex < 0x55) 
		{
			sprintf(tmpBuffer,"AT+CMGR=%u\r\n",SMSIndex);
			SendATCommand(tmpBuffer,"OK",1000,3,GSM_ReadSMS);
			DEBUG ("Check SMS o buffer %u: %u,%s",SMSIndex, event, (char *)resp_buffer);
		}
		else
		{
			if(RetryRead) RetryRead--;
			
			DEBUG ("Cannot read SMS, retry: %u", RetryRead);
			SMSIndex = 0xFF;
				
//			if(RetryRead) {
//				isNewSMSComing = 1;
//			} else {
//				isNewSMSComing = 0;
//			}
//			ChangeGSMState(GSM_OK);
			
			if(RetryRead == 3) {
				//Doc 3 lan khong duoc -> reset module
				isRetryReadSMS = 1;
				DEBUG ("Read SMS failed, reset modem...");
				ChangeGSMState(GSM_RESET);
				return;
			}
			else 
			{
				if(RetryRead == 0) {
					isNewSMSComing = 0;
					isRetryReadSMS = 0;
				}
				ChangeGSMState(GSM_OK);
			}
		}
	}
#endif //0
}

/*****************************************************************************/
/**
 * @brief	:  
 * @param	:  
 * @retval	:
 * @author	:	
 * @created	:	10/11/2014
 * @version	:
 * @reviewer:	
 */

void gsm_at_cb_send_sms(gsm_response_event_t event, void *resp_buffer)
{
    //	uint8_t ucCount;
    //    static uint8_t retry_count = 0;
    //
    //    DEBUG ("Debug SEND SMS: %u %u,%s",gsm_manager.step,event, (char *)resp_buffer);
    //
    //	switch(gsm_manager.step)
    //	{
    //		case 1:
    //			if(gsm_manager.SendSMSAfterRead || gsm_manager.ppp_cmd_state)
    //			{
    ////				gsm_manager.SendSMSAfterRead = 0;	//Neu gui loi lan 1 -> lan 2,3 se gui "+++"  -> khong thanh cong
    //				SendATCommand ("ATV1\r\n", "OK\r\n", "", 1000, 3, GSM_SendSMS);
    //			}
    //			else
    //				SendATCommand ("+++", "OK\r\n", "", 2200, 5, GSM_SendSMS);
    //			break;
    //
    //		case 2:
    //			if(gsm_manager.ppp_cmd_state == 0 && event != EVEN_OK)	//Data -> Command state Fail
    //			{
    //				gsm_manager.step = 0;
    //				gsm_manager.SendSMSAfterRead = 0;
    //				gsm_manager.state = GSM_OK;
    //				return;
    //			}
    //			//Add: 22/12
    //			if(event == EVEN_OK && gsm_manager.ppp_cmd_state == 0)
    //				gsm_manager.ppp_cmd_state = 2;
    //
    //			for(ucCount = 0; ucCount < 3; ucCount++)
    //			{
    //				if(SMSMemory[ucCount].NeedToSent == 2)
    //				{
    //					DEBUG ("Nhan tin den so: %s. Noi dung: %s",
    //						SMSMemory[ucCount].phone_number,SMSMemory[ucCount].Message);
    //
    //					sprintf(tmpBuffer,"AT+CMGS=\"%s\"\r\n",SMSMemory[ucCount].phone_number);
    //					SendATCommand (tmpBuffer, ">", 3000, 5, GSM_SendSMS);
    //					break;
    //				}
    //			}
    //			break;
    //		case 3:
    //			if(event == EVEN_OK)
    //			{
    //				for(ucCount = 0; ucCount < 3; ucCount++)
    //				{
    //					if(SMSMemory[ucCount].NeedToSent == 2)
    //					{
    //						SMSMemory[ucCount].Message[strlen(SMSMemory[ucCount].Message)] = 26;
    //						SMSMemory[ucCount].Message[strlen(SMSMemory[ucCount].Message)] = 13;
    //
    //						SendATCommand (SMSMemory[ucCount].Message, "+CMGS", 15000, 1, GSM_SendSMS);
    //						DEBUG ("Bat dau gui SMS o buffer %u",ucCount);
    //						break;
    //					}
    //				}
    //			}
    //			else
    //			{
    //				retry_count++;
    //				if(retry_count < 3)
    //				{
    //					ChangeGSMState(GSM_SENSMS);
    //					return;
    //				}
    //				else
    //					goto SENDSMSFAIL;
    //			}
    //			break;
    //		case 4:
    //			if(event == EVEN_OK)
    //			{
    //				DEBUG ("SMS: Gui SMS thanh cong.");

    //				for(ucCount = 0; ucCount < 3; ucCount++)
    //				{
    //					if(SMSMemory[ucCount].NeedToSent == 2)
    //					{
    //						SMSMemory[ucCount].NeedToSent = 0;
    //
    //						//Kiem tra xem con SMS can gui trong buffer khong, neu con -> quay lai gui tiep: add 22/12
    //						for(ucCount = ucCount; ucCount < 3; ucCount++)
    //						{
    //							if(SMSMemory[ucCount].NeedToSent == 2)
    //							{
    //								retry_count = 0;
    //								gsm_manager.step = 0;
    //								gsm_manager.state = GSM_OK;
    //								return;
    //							}
    //						}
    //					}
    //				}
    //				retry_count = 0;
    //				gsm_manager.SendSMSAfterRead = 0;
    //				ChangeGSMState(GSM_OK);
    //			}
    //			else
    //			{
    //				DEBUG ("SMS: Nhan tin khong thanh cong.");
    //				retry_count++;
    //				if(retry_count < 3)
    //				{
    //					ChangeGSMState(GSM_SENSMS);
    //					return;
    //				}
    //				else
    //					goto SENDSMSFAIL;
    //			}
    //		return;
    //	}
    //	gsm_manager.step++;
    //	return;
    //
    //SENDSMSFAIL:
    //	for(ucCount = 0; ucCount < 3; ucCount++)
    //	{
    //		if(SMSMemory[ucCount].NeedToSent == 2)
    //		{
    //				SMSMemory[ucCount].NeedToSent = 1;
    ////			ChangeGSMState(GSM_OK);
    //		}
    //	}
    //	retry_count = 0;
    //	gsm_manager.SendSMSAfterRead = 0;
    //	ChangeGSMState(GSM_OK);
}

void gsm_process_at_cmd(char *at_cmd)
{
    uint8_t i = 0;
    memset(m_at_cmd, 0, sizeof(m_at_cmd));
    while (at_cmd[i] && i < sizeof(m_at_cmd))
    {
        m_at_cmd[i] = at_cmd[i];
        i++;
    }
    if (m_at_cmd[i] != '\r')
        m_at_cmd[i] = '\r';

    m_request_to_send_at_cmd = true;
    m_timeout_switch_at_mode = 60;
}

/*****************************************************************************/
/**
 * @brief	:  
 * @param	:  
 * @retval	:
 * @author	:	
 * @created	:	15/01/2016
 * @version	:
 * @reviewer:	
 */
void gsm_reconnect_tcp(void)
{
#if ((__USE_MQTT__))
    uint8_t i;

    DEBUG_PRINTF("Ket noi lai Server...\r\n");

    /* Clear het cac buffer */
    for (i = 0; i < NUM_OF_MQTT_BUFFER; i++)
    {
        xSystem.MQTTData.Buffer[i].State = BUFFER_STATE_IDLE;
        xSystem.MQTTData.Buffer[i].BufferIndex = 0;
    }

    xSystem.Status.TCPNeedToClose = 2;
#endif
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
    if (app_queue_is_full(&m_http_msq))
    {
        DEBUG_PRINTF("HTTP msq full\r\n");
        app_queue_data_t *tmp;
        app_queue_get(&m_http_msq, tmp);
        umm_free(tmp->pointer);
    }

    new_msq.pointer = umm_malloc(256);
    if (new_msq.pointer == NULL)
    {
        DEBUG_PRINTF("[%s-%d] No memory\r\n", __FUNCTION__, __LINE__);
        return 0;
    }
    else
    {
        malloc_count++;
        DEBUG_PRINTF("[%s-%d] Malloc success\r\n", __FUNCTION__, __LINE__);
    }

    new_msq.length = sprintf((char *)new_msq.pointer, "{\"Timestamp\":\"%d\",", xSystem.Status.TimeStamp); //second since 1970
    new_msq.length += sprintf((char *)(new_msq.pointer + new_msq.length), "\"ID\":\"%s\",", xSystem.Parameters.gsm_imei);
    new_msq.length += sprintf((char *)(new_msq.pointer + new_msq.length), "\"PhoneNum\":\"%s\",", xSystem.Parameters.phone_number);
    new_msq.length += sprintf((char *)(new_msq.pointer + new_msq.length), "\"Money\":\"%d\",", 0);
    new_msq.length += sprintf((char *)(new_msq.pointer + new_msq.length), "\"Input1\":\"%d\",",
                              xSystem.MeasureStatus.PulseCounterInBkup / xSystem.Parameters.kFactor + xSystem.Parameters.input1Offset); //so xung

    new_msq.length += sprintf((char *)(new_msq.pointer + new_msq.length), "\"Input2\":\"%.1f\",", xSystem.MeasureStatus.Input420mA); //dau vao 4-20mA
    new_msq.length += sprintf((char *)(new_msq.pointer + new_msq.length), "\"Output1\":\"%d\",", xSystem.Parameters.outputOnOff);    //dau ra on/off
    new_msq.length += sprintf((char *)(new_msq.pointer + new_msq.length), "\"Output2\":\"%d\",", xSystem.Parameters.output420ma);    //dau ra 4-20mA
    new_msq.length += sprintf((char *)(new_msq.pointer + new_msq.length), "\"SignalStrength\":\"%d\",", xSystem.Status.CSQPercent);
    new_msq.length += sprintf((char *)(new_msq.pointer + new_msq.length), "\"WarningLevel\":\"%d\",", xSystem.Status.Alarm);

    new_msq.length += sprintf((char *)(new_msq.pointer + new_msq.length), "\"BatteryLevel\":\"%d\",", xSystem.MeasureStatus.batteryPercent);
    new_msq.length += sprintf((char *)(new_msq.pointer + new_msq.length), "\"BatteryDebug\":\"%d\",", xSystem.MeasureStatus.Vin);
    new_msq.length += sprintf((char *)(new_msq.pointer + new_msq.length), "\"K\":\"%u\",", xSystem.Parameters.kFactor);
    new_msq.length += sprintf((char *)(new_msq.pointer + new_msq.length), "\"Offset\":\"%u\"}", xSystem.Parameters.input1Offset);

    if (app_queue_put(&m_http_msq, &new_msq) == false)
    {
        DEBUG_PRINTF("Put to http msg failed\r\n");
        umm_free(new_msq.pointer);
        return 0;
    }
    else
    {
        DEBUG_PRINTF("Put to http msg success, length %u\r\n", new_msq.length);
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
        DEBUG_PRINTF("DATA: %s\r\n", get_data->data);
        server_msg_process_cmd((char *)get_data->data);
    }
    break;

    case GSM_HTTP_POST_EVENT_DATA:
    {
        if (m_last_http_msg.pointer == NULL)
        {
            app_queue_get(&m_http_msq, &m_last_http_msg);
            ((gsm_http_data_t *)data)->data = (uint8_t *)m_last_http_msg.pointer;
            ((gsm_http_data_t *)data)->data_length = m_last_http_msg.length;
            ((gsm_http_data_t *)data)->header = (uint8_t *)build_http_header(m_last_http_msg.length);
        }
    }
    break;

    case GSM_HTTP_GET_EVENT_FINISH_SUCCESS:
    {
        DEBUG_PRINTF("HTTP get : event success\r\n");
        xSystem.Status.DisconnectTimeout = 0;
        gsm_change_state(GSM_STATE_OK);
        DEBUG_PRINTF("Free um memory, malloc count[%u]\r\n", malloc_count);
    }
    break;

    case GSM_HTTP_POST_EVENT_FINISH_SUCCESS:
    {
        DEBUG_PRINTF("HTTP post : event success\r\n");
        xSystem.Status.DisconnectTimeout = 0;
        if (m_last_http_msg.pointer)
        {
            malloc_count--;
            umm_free(m_last_http_msg.pointer);
            DEBUG_PRINTF("Free um memory, malloc count[%u]\r\n", malloc_count);
            m_last_http_msg.pointer = NULL;
        }
        gsm_change_state(GSM_STATE_OK);
    }
    break;

    case GSM_HTTP_POST_EVENT_FINISH_FAILED:
    case GSM_HTTP_EVENT_FINISH_FAILED:
    {
        DEBUG_PRINTF("HTTP event failed\r\n");
        if (m_last_http_msg.pointer)
        {
            malloc_count--;
            umm_free(m_last_http_msg.pointer);
            DEBUG_PRINTF("Free um memory, malloc count[%u]\r\n", malloc_count);
            m_last_http_msg.pointer = NULL;
        }
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

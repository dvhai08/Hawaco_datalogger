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
#include "umm_malloc_cfg.h"
#include "gsm_filefs.h"
#include "usart.h"
#include "app_rtc.h"

extern gsm_manager_t gsm_manager;

void gsm_at_cb_power_on_gsm(gsm_response_event_t event, void *resp_buffer);
void gsm_at_cb_send_sms(gsm_response_event_t event, void *resp_buffer);
void gsm_hard_reset(void);

uint32_t m_delay_gsm = 0;
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
        if (m_delay_gsm++ == 10)
        {
            gsm_file_fs_start();
        }
    }
        break;
    
    case GSM_STATE_FILE_READ:
        break;

    case GSM_STATE_RESET: /* Hard Reset */
        gsm_manager.gsm_ready = 0;
        gsm_hard_reset();
        break;

    default:
        DEBUG_ERROR("Unhandled case %u\r\n", gsm_manager.state);
        break;
    }
}


void gsm_data_layer_initialize(void)
{
    gsm_manager.ri_signal = 0;
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
    
    case GSM_STATE_POWER_ON:
        DEBUG_RAW("POWERON\r\n");
        gsm_hw_layer_reset_rx_buffer();
        break;
    default:
        break;
    }
    gsm_manager.state = new_state;
    gsm_manager.step = 0;
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
        break;

    case 5:
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
            gsm_change_state(GSM_STATE_OK);
        }
        else
        {
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
    case 5:
        step++;
        break;

    case 6:
        usart1_control(true);
        DEBUG_INFO("Pulse power key\r\n");
        /* Tao xung |_| de Power On module, min 1s  */
        GSM_PWR_KEY(1);
        step++;
        break;

    case 7:
        GSM_PWR_KEY(0);
        GSM_PWR_RESET(0);
        gsm_manager.timeout_after_reset = 90;
        step++;
        break;

    case 8:
    case 9:
        step++;
        break;
    
    case 10:
        step = 0;
        gsm_change_state(GSM_STATE_POWER_ON);
        break;
    
    default:
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
}

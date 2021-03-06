/******************************************************************************
 * @file    	m_gsm_hardwareLayer.c
 * @author
 * @version 	V1.0.0
 * @date    	10/11/2014
 * @brief
 ******************************************************************************/

/******************************************************************************
                                   INCLUDES
 ******************************************************************************/
#include <string.h>
#include "main.h"
#include "gsm.h"
#include "hardware.h"
#include "hardware_manager.h"
#include "app_debug.h"
#include "lwrb.h"
#include "usart.h"

#define Delayms sys_delay_ms

gsm_hardware_t m_gsm_hardware;
gsm_manager_t gsm_manager;

static volatile bool m_new_uart_data = false;
static char m_gsm_imei[16] = {0};
static char m_sim_imei[16];
static char m_nw_operator[32];
static char m_sim_ccid[21];

void gsm_init_hw(void)
{
    // Tat nguon module
    GSM_PWR_EN(0);
    GSM_PWR_RESET(1);
    GSM_PWR_KEY(0);

#if 0  // HW co RI pin
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;	
	GPIO_InitStructure.GPIO_Pin = GSM_RI_PIN;						
	GPIO_Init(GSM_RI_PORT, &GPIO_InitStructure);
	
	 /* Connect GSM_RI_PIN Line to PB6 pin */
	SYSCFG_EXTILineConfig(GSM_RI_PORT_SOURCE, GSM_RI_PIN_SOURCE);
	
	/* Configure EXTI6 line */
	EXTI_InitStructure.EXTI_Line = GSM_EXTI_LINE;
	EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt;
	EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Falling;
	EXTI_InitStructure.EXTI_LineCmd = ENABLE;
	EXTI_Init(&EXTI_InitStructure);
	
	/* Enable and set EXTI6 Interrupt priority */
	NVIC_InitStructure.NVIC_IRQChannel = EXTI9_5_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0x0F;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0x0F;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);
#endif // 0

    usart1_control(true);
    gsm_data_layer_initialize();

    gsm_change_state(GSM_STATE_RESET);

    gsm_manager.timeout_after_reset = 90;
    gsm_change_hw_polling_interval(5);
}

static volatile uint32_t m_dma_rx_expected_size = 0;
void gsm_uart_rx_dma_update_rx_index(bool rx_status)
{
    if (rx_status)
    {
        m_gsm_hardware.atc.recv_buff.index += m_dma_rx_expected_size;
    }
    else
    {
        m_dma_rx_expected_size = 0;
        m_gsm_hardware.atc.recv_buff.buffer[0] = 0;
        DEBUG_ERROR("UART RX error, retry received\r\n");
        NVIC_SystemReset();
    }
}

void gsm_uart_handler(void)
{
    m_new_uart_data = true;
}

uint32_t m_poll_interval;
uint32_t current_response_length;
uint32_t expect_compare_length;
uint8_t *p_compare_end_str;
uint8_t *uart_rx_pointer = m_gsm_hardware.atc.recv_buff.buffer;
uint8_t *expect_end_str;
void gsm_hw_layer_run(void)
{
    static uint32_t m_last_poll = 0;
    uint32_t now = sys_get_ms();
    if ((now - m_last_poll) < m_poll_interval)
    {
        return;
    }

    m_last_poll = now;
    bool ret_now = true;

    if (m_gsm_hardware.atc.retry_count_atc)
    {
        __disable_irq();

        if (m_new_uart_data)
        {
            m_new_uart_data = false;
            ret_now = false;
        }
        __enable_irq();

        if (ret_now == false && (strstr((char *)(m_gsm_hardware.atc.recv_buff.buffer), m_gsm_hardware.atc.expect_resp_from_atc)))
        {
            bool do_cb = true;
            if (m_gsm_hardware.atc.expected_response_at_the_end && strlen(m_gsm_hardware.atc.expected_response_at_the_end))
            {
                // DEBUG_PRINTF("Expected end %s\r\n", m_gsm_hardware.atc.expected_response_at_the_end);
                current_response_length = m_gsm_hardware.atc.recv_buff.index;
                expect_compare_length = strlen(m_gsm_hardware.atc.expected_response_at_the_end);

                if (expect_compare_length < current_response_length)
                {
                    p_compare_end_str = &m_gsm_hardware.atc.recv_buff.buffer[current_response_length - expect_compare_length];
                    if ((memcmp(p_compare_end_str, m_gsm_hardware.atc.expected_response_at_the_end, expect_compare_length) == 0))
                    {
                        do_cb = true;
                    }
                    else
                    {
                        do_cb = false;
                        // For debug only
                        expect_end_str = (uint8_t *)m_gsm_hardware.atc.expected_response_at_the_end;
                        uart_rx_pointer = m_gsm_hardware.atc.recv_buff.buffer;
                    }
                }
                else
                {
                    do_cb = false;
                }
            }

            if (do_cb)
            {
                m_gsm_hardware.atc.retry_count_atc = 0;
                m_gsm_hardware.atc.timeout_atc_ms = 0;
                m_gsm_hardware.atc.recv_buff.buffer[m_gsm_hardware.atc.recv_buff.index] = 0;
                if (m_gsm_hardware.atc.send_at_callback)
                {
                    m_gsm_hardware.atc.send_at_callback(GSM_EVENT_OK, m_gsm_hardware.atc.recv_buff.buffer);
                }
                m_gsm_hardware.atc.recv_buff.index = 0;
                memset(m_gsm_hardware.atc.recv_buff.buffer, 0, sizeof(((sys_ctx_small_buffer_t *)0)->buffer));
            }
        }
        else if (ret_now == false)
        {
            char *p = strstr((char *)(m_gsm_hardware.atc.recv_buff.buffer), "CME ERROR: ");
            if (p && strstr(p, "\r\n"))
            {
                //                DEBUG_VERBOSE("%s", p);
                //                if (m_gsm_hardware.atc.send_at_callback)
                //                {
                //                    m_gsm_hardware.atc.send_at_callback(GSM_EVENT_ERROR, m_gsm_hardware.atc.recv_buff.buffer);
                //                }
                //                m_gsm_hardware.atc.timeout_atc_ms = 0;
            }
        }
    }

    uint32_t diff = sys_get_ms() - m_gsm_hardware.atc.current_timeout_atc_ms;
    if (m_gsm_hardware.atc.timeout_atc_ms &&
        diff >= m_gsm_hardware.atc.timeout_atc_ms)
    {

        if (m_gsm_hardware.atc.retry_count_atc)
        {
            m_gsm_hardware.atc.retry_count_atc--;
        }

        if (m_gsm_hardware.atc.retry_count_atc == 0)
        {
            m_gsm_hardware.atc.timeout_atc_ms = 0;

            if (m_gsm_hardware.atc.send_at_callback != NULL)
            {
                m_gsm_hardware.atc.send_at_callback(GSM_EVENT_TIMEOUT, NULL);
            }
            m_gsm_hardware.atc.recv_buff.index = 0;
            m_gsm_hardware.atc.recv_buff.buffer[m_gsm_hardware.atc.recv_buff.index] = 0;
        }
        else
        {
            //            DEBUG_VERBOSE("Resend ATC: %sExpect %s\r\n", m_gsm_hardware.atc.cmd, m_gsm_hardware.atc.expect_resp_from_atc);
            m_gsm_hardware.atc.current_timeout_atc_ms = sys_get_ms();
            gsm_hw_uart_send_raw((uint8_t *)m_gsm_hardware.atc.cmd, strlen(m_gsm_hardware.atc.cmd));
        }
    }

    //    if (m_gsm_hardware.atc.recv_buff.index > 32
    //        && strstr((char*)m_gsm_hardware.atc.recv_buff.buffer+10, "CUSD:"))
    //        {
    //            DEBUG_VERBOSE("CUSD %s\r\n", m_gsm_hardware.atc.recv_buff.buffer);
    //        }

    if (m_gsm_hardware.atc.retry_count_atc == 0)
    {
        if (m_gsm_hardware.atc.recv_buff.index > 2 && strstr((char *)m_gsm_hardware.atc.recv_buff.buffer + 10, "\r\n"))
        {
            DEBUG_WARN("ATC : unhandled %s\r\n", m_gsm_hardware.atc.recv_buff.buffer);
            m_gsm_hardware.atc.recv_buff.index = 0;
            m_gsm_hardware.atc.recv_buff.buffer[m_gsm_hardware.atc.recv_buff.index] = 0;
        }
    }

    m_last_poll = sys_get_ms();
}

void gsm_change_hw_polling_interval(uint32_t ms)
{
    m_poll_interval = ms;
}

void gsm_hw_layer_reset_rx_buffer(void)
{
    memset(m_gsm_hardware.atc.recv_buff.buffer, 0, sizeof(((sys_ctx_small_buffer_t *)0)->buffer));
    m_gsm_hardware.atc.recv_buff.index = 0;
    m_gsm_hardware.atc.recv_buff.state = SYS_CTX_BUFFER_STATE_IDLE;
    m_gsm_hardware.atc.retry_count_atc = 0;
}

void gsm_hw_send_at_cmd(char *cmd, char *expect_resp,
                        char *expected_response_at_the_end_of_response, uint32_t timeout,
                        uint8_t retry_count, gsm_send_at_cb_t callback)
{
    if (timeout == 0 || callback == NULL)
    {
        gsm_hw_uart_send_raw((uint8_t *)cmd, strlen(m_gsm_hardware.atc.cmd));
        return;
    }

    //    if (strlen(cmd) < 64)
    //    {
    //        DEBUG_INFO("ATC: %s", cmd);
    //    }

    m_gsm_hardware.atc.cmd = cmd;
    m_gsm_hardware.atc.expect_resp_from_atc = expect_resp;
    m_gsm_hardware.atc.expected_response_at_the_end = expected_response_at_the_end_of_response;
    m_gsm_hardware.atc.retry_count_atc = retry_count;
    m_gsm_hardware.atc.send_at_callback = callback;
    m_gsm_hardware.atc.timeout_atc_ms = timeout;
    m_gsm_hardware.atc.current_timeout_atc_ms = sys_get_ms();
    memset(m_gsm_hardware.atc.recv_buff.buffer, 0, sizeof(((sys_ctx_small_buffer_t *)0)->buffer));
    m_gsm_hardware.atc.recv_buff.index = 0;
    m_gsm_hardware.atc.recv_buff.state = SYS_CTX_BUFFER_STATE_IDLE;

    gsm_hw_uart_send_raw((uint8_t *)cmd, strlen(cmd));
}

extern void usart1_hw_uart_send_raw(uint8_t *data, uint32_t length);
void gsm_hw_uart_send_raw(uint8_t *raw, uint32_t length)
{
    usart1_hw_uart_send_raw(raw, length);
}

uint32_t prev_index = 0;
void gsm_hw_layer_uart_fill_rx(uint8_t *data, uint32_t length)
{
    if (length)
    {
        m_new_uart_data = true;
        prev_index = m_gsm_hardware.atc.recv_buff.index;
        for (uint32_t i = 0; i < length; i++)
        {
            m_gsm_hardware.atc.recv_buff.buffer[m_gsm_hardware.atc.recv_buff.index++] = data[i];
            if (m_gsm_hardware.atc.recv_buff.index >= sizeof(((sys_ctx_small_buffer_t *)0)->buffer))
            {
                DEBUG_ERROR("GSM RX overflow\r\n");
                m_gsm_hardware.atc.recv_buff.index = 0;
                m_gsm_hardware.atc.recv_buff.buffer[0] = 0;
                return;
            }
        }
    }
}

char *gsm_get_sim_imei(void)
{
    return m_sim_imei;
}

char *gsm_get_sim_ccid(void)
{
    return m_sim_ccid;
}

char *gsm_get_module_imei(void)
{
    //    #warning "Hardcode imei 123456789012345"
    //    return  "123456789012345";

    return m_gsm_imei;
}

void gsm_set_sim_imei(char *imei)
{
    memcpy(m_sim_imei, imei, 15);
    m_sim_imei[15] = 0;
}

void gsm_set_module_imei(char *imei)
{
    memcpy(m_gsm_imei, imei, 15);
    m_gsm_imei[15] = 0;
}

void gsm_set_network_operator(char *nw_operator)
{
    snprintf(m_nw_operator, sizeof(m_nw_operator) - 1, "%s", nw_operator);
    m_nw_operator[sizeof(m_nw_operator) - 1] = 0;
}

char *gsm_get_network_operator(void)
{
    return m_nw_operator;
}

static uint8_t m_csq, m_csq_percent;

void gsm_set_csq(uint8_t csq)
{
    m_csq = csq;
    if (m_csq == 99) // 99 is invalid CSQ
    {
        m_csq = 0;
    }

    if (m_csq > 31)
    {
        m_csq = 31;
    }

    if (m_csq < 10)
    {
        m_csq = 10;
    }

    m_csq_percent = ((m_csq - 10) * 100) / (31 - 10);
}

uint8_t gsm_get_csq_in_percent()
{
    return m_csq_percent;
}

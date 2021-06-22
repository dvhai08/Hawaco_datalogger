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
#include "DataDefine.h"
#include "lwrb.h"

static gsm_hardware_t m_gsm_hardware;
GSM_Manager_t gsm_manager;

#ifdef GD32E10X
static lwrb_t m_ringbuffer_uart_tx = 
{
    .buff = NULL,
};
static uint8_t m_uart_tx_buffer[256];
static volatile uint32_t m_tx_uart_run = 0;
#else
    #define Delayms     sys_delay_ms
#include "usart.h"
#endif
static volatile bool m_new_uart_data = false;
static void init_serial(void);
static char m_gsm_imei[16];
static char m_sim_imei[16];
static char m_nw_operator[32];

void gsm_init_hw(void)
{
    //Tat nguon module
    GSM_PWR_EN(0);
    GSM_PWR_RESET(1);
    GSM_PWR_KEY(0);
    
#if 0  //HW co RI pin
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
#endif //0

	usart1_control(true);
    gsm_data_layer_initialize();

    gsm_change_state(GSM_STATE_RESET); 

    gsm_manager.timeout_after_reset = 90;
    gsm_change_hw_polling_interval(5);
    init_serial();
}

void gsm_pwr_control(uint8_t State)
{
    if (State == GSM_OFF)
    {
        gsm_manager.is_gsm_power_off = 1;
        if (gsm_manager.gsm_ready)
        {
            /* Tao xung |_| de Power Off module, min 1s  */
            GSM_PWR_KEY(1);
            Delayms(1500);
            GSM_PWR_KEY(0);
        }

        Delayms(5000);
        //Ngat nguon VBAT 4.2V
        GSM_PWR_EN(0);

        //Reset cac bien
        gsm_manager.gsm_ready = 0;
        gsm_manager.RISignal = 0;
        //xSystem.Status.ServerState = NOT_CONNECTED;
        //xSystem.Status.MQTTServerState = MQTT_INIT;

        //Disable luon UART
		usart1_control(false);
    }
    else if (State == GSM_ON)
    {
        gsm_manager.is_gsm_power_off = 0;

        //Khoi tao lai UART
		usart1_control(true);
        /* Cap nguon 4.2V */
        GSM_PWR_EN(1);
        Delayms(1000); //Wait for VBAT stable

        /* Tao xung |_| de Power On module, min 1s  */
        GSM_PWR_KEY(1);
        Delayms(1000);
        GSM_PWR_KEY(0);

        gsm_manager.FirstTimePower = 0;
    }
}


static void init_serial(void)
{
    //m_gsm_hardware.modem.tx_buffer.idx_in = 0;
    //m_gsm_hardware.modem.tx_buffer.idx_out = 0;
#ifdef GD32E10X
    m_tx_uart_run = 0;
    /* Enable GSM interrupts. */
    NVIC_EnableIRQ(GSM_UART_IRQ);
#endif
}
static volatile uint32_t m_dma_rx_expected_size = 0;
void gsm_uart_rx_dma_update_rx_index(bool rx_status)
{
    if (rx_status)
    {
        m_gsm_hardware.atc.recv_buff.BufferIndex += m_dma_rx_expected_size;
        // TODO retried received
    }
    else
    {
        m_dma_rx_expected_size = 0;
        m_gsm_hardware.atc.recv_buff.Buffer[0] = 0;
        DEBUG_PRINTF("UART RX error, retry received\r\n");
    }
}

void gsm_uart_handler(void)
{
#ifdef GD32E10X
    /* Serial Rx and Tx interrupt handler. */
    //gsm_ppp_modem_buffer_t *p;

    if (usart_flag_get(GSM_UART, USART_FLAG_RBNE) == 1) //RBNE = 1
    {
        uint8_t ch = usart_data_receive(GSM_UART);
        m_new_uart_data = true;
        gsm_manager.TimeOutConnection = 0; //Reset UART data timeout

        if (GSM_STATE_RESET != gsm_manager.state)
        {
            if (m_gsm_hardware.atc.timeout_atc_ms)
            {
                m_gsm_hardware.atc.recv_buff.Buffer[m_gsm_hardware.atc.recv_buff.BufferIndex++] = ch;
                if (m_gsm_hardware.atc.recv_buff.BufferIndex >= SMALL_BUFFER_SIZE)
                {
                    DEBUG_PRINTF("[%s] Overflow\r\n", __FUNCTION__);
                    m_gsm_hardware.atc.recv_buff.BufferIndex = 0;
                }
            }
        }
        usart_interrupt_flag_clear(GSM_UART, USART_INT_FLAG_RBNE);
    }

    if ((usart_flag_get(GSM_UART, USART_FLAG_TBE) == 1) //TBE = 1
        && (m_tx_uart_run))
    {
        usart_interrupt_flag_clear(GSM_UART, USART_INT_FLAG_TBE);
        uint8_t ch;
        if (lwrb_read(&m_ringbuffer_uart_tx, &ch, 1))
        {
            usart_data_transmit(GSM_UART, ch);
        }
        else
        {
            usart_interrupt_disable(GSM_UART, USART_INT_TBE); /* No more data need to transfer, disable TXE interrupt */
            m_tx_uart_run = 0;
        }
    }

    if (usart_flag_get(GSM_UART, USART_FLAG_ORERR) == 1) //RBNE = 1
    {
        DEBUG_PRINTF("Overrun\r\n");
        usart_data_receive(GSM_UART);
        usart_interrupt_flag_clear(GSM_UART, USART_INT_FLAG_ERR_ORERR);
    }

    if (usart_flag_get(GSM_UART, USART_FLAG_FERR) == 1) //RBNE = 1
    {
        DEBUG_PRINTF("Frame error\r\n");
        usart_interrupt_flag_clear(GSM_UART, USART_INT_FLAG_ERR_FERR);
    }
#else
    m_new_uart_data = true;
    gsm_manager.TimeOutConnection = 0; //Reset UART data timeout
#endif
}


uint32_t m_poll_interval;
uint32_t current_response_length;
uint32_t expect_compare_length;
uint8_t *p_compare_end_str;
uint8_t *uart_rx_pointer = m_gsm_hardware.atc.recv_buff.Buffer;
uint8_t *expect_end_str;
void gsm_hw_layer_run(void)
{
    static uint32_t m_last_poll = 0;
    uint32_t now = sys_get_ms();
    if ((now - m_last_poll) < m_poll_interval)
    {
        return;
    }

    uint32_t diff = (uint32_t)(now - m_gsm_hardware.atc.current_timeout_atc_ms);
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

        if (ret_now == false
        && (strstr((char *)(m_gsm_hardware.atc.recv_buff.Buffer), m_gsm_hardware.atc.expect_resp_from_atc)))
        {
            bool do_cb = true;
            if (m_gsm_hardware.atc.expected_response_at_the_end 
                && strlen(m_gsm_hardware.atc.expected_response_at_the_end))
            {
                //DEBUG_PRINTF("Expected end %s\r\n", m_gsm_hardware.atc.expected_response_at_the_end);
                current_response_length = m_gsm_hardware.atc.recv_buff.BufferIndex;
                expect_compare_length = strlen(m_gsm_hardware.atc.expected_response_at_the_end);

                if (expect_compare_length < current_response_length)
                {
                    p_compare_end_str = &m_gsm_hardware.atc.recv_buff.Buffer[current_response_length - expect_compare_length];
                    if ((memcmp(p_compare_end_str, m_gsm_hardware.atc.expected_response_at_the_end, expect_compare_length) == 0))
                    {
                        do_cb = true;
                    }
                    else
                    {
                        do_cb = false;
                        // For debug only
                        expect_end_str = (uint8_t*)m_gsm_hardware.atc.expected_response_at_the_end;
                        uart_rx_pointer = m_gsm_hardware.atc.recv_buff.Buffer;
                    }
                }
                else
                {
                    do_cb = false;
                }
            }

            if (do_cb)
            {
                m_gsm_hardware.atc.timeout_atc_ms = 0;
                m_gsm_hardware.atc.recv_buff.Buffer[m_gsm_hardware.atc.recv_buff.BufferIndex] = 0;
                if (m_gsm_hardware.atc.send_at_callback)
                {
                    m_gsm_hardware.atc.send_at_callback(GSM_EVENT_OK, m_gsm_hardware.atc.recv_buff.Buffer);
                }
                m_gsm_hardware.atc.recv_buff.BufferIndex = 0;
                memset(m_gsm_hardware.atc.recv_buff.Buffer, 0, SMALL_BUFFER_SIZE);
            }
        }
        else if (ret_now == false)
        {
            char *p = strstr((char *)(m_gsm_hardware.atc.recv_buff.Buffer), "CME");
            if (p && strstr(p, "\r\n"))
            {
                DEBUG_PRINTF("CME error %s", p);
                if (m_gsm_hardware.atc.send_at_callback)
                {
                    m_gsm_hardware.atc.send_at_callback(GSM_EVENT_ERROR, m_gsm_hardware.atc.recv_buff.Buffer);
                }
                m_gsm_hardware.atc.timeout_atc_ms = 0;
            }
        }
    }

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
            m_gsm_hardware.atc.recv_buff.BufferIndex = 0;
            m_gsm_hardware.atc.recv_buff.Buffer[m_gsm_hardware.atc.recv_buff.BufferIndex] = 0;;
        }
        else
        {
            DEBUG_PRINTF("Resend ATC: %sExpect %s\r\n", m_gsm_hardware.atc.cmd, m_gsm_hardware.atc.expect_resp_from_atc);
            m_gsm_hardware.atc.current_timeout_atc_ms = sys_get_ms();
            gsm_hw_uart_send_raw((uint8_t*)m_gsm_hardware.atc.cmd, strlen(m_gsm_hardware.atc.cmd));
        }
    }

    if (m_gsm_hardware.atc.recv_buff.BufferIndex > 32 
        && strstr((char*)m_gsm_hardware.atc.recv_buff.Buffer+10, "CUSD:"))
        {
            DEBUG_PRINTF("CUSD %s\r\n", m_gsm_hardware.atc.recv_buff.Buffer);
        }

    if (m_gsm_hardware.atc.retry_count_atc == 0)
    {
        if (m_gsm_hardware.atc.recv_buff.BufferIndex > 10 
            && strstr((char*)m_gsm_hardware.atc.recv_buff.Buffer+10, "\r\n"))
        {
            DEBUG_PRINTF("ATC : unhandled %s\r\n", m_gsm_hardware.atc.recv_buff.Buffer);
            m_gsm_hardware.atc.recv_buff.BufferIndex = 0;
            m_gsm_hardware.atc.recv_buff.Buffer[m_gsm_hardware.atc.recv_buff.BufferIndex] = 0;;
        }
    }

    m_last_poll = sys_get_ms();
}

void gsm_change_hw_polling_interval(uint32_t ms)
{
    m_poll_interval = ms;
}

void gsm_hw_send_at_cmd(char *cmd, char *expect_resp, 
                        char * expected_response_at_the_end_of_response, uint32_t timeout,
                        uint8_t retry_count, gsm_send_at_cb_t callback)
{
    if (timeout == 0 || callback == NULL)
    {
        gsm_hw_uart_send_raw((uint8_t*)cmd, strlen(m_gsm_hardware.atc.cmd));
        return;
    }
    

    if (strlen(cmd) < 64)
    {
        DEBUG_PRINTF("ATC: %s", cmd);
    }

    m_gsm_hardware.atc.cmd = cmd;
    m_gsm_hardware.atc.expect_resp_from_atc = expect_resp;
    m_gsm_hardware.atc.expected_response_at_the_end = expected_response_at_the_end_of_response;
    m_gsm_hardware.atc.recv_buff.State = BUFFER_STATE_IDLE;
    m_gsm_hardware.atc.retry_count_atc = retry_count;
    m_gsm_hardware.atc.send_at_callback = callback;
    m_gsm_hardware.atc.timeout_atc_ms = timeout;
    m_gsm_hardware.atc.current_timeout_atc_ms = sys_get_ms();

    memset(m_gsm_hardware.atc.recv_buff.Buffer, 0, SMALL_BUFFER_SIZE);
    m_gsm_hardware.atc.recv_buff.BufferIndex = 0;
    m_gsm_hardware.atc.recv_buff.State = BUFFER_STATE_IDLE;

    gsm_hw_uart_send_raw((uint8_t*)cmd, strlen(cmd));
}

#ifdef GD32E10X
volatile uint32_t m_last_transfer_size = 0;

static inline void config_dma(uint8_t *data, uint32_t len)
{
    LL_DMA_DisableChannel(DMA1, LL_DMA_CHANNEL_2);
    
    LL_DMA_ConfigAddresses(DMA1, LL_DMA_CHANNEL_2,
                         (uint32_t)data,
                         (uint32_t)&(USART1->TDR),
                         LL_DMA_DIRECTION_MEMORY_TO_PERIPH);
    
    LL_DMA_SetDataLength(DMA1, LL_DMA_CHANNEL_2, len);
    LL_DMA_SetPeriphRequest(DMA1, LL_DMA_CHANNEL_2, LL_DMA_REQUEST_2);
    
    /* Enable DMA TX Interrupt */
    LL_USART_EnableDMAReq_TX(USART1);
    LL_DMA_EnableChannel(DMA1, LL_DMA_CHANNEL_2);
}

static inline void gsm_hw_transmit_dma(void)
{
    if (lwrb_get_full(&m_ringbuffer_uart_tx) == 0)
    {
        m_tx_uart_run = false;
        m_last_transfer_size = 0;
        return;
    }
    
    uint8_t *addr = lwrb_get_linear_block_read_address(&m_ringbuffer_uart_tx);
    m_last_transfer_size = lwrb_get_linear_block_read_length(&m_ringbuffer_uart_tx);
    uint32_t bytes_need_to_transfer =  lwrb_get_full(&m_ringbuffer_uart_tx);
    
    if (bytes_need_to_transfer < m_last_transfer_size)
    {
        m_last_transfer_size = bytes_need_to_transfer;
    }
    if (m_tx_uart_run == false)
    {
        m_tx_uart_run = true;
        config_dma(addr, m_last_transfer_size);
    }
}

void gsm_uart_tx_complete_callback(bool status)
{
    m_tx_uart_run = false;
    lwrb_skip(&m_ringbuffer_uart_tx, m_last_transfer_size);
    gsm_hw_transmit_dma();
}
#endif

extern void usart1_hw_uart_send_raw(uint8_t *data, uint32_t length);
void gsm_hw_uart_send_raw(uint8_t* raw, uint32_t length)
{
    usart1_hw_uart_send_raw(raw, length);
}

void gsm_hw_layer_uart_fill_rx(uint8_t *data, uint32_t length)
{
	if (length)
	{			
		m_new_uart_data = true;
		for (uint32_t i = 0; i < length; i++)
		{
			m_gsm_hardware.atc.recv_buff.Buffer[m_gsm_hardware.atc.recv_buff.BufferIndex++] = data[i];
			if (m_gsm_hardware.atc.recv_buff.BufferIndex >= SMALL_BUFFER_SIZE)
			{
				DEBUG_PRINTF("[%s] Overflow\r\n", __FUNCTION__);
				m_gsm_hardware.atc.recv_buff.BufferIndex = 0;
				return;
			}
		}
	}
}

char* gsm_get_sim_imei(void)
{
	return m_sim_imei;
}


char* gsm_get_module_imei(void)
{
	#warning "Hardcode module imei"
	return "863674040981159";
//	return m_gsm_imei;
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
	snprintf(m_nw_operator, sizeof(m_nw_operator)-1, "%s", nw_operator);
	m_nw_operator[sizeof(m_nw_operator)-1] = 0;
}

char *gsm_get_network_operator(void)
{
	return m_nw_operator;
}

static uint8_t m_csq, m_csq_percent;


void gsm_set_csq(uint8_t csq)
{
	m_csq = csq;
	if (m_csq == 99)      // 99 is invalid CSQ
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


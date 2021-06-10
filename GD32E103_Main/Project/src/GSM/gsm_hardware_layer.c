/******************************************************************************
 * @file    	GSM_HardwareLayer.c
 * @author  	
 * @version 	V1.0.0
 * @date    	10/11/2014
 * @brief   	
 ******************************************************************************/

/******************************************************************************
                                   INCLUDES					    			 
 ******************************************************************************/
#include <string.h>
#include "Main.h"
#include "gsm.h"
#include "hardware.h"
#include "HardwareManager.h"
#include "DataDefine.h"
#include "MQTTUser.h"

/******************************************************************************
                                   GLOBAL VARIABLES					    			 
 ******************************************************************************/
extern System_t xSystem;

/******************************************************************************
                                   GLOBAL FUNCTIONS					    			 
 ******************************************************************************/

/******************************************************************************
                                   DATA TYPE DEFINE					    			 
 ******************************************************************************/

/******************************************************************************
                                   PRIVATE VARIABLES					    			 
 ******************************************************************************/
gsm_hardware_t GSM_Hardware;
GSM_Manager_t gsm_manager;

static uint16_t CMEErrorCount = 0;
//static uint16_t CMEErrorTimeout = 0;
static __IO bool m_new_uart_data = false;
static void init_serial(void);

void gsm_init_hw(void)
{
    gpio_pin_remap_config(GPIO_SWJ_SWDPENABLE_REMAP, ENABLE); /*!< JTAG-DP disabled and SW-DP enabled */

    gpio_init(GSM_PWR_EN_PORT, GPIO_MODE_OUT_PP, GPIO_OSPEED_50MHZ, GSM_PWR_EN_PIN);
    gpio_init(GSM_PWR_KEY_PORT, GPIO_MODE_OUT_PP, GPIO_OSPEED_50MHZ, GSM_PWR_KEY_PIN);
    gpio_init(GSM_RESET_PORT, GPIO_MODE_OUT_PP, GPIO_OSPEED_50MHZ, GSM_RESET_PIN);

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

    /* configure RI (PA11) pin as input */
    gpio_init(GSM_RI_PORT, GPIO_MODE_IPU, GPIO_OSPEED_10MHZ, GSM_RI_PIN);
    gpio_init(GSM_STATUS_PORT, GPIO_MODE_IPU, GPIO_OSPEED_10MHZ, GSM_STATUS_PIN);

#if 0  //HW co RI pin
    /* enable and set key user EXTI interrupt to the lowest priority */
    nvic_irq_enable(EXTI10_15_IRQn, 2U, 1U);

    /* connect key user EXTI line to key GPIO pin */
    gpio_exti_source_select(GSM_RI_PORT_SOURCE, GSM_RI_PIN_SOURCE);

    /* configure key user EXTI line */
    exti_init(GSM_RI_EXTI_LINE, EXTI_INTERRUPT, EXTI_TRIG_FALLING);
    exti_interrupt_flag_clear(GSM_RI_EXTI_LINE);
#endif //__RI_INTERRUPT__

    ///* Cap nguon cho Module */
    //Delayms(3000);
    //GSM_PWR_RESET(0);
    //GSM_PWR_EN(1);
    //Delayms(1000); //Cho VBAT on dinh

    ///* Tao xung |_| de Power On module, min 1s  */
    //GSM_PWR_KEY(1);
    //Delayms(1500);
    //GSM_PWR_KEY(0);
    //Delayms(1000);

    /* Khoi tao UART cho GSM modem */
    UART_Init(GSM_UART, 115200);

    gsm_data_layer_initialize();

    gsm_change_state(GSM_STATE_RESET); 

    gsm_manager.TimeOutOffAfterReset = 90;
    gsm_change_hw_polling_interval(5);
    init_serial();
}

void gsm_pwr_control(uint8_t State)
{
    if (State == GSM_OFF)
    {
        //Neu dang trong qua trinh update FW -> Khong OFF
        if (xSystem.FileTransfer.State != FT_NO_TRANSER)
            return;

        gsm_manager.isGSMOff = 1;
        if (gsm_manager.GSMReady)
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
        gsm_manager.Mode = GSM_AT_MODE;
        gsm_manager.GSMReady = 0;
        gsm_manager.RISignal = 0;
        xSystem.Status.ServerState = NOT_CONNECTED;
        xSystem.Status.MQTTServerState = MQTT_INIT;

        //Disable luon UART
        usart_disable(GSM_UART);
        usart_deinit(GSM_UART);
    }
    else if (State == GSM_ON)
    {
        gsm_manager.isGSMOff = 0;

        //Khoi tao lai UART
        UART_Init(GSM_UART, 115200);

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
    GSM_Hardware.modem.tx_buffer.idx_in = 0;
    GSM_Hardware.modem.tx_buffer.idx_out = 0;
    GSM_Hardware.modem.tx_active = 0;

    /* Enable GSM interrupts. */
    NVIC_EnableIRQ(GSM_UART_IRQ);
}


void gsm_uart_handler(void)
{
    /* Serial Rx and Tx interrupt handler. */
    gsm_ppp_modem_buffer_t *p;

    if (usart_flag_get(GSM_UART, USART_FLAG_RBNE) == 1) //RBNE = 1
    {
        uint8_t ch = usart_data_receive(GSM_UART);
        gsm_manager.TimeOutConnection = 0; //Reset UART data timeout

        if (GSM_STATE_RESET != gsm_manager.state)
        {
            if (GSM_Hardware.atc.timeout_atc_ms)
            {
                GSM_Hardware.atc.recv_buff.Buffer[GSM_Hardware.atc.recv_buff.BufferIndex++] = ch;
                if (GSM_Hardware.atc.recv_buff.BufferIndex >= SMALL_BUFFER_SIZE)
                {
                    DEBUG_PRINTF("[%s] Overflow\r\n", __FUNCTION__);
                    GSM_Hardware.atc.recv_buff.BufferIndex = 0;
                }

                //GSM_Hardware.atc.recv_buff.Buffer[GSM_Hardware.atc.recv_buff.BufferIndex] = 0;
            }
        }
        usart_interrupt_flag_clear(GSM_UART, USART_INT_FLAG_RBNE);
    }

    if ((usart_flag_get(GSM_UART, USART_FLAG_TBE) == 1) //TBE = 1
        && (GSM_Hardware.modem.tx_active == __TRUE))
    {
        usart_interrupt_flag_clear(GSM_UART, USART_INT_FLAG_TBE);

        /* Transmit Data Register Empty */
        p = &GSM_Hardware.modem.tx_buffer;
        if (p->idx_in != p->idx_out)
        {
            usart_data_transmit(GSM_UART, (p->Buffer[p->idx_out++]));
            if (p->idx_out == MODEM_BUFFER_SIZE)
            {
                DEBUG_PRINTF("UART TX buffer out : overflow\r\n");
                p->idx_out = 0;
            }
        }
        else
        {
            usart_interrupt_disable(GSM_UART, USART_INT_TBE); /* Disable TXE interrupt */
            p->idx_in = 0;
            p->idx_out = 0;
            GSM_Hardware.modem.tx_active = 0;
            //DEBUG_PRINTF("Tx complete\r\n");
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
}

BOOL com_tx_active(void)
{
    return GSM_Hardware.modem.tx_active;
}


void modem_run()
{
    uint32_t now = sys_get_ms();
    uint32_t diff;

    if (now >= GSM_Hardware.atc.current_timeout_atc_ms)
    {
        diff = now - GSM_Hardware.atc.current_timeout_atc_ms;
    }
    else
    {
        diff = 0xFFFFFFFF - GSM_Hardware.atc.current_timeout_atc_ms + now; 
    }

    if (GSM_Hardware.atc.retry_count_atc
        && (strstr((char *)(GSM_Hardware.atc.recv_buff.Buffer), GSM_Hardware.atc.expect_resp_from_atc)))
    {
        bool do_cb = true;
        if (GSM_Hardware.atc.expected_response_at_the_end 
            && strlen(GSM_Hardware.atc.expected_response_at_the_end))
        {
            //DEBUG_PRINTF("Expected end %s\r\n", GSM_Hardware.atc.expected_response_at_the_end);
            uint32_t current_response_length = GSM_Hardware.atc.recv_buff.BufferIndex;
            uint32_t expect_compare_length = strlen(GSM_Hardware.atc.expected_response_at_the_end);

            if (expect_compare_length < current_response_length)
            {
                uint8_t *p_compare_end_str = &GSM_Hardware.atc.recv_buff.Buffer[current_response_length - expect_compare_length - 1];
                if ((memcmp(p_compare_end_str, GSM_Hardware.atc.expected_response_at_the_end, expect_compare_length) == 0))
                {
                    do_cb = true;
                }
            }
            else
            {
                do_cb = false;
            }
        }

        if (do_cb)
        {
            //DEBUG_PRINTF("Do callback\r\n");
            GSM_Hardware.atc.timeout_atc_ms = 0;
            GSM_Hardware.atc.recv_buff.Buffer[GSM_Hardware.atc.recv_buff.BufferIndex] = 0;
            if (GSM_Hardware.atc.send_at_callback)
            {
                GSM_Hardware.atc.send_at_callback(GSM_EVENT_OK, GSM_Hardware.atc.recv_buff.Buffer);
            }
            GSM_Hardware.atc.recv_buff.BufferIndex = 0;
            memset(GSM_Hardware.atc.recv_buff.Buffer, 0, SMALL_BUFFER_SIZE);
        }
    }
    else 
    {
        char *p = strstr((char *)(GSM_Hardware.atc.recv_buff.Buffer), "CME");
        if (p && strstr(p, "\r\n"))
        {
            DEBUG_PRINTF("CME error %s", p);
            GSM_Hardware.atc.timeout_atc_ms = 0;
        }
        else
        {
            if (GSM_Hardware.atc.retry_count_atc == 0)
            {
                if (GSM_Hardware.atc.recv_buff.BufferIndex > 10 && strstr((char*)GSM_Hardware.atc.recv_buff.Buffer+2, "\r\n"))
                {
                    DEBUG_PRINTF("Unhandled %s\r\n", GSM_Hardware.atc.recv_buff.Buffer);
                    //GSM_Hardware.atc.recv_buff.BufferIndex = 0;
                    //GSM_Hardware.atc.recv_buff.Buffer[GSM_Hardware.atc.recv_buff.BufferIndex] = 0;;
                }
            }
        }

    }

    if (GSM_Hardware.atc.timeout_atc_ms && 
        diff >= GSM_Hardware.atc.timeout_atc_ms)
    {

        if (GSM_Hardware.atc.retry_count_atc)
        {
            GSM_Hardware.atc.retry_count_atc--;
        }

        if (GSM_Hardware.atc.retry_count_atc == 0)
        {
            GSM_Hardware.atc.timeout_atc_ms = 0;

            if (GSM_Hardware.atc.send_at_callback != NULL)
            {
                GSM_Hardware.atc.send_at_callback(GSM_EVENT_TIMEOUT, NULL);
            }
            GSM_Hardware.atc.recv_buff.BufferIndex = 0;
            GSM_Hardware.atc.recv_buff.Buffer[GSM_Hardware.atc.recv_buff.BufferIndex] = 0;;
        }
        else
        {
            DEBUG("Resend ATC: %sExpect %s\r\n", GSM_Hardware.atc.cmd, GSM_Hardware.atc.expect_resp_from_atc);
            GSM_Hardware.atc.current_timeout_atc_ms = now;
            gsm_hw_uart_send_raw((uint8_t*)GSM_Hardware.atc.cmd, strlen(GSM_Hardware.atc.cmd));
        }
    }
}

uint32_t m_poll_interval;
void gsm_hw_layer_run(void)
{
    static uint32_t m_last_poll = 0;
    if (sys_get_ms() - m_last_poll >= m_poll_interval)
    {
        m_last_poll = sys_get_ms();
        modem_run();
    }
}

void gsm_change_hw_polling_interval(uint32_t ms)
{
    m_poll_interval = ms;
}

void gsm_hw_clear_at_serial_rx_buffer(void)
{
    memset(GSM_Hardware.atc.recv_buff.Buffer, 0, sizeof(((gsm_hardware_t*)0)->atc.recv_buff.Buffer));
    GSM_Hardware.atc.recv_buff.BufferIndex = 0;
    GSM_Hardware.atc.recv_buff.State = BUFFER_STATE_IDLE;
}


uint32_t gsm_hw_direct_read_at_command_rx_buffer(uint8_t **output, uint32_t size)
{
    // First read at command buffer
    uint32_t bytes_to_read = GSM_Hardware.atc.recv_buff.BufferIndex;
    if (size > bytes_to_read)
    {
        size = bytes_to_read;
    }
    
    //memcpy(output, m_gsm_hw_buffer.at_cmd.at_buffer.buffer, size);
    *output = GSM_Hardware.atc.recv_buff.Buffer;
    GSM_Hardware.atc.recv_buff.BufferIndex -= size;

    return size;
}

BOOL modem_process(uint8_t ch)
{
    if (GSM_Hardware.atc.timeout_atc_ms)
    {
        GSM_Hardware.atc.recv_buff.Buffer[GSM_Hardware.atc.recv_buff.BufferIndex++] = ch;
        if (GSM_Hardware.atc.recv_buff.BufferIndex >= SMALL_BUFFER_SIZE)
        {
            DEBUG_PRINTF("[%s] Overflow\r\n", __FUNCTION__);
            GSM_Hardware.atc.recv_buff.BufferIndex = 0;
        }

        GSM_Hardware.atc.recv_buff.Buffer[GSM_Hardware.atc.recv_buff.BufferIndex] = 0;
    }
    else
    {
        GSM_Hardware.atc.recv_buff.Buffer[GSM_Hardware.atc.recv_buff.BufferIndex++] = ch;
        GSM_Hardware.atc.recv_buff.Buffer[GSM_Hardware.atc.recv_buff.BufferIndex] = 0;
        if (ch == '\r' || ch == '\n')
        {
            ///* Xu ly du lieu tien +CUSD: 0, "MobiQTKC:20051d,31/07/2016 KM2:50000d KM3:48500d KM1:27800d", 15 */
            //if (strstr((char *)(GSM_Hardware.atc.recv_buff.Buffer), "+CUSD"))
            //{
            //    DEBUG("\r\nTai khoan: %s", strstr((char *)(GSM_Hardware.atc.recv_buff.Buffer), "+CUSD"));
            //    MQTT_SendBufferToServer((char *)GSM_Hardware.atc.recv_buff.Buffer, "CUSD");
            //}

            /* Xu ly ban tin +CME ERROR */
            if (strstr((char *)(GSM_Hardware.atc.recv_buff.Buffer), "+CME ERROR:"))
            {
                CMEErrorCount++;
            }
            if (GSM_Hardware.atc.recv_buff.BufferIndex>2)
            {
                //DEBUG_PRINTF("Unhandled msg %s", GSM_Hardware.atc.recv_buff.Buffer);
            }

            GSM_Hardware.atc.recv_buff.BufferIndex = 0;
            memset(GSM_Hardware.atc.recv_buff.Buffer, 0, SMALL_BUFFER_SIZE);
        }
    }

    return (0);
}

/*****************************************************************************/
/**
 * @brief	:  	Send a command to GSM module
 * @param	:  
 * @retval	:
 * @author	:	
 * @created	:	10/11/2014
 * @version	:
 * @reviewer:	
 */
void gsm_hw_send_at_cmd(char *cmd, char *expect_resp, 
                        char * expected_response_at_the_end_of_response, uint32_t timeout,
                        uint8_t retry_count, gsm_send_at_cb_t callback)
{
    if (timeout == 0 || callback == NULL)
    {
        gsm_hw_uart_send_raw((uint8_t*)cmd, strlen(GSM_Hardware.atc.cmd));
        return;
    }
    

    if (strlen(cmd) < 128)
    {
        DEBUG_PRINTF("ATC: %s", cmd);
    }

    GSM_Hardware.atc.cmd = cmd;
    GSM_Hardware.atc.expect_resp_from_atc = expect_resp;
    GSM_Hardware.atc.expected_response_at_the_end = expected_response_at_the_end_of_response;
    GSM_Hardware.atc.recv_buff.State = BUFFER_STATE_IDLE;
    GSM_Hardware.atc.retry_count_atc = retry_count;
    GSM_Hardware.atc.send_at_callback = callback;
    GSM_Hardware.atc.timeout_atc_ms = timeout;
    GSM_Hardware.atc.current_timeout_atc_ms = sys_get_ms();

    memset(GSM_Hardware.atc.recv_buff.Buffer, 0, SMALL_BUFFER_SIZE);
    GSM_Hardware.atc.recv_buff.BufferIndex = 0;
    GSM_Hardware.atc.recv_buff.State = BUFFER_STATE_IDLE;

    gsm_hw_uart_send_raw((uint8_t*)cmd, strlen(cmd));
}

void gsm_hw_uart_send_raw(uint8_t* raw, uint32_t length)
{

    while (length--)
    {
        gsm_ppp_modem_buffer_t *p = &GSM_Hardware.modem.tx_buffer;

        /* Write a byte to serial interface */
        if ((uint8_t)(p->idx_in + 1) == p->idx_out)
        {
            /* Serial transmit buffer is full. */
            DEBUG_PRINTF("Buffer full\r\n");
            Delayms(5);
            while ((uint8_t)(p->idx_in + 1) == p->idx_out)
            {
                Delayms(1);
            }
            DEBUG_PRINTF("Buffer free\r\n");
        }

        /* Disable ngat USART0 */
        NVIC_DisableIRQ(GSM_UART_IRQ);
        __nop();

        if (GSM_Hardware.modem.tx_active == 0)
        {
            /* Send data to UART. */
            usart_data_transmit(GSM_UART, *raw);
            usart_interrupt_enable(GSM_UART, USART_INT_TBE); /* Enable TXE interrupt */

            GSM_Hardware.modem.tx_active = __TRUE;
        }
        else
        {
            /* Add data to transmit buffer. */
            p->Buffer[p->idx_in++] = *raw;
            if (p->idx_in == MODEM_BUFFER_SIZE)
            {
                DEBUG_PRINTF("[%s] Overflow\r\n", __FUNCTION__);
                p->idx_in = 0;
            }
        }

        raw++;
        /* Enable Ngat USART0 tro lai */
        NVIC_EnableIRQ(GSM_UART_IRQ);
    }
}



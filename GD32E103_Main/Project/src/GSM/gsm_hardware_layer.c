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
#include "Net_Config.h"
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
static uint16_t CMEErrorTimeout = 0;
static __IO bool m_new_uart_data = false;

/******************************************************************************
                                   LOCAL FUNCTIONS					    			 
 ******************************************************************************/

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

#if __USED_HTTP__
/*****************************************************************************/
/**
 * @brief	:  	Init GSM serial
 * @param	:  
 * @retval	:
 * @author	:	
 * @created	:	10/11/2014
 * @version	:
 * @reviewer:	
 */
void gsm_uart_handler(void)
{
    if (usart_interrupt_flag_get(GSM_UART, USART_INT_FLAG_RBNE) != RESET)
    {
        uint8_t ch = usart_data_receive(GSM_UART);
        if (GSM_Hardware.atc.timeout_atc_ms)
        {
            GSM_Hardware.atc.recv_buff.Buffer[GSM_Hardware.atc.recv_buff.BufferIndex++] = ch;
            if (GSM_Hardware.atc.recv_buff.BufferIndex >= SMALL_BUFFER_SIZE)
                GSM_Hardware.atc.recv_buff.BufferIndex = 0;

            GSM_Hardware.atc.recv_buff.Buffer[GSM_Hardware.atc.recv_buff.BufferIndex] = 0;
        }
#if __USED_PPP__
        else
        {
            GSM_Hardware.rx_buffer.Buffer[GSM_Hardware.rx_buffer.idx_in++] = ch;
            if (GSM_Hardware.rx_buffer.idx_in >= MODEM_BUFFER_SIZE)
                GSM_Hardware.rx_buffer.idx_in = 0;
            GSM_Hardware.rx_buffer.Buffer[GSM_Hardware.rx_buffer.idx_in] = 0;
        }
#endif //__USED_PPP__

        usart_interrupt_flag_clear(GSM_UART, USART_INT_FLAG_RBNE);
        m_new_uart_data = true;
    }
}

/*****************************************************************************/
/**
 * @brief	:  	Tick every 10ms
 * @param	:  
 * @retval	:
 * @author	:	
 * @created	:	10/11/2014
 * @version	:
 * @reviewer:	
 */
void gsm_hardware_tick(void)
{
    if (GSM_Hardware.atc.timeout_atc_ms)
    {
        GSM_Hardware.atc.current_timeout_atc_ms++;

        if (GSM_Hardware.atc.current_timeout_atc_ms > GSM_Hardware.atc.timeout_atc_ms)
        {
            GSM_Hardware.atc.current_timeout_atc_ms = 0;

            if (GSM_Hardware.atc.retry_count_atc)
            {
                GSM_Hardware.atc.retry_count_atc--;
            }

            if (GSM_Hardware.atc.retry_count_atc == 0)
            {
                GSM_Hardware.atc.timeout_atc_ms = 0;

                if (GSM_Hardware.atc.send_at_callback != NULL)
                {
                    GSM_Hardware.atc.send_at_callback(EVEN_TIMEOUT, NULL);
                }
            }
            else
            {
                DEBUG("\r\nGui lai lenh AT : %s", GSM_Hardware.atc.cmd);
                UART_Puts(GSM_UART, GSM_Hardware.atc.cmd);
            }
        }

        if (strstr((char *)(GSM_Hardware.atc.recv_buff.Buffer), GSM_Hardware.atc.expect_resp_from_atc))
        {
            GSM_Hardware.atc.timeout_atc_ms = 0;

            if (GSM_Hardware.atc.send_at_callback != NULL)
            {
                GSM_Hardware.atc.send_at_callback(EVEN_OK, GSM_Hardware.atc.recv_buff.Buffer);
            }
        }

        /* A connection has been established; the DCE is moving from command state to online data state */
        if (m_new_uart_data)
        {
            if (strstr((char *)(GSM_Hardware.atc.recv_buff.Buffer), "CONNECT"))
            {
                DEBUG("\r\nModem da ket noi!");
            }
            m_new_uart_data = false;
        }
    }
#if __USED_PPP__
    else
    {
        if (m_new_uart_data)
        {
            /* The connection has been terminated or the attempt to establish a connection failed */
            if (strstr((char *)(GSM_Hardware.rx_buffer.Buffer), "NO CARRIER"))
            {
                printf("\rModem mat ket noi, tu dong ket noi lai");
                memset(GSM_Hardware.rx_buffer.Buffer, 0, MODEM_BUFFER_SIZE);
                if (gsm_manager.PPPPhase != PPP_PHASE_RUNNING)
                {
                    gsm_manager.PPPPhase = PPP_PHASE_DEAD;
                    ppp_close(ppp_control_block, 1);
                }
                else
                {
                    xSystem.GLStatus.LoginReason |= 1;
                    gsm_manager.state = GSM_REOPENPPP;
                    gsm_manager.step = 0;
                }
            }
        }
    }
#endif //__USED_PPP__
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
void SendATCommand(char *cmd, char *expect_resp, uint16_t timeout,
                   uint8_t retry_count, gsm_send_at_cb_t callback)
{
    if (timeout == 0 || callback == NULL)
    {
        UART_Puts(GSM_UART, cmd);
        return;
    }

    GSM_Hardware.atc.cmd = cmd;
    GSM_Hardware.atc.expect_resp_from_atc = expect_resp;
    GSM_Hardware.atc.recv_buff.BufferIndex = 0;
    GSM_Hardware.atc.recv_buff.State = BUFFER_STATE_IDLE;
    GSM_Hardware.atc.retry_count_atc = retry_count;
    GSM_Hardware.atc.send_at_callback = callback;
    GSM_Hardware.atc.timeout_atc_ms = timeout / 10; //gsm_hardware_tick: 10ms /10, 100ms /100
    GSM_Hardware.atc.current_timeout_atc_ms = 0;

    memset(GSM_Hardware.atc.recv_buff.Buffer, 0, SMALL_BUFFER_SIZE);
    GSM_Hardware.atc.recv_buff.BufferIndex = 0;
    GSM_Hardware.atc.recv_buff.State = BUFFER_STATE_IDLE;

    UART_Puts(GSM_UART, cmd);

    DEBUG("\r\nSend AT : %s", cmd);
}

#else //USE_PPP
/*****************************************************************************/
/*						CAC HAM PHUC VU CHO PPP STACK						 */
/*****************************************************************************/

/*****************************************************************************/
/**
 * @brief	:  	Init GSM serial
 * @param	:  
 * @retval	:
 * @author	:	Khanhpd
 * @created	:	10/11/2014
 * @version	:
 * @reviewer:	
 */
void init_serial(void)
{
    GSM_Hardware.modem.rx_buffer.idx_in = 0;
    GSM_Hardware.modem.rx_buffer.idx_out = 0;
    GSM_Hardware.modem.tx_buffer.idx_in = 0;
    GSM_Hardware.modem.tx_buffer.idx_out = 0;
    GSM_Hardware.modem.tx_active = __FALSE;

    /* Enable USART2 interrupts. */
    NVIC_EnableIRQ(GSM_UART_IRQ);
}

/*****************************************************************************/
/**
 * @brief	:  	Write a byte to serial interface in PPP mode
 * @param	:  
 * @retval	:
 * @author	:	Khanhpd
 * @created	:	10/11/2014
 * @version	:
 * @reviewer:	
 */
BOOL com_putchar(uint8_t c)
{
    gms_ppp_modem_buffer_t *p = &GSM_Hardware.modem.tx_buffer;

    if (gsm_manager.Mode == GSM_AT_MODE)
        return __FALSE;

    /* Write a byte to serial interface */
    if ((uint8_t)(p->idx_in + 1) == p->idx_out)
    {
        /* Serial transmit buffer is full. */
        return (__FALSE);
    }

    /* Disable ngat USART2 */
    NVIC_DisableIRQ(GSM_UART_IRQ);
    __nop();

    if (GSM_Hardware.modem.tx_active == __FALSE)
    {
        /* Send data to UART. */
        usart_data_transmit(GSM_UART, (uint32_t)c);
        usart_interrupt_enable(GSM_UART, USART_INT_TBE); /* Enable TBE interrupt */

        GSM_Hardware.modem.tx_active = __TRUE;
    }
    else
    {
        /* Add data to transmit buffer. */
        p->Buffer[p->idx_in++] = c;
        if (p->idx_in == MODEM_BUFFER_SIZE)
            p->idx_in = 0;
    }

    /* Enable Ngat USART2 tro lai */
    NVIC_EnableIRQ(GSM_UART_IRQ);

    return (__TRUE);
}

/*****************************************************************************/
/**
 * @brief	:  	Write a byte to serial interface in AT mode 
 * @param	:  
 * @retval	:
 * @author	:	Khanhpd
 * @created	:	10/11/2014
 * @version	:
 * @reviewer:	
 */
BOOL com_put_at_character(uint8_t c)
{
    gms_ppp_modem_buffer_t *p = &GSM_Hardware.modem.tx_buffer;

    /* Write a byte to serial interface */
    if ((uint8_t)(p->idx_in + 1) == p->idx_out)
    {
        /* Serial transmit buffer is full. */
        DEBUG_PRINTF("Buffer full\r\n");
        Delayms(5);
        while ((uint8_t)(p->idx_in + 1) == p->idx_out)
        {
            
        }
        DEBUG_PRINTF("Buffer free\r\n");
        //return (__FALSE);
    }

    /* Disable ngat USART0 */
    NVIC_DisableIRQ(GSM_UART_IRQ);
    __nop();

    if (GSM_Hardware.modem.tx_active == __FALSE)
    {
        /* Send data to UART. */
        usart_data_transmit(GSM_UART, c);
        usart_interrupt_enable(GSM_UART, USART_INT_TBE); /* Enable TXE interrupt */

        GSM_Hardware.modem.tx_active = __TRUE;
    }
    else
    {
        /* Add data to transmit buffer. */
        p->Buffer[p->idx_in++] = c;
        if (p->idx_in == MODEM_BUFFER_SIZE)
            p->idx_in = 0;
    }

    /* Enable Ngat USART2 tro lai */
    NVIC_EnableIRQ(GSM_UART_IRQ);

    return (__TRUE);
}

/*****************************************************************************/
/**
 * @brief	:  	Write a byte to serial interface
 * @param	:  
 * @retval	:
 * @author	:	Khanhpd
 * @created	:	10/11/2014
 * @version	:
 * @reviewer:	
 */
BOOL com_put_at_string(char *str)
{
    while (*str)
    {
        com_put_at_character(*str++);
    }

    return __TRUE;
}
/*****************************************************************************/
/**
 * @brief	:  	Read a byte from GSM serial interface
 * @param	:  
 * @retval	:
 * @author	:	Khanhpd
 * @created	:	10/11/2014
 * @version	:
 * @reviewer:	
 */
int com_getchar(void)
{
    uint8_t retval;

    /* Read a byte from serial interface */
    gms_ppp_modem_buffer_t *p = &GSM_Hardware.modem.rx_buffer;

    if (p->idx_in == p->idx_out)
    {
        /* Serial receive buffer is empty. */
        return (-1);
    }

    retval = p->Buffer[p->idx_out++];

    if (p->idx_out == MODEM_BUFFER_SIZE)
        p->idx_out = 0;

    return retval;
}

void gsm_uart_handler(void)
{
    /* Serial Rx and Tx interrupt handler. */
    gms_ppp_modem_buffer_t *p;

    if (usart_flag_get(GSM_UART, USART_FLAG_RBNE) == 1) //RBNE = 1
    {
        uint8_t ch = usart_data_receive(GSM_UART);
        gsm_manager.TimeOutConnection = 0; //Reset UART data timeout

        if (GSM_STATE_RESET != gsm_manager.state)
        {
            /* Receive Buffer Not Empty */
            p = &GSM_Hardware.modem.rx_buffer;
            if ((U8)(p->idx_in + 1) != p->idx_out)
            {
                p->Buffer[p->idx_in++] = ch;
                if (p->idx_in == MODEM_BUFFER_SIZE)
                {
                    DEBUG_RAW("%s", p->Buffer);
                    p->idx_in = 0;
                }
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
                p->idx_out = 0;
        }
        else
        {
            usart_interrupt_disable(GSM_UART, USART_INT_TBE); /* Disable TXE interrupt */
            GSM_Hardware.modem.tx_active = __FALSE;
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

/*****************************************************************************/
/**
 * @brief	:  Return status Transmitter active/not active. When transmit buffer is empty, 'tx_active' is FALSE.
 * @param	:  
 * @retval	:
 * @author	:	Khanhpd
 * @created	:	10/11/2014
 * @version	:
 * @reviewer:	
 */
BOOL com_tx_active(void)
{
    return GSM_Hardware.modem.tx_active;
}

/*****************************************************************************/
/**
 * @brief	:  	Initializes the modem variables and control signals DTR & RTS.
 * @param	:  
 * @retval	:
 * @author	:	Khanhpd
 * @created	:	10/11/2014
 * @version	:
 * @reviewer:	
 */
void modem_init()
{
    GSM_Hardware.modem.State = MODEM_IDLE;
}

/*****************************************************************************/
/**
 * @brief	:  	modem dial target number 'dialnum'
 * @param	:  
 * @retval	:
 * @author	:	Khanhpd
 * @created	:	10/11/2014
 * @version	:
 * @reviewer:	
 */
void modem_dial(uint8_t *dialnum)
{
    GSM_Hardware.modem.dial_number = dialnum;
    GSM_Hardware.modem.State = MODEM_DIAL;
}
/*****************************************************************************/
/**
 * @brief	:  	This function puts modem into Answering Mode
 * @param	:  
 * @retval	:
 * @author	:	Khanhpd
 * @created	:	10/11/2014
 * @version	:
 * @reviewer:	
 */
void modem_listen()
{
}

/*****************************************************************************/
/**
 * @brief	:  	This function clears DTR to force the modem to hang up if 
 *				it was on line and/or make the modem to go to command mode.
 * @param	:  
 * @retval	:
 * @author	:	Khanhpd
 * @created	:	10/11/2014
 * @version	:
 * @reviewer:	
 */
void modem_hangup()
{
}
/*****************************************************************************/
/**
 * @brief	:  	Checks if the modem is online. Return false when not
 * @param	:  
 * @retval	:
 * @author	:	Khanhpd
 * @created	:	10/11/2014
 * @version	:
 * @reviewer:	
 */
BOOL modem_online()
{
    if (GSM_Hardware.modem.State == MODEM_ONLINE)
    {
        return __TRUE;
    }
    return __FALSE;
}


void gsm_hw_uart_put_direct(uint8_t *data, uint32_t length)
{
    
}

/*****************************************************************************/
/**
 * @brief	: This is a main thread for MODEM Control module. It is called on every 
 *				system timer timer tick to implement delays easy (called by TCPNet system)
 *				By default this is every 100ms. The 'sytem tick' timeout is set in 'Net_Config.c'
 * @param	:  
 * @retval	:
 * @author	:	Khanhpd
 * @created	:	10/11/2014
 * @version	:
 * @reviewer:	
 */
void modem_run()
{
    if (GSM_Hardware.atc.timeout_atc_ms)
    {
        GSM_Hardware.atc.current_timeout_atc_ms++;

        if (GSM_Hardware.atc.current_timeout_atc_ms > GSM_Hardware.atc.timeout_atc_ms)
        {
            GSM_Hardware.atc.current_timeout_atc_ms = 0;

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
            }
            else
            {
                DEBUG("Resend ATC: %s\r\n", GSM_Hardware.atc.cmd);
                com_put_at_string(GSM_Hardware.atc.cmd);
            }
        }

        if (strstr((char *)(GSM_Hardware.atc.recv_buff.Buffer), GSM_Hardware.atc.expect_resp_from_atc))
        {
#if 0
            GSM_Hardware.atc.timeout_atc_ms = 0;

            if (GSM_Hardware.atc.send_at_callback != NULL)
            {
                GSM_Hardware.atc.send_at_callback(GSM_EVENT_OK, GSM_Hardware.atc.recv_buff.Buffer);
            }
#endif

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
                GSM_Hardware.atc.timeout_atc_ms = 0;
                if (GSM_Hardware.atc.send_at_callback)
                {
                    GSM_Hardware.atc.send_at_callback(GSM_EVENT_OK, GSM_Hardware.atc.recv_buff.Buffer);
                }
            }
        }
        else 
        {
            char *p = strstr((char *)(GSM_Hardware.atc.recv_buff.Buffer), "CME");
            if (p && strstr(p, "\r\n"))
            {
                DEBUG_PRINTF("%s", p);
                GSM_Hardware.atc.timeout_atc_ms = 0;
            }
        }
    }

#if 1
    /* Xu ly time out +CME ERROR */
    if (CMEErrorCount > 0)
    {
        CMEErrorTimeout++;
        if (CMEErrorTimeout > 350) /* Trong vong 35s xuat hien >= 10 lan +CME ERROR */
        {
            CMEErrorTimeout = 0;
            if (CMEErrorCount >= 10)
            {
                DEBUG("+CME ERROR too much!\r\n");
                gsm_manager.state = GSM_STATE_RESET;
                gsm_manager.step = 0;
            }
            CMEErrorCount = 0;
        }
    }
#endif
}

void gsm_hw_clear_at_serial_rx_buffer(void)
{
    memset(GSM_Hardware.atc.recv_buff.Buffer, 0, sizeof(((gsm_hardware_t*)0)->atc.recv_buff.Buffer));
    GSM_Hardware.atc.recv_buff.BufferIndex = 0;
    GSM_Hardware.atc.recv_buff.State = BUFFER_STATE_IDLE;
}

void gsm_hw_clear_non_at_serial_rx_buffer(void)
{
    memset(GSM_Hardware.modem.rx_buffer.Buffer, 0, sizeof(((gsm_hardware_t*)0)->modem.rx_buffer.Buffer));
    GSM_Hardware.modem.rx_buffer.idx_in = 0;
    GSM_Hardware.modem.rx_buffer.idx_out = 0;
}

uint32_t gsm_hw_serial_at_cmd_rx_buffer_size(void)
{
    return sizeof(((gsm_hardware_t*)0)->modem.rx_buffer.Buffer);
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

/*****************************************************************************/
/**
 * @brief	:  	modem character process event handler. This function is called when
 *				a new character has been received from the modem in command mode 
 * @param	:  
 * @retval	:
 * @author	:	
 * @created	:	10/11/2014
 * @version	:
 * @reviewer:	
 */
BOOL modem_process(uint8_t ch)
{
    if (GSM_Hardware.atc.timeout_atc_ms)
    {
        GSM_Hardware.atc.recv_buff.Buffer[GSM_Hardware.atc.recv_buff.BufferIndex++] = ch;
        GSM_Hardware.atc.recv_buff.Buffer[GSM_Hardware.atc.recv_buff.BufferIndex] = 0;
    }
    else
    {
#if __HOPQUY_GSM__
        DEBUG_PRINTF("%c", ch); //DEBUG GSM : Hien thi noi dung gui ra tu module GSM
#endif /* __HOPQUY_GSM__ */

        GSM_Hardware.atc.recv_buff.Buffer[GSM_Hardware.atc.recv_buff.BufferIndex++] = ch;
        if (ch == '\r' || ch == '\n')
        {
            ///* Xu ly du lieu tien +CUSD: 0, "MobiQTKC:20051d,31/07/2016 KM2:50000d KM3:48500d KM1:27800d", 15 */
            //if (strstr((char *)(GSM_Hardware.atc.recv_buff.Buffer), "+CUSD"))
            //{
            //    DEBUG("\r\nTai khoan: %s", strstr((char *)(GSM_Hardware.atc.recv_buff.Buffer), "+CUSD"));
            //    MQTT_SendBufferToServer((char *)GSM_Hardware.atc.recv_buff.Buffer, "CUSD");
            //}

            ///* Xu ly ban tin +CME ERROR */
            //if (strstr((char *)(GSM_Hardware.atc.recv_buff.Buffer), "+CME ERROR:"))
            //{
            //    CMEErrorCount++;
            //}
            if (GSM_Hardware.atc.recv_buff.BufferIndex>2)
            {
                DEBUG_PRINTF("Unhandled msg %s", GSM_Hardware.atc.recv_buff.Buffer);
            }

            GSM_Hardware.atc.recv_buff.BufferIndex = 0;
            memset(GSM_Hardware.atc.recv_buff.Buffer, 0, SMALL_BUFFER_SIZE);
        }
    }

#if GSM_PPP_ENABLE
    /* A connection has been established; the DCE is moving from command state to online data state */
    if (strstr((char *)(GSM_Hardware.atc.recv_buff.Buffer), "CONNECT"))
    {
        GSM_Hardware.modem.State = MODEM_ONLINE;
        gsm_manager.GSMReady = 1;
        return (__TRUE);
    }


    /* The connection has been terminated or the attempt to establish a connection failed */
    if (strstr((char *)(GSM_Hardware.atc.recv_buff.Buffer), "NO CARRIER"))
    {
        memset(GSM_Hardware.atc.recv_buff.Buffer, 0, 20);

        /* Khi goto sleep -> NOCARRIER -> Khong mo lai PPP nua de vao SLEEP mode */
        if (gsm_manager.state != GSM_STATE_SLEEP 
            && gsm_manager.state != GSM_STATE_GOTO_SLEEP 
            && gsm_manager.state != GSM_STATE_READ_SMS)
        {
            DEBUG("\r\nModem disconnected, auto reconnect\r\n");
            gsm_manager.state = GSM_STATE_REOPEN_PPP;
            gsm_manager.step = 0;
            gsm_manager.GSMReady = 0;
        }
        else
        {
            DEBUG("\r\nModem disconnect because of go to sleep mode\r\n");
        }
    }
#endif

    return (__FALSE);
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
        com_put_at_string(cmd);
        return;
    }
    

    if (strlen(cmd) < 128)
    {
        DEBUG_PRINTF("%s", cmd);
    }

    GSM_Hardware.atc.cmd = cmd;
    GSM_Hardware.atc.expect_resp_from_atc = expect_resp;
    GSM_Hardware.atc.expected_response_at_the_end = expected_response_at_the_end_of_response;
    GSM_Hardware.atc.recv_buff.BufferIndex = 0;
    GSM_Hardware.atc.recv_buff.State = BUFFER_STATE_IDLE;
    GSM_Hardware.atc.retry_count_atc = retry_count;
    GSM_Hardware.atc.send_at_callback = callback;
    GSM_Hardware.atc.timeout_atc_ms = timeout / 100;
    GSM_Hardware.atc.current_timeout_atc_ms = 0;

    mem_set(GSM_Hardware.atc.recv_buff.Buffer, 0, SMALL_BUFFER_SIZE);
    GSM_Hardware.atc.recv_buff.BufferIndex = 0;
    GSM_Hardware.atc.recv_buff.State = BUFFER_STATE_IDLE;

    com_put_at_string(cmd);

    //	DEBUG ("ATC: %s\r\n",cmd);
}
#endif //__USED_HTTP__

void gsm_hw_uart_send_raw(char* raw)
{
    com_put_at_string(raw);
}

/********************************* END OF FILE *******************************/

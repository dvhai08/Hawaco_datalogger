/******************************************************************************
 * @file    	DriverUART.c
 * @author  	Phinht
 * @version 	V1.0.0
 * @date    	02/03/2016
 * @brief   	
 ******************************************************************************/

/******************************************************************************
                                   INCLUDES					    			 
 ******************************************************************************/
#ifdef GD32E10X
#include "gd32e10x.h"
#include "gd32e10x_gpio.h"
#include "gd32e10x_usart.h"
#else
#include "stm32l0xx_hal.h"
#endif

#include "hardware.h"
#include <stdarg.h>
#include "driver_uart.h"
#include "lwrb.h"
#include "main.h"

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

/******************************************************************************
                                   LOCAL FUNCTIONS					    			 
 ******************************************************************************/

/*****************************************************************************/
/*!
 * @brief	:  
 * @param	:  
 * @retval	:
 * @author	:	Phinht
 * @created	:	28/02/2016
 * @version	:
 * @reviewer:	
 */

//static lwrb_t m_ringbuff_tx_usart0;
//static uint8_t m_tx_buffer_usart0[256];
//static lwrb_t m_ringbuff_tx_usart1;
//static uint8_t m_tx_buffer_usart1[256];
static bool m_gsm_uart_started = false;
static bool m_rs485_uart_started = false;
void driver_uart_initialize(uint32_t USARTx, uint32_t baudrate)
{
    bool found_gsm_uart = false;
    if (USARTx == DRIVER_UART_PORT_GSM)
    {
        if (m_gsm_uart_started == false)
        {
            m_gsm_uart_started = true;
            nvic_irq_enable(GSM_UART_IRQ, 0, 0);

            /* enable COM GPIO clock */
            //			rcu_periph_clock_enable(GSM_UART_GPIO);

            /* enable USART clock */
            rcu_periph_clock_enable(GSM_UART_CLK);

            /* connect port to USARTx_Tx */
            gpio_init(GSM_UART_GPIO, GPIO_MODE_AF_PP, GPIO_OSPEED_50MHZ, GSM_TX_PIN);

            /* connect port to USARTx_Rx */
            gpio_init(GSM_UART_GPIO, GPIO_MODE_IN_FLOATING, GPIO_OSPEED_50MHZ, GSM_RX_PIN);
            found_gsm_uart = true;
        }
    }
#if __USE_SENSOR_UART__
    else if (USARTx == DEBUG_UART)
    {
        nvic_irq_enable(DEBUG_UART_IRQ, 1, 0);

        /* enable COM GPIO clock */
        rcu_periph_clock_enable(DEBUG_UART_GPIO);

        /* enable USART clock */
        rcu_periph_clock_enable(DEBUG_UART_CLK);

        /* connect port to USARTx_Tx */
        gpio_init(DEBUG_UART_GPIO, GPIO_MODE_AF_PP, GPIO_OSPEED_10MHZ, DEBUG_TX_PIN);

        /* connect port to USARTx_Rx */
        gpio_init(DEBUG_UART_GPIO, GPIO_MODE_IN_FLOATING, GPIO_OSPEED_10MHZ, DEBUG_RX_PIN);
    }
#endif //__USE_SENSOR_UART__
    else if (USARTx == DRIVER_UART_PORT_RS485)
    {
        if (m_rs485_uart_started == false)
        {
            m_rs485_uart_started = true;
            nvic_irq_enable(RS485_UART_IRQ, 1, 1);

            /* enable COM GPIO clock */
            //			rcu_periph_clock_enable(RS485_UART_GPIO);

            /* enable USART clock */
            rcu_periph_clock_enable(RS485_UART_CLK);

            /* connect port to USARTx_Tx */
            gpio_init(RS485_UART_GPIO, GPIO_MODE_AF_PP, GPIO_OSPEED_10MHZ, RS485_TX_PIN);

            /* connect port to USARTx_Rx */
            gpio_init(RS485_UART_GPIO, GPIO_MODE_IN_FLOATING, GPIO_OSPEED_10MHZ, RS485_RX_PIN);
            found_gsm_uart = true;
        }
    }

    if (found_gsm_uart)
    {
        /* USART configure */
        usart_deinit(USARTx);
        usart_word_length_set(USARTx, USART_WL_8BIT);
        usart_stop_bit_set(USARTx, USART_STB_1BIT);
        usart_parity_config(USARTx, USART_PM_NONE);
        usart_baudrate_set(USARTx, baudrate);
        usart_receive_config(USARTx, USART_RECEIVE_ENABLE);
        usart_transmit_config(USARTx, USART_TRANSMIT_ENABLE);

        usart_interrupt_enable(USARTx, USART_INT_RBNE);
        //	  usart_interrupt_enable(USARTx, USART_INT_TBE);

        //Clear interrupt flag
        usart_interrupt_flag_clear(GSM_UART, USART_INT_FLAG_RBNE);
        usart_interrupt_flag_clear(GSM_UART, USART_INT_FLAG_TBE);

        usart_enable(USARTx);
    }
}

void driver_uart_deinitialize(uint32_t USARTx)
{
    if (m_rs485_uart_started && USARTx == RS485_UART)
    {
        m_rs485_uart_started = false;
        nvic_irq_disable(RS485_UART_IRQ);
        usart_disable(RS485_UART_IRQ);
        usart_deinit(RS485_UART_IRQ);
        rcu_periph_clock_disable(RS485_UART_CLK);
    }
    else if (m_gsm_uart_started && USARTx == GSM_UART)
    {
        m_gsm_uart_started = false;
        nvic_irq_disable(GSM_UART_IRQ);
        usart_disable(GSM_UART);
        usart_deinit(GSM_UART);
        rcu_periph_clock_disable(GSM_UART_CLK);
    }
}

//size_t driver_uart0_get_new_data_to_send(uint8_t *c)
//{
//    return lwrb_read(&m_ringbuff_tx_usart0, c, 1);
//}

//size_t driver_uart1_get_new_data_to_send(uint8_t *c)
//{
//    return lwrb_read(&m_ringbuff_tx_usart1, c, 1);
//}

// /*****************************************************************************/
// /**
//  * @brief	:  
//  * @param	:  
//  * @retval	:
//  * @author	:	Phinht
//  * @created	:	05/09/2015
//  * @version	:
//  * @reviewer:	
//  */
// void UART_Putc(uint32_t USARTx, uint8_t c)
// {
// #if 1
//     //    while (USART_GetFlagStatus(USARTx, USART_FLAG_TXE) == RESET);
//     //    USART_SendData(USARTx, (uint16_t)c);
//     while (RESET == usart_flag_get(USARTx, USART_FLAG_TBE));
//     usart_data_transmit((uint32_t)USART1, c);
// #else
//     if (USARTx == 0)
//     {
//         while (!lwrb_get_free(&m_ringbuff_tx_usart0))
//         {
//         }
//         uint32_t tmp = USART_CTL0(USART0);
//         if (!(tmp & USART_CTL0_TBEIE))
//         {
//             usart_interrupt_enable(USART0, USART_INT_TBE);
//         }
//         lwrb_write(&m_ringbuff_tx_usart0, &c, 1);

//         //        usart_data_transmit((uint32_t)USART0, c);
//         //        while (RESET == usart_flag_get((uint32_t)USART0, USART_FLAG_TBE));
//     }
//     else if (USARTx == 1)
//     {
//         //        usart_data_transmit((uint32_t)USART1, c);
//         //        while (RESET == usart_flag_get((uint32_t)USART1, USART_FLAG_TBE));
//         while (!lwrb_get_free(&m_ringbuff_tx_usart1))
//         {
//         }
//         uint32_t tmp = USART_CTL0(USART1);
//         if (!(tmp & USART_CTL0_TBEIE))
//         {
//             usart_interrupt_enable(USART1, USART_INT_TBE);
//         }
//         lwrb_write(&m_ringbuff_tx_usart1, &c, 1);
//     }
// #endif
// }

// /*****************************************************************************/
// /**
//  * @brief	:  
//  * @param	:  
//  * @retval	:
//  * @author	:	Phinht
//  * @created	:	05/09/2015
//  * @version	:
//  * @reviewer:	
//  */
// void UART_Puts(uint32_t USARTx, const char *str)
// {
//     //    DEBUG_PRINTF("UART %d send %s\r\n", (int)USARTx, str);
//     while (*str != '\0')
//     {
//         UART_Putc(USARTx, (uint16_t)(*str));
//         str++;
//     }
// }

// /*****************************************************************************/
// /**
//  * @brief	:  
//  * @param	:  
//  * @retval	:
//  * @author	:	Phinht
//  * @created	:	10/11/2014
//  * @version	:
//  * @reviewer:	
//  */
// void UART_Putb(uint32_t USARTx, uint8_t *Data, uint16_t Length)
// {
//     uint16_t i;

//     for (i = 0; i < Length; i++)
//     {
//         UART_Putc(USARTx, Data[i]);
//     }
// }

// /*****************************************************************************/
// /**
//  * @brief	:  
//  * @param	:  
//  * @retval	:
//  * @author	:	Phinht
//  * @created	:	10/11/2014
//  * @version	:
//  * @reviewer:	
//  */
// void UART_itoa(uint32_t USARTx, long val, int radix, int len)
// {
//     BYTE c, r, sgn = 0, pad = ' ';
//     BYTE s[20], i = 0;
//     DWORD v;

//     if (radix < 0)
//     {
//         radix = -radix;
//         if (val < 0)
//         {
//             val = -val;
//             sgn = '-';
//         }
//     }
//     v = val;
//     r = radix;
//     if (len < 0)
//     {
//         len = -len;
//         pad = '0';
//     }
//     if (len > 20)
//         return;
//     do
//     {
//         c = (BYTE)(v % r);
//         if (c >= 10)
//             c += 7;
//         c += '0';
//         s[i++] = c;
//         v /= r;
//     } while (v);
//     if (sgn)
//         s[i++] = sgn;
//     while (i < len)
//         s[i++] = pad;
//     do
//         UART_Putc(USARTx, s[--i]);
//     while (i);
// }

// /*****************************************************************************/
// /**
//  * @brief	:  
//  * @param	:  
//  * @retval	:
//  * @author	:	Phinht
//  * @created	:	10/11/2014
//  * @version	:
//  * @reviewer:	
//  */
// void UART_Printf(uint32_t USARTx, const char *str, ...)
// {
//     va_list arp;
//     int d, r, w, s, l, i;

//     // Check if only string
//     for (i = 0;; i++)
//     {
//         if (str[i] == '%')
//             break;
//         if (str[i] == 0)
//         {
//             UART_Puts(USARTx, str);
//             return;
//         }
//     }

//     va_start(arp, str);

//     while ((d = *str++) != 0)
//     {
//         if (d != '%')
//         {
//             UART_Putc(USARTx, d);
//             continue;
//         }
//         d = *str++;
//         w = r = s = l = 0;
//         if (d == '0')
//         {
//             d = *str++;
//             s = 1;
//         }
//         while ((d >= '0') && (d <= '9'))
//         {
//             w += w * 10 + (d - '0');
//             d = *str++;
//         }
//         if (s)
//             w = -w;
//         if (d == 'l')
//         {
//             l = 1;
//             d = *str++;
//         }
//         if (!d)
//             break;
//         if (d == 's')
//         {
//             UART_Puts(USARTx, va_arg(arp, char *));
//             continue;
//         }
//         if (d == 'c')
//         {
//             UART_Putc(USARTx, (char)va_arg(arp, int));
//             continue;
//         }
//         if (d == 'u')
//             r = 10;
//         if (d == 'd')
//             r = -10;
//         if (d == 'X' || d == 'x')
//             r = 16; // 'x' added by mthomas in increase compatibility
//         if (d == 'b')
//             r = 2;
//         if (!r)
//             break;
//         if (l)
//         {
//             UART_itoa(USARTx, (long)va_arg(arp, long), r, w);
//         }
//         else
//         {
//             if (r > 0)
//                 UART_itoa(USARTx, (unsigned long)va_arg(arp, int), r, w);
//             else
//                 UART_itoa(USARTx, (long)va_arg(arp, int), r, w);
//         }
//     }

//     va_end(arp);
// }

void driver_uart_control_all_uart_port(bool enable)
{
    if (!enable)
    {
#if 0
		USART_Cmd(GSM_UART, DISABLE);
		USART_Cmd(DEBUG_UART, DISABLE);
#else
        usart_disable(GSM_UART);
        usart_disable(RS485_UART);
#endif
    }
    else
    {
#if 0
		USART_Cmd(GSM_UART, ENABLE);
		USART_Cmd(DEBUG_UART, ENABLE);
#else
        usart_enable(GSM_UART);
        usart_enable(RS485_UART);
#endif
    }
}

/********************************* END OF FILE *******************************/

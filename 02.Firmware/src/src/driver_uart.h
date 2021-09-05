#ifndef __DRIVER_UART_H__
#define __DRIVER_UART_H__

#ifndef STM32L083xx
#include "gd32e10x.h"
#include "gd32e10x_usart.h"
#else
#include "stm32l0xx_hal.h"
#endif
#include <stdint.h>
#include "stdbool.h"

#define DRIVER_UART_PORT_GSM	GSM_UART
#define DRIVER_UART_PORT_RS485

/*!
 * @brief       Init uart port
 * @param[in]   USARTx uart port
 * @param[in]   baudrate uart baudate
 */
void driver_uart_initialize(uint32_t USARTx, uint32_t baudrate);

void driver_uart_deinitialize(uint32_t USARTx);
// void UART_Putc(uint32_t USARTx, uint8_t c);
// void UART_Puts(uint32_t USARTx, const char *str);
// void UART_Putb(uint32_t USARTx, uint8_t *Data, uint16_t Length);
// void UART_Printf(uint32_t USARTx, const char* str, ...);

/*!
 * @brief       Enable/disable all uart port for low power application
 * @param[in]   enable TRUE : Enable all uart port
 *                     FALSE : Disable all uart port 
 */
void driver_uart_control_all_uart_port(bool enable);

#endif // __DRIVER_UART_H__


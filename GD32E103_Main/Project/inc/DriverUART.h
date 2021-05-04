#ifndef __DRIVER_UART_H__
#define __DRIVER_UART_H__

#include "gd32e10x.h"
#include "gd32e10x_usart.h"

void UART_Init(uint32_t USARTx, uint32_t BaudRate);
void UART_Putc(uint32_t USARTx, uint8_t c);
void UART_Puts(uint32_t USARTx, const char *str);
void UART_Putb(uint32_t USARTx, uint8_t *Data, uint16_t Length);
void UART_Printf(uint32_t USARTx, const char* str, ...);

void ControlAllUART(uint8_t Ctrl);

#endif // __DRIVER_UART_H__


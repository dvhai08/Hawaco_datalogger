#ifndef __UART_HANDLER_H__
#define __UART_HANDLER_H__

#include "hardware.h"

void InitUART(USART_TypeDef* USARTx, uint32_t BaudRate);
void UART_Putc(USART_TypeDef* USARTx, uint8_t c);
void UART_Puts(USART_TypeDef* USARTx, const char *str);
uint16_t UART_Putb(USART_TypeDef* USARTx, uint8_t *Data, uint16_t Length);

void UART_Printf(USART_TypeDef* USARTx, char* str, ...);
void ZIG_Prints(char* name, char* value);

void ControlAllUART(uint8_t Ctrl);

#endif // __UART_HANDLER_H__


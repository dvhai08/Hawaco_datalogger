#ifndef APP_DEBUG_H
#define APP_DEBUG_H

#include <stdint.h>
#include <stdio.h>

#if 1

#include "SEGGER_RTT.h"
#define DEBUG_RTT           1
#define DEBUG_UART           0

extern uint32_t sys_get_ms(void);
extern void app_debug_uart_print(uint8_t *data, uint32_t len);

#define DEBUG_RAW        debug_raw
//#define DEBUG_PRINTF(String...) 	{ SEGGER_RTT_printf(0, "[%d] ", systime_get_ms()); SEGGER_RTT_printf(0, String);}
#define DEBUG_PRINTF 	debug_printf

static inline int debug_raw(const char *fmt, ...)
{
    int     n;
    char    tmp_buffer[SEGGER_RTT_PRINTF_BUFFER_SIZE];
    char *p = &tmp_buffer[0];
    int size = SEGGER_RTT_PRINTF_BUFFER_SIZE;
    va_list args;

    va_start (args, fmt);
    n = vsnprintf(p, size, fmt, args);
    if (n > (int)size) 
    {
#if DEBUG_RTT
        SEGGER_RTT_Write(0, tmp_buffer, size);
#endif
#if DEBUG_UART
        app_debug_uart_print((uint8_t*)tmp_buffer, size);
#endif
    } 
    else if (n > 0) 
    {
#if DEBUG_RTT
        SEGGER_RTT_Write(0, tmp_buffer, n);
#endif
#if DEBUG_UART
        app_debug_uart_print((uint8_t*)tmp_buffer, n);
#endif
    }
    va_end(args);
    return n;
}

static inline int debug_printf(const char *fmt, ...)
{
    int     n;
    char    tmp_buffer[SEGGER_RTT_PRINTF_BUFFER_SIZE];
    char *p = &tmp_buffer[0];
    int size = SEGGER_RTT_PRINTF_BUFFER_SIZE;
    int time_stamp_size;

    p += sprintf(tmp_buffer, "<%u>: ", sys_get_ms());
    time_stamp_size = (p-tmp_buffer);
    size -= time_stamp_size;
    va_list args;

    va_start (args, fmt);
    n = vsnprintf(p, size, fmt, args);
    if (n > (int)size) 
    {
#if DEBUG_RTT
        SEGGER_RTT_Write(0, tmp_buffer, size + time_stamp_size);
#endif
#if DEBUG_UART
        app_debug_uart_print((uint8_t*)tmp_buffer, size + time_stamp_size);
#endif
    } 
    else if (n > 0) 
    {
#if DEBUG_RTT
        SEGGER_RTT_Write(0, tmp_buffer, n + time_stamp_size);
#endif
#if DEBUG_UART
        app_debug_uart_print((uint8_t*)tmp_buffer, n + time_stamp_size);
#endif
    }
    va_end(args);
    return n;
}

#else
    #define DEBUG_PRINTF    printf
#endif


#endif // !APP_DEBUG_H

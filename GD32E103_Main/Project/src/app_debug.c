#include "app_debug.h"
#include "stdarg.h"
#include "stdio.h"
#include "main.h"
#include "gd32e10x.h"

#define RTT_PRINTF_BUFFER_SIZE 128
#define BLE_PRINTF_MTU_SIZE 23
static char m_debug_buffer[RTT_PRINTF_BUFFER_SIZE];

int app_debug_rtt(const char *fmt,...)
{
    // Get debug data
    //if (!(CoreDebug->DHCSR & CoreDebug_DHCSR_C_DEBUGEN_Msk))
    //{
    //    return 0;
    //}

    int     n;

    char *p = &m_debug_buffer[0];
    int size = RTT_PRINTF_BUFFER_SIZE;
    int time_stamp_size;

    p += sprintf(m_debug_buffer, "<%d>: ", sys_get_ms());
    time_stamp_size = (p-m_debug_buffer);
    size -= time_stamp_size;
    va_list args;

    va_start (args, fmt);
    n = vsnprintf(p, size, fmt, args);
    if (n > (int)size) 
    {
        SEGGER_RTT_Write(0, m_debug_buffer, size + time_stamp_size);
    } 
    else if (n > 0) 
    {
        SEGGER_RTT_Write(0, m_debug_buffer, n + time_stamp_size);
    }
    va_end(args);
    return n;
}

#ifndef APP_DEBUG_H
#define APP_DEBUG_H

#include <stdint.h>
#include <stdio.h>

#include "SEGGER_RTT.h"

//#define DEBUG_PRINTF            

#define DEBUG_PRINTF            app_debug_rtt
#ifndef DEBUG_PRINTF
#define DEBUG_PRINTF(String...)	SEGGER_RTT_printf(0, String)
#endif

#ifndef DEBUG_FLUSH
#define DEBUG_FLUSH()      while(0)
#endif

int app_debug_rtt(const char *fmt,...);


#endif // !APP_DEBUG_H




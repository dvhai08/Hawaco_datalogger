#ifndef DEBUG_H
#define DEBUG_H

#include <stdint.h>
#include <stdio.h>

#include "SEGGER_RTT.h"

#define KNRM  "\x1B[0m"
#define KRED  "\x1B[31m"
#define KGRN  "\x1B[32m"
#define KYEL  "\x1B[33m"
#define KBLU  "\x1B[34m"
#define KMAG  "\x1B[35m"
#define KCYN  "\x1B[36m"
#define KWHT  "\x1B[37m"

#define DEBUG_RTT               1
#define DEBUG_RAW               app_debug_rtt_raw


#define DEBUG_DUMP                           app_debug_dump
#define DEBUG_INFO(s, args...)               debug_print(KGRN "[I] %s " s KNRM, __FILE__, ##args)
#define DEBUG_ERROR(s, args...)              debug_print(KRED "[E] %s " s KNRM, __FILE__, ##args)
#define DEBUG_WARN(s, args...)               debug_print(KYEL "[W] %s " s KNRM, __FILE__, ##args)
#define DEBUG_COLOR(color, s, args...)       debug_print(color s KNRM, ##args)


#define DEBUG_PRINTF            app_debug_rtt
#ifndef DEBUG_PRINTF
#define DEBUG_PRINTF(String...)	SEGGER_RTT_printf(0, String)
#endif

#ifndef DEBUG_FLUSH
#define DEBUG_FLUSH()      while(0)
#endif

int app_debug_rtt(const char *fmt,...);

int app_debug_rtt_raw(const char *fmt,...);

void app_debug_dump(const void* data, int len, const char* string, ...);

#endif // !DEBUG_H




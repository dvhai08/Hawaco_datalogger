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
#define DEBUG_INFO(s, args...)               app_debug_rtt_raw(KGRN "[I] %s %u " s KNRM, __FILE__, sys_get_ms(), ##args)
#define DEBUG_ERROR(s, args...)              app_debug_rtt_raw(KRED "[E] %s %u " s KNRM, __FILE__, sys_get_ms(), ##args)
#define DEBUG_WARN(s, args...)               app_debug_rtt_raw(KYEL "[W] %s %u " s KNRM, __FILE__, sys_get_ms(), ##args)
#define DEBUG_COLOR(color, s, args...)       app_debug_rtt_raw(color s KNRM, ##args)


#define DEBUG_PRINTF            app_debug_rtt
#ifndef DEBUG_PRINTF
#define DEBUG_PRINTF(String...)	SEGGER_RTT_printf(0, String)
#endif

#ifndef DEBUG_FLUSH
#define DEBUG_FLUSH()      while(0)
#endif

extern uint32_t sys_get_ms(void);

int app_debug_rtt(const char *fmt,...);

int app_debug_rtt_raw(const char *fmt,...);

void app_debug_dump(const void* data, int len, const char* string, ...);

#endif // !DEBUG_H




#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>

#if 1

#include "app_debug.h"
#include "board_hw.h"
#include "app_flash.h"

#define PRINTF_OVER_RTT             DEBUG_RTT
#define PRINTF_OVER_UART            (!PRINTF_OVER_RTT)


#include "app_cli.h"
#include "app_shell.h"
#if PRINTF_OVER_RTT
#include "SEGGER_RTT.h"
#endif
#include "DataDefine.h"

#if PRINTF_OVER_RTT
int rtt_custom_printf(const char *format, ...)
{
    int r;
    va_list ParamList;

    va_start(ParamList, format);
    r = SEGGER_RTT_vprintf(0, format, &ParamList);
    va_end(ParamList);
    
    return r;
}
#else
#define rtt_custom_printf           DEBUG_RAW
#endif

static shell_context_struct m_user_context;
static int32_t cli_reset_system(p_shell_context_t context, int32_t argc, char **argv);
static int32_t cli_trigger_fault(p_shell_context_t context, int32_t argc, char **argv);
static int32_t cli_sleep(p_shell_context_t context, int32_t argc, char **argv);

static const shell_command_context_t cli_command_table[] = 
{
    {"reset",           "\treset: reset system\r\n",                            cli_reset_system,                           0},   
    {"fault",           "\tfault : Trigger fault\r\n",                          cli_trigger_fault,                          1},
    {"sleep",           "\tsleep :enter/exit sleep\r\n",                        cli_sleep,                                  1},
};

void app_cli_puts(uint8_t *buf, uint32_t len)
{
#if PRINTF_OVER_RTT
    SEGGER_RTT_Write(0, buf, len);
#else
    extern void app_debug_uart_print(uint8_t *data, uint32_t len);
    app_debug_uart_print(buf, len);
#endif
}

void app_cli_poll()
{
    app_shell_task();
}


extern uint8_t get_debug_rx_data(void);
void app_cli_gets(uint8_t *buf, uint32_t len)
{
#if PRINTF_OVER_RTT
    for (uint32_t i = 0; i < len; i++)
    {
        buf[i] = 0xFF;
    }
        
    if (!SEGGER_RTT_HASDATA(0))
    {
        return;
    }
    
    int read = SEGGER_RTT_Read(0, buf, len);
    if (read > 0 && read < len)
    {
        for (uint32_t i = read; i < len; i++)
        {
            buf[i] = 0xFF;
        }
    }
#else
	for (uint32_t i = 0; i < len; i++)
	{
		buf[i] = get_debug_rx_data();
	}
#endif
}


void app_cli_start()
{
    app_shell_set_context(&m_user_context);
    app_shell_init(&m_user_context,
                   app_cli_puts,
                   app_cli_gets,
                   rtt_custom_printf,
                   ">",
                   false);

    /* Register CLI commands */
    for (int i = 0; i < sizeof(cli_command_table) / sizeof(shell_command_context_t); i++)
    {
        app_shell_register_cmd(&cli_command_table[i]);
    }

    /* Run CLI task */
    app_shell_task();
}

/* Reset System */
static int32_t cli_reset_system(p_shell_context_t context, int32_t argc, char **argv)
{
    DEBUG_PRINTF("System reset\r\n");
    board_hw_reset();
    return 0;
}


static int32_t cli_trigger_fault(p_shell_context_t context, int32_t argc, char **argv)
{
    if (strstr(argv[1], "wdt"))
    {
        while (1);
    }
    else if (strstr(argv[1], "zero"))
    {
        DEBUG_PRINTF("Div by zero\r\n");
        volatile int x = 1;
        x--;
        x = 0;
        volatile int z = (1/x);
        z++;
    }
    return 0;
}

static int32_t cli_sleep(p_shell_context_t context, int32_t argc, char **argv)
{
    extern System_t xSystem;
    extern bool gsm_data_layer_is_module_sleeping();
    if (strstr(argv[1], "enter"))
    {
        
    }
    else if (strstr(argv[1], "exit"))
    {
        if (gsm_data_layer_is_module_sleeping())
        {
            xSystem.Status.GSMSleepTime = xSystem.Parameters.TGGTDinhKy * 60;
        }
    }
    return 0;
}


#endif /* APP_CLI_ENABLE */

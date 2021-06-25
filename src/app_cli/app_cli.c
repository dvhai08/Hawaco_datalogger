#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>

#if 1

#include "app_debug.h"
//#include "board_hw.h"
//#include "app_flash.h"
#include "app_eeprom.h"

#define PRINTF_OVER_RTT             DEBUG_RTT
#define PRINTF_OVER_UART            (!PRINTF_OVER_RTT)


#include "app_cli.h"
#include "app_shell.h"
#if PRINTF_OVER_RTT
#include "SEGGER_RTT.h"
#endif
#include "gsm.h"
#include "app_eeprom.h"
#include "control_output.h"
#include "sys_ctx.h"

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
static int32_t cli_send_sms(p_shell_context_t context, int32_t argc, char **argv);
static int32_t cli_ota_update(p_shell_context_t context, int32_t argc, char **argv);
static int32_t cli_output_4_20ma(p_shell_context_t context, int32_t argc, char **argv);
static int32_t cli_enter_test_mode(p_shell_context_t context, int32_t argc, char **argv);

static const shell_command_context_t cli_command_table[] = 
{
    {"reset",           "\treset: reset system\r\n",                            cli_reset_system,                           0},   
    {"fault",           "\tfault : Trigger fault\r\n",                          cli_trigger_fault,                          1},
    {"sleep",           "\tsleep :enter/exit sleep\r\n",                        cli_sleep,                                  1},
    {"sms",             "\tsms : Send sms\r\n",                                 cli_send_sms,                               3},
    {"ota",             "\tota : Do an ota update\r\n",                         cli_ota_update,                             1},
	{"420out",          "\t420out : Output 4-20mA\r\n",                         cli_output_4_20ma,                          2},
	{"test",            "\ttest : enter/exit test mode\r\n",                    cli_enter_test_mode,                        1},
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
    NVIC_SystemReset();
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
//    extern System_t xSystem;
//    extern bool gsm_data_layer_is_module_sleeping();
//    if (strstr(argv[1], "enter"))
//    {
//        
//    }
//    else if (strstr(argv[1], "exit"))
//    {
//        if (gsm_data_layer_is_module_sleeping())
//        {
//            xSystem.Status.gsm_sleep_time_s = app_eeprom_read_config_data()->send_to_server_interval_ms / 1000;
//        }
//    }
    return 0;
}

extern bool gsm_send_sms(char *phone_number, char *message);
static int32_t cli_send_sms(p_shell_context_t context, int32_t argc, char **argv)
{
    if (strstr(argv[1], "send"))
    {
        if (!gsm_send_sms(argv[2], argv[3]))
        {
            DEBUG_PRINTF("Send sms failed\r\n");
        }
    }
    else if (strstr(argv[1], "read"))
    {
        DEBUG_PRINTF("Enter read sms mode\r\n");
        gsm_set_flag_prepare_enter_read_sms_mode();
    }
    return  0;
}

//extern System_t xSystem;
static int32_t cli_ota_update(p_shell_context_t context, int32_t argc, char **argv)
{
//    xSystem.file_transfer.ota_is_running = 1;
//    sprintf(xSystem.file_transfer.url, "%s", 
//            "https://iot.wilad.vn/api/v1/860262050129720/attributes");
    return 0;
}

static int32_t cli_output_4_20ma(p_shell_context_t context, int32_t argc, char **argv)
{
	app_eeprom_config_data_t *cfg = app_eeprom_read_config_data();
	cfg->io_enable.name.output_4_20ma_enable = 1;
	cfg->io_enable.name.output_4_20ma_value = atoi(argv[1]);
	cfg->io_enable.name.output_4_20ma_timeout_100ms = atoi(argv[2])/100;
	control_output_dac_enable(atoi(argv[2]));
	DEBUG_PRINTF("Control DAC output %uma in ms %ums\r\n", cfg->io_enable.name.output_4_20ma_value,
															atoi(argv[2]));
	return 0;
}

static int32_t cli_enter_test_mode(p_shell_context_t context, int32_t argc, char **argv)
{
    sys_ctx_t *system = sys_ctx();
	if (strstr(argv[1], "en"))
	{
		DEBUG_PRINTF("Enter test mode\r\n");
		system->status.is_enter_test_mode = true;
	}
	else if (strstr(argv[1], "dis"))
	{
		DEBUG_PRINTF("Exit test mode\r\n");
		system->status.is_enter_test_mode = false;
	}
    return 0;
}


#endif /* APP_CLI_ENABLE */

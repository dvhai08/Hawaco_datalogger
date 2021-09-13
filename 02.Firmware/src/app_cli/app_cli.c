#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>

#if 1

#include "app_debug.h"
#include "ota_update.h"
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
#include "app_spi_flash.h"
#include "tim.h"
#include "spi.h"

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
static int32_t cli_factory_reset(p_shell_context_t context, int32_t argc, char **argv);
//static int32_t cli_sleep(p_shell_context_t context, int32_t argc, char **argv);
//static int32_t cli_send_sms(p_shell_context_t context, int32_t argc, char **argv);
//static int32_t cli_ota_update(p_shell_context_t context, int32_t argc, char **argv);
static int32_t cli_output_4_20ma(p_shell_context_t context, int32_t argc, char **argv);
static int32_t cli_enter_test_mode(p_shell_context_t context, int32_t argc, char **argv);
static int32_t cli_flash_test(p_shell_context_t context, int32_t argc, char **argv);
//static int32_t cli_rs485_test(p_shell_context_t context, int32_t argc, char **argv);
static int32_t cli_pwm_test(p_shell_context_t context, int32_t argc, char **argv);
static int32_t cli_set_server_test(p_shell_context_t context, int32_t argc, char **argv);

static const shell_command_context_t cli_command_table[] = 
{
    {"reset",           "\treset: reset system\r\n",                            cli_reset_system,                           0},   
    {"factory",         "\tfactory : Factory reset\r\n",                        cli_factory_reset,                          0},
//    {"sleep",           "\tsleep :enter/exit sleep\r\n",                        cli_sleep,                                  1},
//    {"sms",             "\tsms : Send sms\r\n",                                 cli_send_sms,                               3},
//    {"ota",             "\tota : Do an ota update\r\n",                         cli_ota_update,                             1},
	{"420out",          "\t420out : Output 4-20mA\r\n",                         cli_output_4_20ma,                          2},
	{"test",            "\ttest : enter/exit test mode\r\n",                    cli_enter_test_mode,                        1},
//    {"flash",           "\tflash : Flash test\r\n",                             cli_flash_test,                             2},
//    {"485",             "\t485 : Test rs485\r\n",                               cli_rs485_test,                             0},
    {"pwm",             "\tpwm : Test pwm\r\n",                                 cli_pwm_test,                               1},
//    {"server",          "\tSet server\r\n",                                     cli_set_server_test,                        1},
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
//    DEBUG_PRINTF("System reset\r\n");
    NVIC_SystemReset();
    return 0;
}


static int32_t cli_factory_reset(p_shell_context_t context, int32_t argc, char **argv)
{
    DEBUG_INFO("Erase all data in eeprom and ext flash\r\n");
    app_eeprom_erase();
    app_spi_flash_erase_all();
	NVIC_SystemReset();
    return 0;
}

//static int32_t cli_sleep(p_shell_context_t context, int32_t argc, char **argv)
//{
////    extern System_t xSystem;
////    extern bool gsm_data_layer_is_module_sleeping();
////    if (strstr(argv[1], "enter"))
////    {
////        
////    }
////    else if (strstr(argv[1], "exit"))
////    {
////        if (gsm_data_layer_is_module_sleeping())
////        {
////            xSystem.Status.gsm_sleep_time_s = app_eeprom_read_config_data()->send_to_server_interval_ms / 1000;
////        }
////    }
//    return 0;
//}

//extern bool gsm_send_sms(char *phone_number, char *message);
//static int32_t cli_send_sms(p_shell_context_t context, int32_t argc, char **argv)
//{
//    if (strstr(argv[1], "send"))
//    {
//        if (!gsm_send_sms(argv[2], argv[3]))
//        {
//            DEBUG_PRINTF("Send sms failed\r\n");
//        }
//    }
//#if GSM_READ_SMS_ENABLE
//    else if (strstr(argv[1], "read"))
//    {
//        DEBUG_PRINTF("Enter read sms mode\r\n");
//        gsm_set_flag_prepare_enter_read_sms_mode();
//    }
//#endif
//    return  0;
//}

////extern System_t xSystem;
//static int32_t cli_ota_update(p_shell_context_t context, int32_t argc, char **argv)
//{
//    DEBUG_PRINTF("Begin ota update\r\n");
//    sys_ctx()->status.enter_ota_update = true;
//    sprintf((char*)sys_ctx()->status.ota_url, "%s", "http://radiotech.vn:2602/Data_logger_DTG1.bin");
//    gsm_set_wakeup_now();
//    return 0;
//}

static int32_t cli_output_4_20ma(p_shell_context_t context, int32_t argc, char **argv)
{
	app_eeprom_config_data_t *cfg = app_eeprom_read_config_data();
    float value = atof(argv[1]);
    
	cfg->io_enable.name.output_4_20ma_enable = 1;
	cfg->output_4_20ma = value;
	cfg->io_enable.name.output_4_20ma_timeout_100ms = atoi(argv[2])/100;
	control_output_dac_enable(atoi(argv[2]));
	DEBUG_INFO("DAC output %.2fma in ms %ums\r\n", cfg->output_4_20ma,
                                                                atoi(argv[2]));
	return 0;
}

static int32_t cli_enter_test_mode(p_shell_context_t context, int32_t argc, char **argv)
{
    sys_ctx_t *system = sys_ctx();
	if (strstr(argv[1], "en"))
	{
		DEBUG_INFO("Enter test mode\r\n");
		system->status.is_enter_test_mode = true;
	}
	else if (strstr(argv[1], "dis"))
	{
		DEBUG_INFO("Exit test mode\r\n");
		system->status.is_enter_test_mode = false;
	}
    return 0;
}

static int32_t cli_flash_test(p_shell_context_t context, int32_t argc, char **argv)
{
    if (strstr(argv[1], "erase"))
	{
		DEBUG_INFO("Erase flash\r\n");
        app_spi_flash_erase_all();
	}
	else if (strstr(argv[1], "stress"))
	{
		DEBUG_INFO("Write flash\r\n");
        app_spi_flash_stress_test(atoi(argv[2]));
	}
    else if (strstr(argv[1], "rdall"))
	{
//		DEBUG_PRINTF("Read flash\r\n");
        app_spi_flash_retransmission_data_test();
	}
    else if (strstr(argv[1], "pageerase"))
	{
		DEBUG_INFO("Erase page flash\r\n");
//        app_spi_flash_erase_all_page(atoi(argv[2]);
	}
    else if (strstr(argv[1], "empty"))
	{
//		DEBUG_PRINTF("Check empty sector\r\n");
        if (app_spi_flash_check_empty_sector(atoi(argv[2])))
        {
            DEBUG_INFO("Empty sector\r\n");
        }
        else
        {
            DEBUG_INFO("Full sector\r\n");
        }
	}
    else if (strstr(argv[1], "writetoend"))
	{
//		DEBUG_INFO("Skip write to end sector\r\n");
        app_spi_flash_skip_to_end_flash_test();
	}
	else if (strstr(argv[1], "dump"))
	{
//		DEBUG_INFO("Dump all flash\r\n");
		spi_init();
		app_spi_flash_wakeup();

		app_spi_flash_dump_to_485();
		app_spi_flash_shutdown();
	}
    return 0;
}

//static int32_t cli_rs485_test(p_shell_context_t context, int32_t argc, char **argv)
//{
//    sys_ctx()->status.is_enter_test_mode = 1;
//    DEBUG_INFO("Test rs485\r\n");
//    return 0;
//}

static int32_t cli_pwm_test(p_shell_context_t context, int32_t argc, char **argv)
{
    tim_pwm_change_freq(atoi(argv[1]));
    return 0;
}

static int32_t cli_set_server_test(p_shell_context_t context, int32_t argc, char **argv)
{
    DEBUG_INFO("Set server %s\r\n", argv[1]);
    app_eeprom_config_data_t *eeprom_cfg = app_eeprom_read_config_data();
    memset(eeprom_cfg->server_addr[APP_EEPROM_MAIN_SERVER_ADDR_INDEX], 0, APP_EEPROM_MAX_SERVER_ADDR_LENGTH);
    sprintf((char*)eeprom_cfg->server_addr[APP_EEPROM_ALTERNATIVE_SERVER_ADDR_INDEX], "%s", argv[1]);

    app_eeprom_save_config();		// Store current config into eeprom
//    DEBUG_INFO("Set new server addr success\r\n");

    return 0;
}


#endif /* APP_CLI_ENABLE */

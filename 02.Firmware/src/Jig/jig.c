#include "jig.h"
#include "main.h"
#include "usart.h"
#include "hardware.h"
#include "measure_input.h"
#include "string.h"
#include "adc.h"
#include <stdarg.h>
#include "stdio.h"
#include <string.h>
#include <ctype.h>
#include "umm_malloc.h"
#include "app_bkup.h"

#define JIG_DEFAULT_TIMEOUT_MS 100
#define JIG_RS485_RX485_TX_BUFFER_SIZE	128
#define JIG_RS485_RX485_RX_BUFFER_SIZE	128

static jig_rs485_buffer_t m_jig_buffer;
volatile uint32_t jig_timeout_ms = JIG_DEFAULT_TIMEOUT_MS;
static bool m_jig_in_test_mode = false;

void jig_print(const char *fmt,...)
{
	int     n;

    char *p = (char*)m_jig_buffer.tx_ptr;
    int size = 64;
    va_list args;

    va_start (args, fmt);
    n = vsnprintf(p, size, fmt, args);
    if (n > (int)size) 
    {
        usart_lpusart_485_send((uint8_t*)m_jig_buffer.tx_ptr, size);
    } 
    else if (n > 0) 
    {
        usart_lpusart_485_send((uint8_t*)m_jig_buffer.tx_ptr, n);
    }
    va_end(args);
}

bool jig_is_in_test_mode(void)
{
	return m_jig_in_test_mode;
}

static char m_jig_buffer_tx[JIG_RS485_RX485_TX_BUFFER_SIZE];
static char m_jig_buffer_rx[JIG_RS485_RX485_RX_BUFFER_SIZE];

void jig_start(void)
{
	m_jig_buffer.rx_idx = 0;
	m_jig_buffer.tx_ptr = m_jig_buffer_tx;
	m_jig_buffer.rx_ptr = m_jig_buffer_rx;

	RS485_POWER_EN(1);
	usart_lpusart_485_control(1);
	sys_delay_ms(200);
	jig_timeout_ms = JIG_DEFAULT_TIMEOUT_MS;
	while (jig_timeout_ms)
	{
		// Reload watchdog
		WRITE_REG(IWDG->KR, LL_IWDG_KEY_RELOAD);
		if (m_jig_buffer.rx_idx && strstr(m_jig_buffer.rx_ptr, "{\"test\":1}"))
		{
            m_jig_buffer.rx_idx = 0;
			m_jig_in_test_mode = true;
		}
		else		// Exit test mode
		{
            break;
		}
	}
    if (m_jig_in_test_mode == false)
    {
        m_jig_buffer.rx_ptr = NULL;
        m_jig_buffer.tx_ptr = NULL;
        RS485_POWER_EN(0);
    }
    else
    {
        RS485_POWER_EN(1);
    }
}

void jig_uart_insert(uint8_t data)
{
	if (m_jig_buffer.rx_ptr && data)
	{
		m_jig_buffer.rx_ptr[m_jig_buffer.rx_idx++] = data;
		if (m_jig_buffer.rx_idx >= JIG_RS485_RX485_RX_BUFFER_SIZE)
		{
			memset(&m_jig_buffer, 0, sizeof(m_jig_buffer));
		}
	}
}

bool jig_found_cmd_sync_data_to_host(void)
{
	if (m_jig_buffer.rx_ptr == NULL)
	{
		m_jig_buffer.rx_ptr = m_jig_buffer_rx;
        memset(m_jig_buffer_rx, 0, sizeof(m_jig_buffer_rx));
		m_jig_buffer.rx_ptr[0] = 0;
	}
	
	if (strstr((char*)m_jig_buffer.rx_ptr, "Hawaco.Datalogger.PingMessage"))
	{
        m_jig_buffer.rx_ptr = NULL;
		memset(&m_jig_buffer, 0, sizeof(m_jig_buffer));
		return true;
	}
	return false;
}

bool jig_found_cmd_change_server(char **ptr, uint32_t *size)
{
    // Format Server:http://iot.wilab.vn\r\n
    *size = 0;
    char *server = strstr((char*)m_jig_buffer.rx_ptr, "Server:http://");
	if (server && strstr(server, "\r\n"))
	{
        *ptr = server + 7;      // 7 = strlen("Server:")
        *size = strstr(server, "\r\n") - *ptr;
        if (*size >= (APP_EEPROM_MAX_SERVER_ADDR_LENGTH-1))
        {
            return false;
        }
        return true;
	}
	else if ((server = strstr((char*)m_jig_buffer.rx_ptr, "Server:https://"))
	        && server)
	{
        *ptr = server + 7;      // 7 = strlen("Server:")
        *size = strstr(server, "\r\n") - *ptr;
        if (*size >= (APP_EEPROM_MAX_SERVER_ADDR_LENGTH-1))
        {
            return false;
        }
        return true;
	}
	return false;
}

bool jig_found_cmd_set_default_server_server(char **ptr, uint32_t *size)
{
    // Format Default:http://iot.wilab.vn\r\n
    *size = 0;
    char *server = strstr((char*)m_jig_buffer.rx_ptr, "Default:http://");
	if (server && strstr(server, "\r\n"))
	{
        *ptr = server + 7;      // 7 = strlen("Server:")
        *size = strstr(server, "\r\n") - *ptr;
        if (*size >= (APP_EEPROM_MAX_SERVER_ADDR_LENGTH-1))
        {
            return false;
        }
        return true;
	}
	else if ((server = strstr((char*)m_jig_buffer.rx_ptr, "Default:https://"))
	        && server)
	{
        *ptr = server + 7;      // 7 = strlen("Server:")
        *size = strstr(server, "\r\n") - *ptr;
        if (*size >= (APP_EEPROM_MAX_SERVER_ADDR_LENGTH-1))
        {
            return false;
        }
        return true;
	}
	return false;
}

bool jig_found_cmd_get_config(void)
{
    // Format GetConfig\r\n
    char *config = strstr((char*)m_jig_buffer.rx_ptr, "GetConfig\r\n");
	if (config)
    {
        return true;
    }
	return false;
}

void jig_release_memory(void)
{
	if (m_jig_buffer.rx_ptr)
	{
        m_jig_buffer.rx_ptr = NULL;
        memset(m_jig_buffer_rx, 0, sizeof(m_jig_buffer_rx));
        memset(m_jig_buffer_tx, 0, sizeof(m_jig_buffer_tx));
		memset(&m_jig_buffer, 0, sizeof(m_jig_buffer));
	}
}




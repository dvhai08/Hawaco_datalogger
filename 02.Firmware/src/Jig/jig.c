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

static void jig_print(const char *fmt,...)
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



void jig_start(void)
{
	m_jig_buffer.rx_idx = 0;
	m_jig_buffer.tx_ptr = umm_malloc(JIG_RS485_RX485_TX_BUFFER_SIZE);
	m_jig_buffer.rx_ptr = umm_malloc(JIG_RS485_RX485_RX_BUFFER_SIZE);
	
	RS485_POWER_EN(1);
	usart_lpusart_485_control(1);
	sys_delay_ms(200);
	jig_timeout_ms = JIG_DEFAULT_TIMEOUT_MS;
	while (jig_timeout_ms)
	{
		// Reload watchdog
		WRITE_REG(IWDG->KR, LL_IWDG_KEY_RELOAD);
		jig_print("test_begin\r\n");
		sys_delay_ms(100);
		if (m_jig_buffer.rx_idx && strstr(m_jig_buffer.rx_ptr, "test_enter\r\n"))
		{
			m_jig_in_test_mode = true;
			jig_timeout_ms = 100000000;
			uint32_t count = 0;
			uint32_t output_value;
			while (1)
			{
				WRITE_REG(IWDG->KR, LL_IWDG_KEY_RELOAD);
				sys_delay_ms(50);
				LED1(1);
#ifdef DTG01
				LED2(1);
#endif
				sys_delay_ms(50);
				LED1(0);
#ifdef DTG01
				LED2(0);
#endif
				// ADC test
				adc_start();
				adc_input_value_t *adc_result = adc_get_input_result();
								
				if (count % 25 == 0)
				{
					jig_print("Vbat: %.2f\r\n", adc_result->bat_mv);
					jig_print("Vin24v: %u\r\n", adc_result->vin_24);
#ifdef DTG02
					for (uint32_t i = 0; i < 4; i++)
#else
					for (uint32_t i = 0; i < 1; i++)
#endif
					{
						jig_print("Input4_20m[%u] = %.2f\r\n", i+1, adc_result->in_4_20ma_in[i]);
					}
				
					jig_print("Temp: %u\r\n", adc_result->temp);
				
					measure_input_counter_t pulse_counter_in_backup[MEASURE_NUMBER_OF_WATER_METER_INPUT];
					extern void measure_input_pulse_counter_poll();
					measure_input_pulse_counter_poll();
					app_bkup_read_pulse_counter(&pulse_counter_in_backup[0]);
					for (uint32_t i = 0; i < MEASURE_NUMBER_OF_WATER_METER_INPUT; i++)
					{
						jig_print("Pulse[%u] value %u-%u\r\n", i+1, pulse_counter_in_backup[i].forward, pulse_counter_in_backup[i].reserve);
					}
					
					// Test input and outout
					output_value++;
					output_value %= 2;
#ifdef DTG01
					TRANS_OUTPUT(0);
					sys_delay_ms(200);
					TRANS_OUTPUT(1);
					sys_delay_ms(200);
#else
					TRANS_1_OUTPUT(output_value);
					TRANS_2_OUTPUT(output_value);
					TRANS_3_OUTPUT(output_value);
					TRANS_4_OUTPUT(output_value);
					sys_delay_ms(1);
					
					if (INPUT_ON_OFF_0() == output_value)
					{
						jig_print("input1 = %u, pass\r\n", output_value);
					}
					else
					{
						jig_print("input1 = %u, fail\r\n", output_value);
					}
					
					if (INPUT_ON_OFF_1() == output_value)
					{
						jig_print("input2 = %u, pass\r\n", output_value);
					}
					else
					{
						jig_print("input2 = %u, fail\r\n", output_value);
					}
					
					if (INPUT_ON_OFF_2() == output_value)
					{
						jig_print("input3 = %u, pass\r\n", output_value);
					}
					else
					{
						jig_print("input3 = %u, fail\r\n", output_value);
					}
					
					if (INPUT_ON_OFF_3() == output_value)
					{
						jig_print("input4 = %u, pass\r\n", output_value);
					}
					else
					{
						jig_print("input4 = %u, fail\r\n", output_value);
					}
					jig_print("-------------\r\n\r\n");
#endif
				}
								
								
				if (count++ == 500)
				{
					NVIC_SystemReset();
				}
			}
		}
		else		// Exit test mode
		{
			__disable_irq();
			jig_timeout_ms = 1;
			__enable_irq();
			sys_delay_ms(2);
		}
	}
	umm_free(m_jig_buffer.rx_ptr);
	umm_free(m_jig_buffer.tx_ptr);
	m_jig_buffer.rx_ptr = NULL;
	m_jig_buffer.tx_ptr = NULL;
	
	RS485_POWER_EN(0);
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
		m_jig_buffer.rx_ptr = umm_calloc(JIG_RS485_RX485_RX_BUFFER_SIZE, 1);
		m_jig_buffer.rx_ptr[0] = 0;
	}
	
	if (strstr((char*)m_jig_buffer.rx_ptr, "Hawaco.Datalogger.PingMessage"))
	{
		umm_free(m_jig_buffer.rx_ptr); 
		memset(&m_jig_buffer, 0, sizeof(m_jig_buffer));
		return true;
	}
	return false;
}

void jig_release_memory(void)
{
	if (m_jig_buffer.rx_ptr)
	{
		umm_free(m_jig_buffer.rx_ptr); 
		memset(&m_jig_buffer, 0, sizeof(m_jig_buffer));
	}
}




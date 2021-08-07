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

#define JIG_DEFAULT_TIMEOUT_MS 100
#define JIG_RS485_RX485_TX_BUFFER_SIZE	128
#define JIG_RS485_RX485_RX_BUFFER_SIZE	128

static jig_rs485_buffer_t m_jig_buffer;
volatile uint32_t jig_timeout_ms = JIG_DEFAULT_TIMEOUT_MS;

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
			jig_timeout_ms = 100000000;
			for (uint32_t j = 0; j < 3; j++)
			{
				// ADC test
				adc_start();
				adc_input_value_t *adc_result = adc_get_input_result();
				
				jig_print("Vbat: %.2f\r\n", adc_result->bat_mv);
				jig_print("Vin24v: %u\r\n", adc_result->vin_24);
	#ifdef DTG02
				for (uint32_t i = 0; i < 4; i++)
	#else
				for (uint32_t i = 0; i < 1; i++)
	#endif
				{
					jig_print("Input4_20m[i] = %.2f\r\n", i, adc_result->in_4_20ma_in[i]);
				}
				
				jig_print("Temp: %u\r\n", adc_result->temp);
				
				// Test input and outout
				for (uint32_t i = 0; i < 2; i++)
				{
					TRANS_1_OUTPUT(i);
					TRANS_2_OUTPUT(i);
					TRANS_3_OUTPUT(i);
					TRANS_4_OUTPUT(i);
					sys_delay_ms(1);
					
					if (INPUT_ON_OFF_0() == i)
					{
						jig_print("input-output1 pass\r\n");
					}
					else
					{
						jig_print("input-output1 fail\r\n");
					}
					
					if (INPUT_ON_OFF_1() == i)
					{
						jig_print("input-output2 pass\r\n");
					}
					else
					{
						jig_print("input-output2 fail\r\n");
					}
					
					if (INPUT_ON_OFF_2() == i)
					{
						jig_print("input-output3 pass\r\n");
					}
					else
					{
						jig_print("input-output3 fail\r\n");
					}
					
					if (INPUT_ON_OFF_3() == i)
					{
						jig_print("input-output4 pass\r\n");
					}
					else
					{
						jig_print("input-output4 fail\r\n");
					}
				}
				jig_print("test_begin\r\n");
				sys_delay_ms(50);
			}
		
			uint32_t count = 0;
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
				if (count++ == 30)
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
}

void jig_uart_insert(uint8_t data)
{
	m_jig_buffer.rx_ptr[m_jig_buffer.rx_idx++] = data;
}

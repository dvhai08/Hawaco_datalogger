/*!
    \file    main.c
    \brief   led spark with systick, USART print and key example
    \version 2019-02-19, V1.0.0, firmware for GD32E23x
*/

#include "gd32e10x.h"
#include "gd32e10x_adc.h"
#include "systick.h"
#include <stdio.h>
#include <string.h>
#include "main.h"
#include "app_wdt.h"
#include "board_hw.h"
#include "app_flash.h"
#include "Transfer.h"
#include "Hardware.h"
#include "HardwareManager.h"
#include "InitSystem.h"

extern System_t xSystem;

static void task_wdt(uint32_t timeout_ms);

static uint32_t sys_now_ms(void);
static app_flash_network_info_t m_net_info __attribute__((aligned(4)));

 static uint8_t TimeOut10ms = 0;
 static uint8_t TimeOut100ms = 0;
 static uint16_t TimeOut500ms = 0;
 static uint16_t TimeOut1000ms = 0;
 
__IO uint16_t TimingDelay = 0;

static void ProcessTimeout10ms(void);
static void ProcessTimeout100ms(void);
static void ProcessTimeout500ms(void);
static void ProcessTimeout1000ms(void);

volatile uint32_t m_sys_tick = 0;

/*!
    \brief      main function
    \param[in]  none
    \param[out] none
    \retval     none
*/
int main(void)
{
//    app_wdt_start();
//    systick_config();
    
    InitSystem();
	
	uint8_t ledToggle = 0;

    while (1)
    {
        if(TimeOut10ms >= 10)
        {
            TimeOut10ms = 0;
            ProcessTimeout10ms();
        }

        if(TimeOut100ms >= 100)
        {
            TimeOut100ms = 0;
            ProcessTimeout100ms();
        }
		  
		  if(TimeOut500ms >= 500)
			{
				TimeOut500ms = 0;
				LED1(ledToggle);
				LED2(ledToggle);
				ledToggle = 1 - ledToggle;
				
				DEBUG("\rHello...");
			}
        
//        if (SEGGER_RTT_HASDATA(0))
//        {
//            uint8_t c;
//            while (SEGGER_RTT_Read(0, &c, 1))
//            {
//                xSystem.Debug->AddData(c);
//            }
//        }
    }
}
__IO uint16_t  adc_vin_value;
static void ProcessTimeout10ms(void)
{	
//    if (SET == adc_flag_get(ADC_FLAG_EOC))
//    {
//        adc_flag_clear(ADC_FLAG_EOC);
//        adc_software_trigger_enable(ADC_REGULAR_CHANNEL);
//        adc_vin_value = ADC_RDATA;
//    }  
    
//    ESP32_UART_Tick();
//    xSystem.Debug->Tick();
}

static void ProcessTimeout100ms(void)
{
//	Hardware_XoaCoLoi();
	app_wdt_feed();
	
//	Button_Tick();
}

/*****************************************************************************/
/**
 * @brief	:  	Process timeout 1s
 * @param	:  	None
 * @retval	:	None
 * @author	:	
 * @created	:	15/01/2016
 * @version	:
 * @reviewer:	
 */

static void ProcessTimeout1000ms(void)
{			
	if(xSystem.FileTransfer.State != FT_TRANSFERRING)
	{
//		GSM_ManagerTask();
//		ProcessPingTimeout();
	}
//	DownloadFileTick();
}

void SystemManagementTask(void)
{
    m_sys_tick++;
    TimeOut10ms++;
    TimeOut100ms++;
    TimeOut500ms++;
    //	TimeOut1000ms++;
}

void Delay_Decrement(void)
{
    if(TimingDelay > 0)
        TimingDelay--;
}

//Ham Delayms
void Delayms(uint16_t ms)
{
//    DEBUG_PRINTF("[%u] Delay %dms\r\n", m_sys_tick, ms);
    TimingDelay = ms;
    while(TimingDelay)
    {
        if(TimingDelay % 5 == 0)
            app_wdt_feed();
    }
}

uint32_t sys_get_ms(void)
{
    return m_sys_tick;
}

///////* retarget the C library printf function to the USART */
//int fputc(int ch, FILE *f)
//{
//    usart_data_transmit(USART1, (uint8_t)ch);
//    while (RESET == usart_flag_get(USART1, USART_FLAG_TBE))
//        ;
//    return ch;
//}

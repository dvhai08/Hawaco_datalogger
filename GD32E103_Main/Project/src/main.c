/*!
    \file    main.c
    \brief   led spark with systick, USART print and key example
    \version 2019-02-19, V1.0.0, firmware for GD32E23x
*/
#include "gd32e10x.h"
#include "gd32e10x_adc.h"
#include <stdio.h>
#include <string.h>
#include "RTL.h"
#include "Net_Config.h"
#include "main.h"
#include "HardwareManager.h"
#include "InitSystem.h"
#include "GSM.h"
#include "MQTTUser.h"
#include "Measurement.h"
#include "ControlOutput.h"
#include "Debug.h"

extern System_t xSystem;
extern LOCALM localm[];   

static uint8_t Timeout1ms = 0;
static uint8_t TimeOut10ms = 0;
static uint8_t TimeOut100ms = 0;
static uint16_t TimeOut1000ms = 0;
static uint16_t TimeOut3000ms = 0;
 
__IO uint16_t TimingDelay = 0;
volatile uint32_t m_sys_tick = 0;
uint8_t getNTPTimeout = 0;

static void ProcessTimeout10ms(void);
static void ProcessTimeout100ms(void);
static void ProcessTimeOut3000ms(void);
static void ProcessTimeout1000ms(void);
void getTimeNTP(void);

/*!
    \brief      main function
    \param[in]  none
    \param[out] none
    \retval     none
*/
int main(void)
{    
    InitSystem();
	
    while (1)
    {
		 main_TcpNet();
		 
		 if(Timeout1ms >= 1)
		 {
			 Timeout1ms = 0;
			 Measure_PulseTick();
		 }
		 
        if(TimeOut10ms >= 10)
        {
            TimeOut10ms = 0;
            ProcessTimeout10ms();
        }

        if(TimeOut100ms >= 100)
        {
			  timer_tick();
			  
            TimeOut100ms = 0;
            ProcessTimeout100ms();
        }
		  
		  if(TimeOut3000ms >= 3000)
			{
				TimeOut3000ms = 0;
				ProcessTimeOut3000ms();
			}
			
			if(TimeOut1000ms >= 1000)
			{
				TimeOut1000ms = 0;
				
				xSystem.Status.TimeStamp++;		//virtual RTC
				ProcessTimeout1000ms();
			}
    }
}

static void ProcessTimeout10ms(void)
{
#if (__USED_HTTP__)
	GSM_HardwareTick();
#else
	MQTT_Tick();
#endif
	    
	Measure_Tick();
	Debug_Tick(); 
}

static void ProcessTimeout100ms(void)
{
	Hardware_XoaCoLoi();
	ResetWatchdog();
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
uint8_t ledToggle = 0;
static void ProcessTimeout1000ms(void)
{			
	if(xSystem.FileTransfer.State != FT_TRANSFERRING)
	{
		GSM_ManagerTick();
//		ProcessPingTimeout();
	}
//	DownloadFileTick();
	
	Output_Tick();
		
	//Test LED
	LED1(ledToggle);
	LED2(ledToggle);
	ledToggle = 1 - ledToggle;
}

static void ProcessTimeOut3000ms(void)
{
	static uint32_t SystemTickCount = 0;
	  	
	DEBUG("\rSystem tick : %u,%u - IP: %u.%u.%u.%u, Vin: %umV", ++SystemTickCount, ppp_is_up(),
		localm[NETIF_PPP].IpAdr[0], localm[NETIF_PPP].IpAdr[1], 
		localm[NETIF_PPP].IpAdr[2], localm[NETIF_PPP].IpAdr[3],
		xSystem.MeasureStatus.Vin);
	
	//get NTP time
	if(ppp_is_up()) {
		if(getNTPTimeout % 5 == 0) {
			getTimeNTP();
		}
		getNTPTimeout++;
	} else
		getNTPTimeout = 0;
}

void SystemManagementTask(void)
{
	m_sys_tick++;

	Timeout1ms++;
	TimeOut10ms++;
	TimeOut100ms++;
	TimeOut1000ms++;
	TimeOut3000ms++;
}

void Delay_Decrement(void)
{
    if(TimingDelay > 0)
        TimingDelay--;
}

//Ham Delayms
void Delayms(uint16_t ms)
{
    TimingDelay = ms;
    while(TimingDelay)
    {
        if(TimingDelay % 5 == 0)
            ResetWatchdog();
    }
}

static void time_cback (U32 time)
{
  if (time == 0) {
	  DEBUG ("\rNTP: Error, server not responding or bad response.");
  } else {
	  DEBUG ("\rNTP: %d seconds elapsed since 1.1.1970", time);
		
	  xSystem.Status.TimeStamp = time;	  
	  //Update RTC
	  //xSystem.Rtc->UpdateTimeFromNTPServer(time);
  }
}

void getTimeNTP(void) 
{	
  U8 ntp_server[4] = {217,79,179,106};
  if (sntp_get_time (&ntp_server[0], time_cback) == __FALSE) {
    DEBUG ("\rFailed, SNTP not ready or bad parameters");
  }
}

uint32_t sys_get_ms(void)
{
    return m_sys_tick;
}

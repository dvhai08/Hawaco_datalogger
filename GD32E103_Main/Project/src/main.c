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
#include "lpf.h"

#if (__GSM_SLEEP_MODE__)
	#warning CHU Y DANG BUILD __GSM_SLEEP_MODE__ = 1
#endif

extern System_t xSystem;
extern LOCALM localm[];   

static uint8_t Timeout1ms = 0;
static uint8_t TimeOut10ms = 0;
static uint8_t TimeOut100ms = 0;
uint16_t TimeOut1000ms = 0;
uint16_t TimeOut3000ms = 0;
 
__IO uint16_t TimingDelay = 0;
volatile uint32_t m_sys_tick = 0;
uint8_t getNTPTimeout = 0;

static void ProcessTimeout10ms(void);
static void ProcessTimeout100ms(void);
static void ProcessTimeOut3000ms(void);
static void ProcessTimeout1000ms(void);
static uint8_t convertVinToPercent(uint32_t vin);
void getTimeNTP(void);
uint32_t sntpTimeoutInverval = 5;
uint8_t adcStarted = 0;
lpf_data_t AdcFilterdValue = 
{
	.estimate_value = 0,
	.gain = 0,
};
/*!
    \brief      main function
    \param[in]  none
    \param[out] none
    \retval     none
*/
int main(void)
{    
    InitSystem();
	adcStarted = 1;
	MqttClientSendFirstMessageWhenReset();
	
    while (1)
    {
		 main_TcpNet();
		 xSystem.Status.TimeStamp = rtc_counter_get();
		 if (xSystem.Parameters.input.name.rs485)
		 {
			 RS485_PWR_ON();
			 UART_Init(RS485_UART, 115200);
			 
		 }
		 else
		 {
			 RS485_PWR_OFF();
			 UART_DeInit(RS485_UART);
		 }
		 if(Timeout1ms >= 1)
		 {
			 Timeout1ms = 0;
		 }
		 
        if(TimeOut10ms >= 10)
        {
            TimeOut10ms = 0;
			if (adcStarted)
			{
				xSystem.MeasureStatus.Vin = AdcUpdate();
				if (AdcFilterdValue.estimate_value == 0 && AdcFilterdValue.gain == 0)
				{
					AdcFilterdValue.gain = 1;		// 1 percent
					AdcFilterdValue.estimate_value = xSystem.MeasureStatus.Vin * 100;
				}
				else
				{
					int32_t tmpVin = xSystem.MeasureStatus.Vin*100;
					lpf_update_estimate(&AdcFilterdValue, &tmpVin);
					xSystem.MeasureStatus.Vin = AdcFilterdValue.estimate_value/100;
				}
				xSystem.MeasureStatus.batteryPercent = convertVinToPercent(xSystem.MeasureStatus.Vin);
				AdcStop();
				adcStarted = 0;
			}
			
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
			if (adcStarted == 0)
			{
				ADC_Config();
				adcStarted = 1;
			}
			TimeOut3000ms = 0;
			ProcessTimeOut3000ms();
		}

		if(TimeOut1000ms >= 1000)
		{
			TimeOut1000ms = 0;
			Measure_PulseTick();
			if(!isGSMSleeping())
			{
				if (xSystem.Status.DisconnectTimeout++ > 60)
				{
					NVIC_SystemReset();
				}
			}
			ResetWatchdog();
			ProcessTimeout1000ms();
		}
		
		if(isGSMSleeping())
		{
			DEBUG_PRINTF("Sleep\r\n");
			rtc_alarm_config(rtc_counter_get() + 1);
			xSystem.Status.DisconnectTimeout = 0;
			pmu_to_deepsleepmode(PMU_LDO_LOWPOWER, WFI_CMD);
            SystemInit();
            /* reconfigure ckout0 */
            rcu_ckout0_config(RCU_CKOUT0SRC_CKSYS);
		}
		else
		{
			
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
	LED1(1);
	LED2(1);
}

static void ProcessTimeOut3000ms(void)
{
	static uint32_t SystemTickCount = 0;

	  	
	DEBUG ("System tick : %u,%u - IP: %u.%u.%u.%u, Vin: %umV\r\n", ++SystemTickCount, ppp_is_up(),
		localm[NETIF_PPP].IpAdr[0], localm[NETIF_PPP].IpAdr[1], 
		localm[NETIF_PPP].IpAdr[2], localm[NETIF_PPP].IpAdr[3],
		xSystem.MeasureStatus.Vin);
	
	//get NTP time
	if(ppp_is_up() && (isGSMSleeping() == 0)) {
		if(getNTPTimeout % sntpTimeoutInverval == 0) {
			getTimeNTP();
		}
		getNTPTimeout++;
	} else
	{
		sntpTimeoutInverval = 5;
		getNTPTimeout = 0;
	}
}

void SystemManagementTask(void)
{
	m_sys_tick++;

	Timeout1ms++;
	TimeOut10ms++;
	TimeOut100ms++;
//	TimeOut1000ms++;
//	TimeOut3000ms++;
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
		pmu_to_sleepmode(WFI_CMD);
        if(TimingDelay % 5 == 0)
            ResetWatchdog();
    }
}

static void time_cback (U32 time)
{
  if (time == 0) {
	  DEBUG ("\r\nNTP: Error, server not responding or bad response.");
  } else {
	  DEBUG ("\r\nNTP: %d seconds elapsed since 1.1.1970", time);
	  sntpTimeoutInverval = 120;
		
	  xSystem.Status.TimeStamp = time;	  
	  __disable_irq();
	  rtc_counter_set(xSystem.Status.TimeStamp);
	  __enable_irq();
	  //Update RTC
	  //xSystem.Rtc->UpdateTimeFromNTPServer(time);
  }
}

void getTimeNTP(void) 
{	
  U8 ntp_server[4] = {217,79,179,106};
  if (sntp_get_time (&ntp_server[0], time_cback) == __FALSE) {
    DEBUG ("\r\nFailed, SNTP not ready or bad parameters");
  }
}

uint32_t sys_get_ms(void)
{
    return m_sys_tick;
}

static uint8_t convertVinToPercent(uint32_t vin)
{
	#define VIN_MAX 25000
	#define VIN_MIN 22000
	
	if (vin >= VIN_MAX)
		return 100;
	if (vin <= VIN_MIN)
		return 0;
	return ((vin-VIN_MIN)*100)/(VIN_MAX-VIN_MIN);
}


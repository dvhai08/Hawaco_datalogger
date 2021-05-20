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
#include "stdbool.h"

#if (__GSM_SLEEP_MODE__)
#warning CHU Y DANG BUILD __GSM_SLEEP_MODE__ = 1
#endif

extern System_t xSystem;
extern LOCALM localm[];
extern __IO uint16_t ADC_RegularConvertedValueTab[ADCMEM_MAXUNIT];

static uint8_t Timeout1ms = 0;
static uint8_t TimeOut10ms = 0;
static uint8_t TimeOut100ms = 0;
uint16_t TimeOut1000ms = 0;
uint16_t TimeOut3000ms = 0;
extern volatile uint32_t StoreMeasureResultTick;
extern uint32_t Measure420mATick;
__IO uint16_t TimingDelay = 0;
volatile uint32_t m_sys_tick = 0;
uint8_t getNTPTimeout = 0;
extern void RCC_Config(void);
static void ProcessTimeout10ms(void);
static void ProcessTimeout100ms(void);
static void ProcessTimeOut3000ms(void);
static void ProcessTimeout1000ms(void);
static uint8_t convertVinToPercent(uint32_t vin);
uint32_t estimate_sleep_time_in_second(uint32_t current_sec, uint32_t interval_sec);
void getTimeNTP(void);
uint32_t sntpTimeoutInverval = 5;
uint8_t adcStarted = 0;
lpf_data_t AdcFilterdValue =
{
        .estimate_value = 0,
        .gain = 0,
};
volatile int32_t delay_sleeping_for_exit_wakeup = 0;

extern volatile uint32_t SendMessageTick;
volatile bool new_adc_data = false;
/*!
    \brief      main function
    \param[in]  none
    \param[out] none
    \retval     none
*/
int main(void)
{
    __enable_irq();
    InitSystem();
    adcStarted = 1;
#if GSM_ENABLE
    MqttClientSendFirstMessageWhenWakeup();
#endif
    while (1)
    {
#if GSM_ENABLE
        main_TcpNet();
#endif
        #warning  "Output test gui tin"
        xSystem.Parameters.input.name.ma420 = 0;
        xSystem.Parameters.outputOnOff = 0;
        //xSystem.Parameters.TGGTDinhKy = 4;
        //xSystem.Parameters.TGDoDinhKy = 1;

        if (new_adc_data)
        {
            new_adc_data = false;
            if (adcStarted)
            {
                xSystem.MeasureStatus.Vin = AdcUpdate();
                if (AdcFilterdValue.estimate_value == 0 && AdcFilterdValue.gain == 0)
                {
                    AdcFilterdValue.gain = 1; // 1 percent
                    AdcFilterdValue.estimate_value = xSystem.MeasureStatus.Vin * 100;
                }
                else
                {
                    int32_t tmpVin = xSystem.MeasureStatus.Vin * 100;
                    lpf_update_estimate(&AdcFilterdValue, &tmpVin);
                    xSystem.MeasureStatus.Vin = AdcFilterdValue.estimate_value / 100;
                }
                xSystem.MeasureStatus.batteryPercent = convertVinToPercent(xSystem.MeasureStatus.Vin);
                AdcStop();
                adcStarted = 0;
            }
            //DEBUG("ADC data %u-%u, vin %uMV, percent %u%%\r\n", ADC_RegularConvertedValueTab[ADCMEM_V20mV], 
            //                            ADC_RegularConvertedValueTab[ADCMEM_VSYS],
            //                            xSystem.MeasureStatus.Vin,
            //                            xSystem.MeasureStatus.batteryPercent);
        }
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
        if (Timeout1ms >= 1)
        {
            Timeout1ms = 0;
        }

        if (TimeOut10ms >= 10)
        {
            TimeOut10ms = 0;
            ProcessTimeout10ms();
            LED1(1);
        }

        if (TimeOut100ms >= 100)
        {
            timer_tick();
            TimeOut100ms = 0;
            ProcessTimeout100ms();
            if (delay_sleeping_for_exit_wakeup > 0)
            {
                delay_sleeping_for_exit_wakeup--;
            }
        }

        if (TimeOut3000ms >= 3000)
        {
            if (adcStarted == 0)
            {
                ADC_Config();
                adcStarted = 1;
            }
            TimeOut3000ms = 0;
            ProcessTimeOut3000ms();
        }

        if (TimeOut1000ms >= 1000)
        {
            TimeOut1000ms = 0;
            Measure_PulseTick();
            MeasureTick1000ms();
            if (!isGSMSleeping())
            {
                if (xSystem.Status.DisconnectTimeout++ > 60)
                {
                    NVIC_SystemReset();
                }
            }
            else
            {
                UART_DeInit(GSM_UART);
                //DEBUG_PRINTF("GPIO get level %u\r\n", getSwitchState());
                if (xSystem.Parameters.outputOnOff == 0)
                {
                    xSystem.Status.DisconnectTimeout = 0;
                    LED1(1);
                    LED2(1);
                    if (adcStarted)
                    {
                        xSystem.MeasureStatus.Vin = AdcUpdate();
                        if (AdcFilterdValue.estimate_value == 0 && AdcFilterdValue.gain == 0)
                        {
                            AdcFilterdValue.gain = 1; // 1 percent
                            AdcFilterdValue.estimate_value = xSystem.MeasureStatus.Vin * 100;
                        }
                        else
                        {
                            int32_t tmpVin = xSystem.MeasureStatus.Vin * 100;
                            lpf_update_estimate(&AdcFilterdValue, &tmpVin);
                            xSystem.MeasureStatus.Vin = AdcFilterdValue.estimate_value / 100;
                        }
                        xSystem.MeasureStatus.batteryPercent = convertVinToPercent(xSystem.MeasureStatus.Vin);
                        AdcStop();
                        adcStarted = 0;
                    }
                    if (delay_sleeping_for_exit_wakeup > 0)
                    {

                    }
                    else
                    {
                        if ((CoreDebug->DHCSR & CoreDebug_DHCSR_C_DEBUGEN_Msk))
                        {
                            dbg_low_power_enable(DBG_LOW_POWER_DEEPSLEEP);
                        }

                        uint32_t tick_before_sleep = rtc_counter_get();

                        uint32_t min_interval = xSystem.Parameters.TGGTDinhKy;
                        // If input 4-20mA enable && 4-20 measure interval < TGGTDinhKy
                        // =>> Select sleeptime = measure interval
                        if (xSystem.Parameters.TGGTDinhKy > xSystem.Parameters.TGDoDinhKy
                            && xSystem.Parameters.input.name.ma420)
                        {
                            min_interval = xSystem.Parameters.TGDoDinhKy;
                        }
                        uint32_t sleep_time = estimate_sleep_time_in_second(xSystem.Status.GSMSleepTime,
                                                                            min_interval*60);       // 1 min = 60s

                        DEBUG_PRINTF("Before deep sleep, gsm time %us, send msg tick %uS, estimate sleep time %usec\r\n", 
                                        xSystem.Status.GSMSleepTime, 
                                        SendMessageTick,
                                        sleep_time);
                        sleep_time = 18;  // Due to watchdog
                        rtc_alarm_config(rtc_counter_get() + sleep_time);
                        rtc_lwoff_wait();
                        ResetWatchdog();
                        pmu_to_deepsleepmode(PMU_LDO_LOWPOWER, WFI_CMD);
                        SystemInit();
                        RCC_Config();
                        //rcu_ckout0_config(RCU_CKOUT0SRC_CKSYS);       // already called in RCC_Config
                        ResetWatchdog();

                        uint32_t diff = rtc_counter_get() - tick_before_sleep;
                        xSystem.Status.GSMSleepTime += diff;
                        SendMessageTick += diff;
                        TimeOut1000ms = diff*1000;
                        TimeOut3000ms += diff*1000;
                        StoreMeasureResultTick += diff;
                        Measure420mATick += diff;
                        DEBUG_PRINTF("After sleep, gsm sleep time %us, send msg tick %uS\r\n", 
                                        xSystem.Status.GSMSleepTime, 
                                        SendMessageTick);
                    }
                }
                else
                {
                    DEBUG("Output 4-20mA enable %u\r\n", xSystem.Parameters.outputOnOff);
                    pmu_to_sleepmode(WFI_CMD);
                    xSystem.Status.GSMSleepTime++;
                    SendMessageTick++;
                    StoreMeasureResultTick++;
                    Measure420mATick++;
                }
            }
            ResetWatchdog();
            ProcessTimeout1000ms();
        }
        __WFI();
    }
}

extern void GSM_ManagerTestSleep(void);
static void ProcessTimeout10ms(void)
{
#if GSM_ENABLE
    #if (__USED_HTTP__)
        GSM_HardwareTick();
    #else
        MQTT_Tick();
    #endif
#else
    GSM_ManagerTestSleep();
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
    if (xSystem.FileTransfer.State != FT_TRANSFERRING)
    {
        GSM_ManagerTick();
        if (isGSMSleeping())
        {
            DEBUG_PRINTF("Process timeout 1000ms in sleeping state\r\n");
        }
        //		ProcessPingTimeout();
    }
    //	DownloadFileTick();

    Output_Tick();
    //LED1(1);
    //LED2(1);
}

static void ProcessTimeOut3000ms(void)
{
    static uint32_t SystemTickCount = 0;

    DEBUG("System tick : %u,%u - IP: %u.%u.%u.%u, Vin: %umV\r\n", ++SystemTickCount, ppp_is_up(),
          localm[NETIF_PPP].IpAdr[0], localm[NETIF_PPP].IpAdr[1],
          localm[NETIF_PPP].IpAdr[2], localm[NETIF_PPP].IpAdr[3],
          xSystem.MeasureStatus.Vin);
#if GSM_ENABLE
    //get NTP time
    if (ppp_is_up() && (isGSMSleeping() == 0))
    {
        if (getNTPTimeout % sntpTimeoutInverval == 0)
        {
            getTimeNTP();
        }
        getNTPTimeout++;
    }
    else
    {
        sntpTimeoutInverval = 5;
        getNTPTimeout = 0;
    }
#endif
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
    if (TimingDelay > 0)
        TimingDelay--;
}

//Ham Delayms
void Delayms(uint16_t ms)
{
    if (ms > 10)
    {
        DEBUG("Delay %ums\r\n", ms);
    }
    TimingDelay = ms;
    while (TimingDelay)
    {
        if (TimingDelay % 5 == 0)
            ResetWatchdog();
        __WFI();
    }
}

static void time_cback(U32 time)
{
    if (time == 0)
    {
        DEBUG("NTP: Error, server not responding or bad response\r\n");
    }
    else
    {
        DEBUG("NTP: %d seconds elapsed since 1.1.1970\r\n", time);
        sntpTimeoutInverval = 120;

        xSystem.Status.TimeStamp = time + 25200;        // GMT+7
        __disable_irq();
        rtc_counter_set(xSystem.Status.TimeStamp);
        __enable_irq();
    }
}

void getTimeNTP(void)
{
    U8 ntp_server[4] = {217, 79, 179, 106};
    if (sntp_get_time(&ntp_server[0], time_cback) == __FALSE)
    {
        DEBUG("Failed, SNTP not ready or bad parameters\r\n");
    }
}

uint32_t sys_get_ms(void)
{
    return m_sys_tick;
}

static uint8_t convertVinToPercent(uint32_t vin)
{
#define VIN_MAX 4200
#define VIN_MIN 3700

    if (vin >= VIN_MAX)
        return 100;
    if (vin <= VIN_MIN)
        return 0;
    return ((vin - VIN_MIN) * 100) / (VIN_MAX - VIN_MIN);
}

uint32_t estimate_sleep_time_in_second(uint32_t current_sec, uint32_t weakup_interval_s)
{
    if (current_sec >= weakup_interval_s)
    {
        return 1;
    }
    return (weakup_interval_s - current_sec);
}


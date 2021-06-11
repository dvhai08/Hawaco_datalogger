/*!
    \file    main.c
    \brief   led spark with systick, USART print and key example
    \version 2019-02-19, V1.0.0, firmware for GD32E23x
*/
#include "gd32e10x.h"
#include "gd32e10x_adc.h"
#include <stdio.h>
#include <string.h>
#include "main.h"
#include "hardware_manager.h"
#include "InitSystem.h"
#include "gsm.h"
#include "measure_input.h"
#include "ControlOutput.h"
#include "lpf.h"
#include "stdbool.h"
#include "app_cli.h"
#include "app_wdt.h"

extern System_t xSystem;
extern __IO uint16_t ADC_RegularConvertedValueTab[MEASURE_INPUT_ADC_DMA_UNIT];

static uint8_t Timeout1ms = 0;
static uint8_t TimeOut10ms = 0;
static uint8_t TimeOut100ms = 0;
uint16_t TimeOut1000ms = 0;
uint16_t TimeOut3000ms = 0;
extern volatile uint32_t store_measure_result_timeout;
extern uint32_t Measure420mATick;
__IO uint16_t TimingDelay = 0;
volatile uint32_t m_sys_tick = 0;
uint8_t getNTPTimeout = 0;
extern void RCC_Config(void);
static void ProcessTimeout10ms(void);
static void ProcessTimeout100ms(void);
static void ProcessTimeOut3000ms(void);
static void ProcessTimeout1000ms(void);
static uint8_t convert_vin_to_percent(uint32_t vin);
uint32_t estimate_sleep_time_in_second(uint32_t current_sec, uint32_t interval_sec);
uint8_t adcStarted = 0;
lpf_data_t AdcFilterdValue =
{
    .estimate_value = 0,
    .gain = 0,
};
volatile int32_t delay_sleeping_for_exit_wakeup = 0;

volatile bool new_adc_data = false;

__align(4) char g_umm_heap[2048];

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

    gsm_internet_mode_t *internet_mode = gsm_get_internet_mode();

    app_cli_start();
    while (1)
    {
        gsm_hw_layer_run();
   
        #warning  "Output test gui tin"
        xSystem.Parameters.input.name.ma420 = 0;
        xSystem.Parameters.outputOnOff = 0;

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
                xSystem.MeasureStatus.batteryPercent = convert_vin_to_percent(xSystem.MeasureStatus.Vin);
                AdcStop();
                adcStarted = 0;
            }
            //DEBUG_PRINTF("ADC data %u-%u, vin %uMV, percent %u%%\r\n", ADC_RegularConvertedValueTab[ADCMEM_V20mV], 
            //                            ADC_RegularConvertedValueTab[ADCMEM_VSYS],
            //                            xSystem.MeasureStatus.Vin,
            //                            xSystem.MeasureStatus.batteryPercent);
        }
        xSystem.Status.TimeStamp = rtc_counter_get();
        if (xSystem.Parameters.input.name.rs485)
        {
            RS485_PWR_ON();
            driver_uart_initialize(RS485_UART, 115200);
        }
        else
        {
            RS485_PWR_OFF();
            driver_uart_deinitialize(RS485_UART);
        }

        if (TimeOut10ms >= 10)
        {
            TimeOut10ms = 0;
            ProcessTimeout10ms();
            LED1(1);
        }

        if (TimeOut100ms >= 100)
        {
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
            MeasureTick1000ms();
            if (!gsm_data_layer_is_module_sleeping())
            {
                uint32_t timeout_sec = 60;
                if (xSystem.file_transfer.ota_is_running)
                {
                    timeout_sec = 120;
                }

                if (xSystem.Status.DisconnectTimeout++ > timeout_sec)
                {
                    DEBUG_PRINTF("Disconnected timeout is over\r\n");
                    NVIC_SystemReset();
                }
            }
            else
            {
                driver_uart_deinitialize(GSM_UART);
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
                        xSystem.MeasureStatus.batteryPercent = convert_vin_to_percent(xSystem.MeasureStatus.Vin);
                        AdcStop();
                        adcStarted = 0;
                    }
                    if (delay_sleeping_for_exit_wakeup > 0)
                    {

                    }
                    else
                    {
                        bool debugger_connected = (CoreDebug->DHCSR & CoreDebug_DHCSR_C_DEBUGEN_Msk) ? true : false;
                        if (debugger_connected)
                        {

                        }
                        if (debugger_connected)
                        {
                            //dbg_low_power_enable(DBG_LOW_POWER_DEEPSLEEP);
                        }
                        else
                        {
                            uint32_t tick_before_sleep = rtc_counter_get();

                            uint32_t min_interval = xSystem.Parameters.period_send_message_to_server_min;
                            // If input 4-20mA enable && 4-20 measure interval < period_send_message_to_server_min
                            // =>> Select sleeptime = measure interval
                            if (xSystem.Parameters.period_send_message_to_server_min > xSystem.Parameters.period_measure_peripheral
                                && xSystem.Parameters.input.name.ma420)
                            {
                                min_interval = xSystem.Parameters.period_measure_peripheral;
                            }
                            uint32_t sleep_time = estimate_sleep_time_in_second(xSystem.Status.gsm_sleep_time_s,
                                                                                min_interval*60);       // 1 min = 60s

                            DEBUG_PRINTF("Before deep sleep, gsm time %us, estimate sleep time %usec\r\n", 
                                            xSystem.Status.gsm_sleep_time_s, 
                                            sleep_time);
                            sleep_time = 18;  // Due to watchdog
                            rtc_alarm_config(rtc_counter_get() + sleep_time);
                            rtc_lwoff_wait();
                            app_wdt_feed();
                            pmu_to_deepsleepmode(PMU_LDO_LOWPOWER, WFI_CMD);
                            SystemInit();
                            RCC_Config();
                            //rcu_ckout0_config(RCU_CKOUT0SRC_CKSYS);       // already called in RCC_Config
                            app_wdt_feed();

                            uint32_t diff = rtc_counter_get() - tick_before_sleep;
                            xSystem.Status.gsm_sleep_time_s += diff;
                        

                            TimeOut1000ms = diff*1000;
                            TimeOut3000ms += diff*1000;
                            store_measure_result_timeout += diff;
                            Measure420mATick += diff;
                            DEBUG_PRINTF("After sleep, gsm sleep time %us\r\n", 
                                            xSystem.Status.gsm_sleep_time_s);
                        }
                    }
                }
                else
                {
                    DEBUG_PRINTF("Output 4-20mA enable %u\r\n", xSystem.Parameters.outputOnOff);
                    pmu_to_sleepmode(WFI_CMD);
                    xSystem.Status.gsm_sleep_time_s++;
                    store_measure_result_timeout++;
                    Measure420mATick++;
                }
            }
            app_wdt_feed();
            ProcessTimeout1000ms();
        }
        app_cli_poll();
        __WFI();
    }
}

static void ProcessTimeout10ms(void)
{
    measure_input_task();
}

static void ProcessTimeout100ms(void)
{
    hardware_manager_clear_uart_error_flag();
    app_wdt_feed();
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
    //if (xSystem.FileTransfer.State != FT_TRANSFERRING)
    //{
        gsm_manager_tick();
    //    if (gsm_data_layer_is_module_sleeping())
    //    {
    //        //DEBUG_PRINTF("Process timeout 1000ms in sleeping state\r\n");
    //    }
    //    //		ProcessPingTimeout();
    //}
    ////	DownloadFileTick();

    control_ouput_task();
    //LED1(1);
    //LED2(1);
}

static void ProcessTimeOut3000ms(void)
{
    DEBUG_PRINTF("System tick : %u,%u. Vin: %umV\r\n", xSystem.Status.TimeStamp,
                                                       xSystem.MeasureStatus.Vin);
}

void SystemManagementTask(void)
{
    m_sys_tick++;
    TimeOut10ms++;
    TimeOut100ms++;
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
        DEBUG_PRINTF("Delay %ums\r\n", ms);
    }
    TimingDelay = ms;
    while (TimingDelay)
    {
        if (TimingDelay % 5 == 0)
            app_wdt_feed();
        __WFI();
    }
}



uint32_t sys_get_ms(void)
{
    return m_sys_tick;
}

static uint8_t convert_vin_to_percent(uint32_t vin)
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


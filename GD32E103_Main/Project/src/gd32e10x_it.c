/*!
    \file  gd32e10x_it.c
    \brief interrupt service routines
    
    \version 2017-12-26, V1.0.0, firmware for GD32E10x
*/

/*
    Copyright (c) 2017, GigaDevice Semiconductor Inc.

    All rights reserved.

    Redistribution and use in source and binary forms, with or without modification, 
are permitted provided that the following conditions are met:

    1. Redistributions of source code must retain the above copyright notice, this 
       list of conditions and the following disclaimer.
    2. Redistributions in binary form must reproduce the above copyright notice, 
       this list of conditions and the following disclaimer in the documentation 
       and/or other materials provided with the distribution.
    3. Neither the name of the copyright holder nor the names of its contributors 
       may be used to endorse or promote products derived from this software without 
       specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" 
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED 
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. 
IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, 
INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT 
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR 
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, 
WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) 
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY 
OF SUCH DAMAGE.
*/

#include "gd32e10x_it.h"
#include "main.h"
#include "Hardware.h"
#include "DataDefine.h"
#include "GSM.h"
#include "Measurement.h"
#include "stdbool.h"

extern void Delay_Decrement(void);
extern void SystemManagementTask(void);
extern System_t xSystem;
extern uint16_t TimeOut1000ms;
extern uint16_t TimeOut3000ms;

/*!
    \brief      this function handles NMI exception
    \param[in]  none
    \param[out] none
    \retval     none
*/
void NMI_Handler(void)
{
}

///*!
//    \brief      this function handles HardFault exception
//    \param[in]  none
//    \param[out] none
//    \retval     none
//*/
//void HardFault_Handler(void)
//{
//	DEBUG ("\r\n!!!!! HARDFAULT !!!!!");
//	
//    /* if Hard Fault exception occurs, go to infinite loop */
//    while (1){
//    }
//}

/*!
    \brief      this function handles MemManage exception
    \param[in]  none
    \param[out] none
    \retval     none
*/
void MemManage_Handler(void)
{
    /* if Memory Manage exception occurs, go to infinite loop */
    NVIC_SystemReset();
    while (1){
    }
}

/*!
    \brief      this function handles BusFault exception
    \param[in]  none
    \param[out] none
    \retval     none
*/
void BusFault_Handler(void)
{
    /* if Bus Fault exception occurs, go to infinite loop */
    NVIC_SystemReset();
    while (1){
    }
}

/*!
    \brief      this function handles UsageFault exception
    \param[in]  none
    \param[out] none
    \retval     none
*/
void UsageFault_Handler(void)
{
    /* if Usage Fault exception occurs, go to infinite loop */
    while (1){
    }
}

/*!
    \brief      this function handles SVC exception
    \param[in]  none
    \param[out] none
    \retval     none
*/
void SVC_Handler(void)
{
}

/*!
    \brief      this function handles DebugMon exception
    \param[in]  none
    \param[out] none
    \retval     none
*/
void DebugMon_Handler(void)
{
}

/*!
    \brief      this function handles PendSV exception
    \param[in]  none
    \param[out] none
    \retval     none
*/
void PendSV_Handler(void)
{
}

/*!
    \brief      this function handles SysTick exception
    \param[in]  none
    \param[out] none
    \retval     none
*/
void SysTick_Handler(void)
{
    SystemManagementTask();
    Delay_Decrement();
}

/*!
    \brief      this function handles external lines 0 interrupt request
    \param[in]  none
    \param[out] none
    \retval     none
*/
void EXTI0_IRQHandler(void)
{
    /* check the key wakeup is pressed or not */
    if (RESET != exti_interrupt_flag_get(EXTI_0))
    {
        if (getSwitchState() == 0)
        {
            if (isGSMSleeping())
            {
                xSystem.Status.GSMSleepTime = xSystem.Parameters.TGGTDinhKy*60;
                LED1(0);
            }
        }
        exti_interrupt_flag_clear(EXTI_0);
    }
}

/*!
    \brief      this function handles external lines 10 to 15 interrupt request
    \param[in]  none
    \param[out] none
    \retval     none
*/
void EXTI10_15_IRQHandler(void)
{
    /* check the GSM_RI pin */
    if (RESET != exti_interrupt_flag_get(GSM_RI_EXTI_LINE))
    {
        exti_interrupt_flag_clear(GSM_RI_EXTI_LINE);
    }
}

void LVD_IRQHandler(void)
{
    NVIC_SystemReset();
    while (1);
}


/*!
    \brief      this function handles RTC global interrupt request
    \param[in]  none
    \param[out] none
    \retval     none
*/
extern volatile uint32_t StoreMeasureResultTick;
extern uint32_t Measure420mATick;
void RTC_IRQHandler(void)
{    
    if(RESET != rtc_interrupt_flag_get(RTC_INT_FLAG_SECOND))
    {
        //DEBUG_PRINTF("RTC 1s\r\n");
        /* clear the RTC second interrupt flag*/
        rtc_interrupt_flag_clear(RTC_INT_FLAG_SECOND);  
        TimeOut1000ms = 1000;
        TimeOut3000ms += 1000;
        StoreMeasureResultTick++;
        Measure420mATick++;
    }

    if(RESET != rtc_interrupt_flag_get(RTC_INT_FLAG_ALARM))
    {
        /* clear the RTC alarm interrupt flag*/
        rtc_interrupt_flag_clear(RTC_INT_FLAG_ALARM);
        TimeOut1000ms = 1000;
        //DEBUG("RTC alarm\r\n", temp);
    }
}


void RTC_Alarm_IRQHandler()
{
    if (RESET != exti_interrupt_flag_get(EXTI_17))
    {
        exti_interrupt_flag_clear(EXTI_17);
    }

    if(RESET != rtc_interrupt_flag_get(RTC_INT_FLAG_ALARM))
    {
        /* clear the RTC alarm interrupt flag*/
        rtc_interrupt_flag_clear(RTC_INT_FLAG_ALARM);
    }
}

#if 0
void EXTI4_15_IRQHandler(void)
{
    if (RESET != exti_interrupt_flag_get(EXTI_12)) // Phase lost interrupt
    {
        board_hw_reset_phase_lost_timeout();
        exti_interrupt_flag_clear(EXTI_12);
    }
    
    if (RESET != exti_interrupt_flag_get(EXTI_15)) // Contactor interrupt
    {
        board_hw_reset_contactor_monitor_timeout();
        exti_interrupt_flag_clear(EXTI_15);
    }
}
#endif

extern __IO uint16_t ADC_RegularConvertedValueTab[ADCMEM_MAXUNIT];
extern volatile bool new_adc_data;
void ADC0_1_IRQHandler(void)
{
    volatile uint16_t adc_value;
    if(adc_interrupt_flag_get(ADC0, ADC_INT_FLAG_EOC))
    {
        adc_interrupt_flag_clear(ADC0, ADC_INT_FLAG_EOC);
//        adc_value = adc_inserted_data_read(ADC0, ADC_SENS_CHANNEL);
//        ADC_RegularConvertedValueTab[ADCMEM_V20mV] = adc_value;
//        adc_value = adc_inserted_data_read(ADC0, ADC_VIN_CHANNEL);
//        ADC_RegularConvertedValueTab[ADCMEM_VSYS] = adc_value;
//        new_adc_data = true;    
    }
}

void DMA0_Channel0_IRQHandler(void)
{
    if(dma_interrupt_flag_get(DMA0, DMA_CH0, DMA_INT_FLAG_FTF))
    {     
        new_adc_data = 1;
        dma_interrupt_flag_clear(DMA0, DMA_CH0, DMA_INT_FLAG_G);
    }
}

#if 0
void USART1_IRQHandler(void)
{
    if (RESET != usart_interrupt_flag_get(USART1, USART_INT_FLAG_RBNE))
    {
        uint8_t rx = usart_data_receive(USART1);
        extern bool ringbuffer_insert(uint8_t data);
        ringbuffer_insert(rx);
    }
}
#endif

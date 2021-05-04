/*----------------------------------------------------------------------------
 *      RL-ARM - RTX
 *----------------------------------------------------------------------------
 *      Name:    RTX_CONFIG.C
 *      Purpose: Configuration of RTX Kernel for Cortex-M
 *      Rev.:    V4.70
 *----------------------------------------------------------------------------
 *      This code is part of the RealView Run-Time Library.
 *      Copyright (c) 2004-2013 KEIL - An ARM Company. All rights reserved.
 *---------------------------------------------------------------------------*/

#include <RTL.h>
#include <stm32l1xx.h>                  /* STM32L1xx Definitions */
#include <stdio.h>
#include "GSM.h"

/*----------------------------------------------------------------------------
 *      RTX User configuration part BEGIN
 *---------------------------------------------------------------------------*/

//-------- <<< Use Configuration Wizard in Context Menu >>> -----------------
//
// <h>Task Configuration
// =====================
//
//   <o>Number of concurrent running tasks <0-250>
//   <i> Define max. number of tasks that will run at the same time.
//   <i> Default: 6
#ifndef OS_TASKCNT
 #define OS_TASKCNT     7
#endif

//   <o>Number of tasks with user-provided stack <0-250>
//   <i> Define the number of tasks that will use a bigger stack.
//   <i> The memory space for the stack is provided by the user.
//   <i> Default: 0
#ifndef OS_PRIVCNT
 #define OS_PRIVCNT     1
#endif

//   <o>Task stack size [bytes] <20-4096:8><#/4>
//   <i> Set the stack size for tasks which is assigned by the system.
//   <i> Default: 512
#ifndef OS_STKSIZE
 #define OS_STKSIZE     256
#endif

// <q>Check for the stack overflow
// ===============================
// <i> Include the stack checking code for a stack overflow.
// <i> Note that additional code reduces the Kernel performance.
#ifndef OS_STKCHECK
 #define OS_STKCHECK    0
#endif

// <q>Run in privileged mode
// =========================
// <i> Run all Tasks in privileged mode.
// <i> Default: Unprivileged
#ifndef OS_RUNPRIV
 #define OS_RUNPRIV     1
#endif

// </h>
// <h>Tick Timer Configuration
// =============================
//   <o>Hardware timer <0=> Core SysTick <1=> Peripheral Timer
//   <i> Define the on-chip timer used as a time-base for RTX.
//   <i> Default: Core SysTick
#ifndef OS_TIMER
 #define OS_TIMER       0
#endif

//   <o>Timer clock value [Hz] <1-1000000000>
//   <i> Set the timer clock value for selected timer.
//   <i> Default: 6000000  (6MHz)

#define OS_CLOCK       32000000	 	

//   <o>Timer tick value [us] <1-1000000>
//   <i> Set the timer tick value for selected timer.
//   <i> Default: 10000  (10ms)
#ifndef OS_TICK
 #define OS_TICK        1000
#endif

// </h>

// <h>System Configuration
// =======================
// <e>Round-Robin Task switching
// =============================
// <i> Enable Round-Robin Task switching.
#ifndef OS_ROBIN
 #define OS_ROBIN       0
#endif

//   <o>Round-Robin Timeout [ticks] <1-1000>
//   <i> Define how long a task will execute before a task switch.
//   <i> Default: 5
#ifndef OS_ROBINTOUT
 #define OS_ROBINTOUT   5
#endif

// </e>

//   <o>Number of user timers <0-250>
//   <i> Define max. number of user timers that will run at the same time.
//   <i> Default: 0  (User timers disabled)
#ifndef OS_TIMERCNT
 #define OS_TIMERCNT    1
#endif

//   <o>ISR FIFO Queue size<4=>   4 entries  <8=>   8 entries
//                         <12=> 12 entries  <16=> 16 entries
//                         <24=> 24 entries  <32=> 32 entries
//                         <48=> 48 entries  <64=> 64 entries
//                         <96=> 96 entries
//   <i> ISR functions store requests to this buffer,
//   <i> when they are called from the iterrupt handler.
//   <i> Default: 16 entries
#ifndef OS_FIFOSZ
 #define OS_FIFOSZ      16
#endif

// </h>

//------------- <<< end of configuration section >>> -----------------------

// Standard library system mutexes
// ===============================
//  Define max. number system mutexes that are used to protect 
//  the arm standard runtime library. For microlib they are not used.
#ifndef OS_MUTEXCNT
 #define OS_MUTEXCNT    8
#endif

/*----------------------------------------------------------------------------
 *      RTX User configuration part END
 *---------------------------------------------------------------------------*/

#define OS_TRV          ((U32)(((double)OS_CLOCK*(double)OS_TICK)/1E6)-1)

/*----------------------------------------------------------------------------
 *      Global Functions
 *---------------------------------------------------------------------------*/
void RTC_WKUP_IRQHandler (void) 
{
	EXTI->PR = (1 << 22);                 /* Clear pending EXTI interrupt      */
	RTC->CR &= ~RTC_CR_WUTE;              /* Disable wakeup timer              */
}

/*--------------------------- os_idle_demon ---------------------------------*/
extern GSM_Manager_t GSM_Manager;
__task void os_idle_demon (void) {
  /* The idle demon is a system task, running when no other task is ready */
  /* to run. The 'os_xxx' function calls are not allowed from this task.  */
//	uint32_t sleep;

//  SCB->SCR |= SCB_SCR_SLEEPDEEP_Msk;    /* Configure Cortex-M3 for deep sleep */
//  PWR->CR &= ~PWR_CR_PDDS;              /* Enter Stop mode when in deepsleep  */
//  PWR->CR  |= PWR_CR_LPSDSR;              /* Voltage regulator in low-power     */
//  
//  /* Enable LSI clock and wait until ready */
//  RCC->CSR |= RCC_CSR_LSION;
//  while ((RCC->CSR & RCC_CSR_LSIRDY) == 0);
//  
//  /* Enable power interface clock */
//  RCC->APB1ENR |= RCC_APB1ENR_PWREN;
//  
//  /* Disable backup domain write protection */
//  PWR->CR |= PWR_CR_DBP;
//  
//  /* Select LSI as clock source for RTC and enable RTC */
////  RCC->BDCR &= ~RCC_BDCR_RTCSEL;
////  RCC->BDCR |=  RCC_BDCR_RTCSEL_1;
////  RCC->BDCR |=  RCC_BDCR_RTCEN;

//  /* Disable the write protection for RTC registers */
//  RTC->WPR   = 0xCA;
//  RTC->WPR   = 0x53;
//  
//  /* Configure RTC auto-wakeup mode */
//  RTC->ISR  &= ~RTC_ISR_WUTF;           /* Clear wakeup timer flag            */
//  RTC->CR   &= ~RTC_CR_WUCKSEL;         /* Set RTC clock to 2kHz              */
//  RTC->CR   |=  RTC_CR_WUTIE;           /* Enable RTC wakeup timer interrupt  */
//  
//  /* Configure EXTI line 22 for wakeup on rising edge */
//  EXTI->EMR  |= (1 << 22);              /* Event request is not masked        */
//  EXTI->RTSR |= (1 << 22);              /* Rising trigger enabled             */

//  NVIC_EnableIRQ (RTC_WKUP_IRQn);       /* Enable RTC WakeUp IRQ              */
//  
  for (;;) {}
//	/* HERE: include optional user code to be executed when no task runs.*/
//	if(GSM_Manager.GSMReady == 0) goto EXIT_TASK;

//	sleep = os_suspend ();              /* OS Suspend                         */
//    
//    if (sleep) 
//	{
//		RTC->ISR &= ~RTC_ISR_WUTF;        /* Clear timer wakeup flag            */
//		RTC->CR &= ~RTC_CR_WUTE;          /* Disable wakeup timer               */
//		while ((RTC->ISR & RTC_ISR_WUTWF) == 0);

//		/* RTC clock is @2kHz, set wakeup time for OS_TICK >= 1ms */
//		RTC->WUTR = (sleep * (OS_TICK / 1000) * 2);

//		RTC->CR |=  RTC_CR_WUTE;          /* Enable wakeup timer                */
//		__WFE ();                         /* Enter STOP mode - Wait For Event            */

//		/* After Wake-up */
//		if ((RTC->ISR & RTC_ISR_WUTF) == 0) {
//		sleep = 0;                      /* We didn't enter Stop mode          */
//		}
//    }
//    os_resume (sleep);                  /* OS Resume                          */
//	EXIT_TASK: ;
//  }
}

/*--------------------------- os_tick_init ----------------------------------*/

#if (OS_TIMER != 0)
int os_tick_init (void) {
  /* Initialize hardware timer as system tick timer. */
  /* ... */
  return (-1);  /* Return IRQ number of timer (0..239) */
}
#endif

/*--------------------------- os_tick_irqack --------------------------------*/

#if (OS_TIMER != 0)
void os_tick_irqack (void) {
  /* Acknowledge timer interrupt. */
  /* ... */
}
#endif

/*--------------------------- os_tmr_call -----------------------------------*/

void os_tmr_call (U16 info) {
  /* This function is called when the user timer has expired. Parameter   */
  /* 'info' holds the value, defined when the timer was created.          */

  /* HERE: include optional user code to be executed on timeout. */
}


/*--------------------------- os_error --------------------------------------*/

void os_error (U32 err_code) {
	/* This function is called when a runtime error is detected. Parameter */
	/* 'err_code' holds the runtime error code (defined in RTL.H).         */

	/* HERE: include optional code to be executed on runtime error. */
/* This function is called when a runtime error is detected. */
  OS_TID err_task;

	switch (err_code) 
	{
		case OS_ERR_STK_OVF:
		  /* Identify the task with stack overflow. */
		  err_task = isr_tsk_get();
		  break;
		case OS_ERR_FIFO_OVF:
		  break;
		case OS_ERR_MBX_OVF:
		  break;
	}
	printf("\rOS error : %u, Tid: %u",err_code, err_task);
	for (;;);
}


/*----------------------------------------------------------------------------
 *      RTX Configuration Functions
 *---------------------------------------------------------------------------*/

#include <RTX_lib.c>

/*----------------------------------------------------------------------------
 * end of file
 *---------------------------------------------------------------------------*/

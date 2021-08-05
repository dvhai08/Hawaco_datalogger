/*
 * Author: HuyTV
 * 
 */

#include "fault.h"
#ifndef STM32L083xx
#include "gd32e10x.h"
#include "app_debug.h"
#include <stdio.h>

void dump_stack(uint32_t *stack);

void analysis_hard_fault(uint32_t *stack)
{
#ifndef STM32L083xx
    volatile uint32_t CFSR_value = SCB->CFSR;
    // if ((CoreDebug->DHCSR & 0x01) != 0) 
    // {
        DEBUG_PRINTF("Hard fault handler : SCB->HFSR = 0x%08x\r\n", SCB->HFSR);
        if ((SCB->HFSR & (1 << 30)) != 0)
        {
            DEBUG_PRINTF("Forced hard fault : SCB->CFSR = 0x%08x\r\n", SCB->CFSR);
            if ((SCB->CFSR & 0xFFFF0000) != 0)
            {
                DEBUG_PRINTF("Usage fault\r\n");
                uint32_t tmp = CFSR_value;
                tmp >>= 16; // right shift to lsb
                if ((tmp & (1 << 9)) != 0)
                {
                    DEBUG_PRINTF("Divide by zero\r\n");
                }
            }
            if ((SCB->CFSR & 0xFF00) != 0)
            {
                DEBUG_PRINTF("Bus fault\r\n");
            }
            if ((SCB->CFSR & 0xFF) != 0)
            {
                DEBUG_PRINTF("Memory Management fault\r\n");
            }
        }

        dump_stack(stack);
        __ASM volatile("BKPT #01"); // insert breakpoint here
    //  }
#endif
    while (1)
    {

    }
}


#if defined(__CC_ARM)
__asm void HardFault_Handler(void)
{
    TST lr, #4 
    ITE EQ 
    MRSEQ r0, MSP 
    MRSNE r0, PSP 
    B __cpp(analysis_hard_fault)
}
#elif defined(__ICCARM__)
void HardFault_Handler(void)
{
    __asm("TST lr, #4          \n"
          "ITE EQ              \n"
          "MRSEQ r0, MSP       \n"
          "MRSNE r0, PSP       \n"
          "B analysis_hard_fault\n");
}
#else
#warning Not supported compiler type
#endif

enum
{
    r0,
    r1,
    r2,
    r3,
    r12,
    lr,
    pc,
    psr
};

void dump_stack(uint32_t *stack)
{
    DEBUG_PRINTF("R0  = 0x%08x\r\n", stack[r0]);
    DEBUG_PRINTF("R1  = 0x%08x\r\n", stack[r1]);
    DEBUG_PRINTF("R2  = 0x%08x\r\n", stack[r2]);
    DEBUG_PRINTF("R3  = 0x%08x\r\n", stack[r3]);
    DEBUG_PRINTF("R12 = 0x%08x\r\n", stack[r12]);
    DEBUG_PRINTF("LR  = 0x%08x\r\n", stack[lr]);
    DEBUG_PRINTF("Please check function at : pc = 0x%08x\r\n", stack[pc]); /* Here !!!!!!*/
    DEBUG_PRINTF("PSR = 0x%08x\r\n", stack[psr]);
}

#endif


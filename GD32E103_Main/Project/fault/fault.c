/*
 * Author: HuyTV
 * 
 */

#include "fault.h"
#include "gd32e10x.h"
#include "app_debug.h"
#include <stdio.h>

void printErrorMsg(const char * err_msg);
void printUsageErrorMsg(uint32_t CFSR_value);
void printBusFaultErrorMsg(uint32_t CFSR_value);
void printMemoryManagementErrorMsg(uint32_t CFSR_value);
void stackDump(uint32_t stack[]);

void HardFaultHandler(uint32_t stack[])
{
  static char msg[80];
  //if((CoreDebug->DHCSR & 0x01) != 0) {
  printErrorMsg("Hard fault handler\r\n");
  sprintf(msg, "SCB->HFSR = 0x%08x\r\n", SCB->HFSR);
  printErrorMsg(msg);
  if ((SCB->HFSR & (1 << 30)) != 0) 
  {
    printErrorMsg("Forced Hard Fault\n");
    sprintf(msg, "SCB->CFSR = 0x%08x\n", SCB->CFSR );
    printErrorMsg(msg);
    if((SCB->CFSR & 0xFFFF0000) != 0) 
    {
      printUsageErrorMsg(SCB->CFSR);
    } 
    if((SCB->CFSR & 0xFF00) != 0) 
    {
      printBusFaultErrorMsg(SCB->CFSR);
    }
    if((SCB->CFSR & 0xFF) != 0) 
    {
      printMemoryManagementErrorMsg(SCB->CFSR);
    }      
  }  
  stackDump(stack);
  __ASM volatile("BKPT #01");   // insert breakpoint here
  //}
   while(1);
}

void printErrorMsg(const char * err_msg)
{
	DEBUG_PRINTF("%s", err_msg);
//   while(*err_msg != '\0')
//	 {
//      ITM_SendChar(*err_msg);
//      ++err_msg;
//   }
}

void printUsageErrorMsg(uint32_t CFSR_value)
{
   printErrorMsg("Usage fault: ");
   CFSR_value >>= 16; // right shift to lsb
   if((CFSR_value & (1<<9)) != 0) 
   {
      printErrorMsg("Divide by zero\r\n");
   }
}

void printBusFaultErrorMsg(uint32_t CFSR_value)
{
   printErrorMsg("Bus fault: ");
   CFSR_value = ((CFSR_value & 0x0000FF00) >> 8); // mask and right shift to lsb
}

void printMemoryManagementErrorMsg(uint32_t CFSR_value)
{
   printErrorMsg("Memory Management fault: ");
   CFSR_value &= 0x000000FF; // mask just mem faults
}

#if defined(__CC_ARM)
__asm void HardFault_Handler(void)
{
   TST lr, #4
   ITE EQ
   MRSEQ r0, MSP
   MRSNE r0, PSP
   B __cpp(HardFaultHandler)
}
#elif defined(__ICCARM__)
void HardFault_Handler(void)
{
   __asm(   "TST lr, #4          \n"
            "ITE EQ              \n"
            "MRSEQ r0, MSP       \n"
            "MRSNE r0, PSP       \n"
            "B HardFaultHandler\n"
   );
}
#else
  #warning Not supported compiler type
#endif

enum { r0, r1, r2, r3, r12, lr, pc, psr };

void stackDump(uint32_t stack[])
{
  static char msg[80];
  sprintf(msg, "r0  = 0x%08x\n", stack[r0]);
  printErrorMsg(msg);
  sprintf(msg, "r1  = 0x%08x\n", stack[r1]);
  printErrorMsg(msg);
  sprintf(msg, "r2  = 0x%08x\n", stack[r2]);
  printErrorMsg(msg);
  sprintf(msg, "r3  = 0x%08x\n", stack[r3]);
  printErrorMsg(msg);
  sprintf(msg, "r12 = 0x%08x\n", stack[r12]);
  printErrorMsg(msg);
  sprintf(msg, "lr  = 0x%08x\n", stack[lr]);
  printErrorMsg(msg);
  sprintf(msg, "Please check function at : pc  = 0x%08x\n", stack[pc]);    /* Here !!!!!!*/
  printErrorMsg(msg);
  sprintf(msg, "psr = 0x%08x\n", stack[psr]);
  printErrorMsg(msg);
}

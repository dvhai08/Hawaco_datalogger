/*
 * Author: HuyTV
 */

#ifndef __FAULT_H__
#define __FAULT_H__

#include <stdint.h>

void printErrorMsg(const char * err_msg);
void printUsageErrorMsg(uint32_t CFSR_value);
void printBusFaultErrorMsg(uint32_t CFSR_value);
void printMemoryManagementErrorMsg(uint32_t CFSR_value);
void stackDump(uint32_t stack[]);

#endif /* __FAULT_H__ */

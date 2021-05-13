#ifndef __INIT_SYSTEM_H__
#define __INIT_SYSTEM_H__

#include "stdint.h"

void InitSystem(void) ;
void ADC_Config(void);
void AdcStop(void);
uint32_t AdcUpdate(void);

#endif // __INIT_SYSTEM_H__


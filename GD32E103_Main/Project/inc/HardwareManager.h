#ifndef __HARDWARE_MANAGER_H__
#define __HARDWARE_MANAGER_H__

#include "DataDefine.h"

void CoreManagementTick(void);
void DetectHardwareVersion(void) ;
void DetectResetReason(void) ;
void SystemReset(uint8_t NguyenNhanReset);
void WatchDogInit(void);
void ResetWatchdog(void);
void Hardware_XoaCoLoi(void);


#endif // __HARDWARE_MANAGER_H__


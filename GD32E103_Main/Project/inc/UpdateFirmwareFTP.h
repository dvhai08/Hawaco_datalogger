#ifndef __UPDATE_FW_FTP_H__
#define __UPDATE_FW_FTP_H__

#include "stdint.h"
#include "stdint.h"
#include <stdio.h>
#include <string.h>
#include <Net_Config.h>
#include "DataDefine.h"
#include "FTP.h"
#include "gsm.h"
#include "InternalFlash.h"
#include "HardwareManager.h"
#include "gsm_utilities.h"
#include "Main.h"

#define UPDATE_MAINFW 3
#define UPDATE_BOOTLOADER 4

void DownloadFileTick(void);
void DownloadFileInit(void);
void ProcessUpdateFirmwareCommand(char *Buffer, uint8_t fromSource);
void UDFW_UpdateMainFWTest(uint8_t source);

#endif // __UPDATE_FW_FTP_H__


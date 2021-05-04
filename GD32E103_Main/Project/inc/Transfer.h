#ifndef __TRANSFER_H__
#define __TRANSFER_H__

#include <stdint.h>
#include "DataDefine.h"

typedef enum {
	BT_YEU_CAU_DU_LIEU = 1,
	BT_BAT_DAU_UPDATE = 2,
	BT_DU_LIEU_UPDATE = 3,
	BT_KET_THUC_UPDATE = 4,
	BT_THONG_SO = 5,
	BT_TRANG_THAI_GHE = 6,
	BT_CAU_HINH = 7
} LoaiBanTin_List;

typedef struct {
	uint8_t TestLEDTime;
	uint8_t ReceiveLEDTime;
	uint8_t SendDataDelay;
} BlackBoxManager_t;

void ESP32_UART_Tick(void);
void ESP32_ManagerTick(void);

uint8_t BLK_ValidMessage(uint8_t *BLBMessage, uint8_t Length); 
uint8_t BLK_ValidSecureCode(uint8_t *Buffer);
uint8_t BAPC_AddHeader(void);
uint8_t BAPC_AddSecureCode(uint8_t Index);

#endif	/* __TRANSFER_H__ */

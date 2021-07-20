#include <string.h>
#include "stm32l0xx_hal.h"
#include "stm32l0xx_ll_rtc.h"
#include "stm32l0xx_ll_pwr.h"
#include "app_bkup.h"
#include "main.h"
#include "app_debug.h"
#include "rtc.h"

#define	BACKUP_FLAG_ADDR	        BKP_DATA_0
#define BACKUP_PULSE_COUNTER_ADDR1	BKP_DATA_1
#define BACKUP_PULSE_COUNTER_ADDR2	BKP_DATA_2

#define	BACKUP_FLAG_VALUE	0xF1A6

static uint32_t m_counter0_f = 0, m_counter1_f = 0, m_counter0_r = 0, m_counter1_r = 0;

void app_bkup_init(void)
{
    
}

#ifdef GD32E10X
uint16_t app_bkup_read_data(bkp_data_register_enum regAddr)
{

	return bkp_data_read(regAddr);
}

void app_bkup_write_data(bkp_data_register_enum regAddr, uint16_t data)
{
	bkp_data_write(regAddr, data);
}
#endif /* GD32E10X */

void app_bkup_write_pulse_counter(uint32_t counter0_f, uint32_t counter1_f, uint32_t counter0_r, uint32_t counter1_r)
{
#ifdef GD32E10X
	// Write flag
	bkp_data_write(BACKUP_FLAG_ADDR, BACKUP_FLAG_VALUE);
	
	// Write data
	bkp_data_write(BACKUP_PULSE_COUNTER_ADDR1, (counter>>16) & 0xFFFF);
	bkp_data_write(BACKUP_PULSE_COUNTER_ADDR2, counter & 0xFFFF);
#else
    
//    __HAL_RTC_WRITEPROTECTION_DISABLE(&hrtc);
//    HAL_PWR_EnableBkUpAccess();
//    HAL_RTCEx_BKUPWrite(&hrtc, LL_RTC_BKP_DR0, RTC_BACKUP_VALID_DATA);
//    HAL_RTCEx_BKUPWrite(&hrtc, RTC_BKP_DR1, counter0_f);
//	HAL_RTCEx_BKUPWrite(&hrtc, LL_RTC_BKP_DR2, counter1_f);
//	HAL_RTCEx_BKUPWrite(&hrtc, LL_RTC_BKP_DR2, counter0_r);
//	HAL_RTCEx_BKUPWrite(&hrtc, LL_RTC_BKP_DR3, counter1_r);
////	LL_PWR_DisableBkUpAccess();
//    
//    
//    // Debug
//    uint32_t tmp[4];
//    tmp[0] = HAL_RTCEx_BKUPRead(&hrtc, LL_RTC_BKP_DR1);
//    tmp[1] = HAL_RTCEx_BKUPRead(&hrtc, LL_RTC_BKP_DR2);
//    tmp[2] = HAL_RTCEx_BKUPRead(&hrtc, LL_RTC_BKP_DR3);
//    tmp[3] = HAL_RTCEx_BKUPRead(&hrtc, LL_RTC_BKP_DR4);
//    __HAL_RTC_WRITEPROTECTION_ENABLE(&hrtc);
//    HAL_PWR_DisableBkUpAccess();
////    LL_RTC_EnableWriteProtection(RTC);
//    if (counter0_f != tmp[0]
//        || counter1_f != tmp[1]
//        || counter0_r != tmp[2]
//        || counter1_r != tmp[3])
//    {
//        DEBUG_ERROR("Write backup data error\r\n");
//    }
//    else
//    {
//        DEBUG_INFO("Write backup data success\r\n");
//    }
    
#endif

    m_counter0_f = counter0_f;
    m_counter1_f = counter1_f;
    m_counter0_r = counter0_r;
    m_counter1_r = counter1_r;
}

void app_bkup_read_pulse_counter(uint32_t *counter0_f, uint32_t *counter1_f, uint32_t *counter0_r, uint32_t *counter1_r)
{
////    LL_PWR_EnableBkUpAccess();
////    LL_RTC_DisableWriteProtection(RTC);
//    if (RTC_BACKUP_VALID_DATA == HAL_RTCEx_BKUPRead(&hrtc, LL_RTC_BKP_DR0))
//	{
//        *counter0_f = HAL_RTCEx_BKUPRead(&hrtc, LL_RTC_BKP_DR1);
//		*counter1_f = HAL_RTCEx_BKUPRead(&hrtc, LL_RTC_BKP_DR2);
//		*counter0_r = HAL_RTCEx_BKUPRead(&hrtc, LL_RTC_BKP_DR3);
//		*counter1_r = HAL_RTCEx_BKUPRead(&hrtc, LL_RTC_BKP_DR4);
//	}
//	else
//	{
//		*counter0_f = 0;
//		*counter1_f = 0;
//		*counter0_r = 0;
//		*counter1_r = 0;
//        DEBUG_WARN("No data in backup\r\n");
//	}
////    LL_RTC_EnableWriteProtection(RTC);
////    LL_PWR_DisableBkUpAccess();
    *counter0_f = m_counter0_f;
    *counter1_f = m_counter1_f;
    *counter0_r = m_counter0_r;
    *counter1_r = m_counter1_r;
}

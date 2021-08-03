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
#define USE_RTC_BACKUP      1

#if USE_RTC_BACKUP == 0
static uint32_t m_counter0_f = 0, m_counter1_f = 0, m_counter0_r = 0, m_counter1_r = 0;
#endif

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
#if USE_RTC_BACKUP
//    LL_RTC_DisableWriteProtection(RTC);
//    HAL_PWR_EnableBkUpAccess();
//    LL_RTC_DisableWriteProtection(RTC);
	uint32_t backup_data = RTC_BACKUP_VALID_DATA & 0x0000FFFF;
    HAL_RTCEx_BKUPWrite(&hrtc, LL_RTC_BKP_DR0, backup_data);
    HAL_RTCEx_BKUPWrite(&hrtc, LL_RTC_BKP_DR1, counter0_f);
	HAL_RTCEx_BKUPWrite(&hrtc, LL_RTC_BKP_DR2, counter1_f);
	HAL_RTCEx_BKUPWrite(&hrtc, LL_RTC_BKP_DR3, counter0_r);
	HAL_RTCEx_BKUPWrite(&hrtc, LL_RTC_BKP_DR4, counter1_r);
//	LL_PWR_DisableBkUpAccess();
    
    
    // Debug
    uint32_t tmp[4];
    tmp[0] = HAL_RTCEx_BKUPRead(&hrtc, LL_RTC_BKP_DR1);
    tmp[1] = HAL_RTCEx_BKUPRead(&hrtc, LL_RTC_BKP_DR2);
    tmp[2] = HAL_RTCEx_BKUPRead(&hrtc, LL_RTC_BKP_DR3);
    tmp[3] = HAL_RTCEx_BKUPRead(&hrtc, LL_RTC_BKP_DR4);
    LL_RTC_EnableWriteProtection(RTC);
    if (counter0_f != tmp[0]
        || counter1_f != tmp[1]
        || counter0_r != tmp[2]
        || counter1_r != tmp[3])
    {
        DEBUG_ERROR("Write backup data error\r\n");
    }
    else
    {
        DEBUG_VERBOSE("Write backup data success\r\n");
    }
#else
    m_counter0_f = counter0_f;
    m_counter1_f = counter1_f;
    m_counter0_r = counter0_r;
    m_counter1_r = counter1_r;    
#endif
}

void app_bkup_read_pulse_counter(uint32_t *counter0_f, uint32_t *counter1_f, uint32_t *counter0_r, uint32_t *counter1_r)
{
#if USE_RTC_BACKUP
//    LL_PWR_EnableBkUpAccess();
//    LL_RTC_DisableWriteProtection(RTC);
	uint32_t backup = HAL_RTCEx_BKUPRead(&hrtc, LL_RTC_BKP_DR0) & 0x0000FFFF;
    if ((RTC_BACKUP_VALID_DATA & 0x0000FFFF) == backup)
	{
        *counter0_f = HAL_RTCEx_BKUPRead(&hrtc, LL_RTC_BKP_DR1);
		*counter1_f = HAL_RTCEx_BKUPRead(&hrtc, LL_RTC_BKP_DR2);
		*counter0_r = HAL_RTCEx_BKUPRead(&hrtc, LL_RTC_BKP_DR3);
		*counter1_r = HAL_RTCEx_BKUPRead(&hrtc, LL_RTC_BKP_DR4);
	}
	else
	{
		*counter0_f = 0;
		*counter1_f = 0;
		*counter0_r = 0;
		*counter1_r = 0;
        backup = 0;
		HAL_RTCEx_BKUPWrite(&hrtc, LL_RTC_BKP_DR0, 0);
		DEBUG_WARN("No data in backup\r\n");
	}
//    LL_RTC_EnableWriteProtection(RTC);
//    LL_PWR_DisableBkUpAccess();
#else
    *counter0_f = m_counter0_f;
    *counter1_f = m_counter1_f;
    *counter0_r = m_counter0_r;
    *counter1_r = m_counter1_r;
#endif
}


uint32_t app_bkup_read_nb_of_wakeup_time(void)
{
	uint32_t backup = HAL_RTCEx_BKUPRead(&hrtc, LL_RTC_BKP_DR0) & 0x0000FFFF;
	if ((RTC_BACKUP_VALID_DATA & 0x0000FFFF) == backup)
	{
		return backup >> 16;
	}
	else
	{
        return 0;
	}
}

void app_bkup_write_nb_of_wakeup_time(uint32_t wake_time)
{
	uint32_t backup_data = HAL_RTCEx_BKUPRead(&hrtc, LL_RTC_BKP_DR0) & 0x0000FFFF;
	backup_data |= (wake_time << 16);
	HAL_RTCEx_BKUPWrite(&hrtc, LL_RTC_BKP_DR0, backup_data);
}


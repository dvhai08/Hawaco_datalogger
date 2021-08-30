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
static uint32_t m_counter[0].forward = 0, m_counter[1].forward = 0, m_counter[0].reserve = 0, m_counter[1].reserve = 0;
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

void app_bkup_write_pulse_counter(measure_input_counter_t *counter)
{
#if USE_RTC_BACKUP
	uint32_t val = HAL_RTCEx_BKUPRead(&hrtc, LL_RTC_BKP_DR0) & 0xFFFF0000;
	uint32_t backup_data = RTC_BACKUP_VALID_DATA & 0x0000FFFF;
	val |= backup_data;
	
    HAL_RTCEx_BKUPWrite(&hrtc, LL_RTC_BKP_DR0, val);
    HAL_RTCEx_BKUPWrite(&hrtc, LL_RTC_BKP_DR1, counter[0].forward);
	HAL_RTCEx_BKUPWrite(&hrtc, LL_RTC_BKP_DR3, counter[0].reserve);
#if defined(DTG02) || defined(DTG02V2)
	HAL_RTCEx_BKUPWrite(&hrtc, LL_RTC_BKP_DR2, counter[1].forward);
	HAL_RTCEx_BKUPWrite(&hrtc, LL_RTC_BKP_DR4, counter[1].reserve);
#endif
    // Debug
    uint32_t tmp[4];
    tmp[0] = HAL_RTCEx_BKUPRead(&hrtc, LL_RTC_BKP_DR1);
    tmp[1] = HAL_RTCEx_BKUPRead(&hrtc, LL_RTC_BKP_DR2);
    tmp[2] = HAL_RTCEx_BKUPRead(&hrtc, LL_RTC_BKP_DR3);
    tmp[3] = HAL_RTCEx_BKUPRead(&hrtc, LL_RTC_BKP_DR4);
	
	uint32_t valid = 1;
	
    LL_RTC_EnableWriteProtection(RTC);
#if defined(DTG02) || defined(DTG02V2)
    if (counter[0].forward != tmp[0]
		|| counter[0].reserve != tmp[2]
        || counter[1].forward != tmp[1]
        || counter[1].reserve != tmp[3])
	{
		valid = 0;
	}
#else
	if (counter[0].forward != tmp[0]
		|| counter[0].reserve != tmp[2])
	{
		valid = 0;
	}
#endif
	if (valid == 0)
    {
        DEBUG_ERROR("Write backup data error\r\n");
    }
#else
    m_counter[0].forward = counter[0].forward;
    m_counter[1].forward = counter[1].forward;
    m_counter[0].reserve = counter[0].reserve;
    m_counter[1].reserve = counter[1].reserve;    
#endif
}

void app_bkup_read_pulse_counter(measure_input_counter_t *counter)
{
#if USE_RTC_BACKUP
	uint32_t backup = HAL_RTCEx_BKUPRead(&hrtc, LL_RTC_BKP_DR0) & 0x0000FFFF;
    if ((RTC_BACKUP_VALID_DATA & 0x0000FFFF) == backup)
	{
        counter[0].forward = HAL_RTCEx_BKUPRead(&hrtc, LL_RTC_BKP_DR1);
		counter[0].reserve = HAL_RTCEx_BKUPRead(&hrtc, LL_RTC_BKP_DR3);
#if defined(DTG02) || defined(DTG02V2)
		counter[1].reserve = HAL_RTCEx_BKUPRead(&hrtc, LL_RTC_BKP_DR4);
		counter[1].forward = HAL_RTCEx_BKUPRead(&hrtc, LL_RTC_BKP_DR2);
#endif
	}
	else
	{
		counter[0].forward = 0;
		counter[0].reserve = 0;
#if defined(DTG02) || defined(DTG02V2)
		counter[1].reserve = 0;
		counter[1].forward = 0;
#endif
        backup = 0;
		HAL_RTCEx_BKUPWrite(&hrtc, LL_RTC_BKP_DR0, 0);
		DEBUG_WARN("No data in backup\r\n");
	}
#else
    counter[0].forward = m_counter[0].forward;
    counter[1].forward = m_counter[1].forward;
    counter[0].reserve = m_counter[0].reserve;
    counter[1].reserve = m_counter[1].reserve;
#endif
}


uint32_t app_bkup_read_nb_of_wakeup_time(void)
{
	uint32_t val = HAL_RTCEx_BKUPRead(&hrtc, LL_RTC_BKP_DR0);
	uint32_t backup = val & 0x0000FFFF;
	if ((RTC_BACKUP_VALID_DATA & 0x0000FFFF) == backup)
	{
		return val >> 16;
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


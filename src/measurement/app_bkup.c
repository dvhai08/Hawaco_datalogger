#include <string.h>
#ifdef GD32E10X
#include "gd32e10x.h"
#else
#include "stm32l0xx_hal.h"
#include "stm32l0xx_ll_rtc.h"
#include "stm32l0xx_ll_pwr.h"
#endif
#include "app_bkup.h"
#include "DataDefine.h"

#define	BACKUP_FLAG_ADDR	        BKP_DATA_0
#define BACKUP_PULSE_COUNTER_ADDR1	BKP_DATA_1
#define BACKUP_PULSE_COUNTER_ADDR2	BKP_DATA_2

#define	BACKUP_FLAG_VALUE	0xF1A6


void app_bkup_init(void)
{
#ifdef GD32E10X
    /* PMU clock enable */
    rcu_periph_clock_enable(RCU_PMU);
    /* BKP clock enable */
    rcu_periph_clock_enable(RCU_BKPI);
	
    /* Enable write access to the registers in backup domain */
    pmu_backup_write_enable();
#endif
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

void app_bkup_write_pulse_counter(uint32_t counter0, uint32_t counter1)
{
#ifdef GD32E10X
	// Write flag
	bkp_data_write(BACKUP_FLAG_ADDR, BACKUP_FLAG_VALUE);
	
	// Write data
	bkp_data_write(BACKUP_PULSE_COUNTER_ADDR1, (counter>>16) & 0xFFFF);
	bkp_data_write(BACKUP_PULSE_COUNTER_ADDR2, counter & 0xFFFF);
#else
    LL_PWR_EnableBkUpAccess();
    LL_RTC_BAK_SetRegister(RTC, LL_RTC_BKP_DR1, counter0);
	LL_RTC_BAK_SetRegister(RTC, LL_RTC_BKP_DR2, counter1);
#endif
}

void app_bkup_read_pulse_counter(uint32_t *counter0, uint32_t *counter1)
{
#ifdef GD32E10X
	uint16_t flag = bkp_data_read(BACKUP_FLAG_ADDR);
	if (flag == BACKUP_FLAG_VALUE)
	{
		uint16_t data1 = bkp_data_read(BACKUP_PULSE_COUNTER_ADDR1);
		uint16_t data2 = bkp_data_read(BACKUP_PULSE_COUNTER_ADDR2);
		return ((data1<<16) | data2);
	}
	else
	{
		DEBUG_PRINTF("BKUP: Flag not found!\r\n");
	}
	return 0;
#else
    if (RTC_BACKUP_VALID_DATA == LL_RTC_BAK_GetRegister(RTC, LL_RTC_BKP_DR0))
	{
        *counter0 = LL_RTC_BAK_GetRegister(RTC, LL_RTC_BKP_DR1);
		*counter1 = LL_RTC_BAK_GetRegister(RTC, LL_RTC_BKP_DR2);
	}
	else
	{
		*counter0 = 0;
		*counter1 = 0;
	}
#endif
}

#include <string.h>
#include "gd32e10x.h"
#include "app_bkup.h"
#include "DataDefine.h"

#define	BACKUP_FLAG_ADDR	BKP_DATA_0
#define BACKUP_PULSE_COUNTER_ADDR1	BKP_DATA_1
#define BACKUP_PULSE_COUNTER_ADDR2	BKP_DATA_2

#define	BACKUP_FLAG_VALUE	0xF1A6

/*!
    \brief      enable the peripheral clock
    \param[in]  none
    \param[out] none
    \retval     none
*/
void app_bkup_init(void)
{
    /* PMU clock enable */
    rcu_periph_clock_enable(RCU_PMU);
    /* BKP clock enable */
    rcu_periph_clock_enable(RCU_BKPI);
	
	 /* enable write access to the registers in backup domain */
    pmu_backup_write_enable();
}

uint16_t app_bkup_read_data(bkp_data_register_enum regAddr)
{
	return bkp_data_read(regAddr);
}

void app_bkup_write_data(bkp_data_register_enum regAddr, uint16_t data)
{
	bkp_data_write(regAddr, data);
}

void app_bkup_write_pulse_counter(uint32_t counter)
{
	//write flag
	bkp_data_write(BACKUP_FLAG_ADDR, BACKUP_FLAG_VALUE);
	
	//write data
	bkp_data_write(BACKUP_PULSE_COUNTER_ADDR1, (counter>>16) & 0xFFFF);
	bkp_data_write(BACKUP_PULSE_COUNTER_ADDR2, counter & 0xFFFF);
}

uint32_t app_bkup_read_pulse_counter(void)
{
	uint16_t flag = bkp_data_read(BACKUP_FLAG_ADDR);
	if(flag == BACKUP_FLAG_VALUE)
	{
		uint16_t data1 = bkp_data_read(BACKUP_PULSE_COUNTER_ADDR1);
		uint16_t data2 = bkp_data_read(BACKUP_PULSE_COUNTER_ADDR2);
		return ((data1<<16) | data2);
	}
	else
	{
		DEBUG ("\r\nBKUP: Flag not found!");
	}
	return 0;
}

#ifndef __APP_BKUP_H__
#define __APP_BKUP_H__

#include <stdint.h>
#include <stdbool.h>

void app_bkup_init(void);
void app_bkup_write_pulse_counter(uint32_t counter);
uint32_t app_bkup_read_pulse_counter(void);

#endif /* __APP_BKUP_H__ */

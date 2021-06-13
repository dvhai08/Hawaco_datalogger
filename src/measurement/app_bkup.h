#ifndef APP_BKUP_H
#define APP_BKUP_H

#include <stdint.h>
#include <stdbool.h>

/**
 *  @brief      Init RTC module to backup measurement sensor data
 */
void app_bkup_init(void);

/**
 * @brief       Write pulse counter to RTC memory region
 * @param[in]   counter New counter data to store in RTC memory
 */
void app_bkup_write_pulse_counter(uint32_t counter);

/**
 * @brief       Read pulse counter from RTC memory region
 * @retval      Current counter
 */
uint32_t app_bkup_read_pulse_counter(void);


#endif /* APP_BKUP_H */

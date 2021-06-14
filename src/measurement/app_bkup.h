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
 * @param[in]   counter0 New counter data of water meter port 0
 * @param[in]   counter1 New counter data of water meter port 1
 */
void app_bkup_write_pulse_counter(uint32_t counter0, uint32_t counter1);

/**
 * @brief       Read pulse counter from RTC memory region
 * @param[out]   counter0 Current counter data of water meter port 0
 * @param[out]   counter1 Current counter data of water meter port 1
 */
void app_bkup_read_pulse_counter(uint32_t *counter0, uint32_t *counter1);


#endif /* APP_BKUP_H */

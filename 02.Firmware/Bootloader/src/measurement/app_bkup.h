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
 * @param[in]   counter0 New counter data of water meter port 0 : forware direction
 * @param[in]   counter1 New counter data of water meter port 1	: forware direction
 * @param[in]   counter0 New counter data of water meter port 0 : reserve direction
 * @param[in]   counter1 New counter data of water meter port 1	: reserve direction
 */
void app_bkup_write_pulse_counter(uint32_t counter0_f, uint32_t counter1_f, uint32_t counter0_r, uint32_t counter1_r);

/**
 * @brief       	Read pulse counter from RTC memory region
 * @param[out]		counter0_f Current counter forward direction data of water meter port 0
 * @param[out]		counter1_f Current counter forward direction data of water meter port 1
  * @param[out]		counter0_r Current counter reserve direction data of water meter port 0
 * @param[out]		counter1_r Current counter reserve direction data of water meter port 1
 */
void app_bkup_read_pulse_counter(uint32_t *counter0_f, uint32_t *counter1_f, uint32_t *counter0_r, uint32_t *counter1_r);


#endif /* APP_BKUP_H */

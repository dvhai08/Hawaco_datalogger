#ifndef APP_BKUP_H
#define APP_BKUP_H

#include <stdint.h>
#include <stdbool.h>

#include "hardware.h"

/**
 *  @brief      Init RTC module to backup measurement sensor data
 */
void app_bkup_init(void);

/*!
 * @brief       Write pulse counter to RTC memory region
 * @param[in]   counter0 New counter data of water meter port 0 : forware direction
 * @param[in]   counter1 New counter data of water meter port 1	: forware direction
 * @param[in]   counter0 New counter data of water meter port 0 : reserve direction
 * @param[in]   counter1 New counter data of water meter port 1	: reserve direction
 */
void app_bkup_write_pulse_counter(measure_input_counter_t *counter);

/*!
 * @brief       	Read pulse counter from RTC memory region
 * @param[out]		counter0_f Current counter forward direction data of water meter port 0
 * @param[out]		counter1_f Current counter forward direction data of water meter port 1
 * @param[out]		counter0_r Current counter reserve direction data of water meter port 0
 * @param[out]		counter1_r Current counter reserve direction data of water meter port 1
 */
void app_bkup_read_pulse_counter(measure_input_counter_t *counter);

/*!
 * @brief       	Read number of wakeup time
 */
uint32_t app_bkup_read_nb_of_wakeup_time(void);

/*!
 * @brief       	Write number of wakeup time
 * @param[in]			Number of wakeup time
 */
void app_bkup_write_nb_of_wakeup_time(uint32_t wake_time);

#endif /* APP_BKUP_H */

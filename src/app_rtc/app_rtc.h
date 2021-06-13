#ifndef APP_RTC_H
#define APP_RTC_H

#include <stdint.h>
#include <stdbool.h>

#define rtc_counter_set     app_rtc_set_counter
#define rtc_counter_get     app_rtc_get_counter

/**
 * @brief          Set counter to RTC
 * @param[in]      counter RTC counter value
 */
void app_rtc_set_counter(uint32_t counter);


/**
 * @brief          Get counter from RTC
 * @retval         RTC counter value
 */
uint32_t app_rtc_get_counter(void);


#endif /* APP_RTC_H */

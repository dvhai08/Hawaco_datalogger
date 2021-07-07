#ifndef APP_RTC_H
#define APP_RTC_H

#include <stdint.h>
#include <stdbool.h>

#define rtc_counter_set     app_rtc_set_counter
#define rtc_counter_get     app_rtc_get_counter


typedef struct
{
    uint8_t year;       // From 2000
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
} rct_date_time_t;

/**
 * @brief          Set counter to RTC
 * @param[in]      counter RTC counter value
 */
void app_rtc_set_counter(rct_date_time_t *date_time);


/**
 * @brief          Get counter from RTC
 * @retval         RTC counter value
 */
uint32_t app_rtc_get_counter(void);


#endif /* APP_RTC_H */

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
} rtc_date_time_t;

/*!
 * @brief          Set counter to RTC
 * @param[in]      counter RTC counter value
 */
void app_rtc_set_counter(rtc_date_time_t *date_time);


/*!
 * @brief          Get counter from RTC
 * @retval         RTC counter value
 */
uint32_t app_rtc_get_counter(void);

/*!
 * @brief          Get time from RTC
 * @retval         TRUE : Valid time
*                 FALSE : Invalid time
 */
bool app_rtc_get_time(rtc_date_time_t *time);

/*!
 * @brief          Get subsecond counter from RTC
 * @retval         RTC subsecond counter value
 */
uint32_t app_rtc_get_subsecond_counter(void);

/*!
 * @brief          Convert rtc date time to unix counter
 * @retval         RTC counter
 */
uint32_t rtc_struct_to_counter(rtc_date_time_t *t);

#endif /* APP_RTC_H */

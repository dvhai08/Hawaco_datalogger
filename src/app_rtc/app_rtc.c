#include "app_rtc.h"
#include "main.h"
#include "stm32l0xx_ll_rtc.h"
#include "main.h"
#include "app_debug.h"

extern RTC_HandleTypeDef hrtc;

static uint32_t rtc_struct_to_counter(rtc_date_time_t *t)
{
    static const uint8_t days_in_month[] = {31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    uint16_t i;
    uint32_t result = 0;
    uint16_t idx, year;

    year = t->year + 2000;

    /* Calculate days of years before */
    result = (uint32_t)year * 365;
    if (t->year >= 1)
    {
        result += (year + 3) / 4;
        result -= (year - 1) / 100;
        result += (year - 1) / 400;
    }

    /* Start with 2000 a.d. */
    result -= 730485UL;

    /* Make month an array index */
    idx = t->month - 1;

    /* Loop thru each month, adding the days */
    for (i = 0; i < idx; i++)
    {
        result += days_in_month[i];
    }

    /* Leap year? adjust February */
    if (year % 400 == 0 || (year % 4 == 0 && year % 100 != 0))
    {
    }
    else
    {
        if (t->month > 2)
        {
            result--;
        }
    }

    /* Add remaining days */
    result += t->day;

    /* Convert to seconds, add all the other stuff */
    result = (result - 1) * 86400L + (uint32_t)t->hour * 3600 +
             (uint32_t)t->minute * 60 + t->second;

    return result;
}

/*
 * https://stackoverflow.com/questions/6054016/c-program-to-find-day-of-week-given-date
 * https://en.wikipedia.org/wiki/Determination_of_the_day_of_the_week#Implementation-dependent_methods
 * Day     1-31
   Month   1-12
   Year    2013
   retval weekday 1-7, 7 = sun, 1 = mon, ... 6 = sat
 */
static void get_weekday(int year, int month, int day, int *weekday)
{
	// weekday 0-6, 0 = sun, 1 = mon, ... 6 = sat
    *weekday  = (day += month < 3 ? year-- : year - 2, 23*month/9 + day + 4 + year/4- year/100 + year/400)%7;  
	// Convert to Mon : 1, sun 7
	if (*weekday == 0)
	{
		*weekday = 7;
	}
}

//static char *weekday_str(int weekday)
//{
//    if (weekday >= 7)
//    {
//        return "null";
//    }

//    static char *day_str[] = {"sun", "mon", "tue", "wed", "thu", "fri", "sat"};
//    return day_str[weekday];
//}


void app_rtc_set_counter(rtc_date_time_t *time)
{
	RTC_TimeTypeDef sTime = {0};
	RTC_DateTypeDef sDate = {0};

//    LL_PWR_EnableBkUpAccess();

	/** Initialize RTC and set the Time and Date
	*/
	sTime.Hours = __LL_RTC_CONVERT_BIN2BCD(time->hour);
	sTime.Minutes = __LL_RTC_CONVERT_BIN2BCD(time->minute);
	sTime.Seconds = __LL_RTC_CONVERT_BIN2BCD(time->second);
	sTime.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
	sTime.StoreOperation = RTC_STOREOPERATION_SET;
	if (HAL_RTC_SetTime(&hrtc, &sTime, RTC_FORMAT_BCD) != HAL_OK)
	{
        DEBUG_ERROR("Set RTC time failed\r\n");
		Error_Handler();
	}
	int week_day;
	get_weekday(time->year + 2000, time->month, time->day, &week_day);
	week_day %= 7;
	sDate.WeekDay = __LL_RTC_CONVERT_BIN2BCD((uint8_t)week_day);
	sDate.Month = __LL_RTC_CONVERT_BIN2BCD(time->month);
	sDate.Date = __LL_RTC_CONVERT_BIN2BCD(time->day);
	sDate.Year = __LL_RTC_CONVERT_BIN2BCD(time->year);

	if (HAL_RTC_SetDate(&hrtc, &sDate, RTC_FORMAT_BCD) != HAL_OK)
	{
        DEBUG_ERROR("Set RTC date failed\r\n");
		Error_Handler();
	}
//    LL_PWR_DisableBkUpAccess();
}


uint32_t app_rtc_get_counter(void)
{
    uint32_t counter;
	uint32_t time = LL_RTC_TIME_Get(RTC); 	// (Format: 0x00HHMMSS)
	uint32_t date = LL_RTC_DATE_Get(RTC);	// (Format: 0xWWDDMMYY)
	rtc_date_time_t time_str;
	
	time_str.hour = __LL_RTC_CONVERT_BCD2BIN((time & 0x00FF0000) >> 16);
	time_str.minute = __LL_RTC_CONVERT_BCD2BIN((time & 0x0000FF00) >> 8);
	time_str.second = __LL_RTC_CONVERT_BCD2BIN((time & 0x000000FF));
	
	time_str.day = __LL_RTC_CONVERT_BCD2BIN((date & 0x00FF0000) >> 16);
	time_str.month = __LL_RTC_CONVERT_BCD2BIN((date & 0x0000FF00) >> 8);
	time_str.year = __LL_RTC_CONVERT_BCD2BIN((date & 0x000000FF));
    
    counter = (rtc_struct_to_counter(&time_str));
	
    counter += (946681200 + 3600);
	return counter;
}

bool app_rtc_get_time(rtc_date_time_t *time)
{
    uint32_t rtc_time = LL_RTC_TIME_Get(RTC); 	// (Format: 0x00HHMMSS)
	uint32_t date = LL_RTC_DATE_Get(RTC);	// (Format: 0xWWDDMMYY)
	time->hour = __LL_RTC_CONVERT_BCD2BIN((rtc_time & 0x00FF0000) >> 16);
	time->minute = __LL_RTC_CONVERT_BCD2BIN((rtc_time & 0x0000FF00) >> 8);
	time->second = __LL_RTC_CONVERT_BCD2BIN((rtc_time & 0x000000FF));
	
	time->day = __LL_RTC_CONVERT_BCD2BIN((date & 0x00FF0000) >> 16);
	time->month = __LL_RTC_CONVERT_BCD2BIN((date & 0x0000FF00) >> 8);
	time->year = __LL_RTC_CONVERT_BCD2BIN((date & 0x000000FF));
    if (time->year > 20 && time->year < 39
        && time->month < 13
        && time->day < 32)
    {
        return true;
    }
    return false;
}


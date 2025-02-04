#ifndef IsoDateTime_H
#define IsoDateTime_H

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"

#include "Calendar.d.h"
#include "CalendarError.d.h"
#include "CalendarParseError.d.h"
#include "DateTime.d.h"
#include "IsoDate.d.h"
#include "IsoWeekday.d.h"
#include "Time.d.h"
#include "WeekCalculator.d.h"
#include "WeekOf.d.h"

#include "IsoDateTime.d.h"






typedef struct icu4x_IsoDateTime_create_mv1_result {union {IsoDateTime* ok; CalendarError err;}; bool is_ok;} icu4x_IsoDateTime_create_mv1_result;
icu4x_IsoDateTime_create_mv1_result icu4x_IsoDateTime_create_mv1(int32_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute, uint8_t second, uint32_t nanosecond);

IsoDateTime* icu4x_IsoDateTime_from_date_and_time_mv1(const IsoDate* date, const Time* time);

typedef struct icu4x_IsoDateTime_from_string_mv1_result {union {IsoDateTime* ok; CalendarParseError err;}; bool is_ok;} icu4x_IsoDateTime_from_string_mv1_result;
icu4x_IsoDateTime_from_string_mv1_result icu4x_IsoDateTime_from_string_mv1(DiplomatStringView v);

IsoDate* icu4x_IsoDateTime_date_mv1(const IsoDateTime* self);

Time* icu4x_IsoDateTime_time_mv1(const IsoDateTime* self);

DateTime* icu4x_IsoDateTime_to_any_mv1(const IsoDateTime* self);

DateTime* icu4x_IsoDateTime_to_calendar_mv1(const IsoDateTime* self, const Calendar* calendar);

uint8_t icu4x_IsoDateTime_hour_mv1(const IsoDateTime* self);

uint8_t icu4x_IsoDateTime_minute_mv1(const IsoDateTime* self);

uint8_t icu4x_IsoDateTime_second_mv1(const IsoDateTime* self);

uint32_t icu4x_IsoDateTime_nanosecond_mv1(const IsoDateTime* self);

uint16_t icu4x_IsoDateTime_day_of_year_mv1(const IsoDateTime* self);

uint8_t icu4x_IsoDateTime_day_of_month_mv1(const IsoDateTime* self);

IsoWeekday icu4x_IsoDateTime_day_of_week_mv1(const IsoDateTime* self);

uint8_t icu4x_IsoDateTime_week_of_month_mv1(const IsoDateTime* self, IsoWeekday first_weekday);

WeekOf icu4x_IsoDateTime_week_of_year_mv1(const IsoDateTime* self, const WeekCalculator* calculator);

uint8_t icu4x_IsoDateTime_month_mv1(const IsoDateTime* self);

int32_t icu4x_IsoDateTime_year_mv1(const IsoDateTime* self);

bool icu4x_IsoDateTime_is_in_leap_year_mv1(const IsoDateTime* self);

uint8_t icu4x_IsoDateTime_months_in_year_mv1(const IsoDateTime* self);

uint8_t icu4x_IsoDateTime_days_in_month_mv1(const IsoDateTime* self);

uint16_t icu4x_IsoDateTime_days_in_year_mv1(const IsoDateTime* self);


void icu4x_IsoDateTime_destroy_mv1(IsoDateTime* self);





#endif // IsoDateTime_H

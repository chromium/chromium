#ifndef IsoDate_H
#define IsoDate_H

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"

#include "Calendar.d.h"
#include "CalendarDateAddError.d.h"
#include "CalendarError.d.h"
#include "Date.d.h"
#include "DateAddOptions.d.h"
#include "DateDifferenceOptions.d.h"
#include "DateDuration.d.h"
#include "IsoWeekOfYear.d.h"
#include "Rfc9557ParseError.d.h"
#include "Weekday.d.h"

#include "IsoDate.d.h"






typedef struct icu4x_IsoDate_create_mv1_result {union {IsoDate* ok; CalendarError err;}; bool is_ok;} icu4x_IsoDate_create_mv1_result;
icu4x_IsoDate_create_mv1_result icu4x_IsoDate_create_mv1(int32_t year, uint8_t month, uint8_t day);

IsoDate* icu4x_IsoDate_from_rata_die_mv1(int64_t rd);

typedef struct icu4x_IsoDate_from_string_mv1_result {union {IsoDate* ok; Rfc9557ParseError err;}; bool is_ok;} icu4x_IsoDate_from_string_mv1_result;
icu4x_IsoDate_from_string_mv1_result icu4x_IsoDate_from_string_mv1(DiplomatStringView v);

Date* icu4x_IsoDate_to_calendar_mv1(const IsoDate* self, const Calendar* calendar);

Date* icu4x_IsoDate_to_any_mv1(const IsoDate* self);

int64_t icu4x_IsoDate_to_rata_die_mv1(const IsoDate* self);

uint16_t icu4x_IsoDate_day_of_year_mv1(const IsoDate* self);

uint8_t icu4x_IsoDate_day_of_month_mv1(const IsoDate* self);

Weekday icu4x_IsoDate_day_of_week_mv1(const IsoDate* self);

Weekday icu4x_IsoDate_weekday_mv1(const IsoDate* self);

IsoWeekOfYear icu4x_IsoDate_week_of_year_mv1(const IsoDate* self);

uint8_t icu4x_IsoDate_month_mv1(const IsoDate* self);

int32_t icu4x_IsoDate_year_mv1(const IsoDate* self);

bool icu4x_IsoDate_is_in_leap_year_mv1(const IsoDate* self);

uint8_t icu4x_IsoDate_months_in_year_mv1(const IsoDate* self);

uint8_t icu4x_IsoDate_days_in_month_mv1(const IsoDate* self);

uint16_t icu4x_IsoDate_days_in_year_mv1(const IsoDate* self);

typedef struct icu4x_IsoDate_try_add_with_options_mv1_result {union {IsoDate* ok; CalendarDateAddError err;}; bool is_ok;} icu4x_IsoDate_try_add_with_options_mv1_result;
icu4x_IsoDate_try_add_with_options_mv1_result icu4x_IsoDate_try_add_with_options_mv1(const IsoDate* self, DateDuration duration, DateAddOptions options);

DateDuration icu4x_IsoDate_until_with_options_mv1(const IsoDate* self, const IsoDate* other, DateDifferenceOptions options);

void icu4x_IsoDate_destroy_mv1(IsoDate* self);





#endif // IsoDate_H

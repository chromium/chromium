#ifndef Date_H
#define Date_H

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"

#include "Calendar.d.h"
#include "CalendarDateAddError.d.h"
#include "CalendarDateFromFieldsError.d.h"
#include "CalendarError.d.h"
#include "DateAddOptions.d.h"
#include "DateDifferenceOptions.d.h"
#include "DateDuration.d.h"
#include "DateFields.d.h"
#include "DateFromFieldsOptions.d.h"
#include "IsoDate.d.h"
#include "Rfc9557ParseError.d.h"
#include "Weekday.d.h"

#include "Date.d.h"






typedef struct icu4x_Date_from_iso_in_calendar_mv1_result {union {Date* ok; CalendarError err;}; bool is_ok;} icu4x_Date_from_iso_in_calendar_mv1_result;
icu4x_Date_from_iso_in_calendar_mv1_result icu4x_Date_from_iso_in_calendar_mv1(int32_t iso_year, uint8_t iso_month, uint8_t iso_day, const Calendar* calendar);

typedef struct icu4x_Date_from_fields_in_calendar_mv1_result {union {Date* ok; CalendarDateFromFieldsError err;}; bool is_ok;} icu4x_Date_from_fields_in_calendar_mv1_result;
icu4x_Date_from_fields_in_calendar_mv1_result icu4x_Date_from_fields_in_calendar_mv1(DateFields fields, DateFromFieldsOptions options, const Calendar* calendar);

typedef struct icu4x_Date_from_codes_in_calendar_mv1_result {union {Date* ok; CalendarError err;}; bool is_ok;} icu4x_Date_from_codes_in_calendar_mv1_result;
icu4x_Date_from_codes_in_calendar_mv1_result icu4x_Date_from_codes_in_calendar_mv1(DiplomatStringView era_code, int32_t year, DiplomatStringView month_code, uint8_t day, const Calendar* calendar);

typedef struct icu4x_Date_from_rata_die_mv1_result {union {Date* ok; CalendarError err;}; bool is_ok;} icu4x_Date_from_rata_die_mv1_result;
icu4x_Date_from_rata_die_mv1_result icu4x_Date_from_rata_die_mv1(int64_t rd, const Calendar* calendar);

typedef struct icu4x_Date_from_string_mv1_result {union {Date* ok; Rfc9557ParseError err;}; bool is_ok;} icu4x_Date_from_string_mv1_result;
icu4x_Date_from_string_mv1_result icu4x_Date_from_string_mv1(DiplomatStringView v, const Calendar* calendar);

Date* icu4x_Date_to_calendar_mv1(const Date* self, const Calendar* calendar);

IsoDate* icu4x_Date_to_iso_mv1(const Date* self);

int64_t icu4x_Date_to_rata_die_mv1(const Date* self);

uint16_t icu4x_Date_day_of_year_mv1(const Date* self);

uint8_t icu4x_Date_day_of_month_mv1(const Date* self);

Weekday icu4x_Date_day_of_week_mv1(const Date* self);

Weekday icu4x_Date_weekday_mv1(const Date* self);

uint8_t icu4x_Date_ordinal_month_mv1(const Date* self);

void icu4x_Date_month_code_mv1(const Date* self, DiplomatWrite* write);

uint8_t icu4x_Date_month_number_mv1(const Date* self);

bool icu4x_Date_month_is_leap_mv1(const Date* self);

int32_t icu4x_Date_era_year_or_related_iso_mv1(const Date* self);

int32_t icu4x_Date_extended_year_mv1(const Date* self);

void icu4x_Date_era_mv1(const Date* self, DiplomatWrite* write);

uint8_t icu4x_Date_months_in_year_mv1(const Date* self);

uint8_t icu4x_Date_days_in_month_mv1(const Date* self);

uint16_t icu4x_Date_days_in_year_mv1(const Date* self);

bool icu4x_Date_is_in_leap_year_mv1(const Date* self);

Calendar* icu4x_Date_calendar_mv1(const Date* self);

typedef struct icu4x_Date_try_add_with_options_mv1_result {union {Date* ok; CalendarDateAddError err;}; bool is_ok;} icu4x_Date_try_add_with_options_mv1_result;
icu4x_Date_try_add_with_options_mv1_result icu4x_Date_try_add_with_options_mv1(const Date* self, DateDuration duration, DateAddOptions options);

typedef struct icu4x_Date_try_until_with_options_mv1_result {union {DateDuration ok; }; bool is_ok;} icu4x_Date_try_until_with_options_mv1_result;
icu4x_Date_try_until_with_options_mv1_result icu4x_Date_try_until_with_options_mv1(const Date* self, const Date* other, DateDifferenceOptions options);

void icu4x_Date_destroy_mv1(Date* self);





#endif // Date_H

#ifndef Calendar_H
#define Calendar_H

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"

#include "AnyCalendarKind.d.h"
#include "ArithmeticOverflow.d.h"
#include "Duration.d.h"
#include "IsoDate.d.h"
#include "PartialDate.d.h"
#include "PlainDate.d.h"
#include "PlainMonthDay.d.h"
#include "PlainYearMonth.d.h"
#include "TemporalError.d.h"
#include "Unit.d.h"

#include "Calendar.d.h"






Calendar* temporal_rs_Calendar_try_new_constrain(AnyCalendarKind kind);

typedef struct temporal_rs_Calendar_from_utf8_result {union {Calendar* ok; TemporalError err;}; bool is_ok;} temporal_rs_Calendar_from_utf8_result;
temporal_rs_Calendar_from_utf8_result temporal_rs_Calendar_from_utf8(DiplomatStringView s);

bool temporal_rs_Calendar_is_iso(const Calendar* self);

DiplomatStringView temporal_rs_Calendar_identifier(const Calendar* self);

typedef struct temporal_rs_Calendar_date_from_partial_result {union {PlainDate* ok; TemporalError err;}; bool is_ok;} temporal_rs_Calendar_date_from_partial_result;
temporal_rs_Calendar_date_from_partial_result temporal_rs_Calendar_date_from_partial(const Calendar* self, PartialDate partial, ArithmeticOverflow overflow);

typedef struct temporal_rs_Calendar_month_day_from_partial_result {union {PlainMonthDay* ok; TemporalError err;}; bool is_ok;} temporal_rs_Calendar_month_day_from_partial_result;
temporal_rs_Calendar_month_day_from_partial_result temporal_rs_Calendar_month_day_from_partial(const Calendar* self, PartialDate partial, ArithmeticOverflow overflow);

typedef struct temporal_rs_Calendar_year_month_from_partial_result {union {PlainYearMonth* ok; TemporalError err;}; bool is_ok;} temporal_rs_Calendar_year_month_from_partial_result;
temporal_rs_Calendar_year_month_from_partial_result temporal_rs_Calendar_year_month_from_partial(const Calendar* self, PartialDate partial, ArithmeticOverflow overflow);

typedef struct temporal_rs_Calendar_date_add_result {union {PlainDate* ok; TemporalError err;}; bool is_ok;} temporal_rs_Calendar_date_add_result;
temporal_rs_Calendar_date_add_result temporal_rs_Calendar_date_add(const Calendar* self, IsoDate date, const Duration* duration, ArithmeticOverflow overflow);

typedef struct temporal_rs_Calendar_date_until_result {union {Duration* ok; TemporalError err;}; bool is_ok;} temporal_rs_Calendar_date_until_result;
temporal_rs_Calendar_date_until_result temporal_rs_Calendar_date_until(const Calendar* self, IsoDate one, IsoDate two, Unit largest_unit);

typedef struct temporal_rs_Calendar_era_result {union { TemporalError err;}; bool is_ok;} temporal_rs_Calendar_era_result;
temporal_rs_Calendar_era_result temporal_rs_Calendar_era(const Calendar* self, IsoDate date, DiplomatWrite* write);

typedef struct temporal_rs_Calendar_era_year_result {union {int32_t ok; }; bool is_ok;} temporal_rs_Calendar_era_year_result;
temporal_rs_Calendar_era_year_result temporal_rs_Calendar_era_year(const Calendar* self, IsoDate date);

int32_t temporal_rs_Calendar_year(const Calendar* self, IsoDate date);

uint8_t temporal_rs_Calendar_month(const Calendar* self, IsoDate date);

typedef struct temporal_rs_Calendar_month_code_result {union { TemporalError err;}; bool is_ok;} temporal_rs_Calendar_month_code_result;
temporal_rs_Calendar_month_code_result temporal_rs_Calendar_month_code(const Calendar* self, IsoDate date, DiplomatWrite* write);

uint8_t temporal_rs_Calendar_day(const Calendar* self, IsoDate date);

typedef struct temporal_rs_Calendar_day_of_week_result {union {uint16_t ok; TemporalError err;}; bool is_ok;} temporal_rs_Calendar_day_of_week_result;
temporal_rs_Calendar_day_of_week_result temporal_rs_Calendar_day_of_week(const Calendar* self, IsoDate date);

uint16_t temporal_rs_Calendar_day_of_year(const Calendar* self, IsoDate date);

typedef struct temporal_rs_Calendar_week_of_year_result {union {uint8_t ok; }; bool is_ok;} temporal_rs_Calendar_week_of_year_result;
temporal_rs_Calendar_week_of_year_result temporal_rs_Calendar_week_of_year(const Calendar* self, IsoDate date);

typedef struct temporal_rs_Calendar_year_of_week_result {union {int32_t ok; }; bool is_ok;} temporal_rs_Calendar_year_of_week_result;
temporal_rs_Calendar_year_of_week_result temporal_rs_Calendar_year_of_week(const Calendar* self, IsoDate date);

typedef struct temporal_rs_Calendar_days_in_week_result {union {uint16_t ok; TemporalError err;}; bool is_ok;} temporal_rs_Calendar_days_in_week_result;
temporal_rs_Calendar_days_in_week_result temporal_rs_Calendar_days_in_week(const Calendar* self, IsoDate date);

uint16_t temporal_rs_Calendar_days_in_month(const Calendar* self, IsoDate date);

uint16_t temporal_rs_Calendar_days_in_year(const Calendar* self, IsoDate date);

uint16_t temporal_rs_Calendar_months_in_year(const Calendar* self, IsoDate date);

bool temporal_rs_Calendar_in_leap_year(const Calendar* self, IsoDate date);

AnyCalendarKind temporal_rs_Calendar_kind(const Calendar* self);

void temporal_rs_Calendar_destroy(Calendar* self);





#endif // Calendar_H

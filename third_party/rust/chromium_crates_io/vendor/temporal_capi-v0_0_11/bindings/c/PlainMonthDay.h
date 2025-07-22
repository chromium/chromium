#ifndef PlainMonthDay_H
#define PlainMonthDay_H

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"

#include "AnyCalendarKind.d.h"
#include "ArithmeticOverflow.d.h"
#include "Calendar.d.h"
#include "DisplayCalendar.d.h"
#include "PartialDate.d.h"
#include "PlainDate.d.h"
#include "TemporalError.d.h"
#include "TimeZone.d.h"

#include "PlainMonthDay.d.h"






typedef struct temporal_rs_PlainMonthDay_try_new_with_overflow_result {union {PlainMonthDay* ok; TemporalError err;}; bool is_ok;} temporal_rs_PlainMonthDay_try_new_with_overflow_result;
temporal_rs_PlainMonthDay_try_new_with_overflow_result temporal_rs_PlainMonthDay_try_new_with_overflow(uint8_t month, uint8_t day, AnyCalendarKind calendar, ArithmeticOverflow overflow, OptionI32 ref_year);

typedef struct temporal_rs_PlainMonthDay_from_partial_result {union {PlainMonthDay* ok; TemporalError err;}; bool is_ok;} temporal_rs_PlainMonthDay_from_partial_result;
temporal_rs_PlainMonthDay_from_partial_result temporal_rs_PlainMonthDay_from_partial(PartialDate partial, ArithmeticOverflow_option overflow);

typedef struct temporal_rs_PlainMonthDay_with_result {union {PlainMonthDay* ok; TemporalError err;}; bool is_ok;} temporal_rs_PlainMonthDay_with_result;
temporal_rs_PlainMonthDay_with_result temporal_rs_PlainMonthDay_with(const PlainMonthDay* self, PartialDate partial, ArithmeticOverflow_option overflow);

bool temporal_rs_PlainMonthDay_equals(const PlainMonthDay* self, const PlainMonthDay* other);

int8_t temporal_rs_PlainMonthDay_compare(const PlainMonthDay* one, const PlainMonthDay* two);

typedef struct temporal_rs_PlainMonthDay_from_utf8_result {union {PlainMonthDay* ok; TemporalError err;}; bool is_ok;} temporal_rs_PlainMonthDay_from_utf8_result;
temporal_rs_PlainMonthDay_from_utf8_result temporal_rs_PlainMonthDay_from_utf8(DiplomatStringView s);

typedef struct temporal_rs_PlainMonthDay_from_utf16_result {union {PlainMonthDay* ok; TemporalError err;}; bool is_ok;} temporal_rs_PlainMonthDay_from_utf16_result;
temporal_rs_PlainMonthDay_from_utf16_result temporal_rs_PlainMonthDay_from_utf16(DiplomatString16View s);

int32_t temporal_rs_PlainMonthDay_iso_year(const PlainMonthDay* self);

uint8_t temporal_rs_PlainMonthDay_iso_month(const PlainMonthDay* self);

uint8_t temporal_rs_PlainMonthDay_iso_day(const PlainMonthDay* self);

const Calendar* temporal_rs_PlainMonthDay_calendar(const PlainMonthDay* self);

void temporal_rs_PlainMonthDay_month_code(const PlainMonthDay* self, DiplomatWrite* write);

typedef struct temporal_rs_PlainMonthDay_to_plain_date_result {union {PlainDate* ok; TemporalError err;}; bool is_ok;} temporal_rs_PlainMonthDay_to_plain_date_result;
temporal_rs_PlainMonthDay_to_plain_date_result temporal_rs_PlainMonthDay_to_plain_date(const PlainMonthDay* self, PartialDate_option year);

typedef struct temporal_rs_PlainMonthDay_epoch_ns_for_result {union {int64_t ok; TemporalError err;}; bool is_ok;} temporal_rs_PlainMonthDay_epoch_ns_for_result;
temporal_rs_PlainMonthDay_epoch_ns_for_result temporal_rs_PlainMonthDay_epoch_ns_for(const PlainMonthDay* self, const TimeZone* time_zone);

void temporal_rs_PlainMonthDay_to_ixdtf_string(const PlainMonthDay* self, DisplayCalendar display_calendar, DiplomatWrite* write);

void temporal_rs_PlainMonthDay_destroy(PlainMonthDay* self);





#endif // PlainMonthDay_H

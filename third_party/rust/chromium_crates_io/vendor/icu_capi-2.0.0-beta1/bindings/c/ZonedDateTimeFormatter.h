#ifndef ZonedDateTimeFormatter_H
#define ZonedDateTimeFormatter_H

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"

#include "DataProvider.d.h"
#include "DateTime.d.h"
#include "DateTimeFormatError.d.h"
#include "DateTimeFormatterLoadError.d.h"
#include "DateTimeLength.d.h"
#include "IsoDateTime.d.h"
#include "Locale.d.h"
#include "TimeZoneInfo.d.h"

#include "ZonedDateTimeFormatter.d.h"






typedef struct icu4x_ZonedDateTimeFormatter_create_with_length_mv1_result {union {ZonedDateTimeFormatter* ok; DateTimeFormatterLoadError err;}; bool is_ok;} icu4x_ZonedDateTimeFormatter_create_with_length_mv1_result;
icu4x_ZonedDateTimeFormatter_create_with_length_mv1_result icu4x_ZonedDateTimeFormatter_create_with_length_mv1(const DataProvider* provider, const Locale* locale, DateTimeLength length);

typedef struct icu4x_ZonedDateTimeFormatter_format_datetime_with_custom_time_zone_mv1_result {union { DateTimeFormatError err;}; bool is_ok;} icu4x_ZonedDateTimeFormatter_format_datetime_with_custom_time_zone_mv1_result;
icu4x_ZonedDateTimeFormatter_format_datetime_with_custom_time_zone_mv1_result icu4x_ZonedDateTimeFormatter_format_datetime_with_custom_time_zone_mv1(const ZonedDateTimeFormatter* self, const DateTime* datetime, const TimeZoneInfo* time_zone, DiplomatWrite* write);

typedef struct icu4x_ZonedDateTimeFormatter_format_iso_datetime_with_custom_time_zone_mv1_result {union { DateTimeFormatError err;}; bool is_ok;} icu4x_ZonedDateTimeFormatter_format_iso_datetime_with_custom_time_zone_mv1_result;
icu4x_ZonedDateTimeFormatter_format_iso_datetime_with_custom_time_zone_mv1_result icu4x_ZonedDateTimeFormatter_format_iso_datetime_with_custom_time_zone_mv1(const ZonedDateTimeFormatter* self, const IsoDateTime* datetime, const TimeZoneInfo* time_zone, DiplomatWrite* write);


void icu4x_ZonedDateTimeFormatter_destroy_mv1(ZonedDateTimeFormatter* self);





#endif // ZonedDateTimeFormatter_H

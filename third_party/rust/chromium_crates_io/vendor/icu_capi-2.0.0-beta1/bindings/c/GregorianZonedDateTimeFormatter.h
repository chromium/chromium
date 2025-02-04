#ifndef GregorianZonedDateTimeFormatter_H
#define GregorianZonedDateTimeFormatter_H

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"

#include "DataProvider.d.h"
#include "DateTimeFormatError.d.h"
#include "DateTimeFormatterLoadError.d.h"
#include "DateTimeLength.d.h"
#include "IsoDateTime.d.h"
#include "Locale.d.h"
#include "TimeZoneInfo.d.h"

#include "GregorianZonedDateTimeFormatter.d.h"






typedef struct icu4x_GregorianZonedDateTimeFormatter_create_with_length_mv1_result {union {GregorianZonedDateTimeFormatter* ok; DateTimeFormatterLoadError err;}; bool is_ok;} icu4x_GregorianZonedDateTimeFormatter_create_with_length_mv1_result;
icu4x_GregorianZonedDateTimeFormatter_create_with_length_mv1_result icu4x_GregorianZonedDateTimeFormatter_create_with_length_mv1(const DataProvider* provider, const Locale* locale, DateTimeLength length);

typedef struct icu4x_GregorianZonedDateTimeFormatter_format_iso_datetime_with_custom_time_zone_mv1_result {union { DateTimeFormatError err;}; bool is_ok;} icu4x_GregorianZonedDateTimeFormatter_format_iso_datetime_with_custom_time_zone_mv1_result;
icu4x_GregorianZonedDateTimeFormatter_format_iso_datetime_with_custom_time_zone_mv1_result icu4x_GregorianZonedDateTimeFormatter_format_iso_datetime_with_custom_time_zone_mv1(const GregorianZonedDateTimeFormatter* self, const IsoDateTime* datetime, const TimeZoneInfo* time_zone, DiplomatWrite* write);


void icu4x_GregorianZonedDateTimeFormatter_destroy_mv1(GregorianZonedDateTimeFormatter* self);





#endif // GregorianZonedDateTimeFormatter_H

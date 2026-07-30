#ifndef ZonedDateFormatter_H
#define ZonedDateFormatter_H

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"

#include "DataProvider.d.h"
#include "Date.d.h"
#include "DateFormatter.d.h"
#include "DateTimeFormatterLoadError.d.h"
#include "DateTimeMismatchedCalendarError.d.h"
#include "DateTimeWriteError.d.h"
#include "IsoDate.d.h"
#include "Locale.d.h"
#include "TimeZoneInfo.d.h"

#include "ZonedDateFormatter.d.h"






typedef struct icu4x_ZonedDateFormatter_create_specific_long_mv1_result {union {ZonedDateFormatter* ok; DateTimeFormatterLoadError err;}; bool is_ok;} icu4x_ZonedDateFormatter_create_specific_long_mv1_result;
icu4x_ZonedDateFormatter_create_specific_long_mv1_result icu4x_ZonedDateFormatter_create_specific_long_mv1(const Locale* locale, const DateFormatter* formatter);

typedef struct icu4x_ZonedDateFormatter_create_specific_long_with_provider_mv1_result {union {ZonedDateFormatter* ok; DateTimeFormatterLoadError err;}; bool is_ok;} icu4x_ZonedDateFormatter_create_specific_long_with_provider_mv1_result;
icu4x_ZonedDateFormatter_create_specific_long_with_provider_mv1_result icu4x_ZonedDateFormatter_create_specific_long_with_provider_mv1(const DataProvider* provider, const Locale* locale, const DateFormatter* formatter);

typedef struct icu4x_ZonedDateFormatter_create_specific_short_mv1_result {union {ZonedDateFormatter* ok; DateTimeFormatterLoadError err;}; bool is_ok;} icu4x_ZonedDateFormatter_create_specific_short_mv1_result;
icu4x_ZonedDateFormatter_create_specific_short_mv1_result icu4x_ZonedDateFormatter_create_specific_short_mv1(const Locale* locale, const DateFormatter* formatter);

typedef struct icu4x_ZonedDateFormatter_create_specific_short_with_provider_mv1_result {union {ZonedDateFormatter* ok; DateTimeFormatterLoadError err;}; bool is_ok;} icu4x_ZonedDateFormatter_create_specific_short_with_provider_mv1_result;
icu4x_ZonedDateFormatter_create_specific_short_with_provider_mv1_result icu4x_ZonedDateFormatter_create_specific_short_with_provider_mv1(const DataProvider* provider, const Locale* locale, const DateFormatter* formatter);

typedef struct icu4x_ZonedDateFormatter_create_localized_offset_long_mv1_result {union {ZonedDateFormatter* ok; DateTimeFormatterLoadError err;}; bool is_ok;} icu4x_ZonedDateFormatter_create_localized_offset_long_mv1_result;
icu4x_ZonedDateFormatter_create_localized_offset_long_mv1_result icu4x_ZonedDateFormatter_create_localized_offset_long_mv1(const Locale* locale, const DateFormatter* formatter);

typedef struct icu4x_ZonedDateFormatter_create_localized_offset_long_with_provider_mv1_result {union {ZonedDateFormatter* ok; DateTimeFormatterLoadError err;}; bool is_ok;} icu4x_ZonedDateFormatter_create_localized_offset_long_with_provider_mv1_result;
icu4x_ZonedDateFormatter_create_localized_offset_long_with_provider_mv1_result icu4x_ZonedDateFormatter_create_localized_offset_long_with_provider_mv1(const DataProvider* provider, const Locale* locale, const DateFormatter* formatter);

typedef struct icu4x_ZonedDateFormatter_create_localized_offset_short_mv1_result {union {ZonedDateFormatter* ok; DateTimeFormatterLoadError err;}; bool is_ok;} icu4x_ZonedDateFormatter_create_localized_offset_short_mv1_result;
icu4x_ZonedDateFormatter_create_localized_offset_short_mv1_result icu4x_ZonedDateFormatter_create_localized_offset_short_mv1(const Locale* locale, const DateFormatter* formatter);

typedef struct icu4x_ZonedDateFormatter_create_localized_offset_short_with_provider_mv1_result {union {ZonedDateFormatter* ok; DateTimeFormatterLoadError err;}; bool is_ok;} icu4x_ZonedDateFormatter_create_localized_offset_short_with_provider_mv1_result;
icu4x_ZonedDateFormatter_create_localized_offset_short_with_provider_mv1_result icu4x_ZonedDateFormatter_create_localized_offset_short_with_provider_mv1(const DataProvider* provider, const Locale* locale, const DateFormatter* formatter);

typedef struct icu4x_ZonedDateFormatter_create_generic_long_mv1_result {union {ZonedDateFormatter* ok; DateTimeFormatterLoadError err;}; bool is_ok;} icu4x_ZonedDateFormatter_create_generic_long_mv1_result;
icu4x_ZonedDateFormatter_create_generic_long_mv1_result icu4x_ZonedDateFormatter_create_generic_long_mv1(const Locale* locale, const DateFormatter* formatter);

typedef struct icu4x_ZonedDateFormatter_create_generic_long_with_provider_mv1_result {union {ZonedDateFormatter* ok; DateTimeFormatterLoadError err;}; bool is_ok;} icu4x_ZonedDateFormatter_create_generic_long_with_provider_mv1_result;
icu4x_ZonedDateFormatter_create_generic_long_with_provider_mv1_result icu4x_ZonedDateFormatter_create_generic_long_with_provider_mv1(const DataProvider* provider, const Locale* locale, const DateFormatter* formatter);

typedef struct icu4x_ZonedDateFormatter_create_generic_short_mv1_result {union {ZonedDateFormatter* ok; DateTimeFormatterLoadError err;}; bool is_ok;} icu4x_ZonedDateFormatter_create_generic_short_mv1_result;
icu4x_ZonedDateFormatter_create_generic_short_mv1_result icu4x_ZonedDateFormatter_create_generic_short_mv1(const Locale* locale, const DateFormatter* formatter);

typedef struct icu4x_ZonedDateFormatter_create_generic_short_with_provider_mv1_result {union {ZonedDateFormatter* ok; DateTimeFormatterLoadError err;}; bool is_ok;} icu4x_ZonedDateFormatter_create_generic_short_with_provider_mv1_result;
icu4x_ZonedDateFormatter_create_generic_short_with_provider_mv1_result icu4x_ZonedDateFormatter_create_generic_short_with_provider_mv1(const DataProvider* provider, const Locale* locale, const DateFormatter* formatter);

typedef struct icu4x_ZonedDateFormatter_create_location_mv1_result {union {ZonedDateFormatter* ok; DateTimeFormatterLoadError err;}; bool is_ok;} icu4x_ZonedDateFormatter_create_location_mv1_result;
icu4x_ZonedDateFormatter_create_location_mv1_result icu4x_ZonedDateFormatter_create_location_mv1(const Locale* locale, const DateFormatter* formatter);

typedef struct icu4x_ZonedDateFormatter_create_location_with_provider_mv1_result {union {ZonedDateFormatter* ok; DateTimeFormatterLoadError err;}; bool is_ok;} icu4x_ZonedDateFormatter_create_location_with_provider_mv1_result;
icu4x_ZonedDateFormatter_create_location_with_provider_mv1_result icu4x_ZonedDateFormatter_create_location_with_provider_mv1(const DataProvider* provider, const Locale* locale, const DateFormatter* formatter);

typedef struct icu4x_ZonedDateFormatter_create_exemplar_city_mv1_result {union {ZonedDateFormatter* ok; DateTimeFormatterLoadError err;}; bool is_ok;} icu4x_ZonedDateFormatter_create_exemplar_city_mv1_result;
icu4x_ZonedDateFormatter_create_exemplar_city_mv1_result icu4x_ZonedDateFormatter_create_exemplar_city_mv1(const Locale* locale, const DateFormatter* formatter);

typedef struct icu4x_ZonedDateFormatter_create_exemplar_city_with_provider_mv1_result {union {ZonedDateFormatter* ok; DateTimeFormatterLoadError err;}; bool is_ok;} icu4x_ZonedDateFormatter_create_exemplar_city_with_provider_mv1_result;
icu4x_ZonedDateFormatter_create_exemplar_city_with_provider_mv1_result icu4x_ZonedDateFormatter_create_exemplar_city_with_provider_mv1(const DataProvider* provider, const Locale* locale, const DateFormatter* formatter);

typedef struct icu4x_ZonedDateFormatter_format_iso_mv1_result {union { DateTimeWriteError err;}; bool is_ok;} icu4x_ZonedDateFormatter_format_iso_mv1_result;
icu4x_ZonedDateFormatter_format_iso_mv1_result icu4x_ZonedDateFormatter_format_iso_mv1(const ZonedDateFormatter* self, const IsoDate* iso_date, const TimeZoneInfo* zone, DiplomatWrite* write);

typedef struct icu4x_ZonedDateFormatter_format_same_calendar_mv1_result {union { DateTimeMismatchedCalendarError err;}; bool is_ok;} icu4x_ZonedDateFormatter_format_same_calendar_mv1_result;
icu4x_ZonedDateFormatter_format_same_calendar_mv1_result icu4x_ZonedDateFormatter_format_same_calendar_mv1(const ZonedDateFormatter* self, const Date* date, const TimeZoneInfo* zone, DiplomatWrite* write);

void icu4x_ZonedDateFormatter_destroy_mv1(ZonedDateFormatter* self);





#endif // ZonedDateFormatter_H

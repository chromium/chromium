#ifndef TimeFormatter_H
#define TimeFormatter_H

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"

#include "DataProvider.d.h"
#include "DateTimeAlignment.d.h"
#include "DateTimeFormatterLoadError.d.h"
#include "DateTimeLength.d.h"
#include "Locale.d.h"
#include "Time.d.h"
#include "TimePrecision.d.h"

#include "TimeFormatter.d.h"






typedef struct icu4x_TimeFormatter_create_mv1_result {union {TimeFormatter* ok; DateTimeFormatterLoadError err;}; bool is_ok;} icu4x_TimeFormatter_create_mv1_result;
icu4x_TimeFormatter_create_mv1_result icu4x_TimeFormatter_create_mv1(const Locale* locale, DateTimeLength_option length, TimePrecision_option time_precision, DateTimeAlignment_option alignment);

typedef struct icu4x_TimeFormatter_create_with_provider_mv1_result {union {TimeFormatter* ok; DateTimeFormatterLoadError err;}; bool is_ok;} icu4x_TimeFormatter_create_with_provider_mv1_result;
icu4x_TimeFormatter_create_with_provider_mv1_result icu4x_TimeFormatter_create_with_provider_mv1(const DataProvider* provider, const Locale* locale, DateTimeLength_option length, TimePrecision_option time_precision, DateTimeAlignment_option alignment);

void icu4x_TimeFormatter_format_mv1(const TimeFormatter* self, const Time* time, DiplomatWrite* write);

void icu4x_TimeFormatter_destroy_mv1(TimeFormatter* self);





#endif // TimeFormatter_H

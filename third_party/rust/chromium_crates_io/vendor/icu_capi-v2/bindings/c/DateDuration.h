#ifndef DateDuration_H
#define DateDuration_H

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"

#include "DateDurationParseError.d.h"

#include "DateDuration.d.h"






typedef struct icu4x_DateDuration_from_string_mv1_result {union {DateDuration ok; DateDurationParseError err;}; bool is_ok;} icu4x_DateDuration_from_string_mv1_result;
icu4x_DateDuration_from_string_mv1_result icu4x_DateDuration_from_string_mv1(DiplomatStringView v);

DateDuration icu4x_DateDuration_for_years_mv1(int32_t years);

DateDuration icu4x_DateDuration_for_months_mv1(int32_t months);

DateDuration icu4x_DateDuration_for_weeks_mv1(int32_t weeks);

DateDuration icu4x_DateDuration_for_days_mv1(int32_t days);





#endif // DateDuration_H

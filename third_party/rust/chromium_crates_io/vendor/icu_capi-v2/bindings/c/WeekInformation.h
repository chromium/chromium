#ifndef WeekInformation_H
#define WeekInformation_H

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"

#include "DataError.d.h"
#include "DataProvider.d.h"
#include "Locale.d.h"
#include "Weekday.d.h"
#include "WeekdaySetIterator.d.h"

#include "WeekInformation.d.h"






typedef struct icu4x_WeekInformation_create_mv1_result {union {WeekInformation* ok; DataError err;}; bool is_ok;} icu4x_WeekInformation_create_mv1_result;
icu4x_WeekInformation_create_mv1_result icu4x_WeekInformation_create_mv1(const Locale* locale);

typedef struct icu4x_WeekInformation_create_with_provider_mv1_result {union {WeekInformation* ok; DataError err;}; bool is_ok;} icu4x_WeekInformation_create_with_provider_mv1_result;
icu4x_WeekInformation_create_with_provider_mv1_result icu4x_WeekInformation_create_with_provider_mv1(const DataProvider* provider, const Locale* locale);

Weekday icu4x_WeekInformation_first_weekday_mv1(const WeekInformation* self);

bool icu4x_WeekInformation_is_weekend_mv1(const WeekInformation* self, Weekday day);

WeekdaySetIterator* icu4x_WeekInformation_weekend_mv1(const WeekInformation* self);

void icu4x_WeekInformation_destroy_mv1(WeekInformation* self);





#endif // WeekInformation_H

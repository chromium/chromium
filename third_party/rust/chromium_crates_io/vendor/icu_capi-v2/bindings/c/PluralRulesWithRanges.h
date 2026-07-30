#ifndef PluralRulesWithRanges_H
#define PluralRulesWithRanges_H

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"

#include "DataError.d.h"
#include "DataProvider.d.h"
#include "Locale.d.h"
#include "PluralCategory.d.h"
#include "PluralOperands.d.h"

#include "PluralRulesWithRanges.d.h"






typedef struct icu4x_PluralRulesWithRanges_create_cardinal_mv1_result {union {PluralRulesWithRanges* ok; DataError err;}; bool is_ok;} icu4x_PluralRulesWithRanges_create_cardinal_mv1_result;
icu4x_PluralRulesWithRanges_create_cardinal_mv1_result icu4x_PluralRulesWithRanges_create_cardinal_mv1(const Locale* locale);

typedef struct icu4x_PluralRulesWithRanges_create_cardinal_with_provider_mv1_result {union {PluralRulesWithRanges* ok; DataError err;}; bool is_ok;} icu4x_PluralRulesWithRanges_create_cardinal_with_provider_mv1_result;
icu4x_PluralRulesWithRanges_create_cardinal_with_provider_mv1_result icu4x_PluralRulesWithRanges_create_cardinal_with_provider_mv1(const DataProvider* provider, const Locale* locale);

typedef struct icu4x_PluralRulesWithRanges_create_ordinal_mv1_result {union {PluralRulesWithRanges* ok; DataError err;}; bool is_ok;} icu4x_PluralRulesWithRanges_create_ordinal_mv1_result;
icu4x_PluralRulesWithRanges_create_ordinal_mv1_result icu4x_PluralRulesWithRanges_create_ordinal_mv1(const Locale* locale);

typedef struct icu4x_PluralRulesWithRanges_create_ordinal_with_provider_mv1_result {union {PluralRulesWithRanges* ok; DataError err;}; bool is_ok;} icu4x_PluralRulesWithRanges_create_ordinal_with_provider_mv1_result;
icu4x_PluralRulesWithRanges_create_ordinal_with_provider_mv1_result icu4x_PluralRulesWithRanges_create_ordinal_with_provider_mv1(const DataProvider* provider, const Locale* locale);

PluralCategory icu4x_PluralRulesWithRanges_category_for_range_mv1(const PluralRulesWithRanges* self, const PluralOperands* start, const PluralOperands* end);

void icu4x_PluralRulesWithRanges_destroy_mv1(PluralRulesWithRanges* self);





#endif // PluralRulesWithRanges_H

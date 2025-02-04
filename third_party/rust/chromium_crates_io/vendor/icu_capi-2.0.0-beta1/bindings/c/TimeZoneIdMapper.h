#ifndef TimeZoneIdMapper_H
#define TimeZoneIdMapper_H

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"

#include "DataError.d.h"
#include "DataProvider.d.h"

#include "TimeZoneIdMapper.d.h"






typedef struct icu4x_TimeZoneIdMapper_create_mv1_result {union {TimeZoneIdMapper* ok; DataError err;}; bool is_ok;} icu4x_TimeZoneIdMapper_create_mv1_result;
icu4x_TimeZoneIdMapper_create_mv1_result icu4x_TimeZoneIdMapper_create_mv1(const DataProvider* provider);

void icu4x_TimeZoneIdMapper_iana_to_bcp47_mv1(const TimeZoneIdMapper* self, DiplomatStringView value, DiplomatWrite* write);

typedef struct icu4x_TimeZoneIdMapper_normalize_iana_mv1_result { bool is_ok;} icu4x_TimeZoneIdMapper_normalize_iana_mv1_result;
icu4x_TimeZoneIdMapper_normalize_iana_mv1_result icu4x_TimeZoneIdMapper_normalize_iana_mv1(const TimeZoneIdMapper* self, DiplomatStringView value, DiplomatWrite* write);

typedef struct icu4x_TimeZoneIdMapper_canonicalize_iana_mv1_result { bool is_ok;} icu4x_TimeZoneIdMapper_canonicalize_iana_mv1_result;
icu4x_TimeZoneIdMapper_canonicalize_iana_mv1_result icu4x_TimeZoneIdMapper_canonicalize_iana_mv1(const TimeZoneIdMapper* self, DiplomatStringView value, DiplomatWrite* write);

typedef struct icu4x_TimeZoneIdMapper_find_canonical_iana_from_bcp47_mv1_result { bool is_ok;} icu4x_TimeZoneIdMapper_find_canonical_iana_from_bcp47_mv1_result;
icu4x_TimeZoneIdMapper_find_canonical_iana_from_bcp47_mv1_result icu4x_TimeZoneIdMapper_find_canonical_iana_from_bcp47_mv1(const TimeZoneIdMapper* self, DiplomatStringView value, DiplomatWrite* write);


void icu4x_TimeZoneIdMapper_destroy_mv1(TimeZoneIdMapper* self);





#endif // TimeZoneIdMapper_H

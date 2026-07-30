#ifndef ZonedTime_H
#define ZonedTime_H

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"

#include "IanaParser.d.h"
#include "Rfc9557ParseError.d.h"

#include "ZonedTime.d.h"






typedef struct icu4x_ZonedTime_strict_from_string_mv1_result {union {ZonedTime ok; Rfc9557ParseError err;}; bool is_ok;} icu4x_ZonedTime_strict_from_string_mv1_result;
icu4x_ZonedTime_strict_from_string_mv1_result icu4x_ZonedTime_strict_from_string_mv1(DiplomatStringView v, const IanaParser* iana_parser);

typedef struct icu4x_ZonedTime_location_only_from_string_mv1_result {union {ZonedTime ok; Rfc9557ParseError err;}; bool is_ok;} icu4x_ZonedTime_location_only_from_string_mv1_result;
icu4x_ZonedTime_location_only_from_string_mv1_result icu4x_ZonedTime_location_only_from_string_mv1(DiplomatStringView v, const IanaParser* iana_parser);

typedef struct icu4x_ZonedTime_offset_only_from_string_mv1_result {union {ZonedTime ok; Rfc9557ParseError err;}; bool is_ok;} icu4x_ZonedTime_offset_only_from_string_mv1_result;
icu4x_ZonedTime_offset_only_from_string_mv1_result icu4x_ZonedTime_offset_only_from_string_mv1(DiplomatStringView v);

typedef struct icu4x_ZonedTime_lenient_from_string_mv1_result {union {ZonedTime ok; Rfc9557ParseError err;}; bool is_ok;} icu4x_ZonedTime_lenient_from_string_mv1_result;
icu4x_ZonedTime_lenient_from_string_mv1_result icu4x_ZonedTime_lenient_from_string_mv1(DiplomatStringView v, const IanaParser* iana_parser);





#endif // ZonedTime_H

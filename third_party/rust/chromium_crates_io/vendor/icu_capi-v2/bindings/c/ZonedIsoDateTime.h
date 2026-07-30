#ifndef ZonedIsoDateTime_H
#define ZonedIsoDateTime_H

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"

#include "IanaParser.d.h"
#include "Rfc9557ParseError.d.h"
#include "UtcOffset.d.h"
#include "VariantOffsetsCalculator.d.h"

#include "ZonedIsoDateTime.d.h"






typedef struct icu4x_ZonedIsoDateTime_strict_from_string_mv1_result {union {ZonedIsoDateTime ok; Rfc9557ParseError err;}; bool is_ok;} icu4x_ZonedIsoDateTime_strict_from_string_mv1_result;
icu4x_ZonedIsoDateTime_strict_from_string_mv1_result icu4x_ZonedIsoDateTime_strict_from_string_mv1(DiplomatStringView v, const IanaParser* iana_parser);

typedef struct icu4x_ZonedIsoDateTime_full_from_string_mv1_result {union {ZonedIsoDateTime ok; Rfc9557ParseError err;}; bool is_ok;} icu4x_ZonedIsoDateTime_full_from_string_mv1_result;
icu4x_ZonedIsoDateTime_full_from_string_mv1_result icu4x_ZonedIsoDateTime_full_from_string_mv1(DiplomatStringView v, const IanaParser* iana_parser, const VariantOffsetsCalculator* _offset_calculator);

ZonedIsoDateTime icu4x_ZonedIsoDateTime_from_epoch_milliseconds_and_utc_offset_mv1(int64_t epoch_milliseconds, const UtcOffset* utc_offset);





#endif // ZonedIsoDateTime_H

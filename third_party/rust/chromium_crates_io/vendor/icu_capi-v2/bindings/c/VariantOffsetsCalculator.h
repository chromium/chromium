#ifndef VariantOffsetsCalculator_H
#define VariantOffsetsCalculator_H

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"

#include "DataError.d.h"
#include "DataProvider.d.h"
#include "IsoDate.d.h"
#include "Time.d.h"
#include "TimeZone.d.h"
#include "VariantOffsets.d.h"

#include "VariantOffsetsCalculator.d.h"






VariantOffsetsCalculator* icu4x_VariantOffsetsCalculator_create_mv1(void);

typedef struct icu4x_VariantOffsetsCalculator_create_with_provider_mv1_result {union {VariantOffsetsCalculator* ok; DataError err;}; bool is_ok;} icu4x_VariantOffsetsCalculator_create_with_provider_mv1_result;
icu4x_VariantOffsetsCalculator_create_with_provider_mv1_result icu4x_VariantOffsetsCalculator_create_with_provider_mv1(const DataProvider* provider);

typedef struct icu4x_VariantOffsetsCalculator_compute_offsets_from_time_zone_and_date_time_mv1_result {union {VariantOffsets ok; }; bool is_ok;} icu4x_VariantOffsetsCalculator_compute_offsets_from_time_zone_and_date_time_mv1_result;
icu4x_VariantOffsetsCalculator_compute_offsets_from_time_zone_and_date_time_mv1_result icu4x_VariantOffsetsCalculator_compute_offsets_from_time_zone_and_date_time_mv1(const VariantOffsetsCalculator* self, const TimeZone* time_zone, const IsoDate* utc_date, const Time* utc_time);

typedef struct icu4x_VariantOffsetsCalculator_compute_offsets_from_time_zone_and_timestamp_mv1_result {union {VariantOffsets ok; }; bool is_ok;} icu4x_VariantOffsetsCalculator_compute_offsets_from_time_zone_and_timestamp_mv1_result;
icu4x_VariantOffsetsCalculator_compute_offsets_from_time_zone_and_timestamp_mv1_result icu4x_VariantOffsetsCalculator_compute_offsets_from_time_zone_and_timestamp_mv1(const VariantOffsetsCalculator* self, const TimeZone* time_zone, int64_t timestamp);

void icu4x_VariantOffsetsCalculator_destroy_mv1(VariantOffsetsCalculator* self);





#endif // VariantOffsetsCalculator_H

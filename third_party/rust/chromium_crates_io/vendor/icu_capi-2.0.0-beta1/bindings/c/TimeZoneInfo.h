#ifndef TimeZoneInfo_H
#define TimeZoneInfo_H

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"

#include "IsoDateTime.d.h"
#include "TimeZoneIdMapper.d.h"

#include "TimeZoneInfo.d.h"






TimeZoneInfo* icu4x_TimeZoneInfo_unknown_mv1(void);

TimeZoneInfo* icu4x_TimeZoneInfo_utc_mv1(void);

TimeZoneInfo* icu4x_TimeZoneInfo_from_parts_mv1(DiplomatStringView bcp47_id, int32_t offset_seconds, bool dst);

typedef struct icu4x_TimeZoneInfo_try_set_offset_seconds_mv1_result { bool is_ok;} icu4x_TimeZoneInfo_try_set_offset_seconds_mv1_result;
icu4x_TimeZoneInfo_try_set_offset_seconds_mv1_result icu4x_TimeZoneInfo_try_set_offset_seconds_mv1(TimeZoneInfo* self, int32_t offset_seconds);

void icu4x_TimeZoneInfo_set_offset_eighths_of_hour_mv1(TimeZoneInfo* self, int8_t offset_eighths_of_hour);

typedef struct icu4x_TimeZoneInfo_try_set_offset_str_mv1_result { bool is_ok;} icu4x_TimeZoneInfo_try_set_offset_str_mv1_result;
icu4x_TimeZoneInfo_try_set_offset_str_mv1_result icu4x_TimeZoneInfo_try_set_offset_str_mv1(TimeZoneInfo* self, DiplomatStringView offset);

typedef struct icu4x_TimeZoneInfo_offset_eighths_of_hour_mv1_result {union {int8_t ok; }; bool is_ok;} icu4x_TimeZoneInfo_offset_eighths_of_hour_mv1_result;
icu4x_TimeZoneInfo_offset_eighths_of_hour_mv1_result icu4x_TimeZoneInfo_offset_eighths_of_hour_mv1(const TimeZoneInfo* self);

void icu4x_TimeZoneInfo_clear_offset_mv1(TimeZoneInfo* self);

typedef struct icu4x_TimeZoneInfo_offset_seconds_mv1_result {union {int32_t ok; }; bool is_ok;} icu4x_TimeZoneInfo_offset_seconds_mv1_result;
icu4x_TimeZoneInfo_offset_seconds_mv1_result icu4x_TimeZoneInfo_offset_seconds_mv1(const TimeZoneInfo* self);

typedef struct icu4x_TimeZoneInfo_is_offset_non_negative_mv1_result {union {bool ok; }; bool is_ok;} icu4x_TimeZoneInfo_is_offset_non_negative_mv1_result;
icu4x_TimeZoneInfo_is_offset_non_negative_mv1_result icu4x_TimeZoneInfo_is_offset_non_negative_mv1(const TimeZoneInfo* self);

typedef struct icu4x_TimeZoneInfo_is_offset_zero_mv1_result {union {bool ok; }; bool is_ok;} icu4x_TimeZoneInfo_is_offset_zero_mv1_result;
icu4x_TimeZoneInfo_is_offset_zero_mv1_result icu4x_TimeZoneInfo_is_offset_zero_mv1(const TimeZoneInfo* self);

typedef struct icu4x_TimeZoneInfo_offset_hours_part_mv1_result {union {int32_t ok; }; bool is_ok;} icu4x_TimeZoneInfo_offset_hours_part_mv1_result;
icu4x_TimeZoneInfo_offset_hours_part_mv1_result icu4x_TimeZoneInfo_offset_hours_part_mv1(const TimeZoneInfo* self);

typedef struct icu4x_TimeZoneInfo_offset_minutes_part_mv1_result {union {uint32_t ok; }; bool is_ok;} icu4x_TimeZoneInfo_offset_minutes_part_mv1_result;
icu4x_TimeZoneInfo_offset_minutes_part_mv1_result icu4x_TimeZoneInfo_offset_minutes_part_mv1(const TimeZoneInfo* self);

typedef struct icu4x_TimeZoneInfo_offset_seconds_part_mv1_result {union {uint32_t ok; }; bool is_ok;} icu4x_TimeZoneInfo_offset_seconds_part_mv1_result;
icu4x_TimeZoneInfo_offset_seconds_part_mv1_result icu4x_TimeZoneInfo_offset_seconds_part_mv1(const TimeZoneInfo* self);

void icu4x_TimeZoneInfo_set_time_zone_id_mv1(TimeZoneInfo* self, DiplomatStringView id);

void icu4x_TimeZoneInfo_set_iana_time_zone_id_mv1(TimeZoneInfo* self, const TimeZoneIdMapper* mapper, DiplomatStringView id);

void icu4x_TimeZoneInfo_time_zone_id_mv1(const TimeZoneInfo* self, DiplomatWrite* write);

void icu4x_TimeZoneInfo_clear_zone_variant_mv1(TimeZoneInfo* self);

void icu4x_TimeZoneInfo_set_standard_time_mv1(TimeZoneInfo* self);

void icu4x_TimeZoneInfo_set_daylight_time_mv1(TimeZoneInfo* self);

typedef struct icu4x_TimeZoneInfo_is_standard_time_mv1_result {union {bool ok; }; bool is_ok;} icu4x_TimeZoneInfo_is_standard_time_mv1_result;
icu4x_TimeZoneInfo_is_standard_time_mv1_result icu4x_TimeZoneInfo_is_standard_time_mv1(const TimeZoneInfo* self);

typedef struct icu4x_TimeZoneInfo_is_daylight_time_mv1_result {union {bool ok; }; bool is_ok;} icu4x_TimeZoneInfo_is_daylight_time_mv1_result;
icu4x_TimeZoneInfo_is_daylight_time_mv1_result icu4x_TimeZoneInfo_is_daylight_time_mv1(const TimeZoneInfo* self);

void icu4x_TimeZoneInfo_set_local_time_mv1(TimeZoneInfo* self, const IsoDateTime* datetime);

void icu4x_TimeZoneInfo_clear_local_time_mv1(TimeZoneInfo* self);

IsoDateTime* icu4x_TimeZoneInfo_get_local_time_mv1(const TimeZoneInfo* self);


void icu4x_TimeZoneInfo_destroy_mv1(TimeZoneInfo* self);





#endif // TimeZoneInfo_H

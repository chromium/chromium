#ifndef TimeZone_H
#define TimeZone_H

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"

#include "TemporalError.d.h"

#include "TimeZone.d.h"






typedef struct temporal_rs_TimeZone_try_from_identifier_str_result {union {TimeZone* ok; TemporalError err;}; bool is_ok;} temporal_rs_TimeZone_try_from_identifier_str_result;
temporal_rs_TimeZone_try_from_identifier_str_result temporal_rs_TimeZone_try_from_identifier_str(DiplomatStringView ident);

typedef struct temporal_rs_TimeZone_try_from_offset_str_result {union {TimeZone* ok; TemporalError err;}; bool is_ok;} temporal_rs_TimeZone_try_from_offset_str_result;
temporal_rs_TimeZone_try_from_offset_str_result temporal_rs_TimeZone_try_from_offset_str(DiplomatStringView ident);

typedef struct temporal_rs_TimeZone_try_from_str_result {union {TimeZone* ok; TemporalError err;}; bool is_ok;} temporal_rs_TimeZone_try_from_str_result;
temporal_rs_TimeZone_try_from_str_result temporal_rs_TimeZone_try_from_str(DiplomatStringView ident);

void temporal_rs_TimeZone_identifier(const TimeZone* self, DiplomatWrite* write);

TimeZone* temporal_rs_TimeZone_utc(void);

TimeZone* temporal_rs_TimeZone_clone(const TimeZone* self);

bool temporal_rs_TimeZone_is_valid(const TimeZone* self);

void temporal_rs_TimeZone_destroy(TimeZone* self);





#endif // TimeZone_H

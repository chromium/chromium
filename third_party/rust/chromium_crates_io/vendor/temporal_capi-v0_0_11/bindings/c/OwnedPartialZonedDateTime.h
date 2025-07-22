#ifndef OwnedPartialZonedDateTime_H
#define OwnedPartialZonedDateTime_H

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"

#include "TemporalError.d.h"

#include "OwnedPartialZonedDateTime.d.h"






typedef struct temporal_rs_OwnedPartialZonedDateTime_from_utf8_result {union {OwnedPartialZonedDateTime* ok; TemporalError err;}; bool is_ok;} temporal_rs_OwnedPartialZonedDateTime_from_utf8_result;
temporal_rs_OwnedPartialZonedDateTime_from_utf8_result temporal_rs_OwnedPartialZonedDateTime_from_utf8(DiplomatStringView s);

typedef struct temporal_rs_OwnedPartialZonedDateTime_from_utf16_result {union {OwnedPartialZonedDateTime* ok; TemporalError err;}; bool is_ok;} temporal_rs_OwnedPartialZonedDateTime_from_utf16_result;
temporal_rs_OwnedPartialZonedDateTime_from_utf16_result temporal_rs_OwnedPartialZonedDateTime_from_utf16(DiplomatString16View s);

void temporal_rs_OwnedPartialZonedDateTime_destroy(OwnedPartialZonedDateTime* self);





#endif // OwnedPartialZonedDateTime_H

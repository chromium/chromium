#ifndef ParsedZonedDateTime_H
#define ParsedZonedDateTime_H

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"

#include "TemporalError.d.h"

#include "ParsedZonedDateTime.d.h"






typedef struct temporal_rs_ParsedZonedDateTime_from_utf8_result {union {ParsedZonedDateTime* ok; TemporalError err;}; bool is_ok;} temporal_rs_ParsedZonedDateTime_from_utf8_result;
temporal_rs_ParsedZonedDateTime_from_utf8_result temporal_rs_ParsedZonedDateTime_from_utf8(DiplomatStringView s);

typedef struct temporal_rs_ParsedZonedDateTime_from_utf16_result {union {ParsedZonedDateTime* ok; TemporalError err;}; bool is_ok;} temporal_rs_ParsedZonedDateTime_from_utf16_result;
temporal_rs_ParsedZonedDateTime_from_utf16_result temporal_rs_ParsedZonedDateTime_from_utf16(DiplomatString16View s);

void temporal_rs_ParsedZonedDateTime_destroy(ParsedZonedDateTime* self);





#endif // ParsedZonedDateTime_H

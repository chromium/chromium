#ifndef TimeZone_H
#define TimeZone_H

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"

#include "TimeZoneInfo.d.h"
#include "UtcOffset.d.h"

#include "TimeZone.d.h"






TimeZone* icu4x_TimeZone_unknown_mv1(void);

bool icu4x_TimeZone_is_unknown_mv1(const TimeZone* self);

TimeZone* icu4x_TimeZone_create_from_iana_id_mv1(DiplomatStringView iana_id);

TimeZone* icu4x_TimeZone_create_from_windows_id_mv1(DiplomatStringView windows_id, DiplomatStringView region);

TimeZone* icu4x_TimeZone_create_from_system_id_mv1(DiplomatStringView id, DiplomatStringView _region);

TimeZone* icu4x_TimeZone_create_from_bcp47_mv1(DiplomatStringView id);

TimeZoneInfo* icu4x_TimeZone_with_offset_mv1(const TimeZone* self, const UtcOffset* offset);

TimeZoneInfo* icu4x_TimeZone_without_offset_mv1(const TimeZone* self);

void icu4x_TimeZone_destroy_mv1(TimeZone* self);





#endif // TimeZone_H

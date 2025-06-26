#ifndef AnyCalendarKind_H
#define AnyCalendarKind_H

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"


#include "AnyCalendarKind.d.h"






typedef struct temporal_rs_AnyCalendarKind_get_for_str_result {union {AnyCalendarKind ok; }; bool is_ok;} temporal_rs_AnyCalendarKind_get_for_str_result;
temporal_rs_AnyCalendarKind_get_for_str_result temporal_rs_AnyCalendarKind_get_for_str(DiplomatStringView s);





#endif // AnyCalendarKind_H

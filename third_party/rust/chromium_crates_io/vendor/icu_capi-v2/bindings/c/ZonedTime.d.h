#ifndef ZonedTime_D_H
#define ZonedTime_D_H

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"

#include "Time.d.h"
#include "TimeZoneInfo.d.h"




typedef struct ZonedTime {
  Time* time;
  TimeZoneInfo* zone;
} ZonedTime;

typedef struct ZonedTime_option {union { ZonedTime ok; }; bool is_ok; } ZonedTime_option;



#endif // ZonedTime_D_H

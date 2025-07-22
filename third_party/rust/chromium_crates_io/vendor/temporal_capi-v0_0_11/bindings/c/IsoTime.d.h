#ifndef IsoTime_D_H
#define IsoTime_D_H

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"





typedef struct IsoTime {
  uint8_t hour;
  uint8_t minute;
  uint8_t second;
  uint16_t millisecond;
  uint16_t microsecond;
  uint16_t nanosecond;
} IsoTime;

typedef struct IsoTime_option {union { IsoTime ok; }; bool is_ok; } IsoTime_option;



#endif // IsoTime_D_H

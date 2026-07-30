#ifndef DateDuration_D_H
#define DateDuration_D_H

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"





typedef struct DateDuration {
  bool is_negative;
  uint32_t years;
  uint32_t months;
  uint32_t weeks;
  uint32_t days;
} DateDuration;

typedef struct DateDuration_option {union { DateDuration ok; }; bool is_ok; } DateDuration_option;



#endif // DateDuration_D_H

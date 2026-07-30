#ifndef DateDurationUnit_D_H
#define DateDurationUnit_D_H

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"





typedef enum DateDurationUnit {
  DateDurationUnit_Years = 0,
  DateDurationUnit_Months = 1,
  DateDurationUnit_Weeks = 2,
  DateDurationUnit_Days = 3,
} DateDurationUnit;

typedef struct DateDurationUnit_option {union { DateDurationUnit ok; }; bool is_ok; } DateDurationUnit_option;



#endif // DateDurationUnit_D_H

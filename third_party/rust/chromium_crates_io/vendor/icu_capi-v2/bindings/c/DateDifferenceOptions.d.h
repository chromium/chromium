#ifndef DateDifferenceOptions_D_H
#define DateDifferenceOptions_D_H

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"

#include "DateDurationUnit.d.h"




typedef struct DateDifferenceOptions {
  DateDurationUnit_option largest_unit;
} DateDifferenceOptions;

typedef struct DateDifferenceOptions_option {union { DateDifferenceOptions ok; }; bool is_ok; } DateDifferenceOptions_option;



#endif // DateDifferenceOptions_D_H

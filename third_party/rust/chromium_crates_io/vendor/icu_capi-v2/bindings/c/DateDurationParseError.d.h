#ifndef DateDurationParseError_D_H
#define DateDurationParseError_D_H

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"





typedef enum DateDurationParseError {
  DateDurationParseError_InvalidStructure = 0,
  DateDurationParseError_TimeNotSupported = 1,
  DateDurationParseError_MissingValue = 2,
  DateDurationParseError_DuplicateUnit = 3,
  DateDurationParseError_NumberOverflow = 4,
  DateDurationParseError_PlusNotAllowed = 5,
} DateDurationParseError;

typedef struct DateDurationParseError_option {union { DateDurationParseError ok; }; bool is_ok; } DateDurationParseError_option;



#endif // DateDurationParseError_D_H

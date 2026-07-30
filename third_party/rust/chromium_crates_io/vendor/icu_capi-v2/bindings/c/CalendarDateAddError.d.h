#ifndef CalendarDateAddError_D_H
#define CalendarDateAddError_D_H

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"





typedef enum CalendarDateAddError {
  CalendarDateAddError_Unknown = 0,
  CalendarDateAddError_InvalidDay = 1,
  CalendarDateAddError_MonthNotInYear = 2,
  CalendarDateAddError_Overflow = 3,
} CalendarDateAddError;

typedef struct CalendarDateAddError_option {union { CalendarDateAddError ok; }; bool is_ok; } CalendarDateAddError_option;



#endif // CalendarDateAddError_D_H

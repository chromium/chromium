#ifndef CalendarDateFromFieldsError_D_H
#define CalendarDateFromFieldsError_D_H

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"





typedef enum CalendarDateFromFieldsError {
  CalendarDateFromFieldsError_Unknown = 0,
  CalendarDateFromFieldsError_InvalidDay = 1,
  CalendarDateFromFieldsError_InvalidOrdinalMonth = 9,
  CalendarDateFromFieldsError_InvalidEra = 2,
  CalendarDateFromFieldsError_MonthCodeInvalidSyntax = 3,
  CalendarDateFromFieldsError_MonthNotInCalendar = 4,
  CalendarDateFromFieldsError_MonthNotInYear = 5,
  CalendarDateFromFieldsError_InconsistentYear = 6,
  CalendarDateFromFieldsError_InconsistentMonth = 7,
  CalendarDateFromFieldsError_NotEnoughFields = 8,
  CalendarDateFromFieldsError_TooManyFields = 10,
  CalendarDateFromFieldsError_Overflow = 11,
} CalendarDateFromFieldsError;

typedef struct CalendarDateFromFieldsError_option {union { CalendarDateFromFieldsError ok; }; bool is_ok; } CalendarDateFromFieldsError_option;



#endif // CalendarDateFromFieldsError_D_H

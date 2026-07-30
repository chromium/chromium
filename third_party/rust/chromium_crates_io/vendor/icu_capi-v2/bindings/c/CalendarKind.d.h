#ifndef CalendarKind_D_H
#define CalendarKind_D_H

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"





typedef enum CalendarKind {
  CalendarKind_Iso = 0,
  CalendarKind_Gregorian = 1,
  CalendarKind_Buddhist = 2,
  CalendarKind_Japanese = 3,
  CalendarKind_JapaneseExtended = 4,
  CalendarKind_Ethiopian = 5,
  CalendarKind_EthiopianAmeteAlem = 6,
  CalendarKind_Indian = 7,
  CalendarKind_Coptic = 8,
  CalendarKind_Dangi = 9,
  CalendarKind_Chinese = 10,
  CalendarKind_Hebrew = 11,
  CalendarKind_HijriTabularTypeIIFriday = 12,
  CalendarKind_HijriSimulatedMecca = 18,
  CalendarKind_HijriTabularTypeIIThursday = 14,
  CalendarKind_HijriUmmAlQura = 15,
  CalendarKind_Persian = 16,
  CalendarKind_Roc = 17,
} CalendarKind;

typedef struct CalendarKind_option {union { CalendarKind ok; }; bool is_ok; } CalendarKind_option;



#endif // CalendarKind_D_H

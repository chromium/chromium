#ifndef IsoWeekday_D_H
#define IsoWeekday_D_H

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"





typedef enum IsoWeekday {
  IsoWeekday_Monday = 1,
  IsoWeekday_Tuesday = 2,
  IsoWeekday_Wednesday = 3,
  IsoWeekday_Thursday = 4,
  IsoWeekday_Friday = 5,
  IsoWeekday_Saturday = 6,
  IsoWeekday_Sunday = 7,
} IsoWeekday;

typedef struct IsoWeekday_option {union { IsoWeekday ok; }; bool is_ok; } IsoWeekday_option;



#endif // IsoWeekday_D_H

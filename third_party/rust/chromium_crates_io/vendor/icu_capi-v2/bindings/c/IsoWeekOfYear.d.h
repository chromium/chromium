#ifndef IsoWeekOfYear_D_H
#define IsoWeekOfYear_D_H

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"





typedef struct IsoWeekOfYear {
  uint8_t week_number;
  int32_t iso_year;
} IsoWeekOfYear;

typedef struct IsoWeekOfYear_option {union { IsoWeekOfYear ok; }; bool is_ok; } IsoWeekOfYear_option;



#endif // IsoWeekOfYear_D_H

#ifndef IsoDate_D_H
#define IsoDate_D_H

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"





typedef struct IsoDate {
  int32_t year;
  uint8_t month;
  uint8_t day;
} IsoDate;

typedef struct IsoDate_option {union { IsoDate ok; }; bool is_ok; } IsoDate_option;



#endif // IsoDate_D_H

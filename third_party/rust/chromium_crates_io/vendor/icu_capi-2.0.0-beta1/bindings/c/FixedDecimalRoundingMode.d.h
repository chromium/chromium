#ifndef FixedDecimalRoundingMode_D_H
#define FixedDecimalRoundingMode_D_H

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"





typedef enum FixedDecimalRoundingMode {
  FixedDecimalRoundingMode_Ceil = 0,
  FixedDecimalRoundingMode_Expand = 1,
  FixedDecimalRoundingMode_Floor = 2,
  FixedDecimalRoundingMode_Trunc = 3,
  FixedDecimalRoundingMode_HalfCeil = 4,
  FixedDecimalRoundingMode_HalfExpand = 5,
  FixedDecimalRoundingMode_HalfFloor = 6,
  FixedDecimalRoundingMode_HalfTrunc = 7,
  FixedDecimalRoundingMode_HalfEven = 8,
} FixedDecimalRoundingMode;

typedef struct FixedDecimalRoundingMode_option {union { FixedDecimalRoundingMode ok; }; bool is_ok; } FixedDecimalRoundingMode_option;



#endif // FixedDecimalRoundingMode_D_H

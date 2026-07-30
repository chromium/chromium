#ifndef DecimalSignedRoundingMode_D_H
#define DecimalSignedRoundingMode_D_H

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"





typedef enum DecimalSignedRoundingMode {
  DecimalSignedRoundingMode_Expand = 0,
  DecimalSignedRoundingMode_Trunc = 1,
  DecimalSignedRoundingMode_HalfExpand = 2,
  DecimalSignedRoundingMode_HalfTrunc = 3,
  DecimalSignedRoundingMode_HalfEven = 4,
  DecimalSignedRoundingMode_Ceil = 5,
  DecimalSignedRoundingMode_Floor = 6,
  DecimalSignedRoundingMode_HalfCeil = 7,
  DecimalSignedRoundingMode_HalfFloor = 8,
} DecimalSignedRoundingMode;

typedef struct DecimalSignedRoundingMode_option {union { DecimalSignedRoundingMode ok; }; bool is_ok; } DecimalSignedRoundingMode_option;



#endif // DecimalSignedRoundingMode_D_H

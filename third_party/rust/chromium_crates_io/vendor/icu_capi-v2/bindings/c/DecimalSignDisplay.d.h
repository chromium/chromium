#ifndef DecimalSignDisplay_D_H
#define DecimalSignDisplay_D_H

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"





typedef enum DecimalSignDisplay {
  DecimalSignDisplay_Auto = 0,
  DecimalSignDisplay_Never = 1,
  DecimalSignDisplay_Always = 2,
  DecimalSignDisplay_ExceptZero = 3,
  DecimalSignDisplay_Negative = 4,
} DecimalSignDisplay;

typedef struct DecimalSignDisplay_option {union { DecimalSignDisplay ok; }; bool is_ok; } DecimalSignDisplay_option;



#endif // DecimalSignDisplay_D_H

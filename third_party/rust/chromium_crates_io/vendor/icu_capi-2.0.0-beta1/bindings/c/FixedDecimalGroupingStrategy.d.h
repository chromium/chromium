#ifndef FixedDecimalGroupingStrategy_D_H
#define FixedDecimalGroupingStrategy_D_H

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"





typedef enum FixedDecimalGroupingStrategy {
  FixedDecimalGroupingStrategy_Auto = 0,
  FixedDecimalGroupingStrategy_Never = 1,
  FixedDecimalGroupingStrategy_Always = 2,
  FixedDecimalGroupingStrategy_Min2 = 3,
} FixedDecimalGroupingStrategy;

typedef struct FixedDecimalGroupingStrategy_option {union { FixedDecimalGroupingStrategy ok; }; bool is_ok; } FixedDecimalGroupingStrategy_option;



#endif // FixedDecimalGroupingStrategy_D_H

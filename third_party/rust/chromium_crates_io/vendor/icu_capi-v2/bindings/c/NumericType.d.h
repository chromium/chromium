#ifndef NumericType_D_H
#define NumericType_D_H

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"





typedef enum NumericType {
  NumericType_None = 0,
  NumericType_Decimal = 1,
  NumericType_Digit = 2,
  NumericType_Numeric = 3,
} NumericType;

typedef struct NumericType_option {union { NumericType ok; }; bool is_ok; } NumericType_option;



#endif // NumericType_D_H

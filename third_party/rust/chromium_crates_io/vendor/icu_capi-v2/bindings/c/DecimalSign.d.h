#ifndef DecimalSign_D_H
#define DecimalSign_D_H

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"





typedef enum DecimalSign {
  DecimalSign_None = 0,
  DecimalSign_Negative = 1,
  DecimalSign_Positive = 2,
} DecimalSign;

typedef struct DecimalSign_option {union { DecimalSign ok; }; bool is_ok; } DecimalSign_option;



#endif // DecimalSign_D_H

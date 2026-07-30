#ifndef DateMissingFieldsStrategy_D_H
#define DateMissingFieldsStrategy_D_H

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"





typedef enum DateMissingFieldsStrategy {
  DateMissingFieldsStrategy_Reject = 0,
  DateMissingFieldsStrategy_Ecma = 1,
} DateMissingFieldsStrategy;

typedef struct DateMissingFieldsStrategy_option {union { DateMissingFieldsStrategy ok; }; bool is_ok; } DateMissingFieldsStrategy_option;



#endif // DateMissingFieldsStrategy_D_H

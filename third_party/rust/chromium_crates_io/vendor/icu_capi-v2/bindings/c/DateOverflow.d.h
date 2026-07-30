#ifndef DateOverflow_D_H
#define DateOverflow_D_H

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"





typedef enum DateOverflow {
  DateOverflow_Constrain = 0,
  DateOverflow_Reject = 1,
} DateOverflow;

typedef struct DateOverflow_option {union { DateOverflow ok; }; bool is_ok; } DateOverflow_option;



#endif // DateOverflow_D_H

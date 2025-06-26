#ifndef DurationOverflow_D_H
#define DurationOverflow_D_H

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"





typedef enum DurationOverflow {
  DurationOverflow_Constrain = 0,
  DurationOverflow_Balance = 1,
} DurationOverflow;

typedef struct DurationOverflow_option {union { DurationOverflow ok; }; bool is_ok; } DurationOverflow_option;



#endif // DurationOverflow_D_H

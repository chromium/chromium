#ifndef DateAddOptions_D_H
#define DateAddOptions_D_H

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"

#include "DateOverflow.d.h"




typedef struct DateAddOptions {
  DateOverflow_option overflow;
} DateAddOptions;

typedef struct DateAddOptions_option {union { DateAddOptions ok; }; bool is_ok; } DateAddOptions_option;



#endif // DateAddOptions_D_H

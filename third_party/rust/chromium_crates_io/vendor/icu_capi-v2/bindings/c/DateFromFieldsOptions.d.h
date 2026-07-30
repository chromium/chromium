#ifndef DateFromFieldsOptions_D_H
#define DateFromFieldsOptions_D_H

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"

#include "DateMissingFieldsStrategy.d.h"
#include "DateOverflow.d.h"




typedef struct DateFromFieldsOptions {
  DateOverflow_option overflow;
  DateMissingFieldsStrategy_option missing_fields_strategy;
} DateFromFieldsOptions;

typedef struct DateFromFieldsOptions_option {union { DateFromFieldsOptions ok; }; bool is_ok; } DateFromFieldsOptions_option;



#endif // DateFromFieldsOptions_D_H

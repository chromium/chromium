#ifndef DateFields_D_H
#define DateFields_D_H

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"





typedef struct DateFields {
  OptionStringView era;
  OptionI32 era_year;
  OptionI32 extended_year;
  OptionStringView month_code;
  OptionU8 ordinal_month;
  OptionU8 day;
} DateFields;

typedef struct DateFields_option {union { DateFields ok; }; bool is_ok; } DateFields_option;



#endif // DateFields_D_H

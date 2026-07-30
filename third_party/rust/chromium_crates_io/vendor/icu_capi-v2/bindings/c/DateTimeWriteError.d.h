#ifndef DateTimeWriteError_D_H
#define DateTimeWriteError_D_H

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"





typedef enum DateTimeWriteError {
  DateTimeWriteError_Unknown = 0,
  DateTimeWriteError_MissingTimeZoneVariant = 1,
} DateTimeWriteError;

typedef struct DateTimeWriteError_option {union { DateTimeWriteError ok; }; bool is_ok; } DateTimeWriteError_option;



#endif // DateTimeWriteError_D_H

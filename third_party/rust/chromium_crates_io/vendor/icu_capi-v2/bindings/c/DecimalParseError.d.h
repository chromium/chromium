#ifndef DecimalParseError_D_H
#define DecimalParseError_D_H

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"





typedef enum DecimalParseError {
  DecimalParseError_Unknown = 0,
  DecimalParseError_Limit = 1,
  DecimalParseError_Syntax = 2,
} DecimalParseError;

typedef struct DecimalParseError_option {union { DecimalParseError ok; }; bool is_ok; } DecimalParseError_option;



#endif // DecimalParseError_D_H

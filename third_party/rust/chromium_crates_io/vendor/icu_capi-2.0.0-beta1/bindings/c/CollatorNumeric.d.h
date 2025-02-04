#ifndef CollatorNumeric_D_H
#define CollatorNumeric_D_H

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"





typedef enum CollatorNumeric {
  CollatorNumeric_Off = 0,
  CollatorNumeric_On = 1,
} CollatorNumeric;

typedef struct CollatorNumeric_option {union { CollatorNumeric ok; }; bool is_ok; } CollatorNumeric_option;



#endif // CollatorNumeric_D_H

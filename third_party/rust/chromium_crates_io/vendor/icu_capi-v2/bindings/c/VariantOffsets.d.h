#ifndef VariantOffsets_D_H
#define VariantOffsets_D_H

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"

#include "UtcOffset.d.h"




typedef struct VariantOffsets {
  UtcOffset* standard;
  UtcOffset* daylight;
} VariantOffsets;

typedef struct VariantOffsets_option {union { VariantOffsets ok; }; bool is_ok; } VariantOffsets_option;



#endif // VariantOffsets_D_H

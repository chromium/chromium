#ifndef BidiClass_H
#define BidiClass_H

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"


#include "BidiClass.d.h"






uint8_t icu4x_BidiClass_to_integer_mv1(BidiClass self);

typedef struct icu4x_BidiClass_from_integer_mv1_result {union {BidiClass ok; }; bool is_ok;} icu4x_BidiClass_from_integer_mv1_result;
icu4x_BidiClass_from_integer_mv1_result icu4x_BidiClass_from_integer_mv1(uint8_t other);






#endif // BidiClass_H

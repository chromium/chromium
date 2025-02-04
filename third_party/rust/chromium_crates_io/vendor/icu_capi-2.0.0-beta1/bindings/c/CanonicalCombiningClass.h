#ifndef CanonicalCombiningClass_H
#define CanonicalCombiningClass_H

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"


#include "CanonicalCombiningClass.d.h"






uint8_t icu4x_CanonicalCombiningClass_to_integer_mv1(CanonicalCombiningClass self);

typedef struct icu4x_CanonicalCombiningClass_from_integer_mv1_result {union {CanonicalCombiningClass ok; }; bool is_ok;} icu4x_CanonicalCombiningClass_from_integer_mv1_result;
icu4x_CanonicalCombiningClass_from_integer_mv1_result icu4x_CanonicalCombiningClass_from_integer_mv1(uint8_t other);






#endif // CanonicalCombiningClass_H

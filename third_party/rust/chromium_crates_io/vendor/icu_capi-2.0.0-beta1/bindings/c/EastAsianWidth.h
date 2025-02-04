#ifndef EastAsianWidth_H
#define EastAsianWidth_H

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"


#include "EastAsianWidth.d.h"






uint8_t icu4x_EastAsianWidth_to_integer_mv1(EastAsianWidth self);

typedef struct icu4x_EastAsianWidth_from_integer_mv1_result {union {EastAsianWidth ok; }; bool is_ok;} icu4x_EastAsianWidth_from_integer_mv1_result;
icu4x_EastAsianWidth_from_integer_mv1_result icu4x_EastAsianWidth_from_integer_mv1(uint8_t other);






#endif // EastAsianWidth_H

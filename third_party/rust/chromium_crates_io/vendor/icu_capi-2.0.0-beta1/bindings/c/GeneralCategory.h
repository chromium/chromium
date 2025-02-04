#ifndef GeneralCategory_H
#define GeneralCategory_H

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"


#include "GeneralCategory.d.h"






uint8_t icu4x_GeneralCategory_to_integer_mv1(GeneralCategory self);

typedef struct icu4x_GeneralCategory_from_integer_mv1_result {union {GeneralCategory ok; }; bool is_ok;} icu4x_GeneralCategory_from_integer_mv1_result;
icu4x_GeneralCategory_from_integer_mv1_result icu4x_GeneralCategory_from_integer_mv1(uint8_t other);






#endif // GeneralCategory_H

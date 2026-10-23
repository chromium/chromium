#ifndef Block_H
#define Block_H

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"


#include "Block.d.h"






Block icu4x_Block_for_char_mv1(char32_t ch);

typedef struct icu4x_Block_long_name_mv1_result {union {DiplomatStringView ok; }; bool is_ok;} icu4x_Block_long_name_mv1_result;
icu4x_Block_long_name_mv1_result icu4x_Block_long_name_mv1(Block self);

typedef struct icu4x_Block_short_name_mv1_result {union {DiplomatStringView ok; }; bool is_ok;} icu4x_Block_short_name_mv1_result;
icu4x_Block_short_name_mv1_result icu4x_Block_short_name_mv1(Block self);

uint16_t icu4x_Block_to_integer_value_mv1(Block self);

typedef struct icu4x_Block_from_integer_value_mv1_result {union {Block ok; }; bool is_ok;} icu4x_Block_from_integer_value_mv1_result;
icu4x_Block_from_integer_value_mv1_result icu4x_Block_from_integer_value_mv1(uint16_t other);

typedef struct icu4x_Block_try_from_str_mv1_result {union {Block ok; }; bool is_ok;} icu4x_Block_try_from_str_mv1_result;
icu4x_Block_try_from_str_mv1_result icu4x_Block_try_from_str_mv1(DiplomatStringView s);





#endif // Block_H

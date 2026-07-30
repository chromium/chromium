#ifndef NumericType_H
#define NumericType_H

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"


#include "NumericType.d.h"






NumericType icu4x_NumericType_for_char_mv1(char32_t ch);

typedef struct icu4x_NumericType_long_name_mv1_result {union {DiplomatStringView ok; }; bool is_ok;} icu4x_NumericType_long_name_mv1_result;
icu4x_NumericType_long_name_mv1_result icu4x_NumericType_long_name_mv1(NumericType self);

typedef struct icu4x_NumericType_short_name_mv1_result {union {DiplomatStringView ok; }; bool is_ok;} icu4x_NumericType_short_name_mv1_result;
icu4x_NumericType_short_name_mv1_result icu4x_NumericType_short_name_mv1(NumericType self);

uint8_t icu4x_NumericType_to_integer_value_mv1(NumericType self);

typedef struct icu4x_NumericType_from_integer_value_mv1_result {union {NumericType ok; }; bool is_ok;} icu4x_NumericType_from_integer_value_mv1_result;
icu4x_NumericType_from_integer_value_mv1_result icu4x_NumericType_from_integer_value_mv1(uint8_t other);

typedef struct icu4x_NumericType_try_from_str_mv1_result {union {NumericType ok; }; bool is_ok;} icu4x_NumericType_try_from_str_mv1_result;
icu4x_NumericType_try_from_str_mv1_result icu4x_NumericType_try_from_str_mv1(DiplomatStringView s);





#endif // NumericType_H

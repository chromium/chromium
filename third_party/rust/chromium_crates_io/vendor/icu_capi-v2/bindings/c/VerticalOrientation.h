#ifndef VerticalOrientation_H
#define VerticalOrientation_H

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"


#include "VerticalOrientation.d.h"






VerticalOrientation icu4x_VerticalOrientation_for_char_mv1(char32_t ch);

typedef struct icu4x_VerticalOrientation_long_name_mv1_result {union {DiplomatStringView ok; }; bool is_ok;} icu4x_VerticalOrientation_long_name_mv1_result;
icu4x_VerticalOrientation_long_name_mv1_result icu4x_VerticalOrientation_long_name_mv1(VerticalOrientation self);

typedef struct icu4x_VerticalOrientation_short_name_mv1_result {union {DiplomatStringView ok; }; bool is_ok;} icu4x_VerticalOrientation_short_name_mv1_result;
icu4x_VerticalOrientation_short_name_mv1_result icu4x_VerticalOrientation_short_name_mv1(VerticalOrientation self);

uint8_t icu4x_VerticalOrientation_to_integer_value_mv1(VerticalOrientation self);

typedef struct icu4x_VerticalOrientation_from_integer_value_mv1_result {union {VerticalOrientation ok; }; bool is_ok;} icu4x_VerticalOrientation_from_integer_value_mv1_result;
icu4x_VerticalOrientation_from_integer_value_mv1_result icu4x_VerticalOrientation_from_integer_value_mv1(uint8_t other);

typedef struct icu4x_VerticalOrientation_try_from_str_mv1_result {union {VerticalOrientation ok; }; bool is_ok;} icu4x_VerticalOrientation_try_from_str_mv1_result;
icu4x_VerticalOrientation_try_from_str_mv1_result icu4x_VerticalOrientation_try_from_str_mv1(DiplomatStringView s);





#endif // VerticalOrientation_H

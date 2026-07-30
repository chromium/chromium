#ifndef IndicSyllabicCategory_H
#define IndicSyllabicCategory_H

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"


#include "IndicSyllabicCategory.d.h"






IndicSyllabicCategory icu4x_IndicSyllabicCategory_for_char_mv1(char32_t ch);

typedef struct icu4x_IndicSyllabicCategory_long_name_mv1_result {union {DiplomatStringView ok; }; bool is_ok;} icu4x_IndicSyllabicCategory_long_name_mv1_result;
icu4x_IndicSyllabicCategory_long_name_mv1_result icu4x_IndicSyllabicCategory_long_name_mv1(IndicSyllabicCategory self);

typedef struct icu4x_IndicSyllabicCategory_short_name_mv1_result {union {DiplomatStringView ok; }; bool is_ok;} icu4x_IndicSyllabicCategory_short_name_mv1_result;
icu4x_IndicSyllabicCategory_short_name_mv1_result icu4x_IndicSyllabicCategory_short_name_mv1(IndicSyllabicCategory self);

uint8_t icu4x_IndicSyllabicCategory_to_integer_value_mv1(IndicSyllabicCategory self);

typedef struct icu4x_IndicSyllabicCategory_from_integer_value_mv1_result {union {IndicSyllabicCategory ok; }; bool is_ok;} icu4x_IndicSyllabicCategory_from_integer_value_mv1_result;
icu4x_IndicSyllabicCategory_from_integer_value_mv1_result icu4x_IndicSyllabicCategory_from_integer_value_mv1(uint8_t other);

typedef struct icu4x_IndicSyllabicCategory_try_from_str_mv1_result {union {IndicSyllabicCategory ok; }; bool is_ok;} icu4x_IndicSyllabicCategory_try_from_str_mv1_result;
icu4x_IndicSyllabicCategory_try_from_str_mv1_result icu4x_IndicSyllabicCategory_try_from_str_mv1(DiplomatStringView s);





#endif // IndicSyllabicCategory_H

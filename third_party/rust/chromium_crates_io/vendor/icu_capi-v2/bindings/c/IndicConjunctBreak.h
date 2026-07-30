#ifndef IndicConjunctBreak_H
#define IndicConjunctBreak_H

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"


#include "IndicConjunctBreak.d.h"






IndicConjunctBreak icu4x_IndicConjunctBreak_for_char_mv1(char32_t ch);

typedef struct icu4x_IndicConjunctBreak_long_name_mv1_result {union {DiplomatStringView ok; }; bool is_ok;} icu4x_IndicConjunctBreak_long_name_mv1_result;
icu4x_IndicConjunctBreak_long_name_mv1_result icu4x_IndicConjunctBreak_long_name_mv1(IndicConjunctBreak self);

typedef struct icu4x_IndicConjunctBreak_short_name_mv1_result {union {DiplomatStringView ok; }; bool is_ok;} icu4x_IndicConjunctBreak_short_name_mv1_result;
icu4x_IndicConjunctBreak_short_name_mv1_result icu4x_IndicConjunctBreak_short_name_mv1(IndicConjunctBreak self);

uint8_t icu4x_IndicConjunctBreak_to_integer_value_mv1(IndicConjunctBreak self);

typedef struct icu4x_IndicConjunctBreak_from_integer_value_mv1_result {union {IndicConjunctBreak ok; }; bool is_ok;} icu4x_IndicConjunctBreak_from_integer_value_mv1_result;
icu4x_IndicConjunctBreak_from_integer_value_mv1_result icu4x_IndicConjunctBreak_from_integer_value_mv1(uint8_t other);

typedef struct icu4x_IndicConjunctBreak_try_from_str_mv1_result {union {IndicConjunctBreak ok; }; bool is_ok;} icu4x_IndicConjunctBreak_try_from_str_mv1_result;
icu4x_IndicConjunctBreak_try_from_str_mv1_result icu4x_IndicConjunctBreak_try_from_str_mv1(DiplomatStringView s);





#endif // IndicConjunctBreak_H

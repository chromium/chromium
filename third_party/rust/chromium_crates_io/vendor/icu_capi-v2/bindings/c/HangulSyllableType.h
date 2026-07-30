#ifndef HangulSyllableType_H
#define HangulSyllableType_H

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"


#include "HangulSyllableType.d.h"






HangulSyllableType icu4x_HangulSyllableType_for_char_mv1(char32_t ch);

typedef struct icu4x_HangulSyllableType_long_name_mv1_result {union {DiplomatStringView ok; }; bool is_ok;} icu4x_HangulSyllableType_long_name_mv1_result;
icu4x_HangulSyllableType_long_name_mv1_result icu4x_HangulSyllableType_long_name_mv1(HangulSyllableType self);

typedef struct icu4x_HangulSyllableType_short_name_mv1_result {union {DiplomatStringView ok; }; bool is_ok;} icu4x_HangulSyllableType_short_name_mv1_result;
icu4x_HangulSyllableType_short_name_mv1_result icu4x_HangulSyllableType_short_name_mv1(HangulSyllableType self);

uint8_t icu4x_HangulSyllableType_to_integer_value_mv1(HangulSyllableType self);

typedef struct icu4x_HangulSyllableType_from_integer_value_mv1_result {union {HangulSyllableType ok; }; bool is_ok;} icu4x_HangulSyllableType_from_integer_value_mv1_result;
icu4x_HangulSyllableType_from_integer_value_mv1_result icu4x_HangulSyllableType_from_integer_value_mv1(uint8_t other);

typedef struct icu4x_HangulSyllableType_try_from_str_mv1_result {union {HangulSyllableType ok; }; bool is_ok;} icu4x_HangulSyllableType_try_from_str_mv1_result;
icu4x_HangulSyllableType_try_from_str_mv1_result icu4x_HangulSyllableType_try_from_str_mv1(DiplomatStringView s);





#endif // HangulSyllableType_H

#ifndef CanonicalCombiningClass_H
#define CanonicalCombiningClass_H

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"


#include "CanonicalCombiningClass.d.h"






CanonicalCombiningClass icu4x_CanonicalCombiningClass_for_char_mv1(char32_t ch);

typedef struct icu4x_CanonicalCombiningClass_long_name_mv1_result {union {DiplomatStringView ok; }; bool is_ok;} icu4x_CanonicalCombiningClass_long_name_mv1_result;
icu4x_CanonicalCombiningClass_long_name_mv1_result icu4x_CanonicalCombiningClass_long_name_mv1(CanonicalCombiningClass self);

typedef struct icu4x_CanonicalCombiningClass_short_name_mv1_result {union {DiplomatStringView ok; }; bool is_ok;} icu4x_CanonicalCombiningClass_short_name_mv1_result;
icu4x_CanonicalCombiningClass_short_name_mv1_result icu4x_CanonicalCombiningClass_short_name_mv1(CanonicalCombiningClass self);

uint8_t icu4x_CanonicalCombiningClass_to_integer_value_mv1(CanonicalCombiningClass self);

typedef struct icu4x_CanonicalCombiningClass_from_integer_value_mv1_result {union {CanonicalCombiningClass ok; }; bool is_ok;} icu4x_CanonicalCombiningClass_from_integer_value_mv1_result;
icu4x_CanonicalCombiningClass_from_integer_value_mv1_result icu4x_CanonicalCombiningClass_from_integer_value_mv1(uint8_t other);

typedef struct icu4x_CanonicalCombiningClass_try_from_str_mv1_result {union {CanonicalCombiningClass ok; }; bool is_ok;} icu4x_CanonicalCombiningClass_try_from_str_mv1_result;
icu4x_CanonicalCombiningClass_try_from_str_mv1_result icu4x_CanonicalCombiningClass_try_from_str_mv1(DiplomatStringView s);





#endif // CanonicalCombiningClass_H

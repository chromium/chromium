#ifndef JoiningGroup_H
#define JoiningGroup_H

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"


#include "JoiningGroup.d.h"






JoiningGroup icu4x_JoiningGroup_for_char_mv1(char32_t ch);

typedef struct icu4x_JoiningGroup_long_name_mv1_result {union {DiplomatStringView ok; }; bool is_ok;} icu4x_JoiningGroup_long_name_mv1_result;
icu4x_JoiningGroup_long_name_mv1_result icu4x_JoiningGroup_long_name_mv1(JoiningGroup self);

typedef struct icu4x_JoiningGroup_short_name_mv1_result {union {DiplomatStringView ok; }; bool is_ok;} icu4x_JoiningGroup_short_name_mv1_result;
icu4x_JoiningGroup_short_name_mv1_result icu4x_JoiningGroup_short_name_mv1(JoiningGroup self);

uint8_t icu4x_JoiningGroup_to_integer_value_mv1(JoiningGroup self);

typedef struct icu4x_JoiningGroup_from_integer_value_mv1_result {union {JoiningGroup ok; }; bool is_ok;} icu4x_JoiningGroup_from_integer_value_mv1_result;
icu4x_JoiningGroup_from_integer_value_mv1_result icu4x_JoiningGroup_from_integer_value_mv1(uint8_t other);

typedef struct icu4x_JoiningGroup_try_from_str_mv1_result {union {JoiningGroup ok; }; bool is_ok;} icu4x_JoiningGroup_try_from_str_mv1_result;
icu4x_JoiningGroup_try_from_str_mv1_result icu4x_JoiningGroup_try_from_str_mv1(DiplomatStringView s);





#endif // JoiningGroup_H

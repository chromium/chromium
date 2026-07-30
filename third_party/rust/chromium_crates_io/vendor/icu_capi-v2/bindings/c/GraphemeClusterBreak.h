#ifndef GraphemeClusterBreak_H
#define GraphemeClusterBreak_H

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"


#include "GraphemeClusterBreak.d.h"






GraphemeClusterBreak icu4x_GraphemeClusterBreak_for_char_mv1(char32_t ch);

typedef struct icu4x_GraphemeClusterBreak_long_name_mv1_result {union {DiplomatStringView ok; }; bool is_ok;} icu4x_GraphemeClusterBreak_long_name_mv1_result;
icu4x_GraphemeClusterBreak_long_name_mv1_result icu4x_GraphemeClusterBreak_long_name_mv1(GraphemeClusterBreak self);

typedef struct icu4x_GraphemeClusterBreak_short_name_mv1_result {union {DiplomatStringView ok; }; bool is_ok;} icu4x_GraphemeClusterBreak_short_name_mv1_result;
icu4x_GraphemeClusterBreak_short_name_mv1_result icu4x_GraphemeClusterBreak_short_name_mv1(GraphemeClusterBreak self);

uint8_t icu4x_GraphemeClusterBreak_to_integer_value_mv1(GraphemeClusterBreak self);

typedef struct icu4x_GraphemeClusterBreak_from_integer_value_mv1_result {union {GraphemeClusterBreak ok; }; bool is_ok;} icu4x_GraphemeClusterBreak_from_integer_value_mv1_result;
icu4x_GraphemeClusterBreak_from_integer_value_mv1_result icu4x_GraphemeClusterBreak_from_integer_value_mv1(uint8_t other);

typedef struct icu4x_GraphemeClusterBreak_try_from_str_mv1_result {union {GraphemeClusterBreak ok; }; bool is_ok;} icu4x_GraphemeClusterBreak_try_from_str_mv1_result;
icu4x_GraphemeClusterBreak_try_from_str_mv1_result icu4x_GraphemeClusterBreak_try_from_str_mv1(DiplomatStringView s);





#endif // GraphemeClusterBreak_H

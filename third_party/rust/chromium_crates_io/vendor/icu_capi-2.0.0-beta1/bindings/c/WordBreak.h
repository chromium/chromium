#ifndef WordBreak_H
#define WordBreak_H

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"


#include "WordBreak.d.h"






uint8_t icu4x_WordBreak_to_integer_mv1(WordBreak self);

typedef struct icu4x_WordBreak_from_integer_mv1_result {union {WordBreak ok; }; bool is_ok;} icu4x_WordBreak_from_integer_mv1_result;
icu4x_WordBreak_from_integer_mv1_result icu4x_WordBreak_from_integer_mv1(uint8_t other);






#endif // WordBreak_H

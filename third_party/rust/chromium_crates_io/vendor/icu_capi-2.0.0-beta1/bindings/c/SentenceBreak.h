#ifndef SentenceBreak_H
#define SentenceBreak_H

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"


#include "SentenceBreak.d.h"






uint8_t icu4x_SentenceBreak_to_integer_mv1(SentenceBreak self);

typedef struct icu4x_SentenceBreak_from_integer_mv1_result {union {SentenceBreak ok; }; bool is_ok;} icu4x_SentenceBreak_from_integer_mv1_result;
icu4x_SentenceBreak_from_integer_mv1_result icu4x_SentenceBreak_from_integer_mv1(uint8_t other);






#endif // SentenceBreak_H

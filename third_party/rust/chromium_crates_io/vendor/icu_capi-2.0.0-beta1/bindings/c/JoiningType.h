#ifndef JoiningType_H
#define JoiningType_H

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"


#include "JoiningType.d.h"






uint8_t icu4x_JoiningType_to_integer_mv1(JoiningType self);

typedef struct icu4x_JoiningType_from_integer_mv1_result {union {JoiningType ok; }; bool is_ok;} icu4x_JoiningType_from_integer_mv1_result;
icu4x_JoiningType_from_integer_mv1_result icu4x_JoiningType_from_integer_mv1(uint8_t other);






#endif // JoiningType_H

#ifndef Script_H
#define Script_H

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"


#include "Script.d.h"






uint16_t icu4x_Script_to_integer_mv1(Script self);

typedef struct icu4x_Script_from_integer_mv1_result {union {Script ok; }; bool is_ok;} icu4x_Script_from_integer_mv1_result;
icu4x_Script_from_integer_mv1_result icu4x_Script_from_integer_mv1(uint16_t other);






#endif // Script_H

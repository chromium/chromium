#ifndef TimePrecision_H
#define TimePrecision_H

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"


#include "TimePrecision.d.h"






typedef struct icu4x_TimePrecision_from_subsecond_digits_mv1_result {union {TimePrecision ok; }; bool is_ok;} icu4x_TimePrecision_from_subsecond_digits_mv1_result;
icu4x_TimePrecision_from_subsecond_digits_mv1_result icu4x_TimePrecision_from_subsecond_digits_mv1(uint8_t digits);





#endif // TimePrecision_H

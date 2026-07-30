#ifndef WeekdaySetIterator_H
#define WeekdaySetIterator_H

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"

#include "Weekday.d.h"

#include "WeekdaySetIterator.d.h"






typedef struct icu4x_WeekdaySetIterator_next_mv1_result {union {Weekday ok; }; bool is_ok;} icu4x_WeekdaySetIterator_next_mv1_result;
icu4x_WeekdaySetIterator_next_mv1_result icu4x_WeekdaySetIterator_next_mv1(WeekdaySetIterator* self);

void icu4x_WeekdaySetIterator_destroy_mv1(WeekdaySetIterator* self);





#endif // WeekdaySetIterator_H

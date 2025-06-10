#ifndef TimeDuration_H
#define TimeDuration_H

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"

#include "Sign.d.h"
#include "TemporalError.d.h"

#include "TimeDuration.d.h"






typedef struct temporal_rs_TimeDuration_try_new_result {union {TimeDuration* ok; TemporalError err;}; bool is_ok;} temporal_rs_TimeDuration_try_new_result;
temporal_rs_TimeDuration_try_new_result temporal_rs_TimeDuration_try_new(int64_t hours, int64_t minutes, int64_t seconds, int64_t milliseconds, double microseconds, double nanoseconds);

TimeDuration* temporal_rs_TimeDuration_abs(const TimeDuration* self);

TimeDuration* temporal_rs_TimeDuration_negated(const TimeDuration* self);

bool temporal_rs_TimeDuration_is_within_range(const TimeDuration* self);

Sign temporal_rs_TimeDuration_sign(const TimeDuration* self);

void temporal_rs_TimeDuration_destroy(TimeDuration* self);





#endif // TimeDuration_H

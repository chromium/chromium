#ifndef IsoDateTime_H
#define IsoDateTime_H

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"

#include "Rfc9557ParseError.d.h"

#include "IsoDateTime.d.h"






typedef struct icu4x_IsoDateTime_from_string_mv1_result {union {IsoDateTime ok; Rfc9557ParseError err;}; bool is_ok;} icu4x_IsoDateTime_from_string_mv1_result;
icu4x_IsoDateTime_from_string_mv1_result icu4x_IsoDateTime_from_string_mv1(DiplomatStringView v);





#endif // IsoDateTime_H

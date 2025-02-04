#ifndef GeneralCategoryNameToMaskMapper_H
#define GeneralCategoryNameToMaskMapper_H

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"

#include "DataError.d.h"
#include "DataProvider.d.h"

#include "GeneralCategoryNameToMaskMapper.d.h"






uint32_t icu4x_GeneralCategoryNameToMaskMapper_get_strict_mv1(const GeneralCategoryNameToMaskMapper* self, DiplomatStringView name);

uint32_t icu4x_GeneralCategoryNameToMaskMapper_get_loose_mv1(const GeneralCategoryNameToMaskMapper* self, DiplomatStringView name);

typedef struct icu4x_GeneralCategoryNameToMaskMapper_load_mv1_result {union {GeneralCategoryNameToMaskMapper* ok; DataError err;}; bool is_ok;} icu4x_GeneralCategoryNameToMaskMapper_load_mv1_result;
icu4x_GeneralCategoryNameToMaskMapper_load_mv1_result icu4x_GeneralCategoryNameToMaskMapper_load_mv1(const DataProvider* provider);


void icu4x_GeneralCategoryNameToMaskMapper_destroy_mv1(GeneralCategoryNameToMaskMapper* self);





#endif // GeneralCategoryNameToMaskMapper_H

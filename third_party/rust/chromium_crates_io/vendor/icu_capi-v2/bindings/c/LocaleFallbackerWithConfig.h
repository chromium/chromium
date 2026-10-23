#ifndef LocaleFallbackerWithConfig_H
#define LocaleFallbackerWithConfig_H

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"

#include "Locale.d.h"
#include "LocaleFallbackConfig.d.h"
#include "LocaleFallbackIterator.d.h"

#include "LocaleFallbackerWithConfig.d.h"






LocaleFallbackConfig icu4x_LocaleFallbackerWithConfig_config_mv1(const LocaleFallbackerWithConfig* self);

LocaleFallbackIterator* icu4x_LocaleFallbackerWithConfig_fallback_for_locale_mv1(const LocaleFallbackerWithConfig* self, const Locale* locale);

void icu4x_LocaleFallbackerWithConfig_destroy_mv1(LocaleFallbackerWithConfig* self);





#endif // LocaleFallbackerWithConfig_H

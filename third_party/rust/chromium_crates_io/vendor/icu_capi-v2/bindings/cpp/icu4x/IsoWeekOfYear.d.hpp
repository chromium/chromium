#ifndef ICU4X_IsoWeekOfYear_D_HPP
#define ICU4X_IsoWeekOfYear_D_HPP

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include <cstdlib>
#include "diplomat_runtime.hpp"


namespace icu4x {
namespace capi {
    struct IsoWeekOfYear {
      uint8_t week_number;
      int32_t iso_year;
    };

    typedef struct IsoWeekOfYear_option {union { IsoWeekOfYear ok; }; bool is_ok; } IsoWeekOfYear_option;
} // namespace capi
} // namespace


namespace icu4x {
/**
 * See the [Rust documentation for `IsoWeekOfYear`](https://docs.rs/icu/2.2.0/icu/calendar/types/struct.IsoWeekOfYear.html) for more information.
 */
struct IsoWeekOfYear {
    uint8_t week_number;
    int32_t iso_year;

    inline icu4x::capi::IsoWeekOfYear AsFFI() const;
    inline static icu4x::IsoWeekOfYear FromFFI(icu4x::capi::IsoWeekOfYear c_struct);
};

} // namespace
#endif // ICU4X_IsoWeekOfYear_D_HPP

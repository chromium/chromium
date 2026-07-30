#ifndef ICU4X_DateDifferenceOptions_D_HPP
#define ICU4X_DateDifferenceOptions_D_HPP

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include <cstdlib>
#include "DateDurationUnit.d.hpp"
#include "diplomat_runtime.hpp"
namespace icu4x {
class DateDurationUnit;
} // namespace icu4x



namespace icu4x {
namespace capi {
    struct DateDifferenceOptions {
      icu4x::capi::DateDurationUnit_option largest_unit;
    };

    typedef struct DateDifferenceOptions_option {union { DateDifferenceOptions ok; }; bool is_ok; } DateDifferenceOptions_option;
} // namespace capi
} // namespace


namespace icu4x {
/**
 * See the [Rust documentation for `DateDifferenceOptions`](https://docs.rs/icu/2.2.0/icu/calendar/options/struct.DateDifferenceOptions.html) for more information.
 */
struct DateDifferenceOptions {
    std::optional<icu4x::DateDurationUnit> largest_unit;

    inline icu4x::capi::DateDifferenceOptions AsFFI() const;
    inline static icu4x::DateDifferenceOptions FromFFI(icu4x::capi::DateDifferenceOptions c_struct);
};

} // namespace
#endif // ICU4X_DateDifferenceOptions_D_HPP

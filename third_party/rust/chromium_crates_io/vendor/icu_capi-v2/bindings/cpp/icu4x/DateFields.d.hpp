#ifndef ICU4X_DateFields_D_HPP
#define ICU4X_DateFields_D_HPP

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
    struct DateFields {
      icu4x::diplomat::capi::OptionStringView era;
      icu4x::diplomat::capi::OptionI32 era_year;
      icu4x::diplomat::capi::OptionI32 extended_year;
      icu4x::diplomat::capi::OptionStringView month_code;
      icu4x::diplomat::capi::OptionU8 ordinal_month;
      icu4x::diplomat::capi::OptionU8 day;
    };

    typedef struct DateFields_option {union { DateFields ok; }; bool is_ok; } DateFields_option;
} // namespace capi
} // namespace


namespace icu4x {
/**
 * See the [Rust documentation for `DateFields`](https://docs.rs/icu/2.2.0/icu/calendar/types/struct.DateFields.html) for more information.
 */
struct DateFields {
    std::optional<std::string_view> era;
    std::optional<int32_t> era_year;
    std::optional<int32_t> extended_year;
    std::optional<std::string_view> month_code;
    std::optional<uint8_t> ordinal_month;
    std::optional<uint8_t> day;

    inline icu4x::capi::DateFields AsFFI() const;
    inline static icu4x::DateFields FromFFI(icu4x::capi::DateFields c_struct);
};

} // namespace
#endif // ICU4X_DateFields_D_HPP

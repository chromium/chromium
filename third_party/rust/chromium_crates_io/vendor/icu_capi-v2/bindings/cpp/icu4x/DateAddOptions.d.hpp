#ifndef ICU4X_DateAddOptions_D_HPP
#define ICU4X_DateAddOptions_D_HPP

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include <cstdlib>
#include "DateOverflow.d.hpp"
#include "diplomat_runtime.hpp"
namespace icu4x {
class DateOverflow;
} // namespace icu4x



namespace icu4x {
namespace capi {
    struct DateAddOptions {
      icu4x::capi::DateOverflow_option overflow;
    };

    typedef struct DateAddOptions_option {union { DateAddOptions ok; }; bool is_ok; } DateAddOptions_option;
} // namespace capi
} // namespace


namespace icu4x {
/**
 * See the [Rust documentation for `DateAddOptions`](https://docs.rs/icu/2.2.0/icu/calendar/options/struct.DateAddOptions.html) for more information.
 */
struct DateAddOptions {
    std::optional<icu4x::DateOverflow> overflow;

    inline icu4x::capi::DateAddOptions AsFFI() const;
    inline static icu4x::DateAddOptions FromFFI(icu4x::capi::DateAddOptions c_struct);
};

} // namespace
#endif // ICU4X_DateAddOptions_D_HPP

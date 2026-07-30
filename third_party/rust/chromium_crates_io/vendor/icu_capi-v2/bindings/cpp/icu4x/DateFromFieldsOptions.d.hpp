#ifndef ICU4X_DateFromFieldsOptions_D_HPP
#define ICU4X_DateFromFieldsOptions_D_HPP

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include <cstdlib>
#include "DateMissingFieldsStrategy.d.hpp"
#include "DateOverflow.d.hpp"
#include "diplomat_runtime.hpp"
namespace icu4x {
class DateMissingFieldsStrategy;
class DateOverflow;
} // namespace icu4x



namespace icu4x {
namespace capi {
    struct DateFromFieldsOptions {
      icu4x::capi::DateOverflow_option overflow;
      icu4x::capi::DateMissingFieldsStrategy_option missing_fields_strategy;
    };

    typedef struct DateFromFieldsOptions_option {union { DateFromFieldsOptions ok; }; bool is_ok; } DateFromFieldsOptions_option;
} // namespace capi
} // namespace


namespace icu4x {
/**
 * See the [Rust documentation for `DateFromFieldsOptions`](https://docs.rs/icu/2.2.0/icu/calendar/options/struct.DateFromFieldsOptions.html) for more information.
 */
struct DateFromFieldsOptions {
    std::optional<icu4x::DateOverflow> overflow;
    std::optional<icu4x::DateMissingFieldsStrategy> missing_fields_strategy;

    inline icu4x::capi::DateFromFieldsOptions AsFFI() const;
    inline static icu4x::DateFromFieldsOptions FromFFI(icu4x::capi::DateFromFieldsOptions c_struct);
};

} // namespace
#endif // ICU4X_DateFromFieldsOptions_D_HPP

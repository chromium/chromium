#ifndef ICU4X_DateFromFieldsOptions_HPP
#define ICU4X_DateFromFieldsOptions_HPP

#include "DateFromFieldsOptions.d.hpp"

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include <cstdlib>
#include "DateMissingFieldsStrategy.hpp"
#include "DateOverflow.hpp"
#include "diplomat_runtime.hpp"


namespace icu4x {
namespace capi {

} // namespace capi
} // namespace


inline icu4x::capi::DateFromFieldsOptions icu4x::DateFromFieldsOptions::AsFFI() const {
    return icu4x::capi::DateFromFieldsOptions {
        /* .overflow = */ overflow.has_value() ? (icu4x::capi::DateOverflow_option{ { overflow.value().AsFFI() }, true }) : (icu4x::capi::DateOverflow_option{ {}, false }),
        /* .missing_fields_strategy = */ missing_fields_strategy.has_value() ? (icu4x::capi::DateMissingFieldsStrategy_option{ { missing_fields_strategy.value().AsFFI() }, true }) : (icu4x::capi::DateMissingFieldsStrategy_option{ {}, false }),
    };
}

inline icu4x::DateFromFieldsOptions icu4x::DateFromFieldsOptions::FromFFI(icu4x::capi::DateFromFieldsOptions c_struct) {
    return icu4x::DateFromFieldsOptions {
        /* .overflow = */ c_struct.overflow.is_ok ? std::optional(icu4x::DateOverflow::FromFFI(c_struct.overflow.ok)) : std::nullopt,
        /* .missing_fields_strategy = */ c_struct.missing_fields_strategy.is_ok ? std::optional(icu4x::DateMissingFieldsStrategy::FromFFI(c_struct.missing_fields_strategy.ok)) : std::nullopt,
    };
}


#endif // ICU4X_DateFromFieldsOptions_HPP

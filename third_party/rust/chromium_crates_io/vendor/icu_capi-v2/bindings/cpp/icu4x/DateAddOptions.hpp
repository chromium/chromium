#ifndef ICU4X_DateAddOptions_HPP
#define ICU4X_DateAddOptions_HPP

#include "DateAddOptions.d.hpp"

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include <cstdlib>
#include "DateOverflow.hpp"
#include "diplomat_runtime.hpp"


namespace icu4x {
namespace capi {

} // namespace capi
} // namespace


inline icu4x::capi::DateAddOptions icu4x::DateAddOptions::AsFFI() const {
    return icu4x::capi::DateAddOptions {
        /* .overflow = */ overflow.has_value() ? (icu4x::capi::DateOverflow_option{ { overflow.value().AsFFI() }, true }) : (icu4x::capi::DateOverflow_option{ {}, false }),
    };
}

inline icu4x::DateAddOptions icu4x::DateAddOptions::FromFFI(icu4x::capi::DateAddOptions c_struct) {
    return icu4x::DateAddOptions {
        /* .overflow = */ c_struct.overflow.is_ok ? std::optional(icu4x::DateOverflow::FromFFI(c_struct.overflow.ok)) : std::nullopt,
    };
}


#endif // ICU4X_DateAddOptions_HPP

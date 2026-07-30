#ifndef ICU4X_DateOverflow_HPP
#define ICU4X_DateOverflow_HPP

#include "DateOverflow.d.hpp"

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

} // namespace capi
} // namespace

inline icu4x::capi::DateOverflow icu4x::DateOverflow::AsFFI() const {
    return static_cast<icu4x::capi::DateOverflow>(value);
}

inline icu4x::DateOverflow icu4x::DateOverflow::FromFFI(icu4x::capi::DateOverflow c_enum) {
    switch (c_enum) {
        case icu4x::capi::DateOverflow_Constrain:
        case icu4x::capi::DateOverflow_Reject:
            return static_cast<icu4x::DateOverflow::Value>(c_enum);
        default:
            std::abort();
    }
}
#endif // ICU4X_DateOverflow_HPP

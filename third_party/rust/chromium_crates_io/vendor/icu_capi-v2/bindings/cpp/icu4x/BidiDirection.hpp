#ifndef ICU4X_BidiDirection_HPP
#define ICU4X_BidiDirection_HPP

#include "BidiDirection.d.hpp"

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

inline icu4x::capi::BidiDirection icu4x::BidiDirection::AsFFI() const {
    return static_cast<icu4x::capi::BidiDirection>(value);
}

inline icu4x::BidiDirection icu4x::BidiDirection::FromFFI(icu4x::capi::BidiDirection c_enum) {
    switch (c_enum) {
        case icu4x::capi::BidiDirection_Ltr:
        case icu4x::capi::BidiDirection_Rtl:
        case icu4x::capi::BidiDirection_Mixed:
            return static_cast<icu4x::BidiDirection::Value>(c_enum);
        default:
            std::abort();
    }
}
#endif // ICU4X_BidiDirection_HPP

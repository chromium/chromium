#ifndef ICU4X_DecimalSign_HPP
#define ICU4X_DecimalSign_HPP

#include "DecimalSign.d.hpp"

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

inline icu4x::capi::DecimalSign icu4x::DecimalSign::AsFFI() const {
    return static_cast<icu4x::capi::DecimalSign>(value);
}

inline icu4x::DecimalSign icu4x::DecimalSign::FromFFI(icu4x::capi::DecimalSign c_enum) {
    switch (c_enum) {
        case icu4x::capi::DecimalSign_None:
        case icu4x::capi::DecimalSign_Negative:
        case icu4x::capi::DecimalSign_Positive:
            return static_cast<icu4x::DecimalSign::Value>(c_enum);
        default:
            std::abort();
    }
}
#endif // ICU4X_DecimalSign_HPP

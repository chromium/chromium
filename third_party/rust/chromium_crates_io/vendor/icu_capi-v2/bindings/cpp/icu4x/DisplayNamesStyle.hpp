#ifndef ICU4X_DisplayNamesStyle_HPP
#define ICU4X_DisplayNamesStyle_HPP

#include "DisplayNamesStyle.d.hpp"

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

inline icu4x::capi::DisplayNamesStyle icu4x::DisplayNamesStyle::AsFFI() const {
    return static_cast<icu4x::capi::DisplayNamesStyle>(value);
}

inline icu4x::DisplayNamesStyle icu4x::DisplayNamesStyle::FromFFI(icu4x::capi::DisplayNamesStyle c_enum) {
    switch (c_enum) {
        case icu4x::capi::DisplayNamesStyle_Narrow:
        case icu4x::capi::DisplayNamesStyle_Short:
        case icu4x::capi::DisplayNamesStyle_Long:
        case icu4x::capi::DisplayNamesStyle_Menu:
            return static_cast<icu4x::DisplayNamesStyle::Value>(c_enum);
        default:
            std::abort();
    }
}
#endif // ICU4X_DisplayNamesStyle_HPP

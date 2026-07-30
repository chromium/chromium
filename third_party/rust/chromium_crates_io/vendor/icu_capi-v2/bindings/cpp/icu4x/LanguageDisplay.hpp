#ifndef ICU4X_LanguageDisplay_HPP
#define ICU4X_LanguageDisplay_HPP

#include "LanguageDisplay.d.hpp"

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

inline icu4x::capi::LanguageDisplay icu4x::LanguageDisplay::AsFFI() const {
    return static_cast<icu4x::capi::LanguageDisplay>(value);
}

inline icu4x::LanguageDisplay icu4x::LanguageDisplay::FromFFI(icu4x::capi::LanguageDisplay c_enum) {
    switch (c_enum) {
        case icu4x::capi::LanguageDisplay_Dialect:
        case icu4x::capi::LanguageDisplay_Standard:
            return static_cast<icu4x::LanguageDisplay::Value>(c_enum);
        default:
            std::abort();
    }
}
#endif // ICU4X_LanguageDisplay_HPP

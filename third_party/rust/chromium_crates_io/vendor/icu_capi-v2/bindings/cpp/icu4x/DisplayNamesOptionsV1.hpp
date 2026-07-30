#ifndef ICU4X_DisplayNamesOptionsV1_HPP
#define ICU4X_DisplayNamesOptionsV1_HPP

#include "DisplayNamesOptionsV1.d.hpp"

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include <cstdlib>
#include "DisplayNamesFallback.hpp"
#include "DisplayNamesStyle.hpp"
#include "LanguageDisplay.hpp"
#include "diplomat_runtime.hpp"


namespace icu4x {
namespace capi {

} // namespace capi
} // namespace


inline icu4x::capi::DisplayNamesOptionsV1 icu4x::DisplayNamesOptionsV1::AsFFI() const {
    return icu4x::capi::DisplayNamesOptionsV1 {
        /* .style = */ style.has_value() ? (icu4x::capi::DisplayNamesStyle_option{ { style.value().AsFFI() }, true }) : (icu4x::capi::DisplayNamesStyle_option{ {}, false }),
        /* .fallback = */ fallback.has_value() ? (icu4x::capi::DisplayNamesFallback_option{ { fallback.value().AsFFI() }, true }) : (icu4x::capi::DisplayNamesFallback_option{ {}, false }),
        /* .language_display = */ language_display.has_value() ? (icu4x::capi::LanguageDisplay_option{ { language_display.value().AsFFI() }, true }) : (icu4x::capi::LanguageDisplay_option{ {}, false }),
    };
}

inline icu4x::DisplayNamesOptionsV1 icu4x::DisplayNamesOptionsV1::FromFFI(icu4x::capi::DisplayNamesOptionsV1 c_struct) {
    return icu4x::DisplayNamesOptionsV1 {
        /* .style = */ c_struct.style.is_ok ? std::optional(icu4x::DisplayNamesStyle::FromFFI(c_struct.style.ok)) : std::nullopt,
        /* .fallback = */ c_struct.fallback.is_ok ? std::optional(icu4x::DisplayNamesFallback::FromFFI(c_struct.fallback.ok)) : std::nullopt,
        /* .language_display = */ c_struct.language_display.is_ok ? std::optional(icu4x::LanguageDisplay::FromFFI(c_struct.language_display.ok)) : std::nullopt,
    };
}


#endif // ICU4X_DisplayNamesOptionsV1_HPP

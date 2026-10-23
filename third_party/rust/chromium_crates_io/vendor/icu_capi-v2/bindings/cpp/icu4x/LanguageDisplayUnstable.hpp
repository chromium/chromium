#ifndef ICU4X_LanguageDisplayUnstable_HPP
#define ICU4X_LanguageDisplayUnstable_HPP

#include "LanguageDisplayUnstable.d.hpp"

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

inline icu4x::capi::LanguageDisplayUnstable icu4x::LanguageDisplayUnstable::AsFFI() const {
    return static_cast<icu4x::capi::LanguageDisplayUnstable>(value);
}

inline icu4x::LanguageDisplayUnstable icu4x::LanguageDisplayUnstable::FromFFI(icu4x::capi::LanguageDisplayUnstable c_enum) {
    switch (c_enum) {
        case icu4x::capi::LanguageDisplayUnstable_Dialect:
        case icu4x::capi::LanguageDisplayUnstable_Standard:
            return static_cast<icu4x::LanguageDisplayUnstable::Value>(c_enum);
        default:
            std::abort();
    }
}
#endif // ICU4X_LanguageDisplayUnstable_HPP

#ifndef ICU4X_DateTimeAlignment_HPP
#define ICU4X_DateTimeAlignment_HPP

#include "DateTimeAlignment.d.hpp"

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

inline icu4x::capi::DateTimeAlignment icu4x::DateTimeAlignment::AsFFI() const {
    return static_cast<icu4x::capi::DateTimeAlignment>(value);
}

inline icu4x::DateTimeAlignment icu4x::DateTimeAlignment::FromFFI(icu4x::capi::DateTimeAlignment c_enum) {
    switch (c_enum) {
        case icu4x::capi::DateTimeAlignment_Auto:
        case icu4x::capi::DateTimeAlignment_Column:
            return static_cast<icu4x::DateTimeAlignment::Value>(c_enum);
        default:
            std::abort();
    }
}
#endif // ICU4X_DateTimeAlignment_HPP

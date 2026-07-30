#ifndef ICU4X_DateTimeLength_HPP
#define ICU4X_DateTimeLength_HPP

#include "DateTimeLength.d.hpp"

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

inline icu4x::capi::DateTimeLength icu4x::DateTimeLength::AsFFI() const {
    return static_cast<icu4x::capi::DateTimeLength>(value);
}

inline icu4x::DateTimeLength icu4x::DateTimeLength::FromFFI(icu4x::capi::DateTimeLength c_enum) {
    switch (c_enum) {
        case icu4x::capi::DateTimeLength_Long:
        case icu4x::capi::DateTimeLength_Medium:
        case icu4x::capi::DateTimeLength_Short:
            return static_cast<icu4x::DateTimeLength::Value>(c_enum);
        default:
            std::abort();
    }
}
#endif // ICU4X_DateTimeLength_HPP

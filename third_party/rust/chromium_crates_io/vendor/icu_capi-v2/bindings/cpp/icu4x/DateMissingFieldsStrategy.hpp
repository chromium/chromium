#ifndef ICU4X_DateMissingFieldsStrategy_HPP
#define ICU4X_DateMissingFieldsStrategy_HPP

#include "DateMissingFieldsStrategy.d.hpp"

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

inline icu4x::capi::DateMissingFieldsStrategy icu4x::DateMissingFieldsStrategy::AsFFI() const {
    return static_cast<icu4x::capi::DateMissingFieldsStrategy>(value);
}

inline icu4x::DateMissingFieldsStrategy icu4x::DateMissingFieldsStrategy::FromFFI(icu4x::capi::DateMissingFieldsStrategy c_enum) {
    switch (c_enum) {
        case icu4x::capi::DateMissingFieldsStrategy_Reject:
        case icu4x::capi::DateMissingFieldsStrategy_Ecma:
            return static_cast<icu4x::DateMissingFieldsStrategy::Value>(c_enum);
        default:
            std::abort();
    }
}
#endif // ICU4X_DateMissingFieldsStrategy_HPP

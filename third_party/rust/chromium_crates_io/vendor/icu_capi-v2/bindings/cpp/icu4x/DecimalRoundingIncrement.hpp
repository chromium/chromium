#ifndef ICU4X_DecimalRoundingIncrement_HPP
#define ICU4X_DecimalRoundingIncrement_HPP

#include "DecimalRoundingIncrement.d.hpp"

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

inline icu4x::capi::DecimalRoundingIncrement icu4x::DecimalRoundingIncrement::AsFFI() const {
    return static_cast<icu4x::capi::DecimalRoundingIncrement>(value);
}

inline icu4x::DecimalRoundingIncrement icu4x::DecimalRoundingIncrement::FromFFI(icu4x::capi::DecimalRoundingIncrement c_enum) {
    switch (c_enum) {
        case icu4x::capi::DecimalRoundingIncrement_MultiplesOf1:
        case icu4x::capi::DecimalRoundingIncrement_MultiplesOf2:
        case icu4x::capi::DecimalRoundingIncrement_MultiplesOf5:
        case icu4x::capi::DecimalRoundingIncrement_MultiplesOf25:
            return static_cast<icu4x::DecimalRoundingIncrement::Value>(c_enum);
        default:
            std::abort();
    }
}
#endif // ICU4X_DecimalRoundingIncrement_HPP

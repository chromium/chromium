#ifndef ICU4X_LineBreakWordOption_HPP
#define ICU4X_LineBreakWordOption_HPP

#include "LineBreakWordOption.d.hpp"

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

inline icu4x::capi::LineBreakWordOption icu4x::LineBreakWordOption::AsFFI() const {
    return static_cast<icu4x::capi::LineBreakWordOption>(value);
}

inline icu4x::LineBreakWordOption icu4x::LineBreakWordOption::FromFFI(icu4x::capi::LineBreakWordOption c_enum) {
    switch (c_enum) {
        case icu4x::capi::LineBreakWordOption_Normal:
        case icu4x::capi::LineBreakWordOption_BreakAll:
        case icu4x::capi::LineBreakWordOption_KeepAll:
            return static_cast<icu4x::LineBreakWordOption::Value>(c_enum);
        default:
            std::abort();
    }
}
#endif // ICU4X_LineBreakWordOption_HPP

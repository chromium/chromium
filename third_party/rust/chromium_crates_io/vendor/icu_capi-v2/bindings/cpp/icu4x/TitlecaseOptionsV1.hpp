#ifndef ICU4X_TitlecaseOptionsV1_HPP
#define ICU4X_TitlecaseOptionsV1_HPP

#include "TitlecaseOptionsV1.d.hpp"

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include <cstdlib>
#include "LeadingAdjustment.hpp"
#include "TrailingCase.hpp"
#include "diplomat_runtime.hpp"


namespace icu4x {
namespace capi {
    extern "C" {

    icu4x::capi::TitlecaseOptionsV1 icu4x_TitlecaseOptionsV1_default_mv1(void);

    } // extern "C"
} // namespace capi
} // namespace

inline icu4x::TitlecaseOptionsV1 icu4x::TitlecaseOptionsV1::default_options() {
    auto result = icu4x::capi::icu4x_TitlecaseOptionsV1_default_mv1();
    return icu4x::TitlecaseOptionsV1::FromFFI(result);
}


inline icu4x::capi::TitlecaseOptionsV1 icu4x::TitlecaseOptionsV1::AsFFI() const {
    return icu4x::capi::TitlecaseOptionsV1 {
        /* .leading_adjustment = */ leading_adjustment.has_value() ? (icu4x::capi::LeadingAdjustment_option{ { leading_adjustment.value().AsFFI() }, true }) : (icu4x::capi::LeadingAdjustment_option{ {}, false }),
        /* .trailing_case = */ trailing_case.has_value() ? (icu4x::capi::TrailingCase_option{ { trailing_case.value().AsFFI() }, true }) : (icu4x::capi::TrailingCase_option{ {}, false }),
    };
}

inline icu4x::TitlecaseOptionsV1 icu4x::TitlecaseOptionsV1::FromFFI(icu4x::capi::TitlecaseOptionsV1 c_struct) {
    return icu4x::TitlecaseOptionsV1 {
        /* .leading_adjustment = */ c_struct.leading_adjustment.is_ok ? std::optional(icu4x::LeadingAdjustment::FromFFI(c_struct.leading_adjustment.ok)) : std::nullopt,
        /* .trailing_case = */ c_struct.trailing_case.is_ok ? std::optional(icu4x::TrailingCase::FromFFI(c_struct.trailing_case.ok)) : std::nullopt,
    };
}


#endif // ICU4X_TitlecaseOptionsV1_HPP

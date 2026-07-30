#ifndef ICU4X_TitlecaseOptionsV1_D_HPP
#define ICU4X_TitlecaseOptionsV1_D_HPP

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include <cstdlib>
#include "LeadingAdjustment.d.hpp"
#include "TrailingCase.d.hpp"
#include "diplomat_runtime.hpp"
namespace icu4x {
struct TitlecaseOptionsV1;
class LeadingAdjustment;
class TrailingCase;
} // namespace icu4x



namespace icu4x {
namespace capi {
    struct TitlecaseOptionsV1 {
      icu4x::capi::LeadingAdjustment_option leading_adjustment;
      icu4x::capi::TrailingCase_option trailing_case;
    };

    typedef struct TitlecaseOptionsV1_option {union { TitlecaseOptionsV1 ok; }; bool is_ok; } TitlecaseOptionsV1_option;
} // namespace capi
} // namespace


namespace icu4x {
/**
 * See the [Rust documentation for `TitlecaseOptions`](https://docs.rs/icu/2.2.0/icu/casemap/options/struct.TitlecaseOptions.html) for more information.
 */
struct TitlecaseOptionsV1 {
    std::optional<icu4x::LeadingAdjustment> leading_adjustment;
    std::optional<icu4x::TrailingCase> trailing_case;

  /**
   * See the [Rust documentation for `default`](https://docs.rs/icu/2.2.0/icu/casemap/options/struct.TitlecaseOptions.html#method.default) for more information.
   */
  inline static icu4x::TitlecaseOptionsV1 default_options();

    inline icu4x::capi::TitlecaseOptionsV1 AsFFI() const;
    inline static icu4x::TitlecaseOptionsV1 FromFFI(icu4x::capi::TitlecaseOptionsV1 c_struct);
};

} // namespace
#endif // ICU4X_TitlecaseOptionsV1_D_HPP

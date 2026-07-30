#ifndef ICU4X_VariantOffsets_D_HPP
#define ICU4X_VariantOffsets_D_HPP

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
namespace capi { struct UtcOffset; }
class UtcOffset;
} // namespace icu4x



namespace icu4x {
namespace capi {
    struct VariantOffsets {
      icu4x::capi::UtcOffset* standard;
      icu4x::capi::UtcOffset* daylight;
    };

    typedef struct VariantOffsets_option {union { VariantOffsets ok; }; bool is_ok; } VariantOffsets_option;
} // namespace capi
} // namespace


namespace icu4x {
/**
 * See the [Rust documentation for `VariantOffsets`](https://docs.rs/icu/2.2.0/icu/time/zone/struct.VariantOffsets.html) for more information.
 */
struct VariantOffsets {
    std::unique_ptr<icu4x::UtcOffset> standard;
    std::unique_ptr<icu4x::UtcOffset> daylight;

    inline icu4x::capi::VariantOffsets AsFFI() const;
    inline static icu4x::VariantOffsets FromFFI(icu4x::capi::VariantOffsets c_struct);
};

} // namespace
#endif // ICU4X_VariantOffsets_D_HPP

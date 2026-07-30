#ifndef ICU4X_LocaleFallbackPriority_D_HPP
#define ICU4X_LocaleFallbackPriority_D_HPP

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
    enum LocaleFallbackPriority {
      LocaleFallbackPriority_Language = 0,
      LocaleFallbackPriority_Region = 1,
    };

    typedef struct LocaleFallbackPriority_option {union { LocaleFallbackPriority ok; }; bool is_ok; } LocaleFallbackPriority_option;
} // namespace capi
} // namespace

namespace icu4x {
/**
 * Priority mode for the ICU4X fallback algorithm.
 *
 * See the [Rust documentation for `LocaleFallbackPriority`](https://docs.rs/icu/2.2.0/icu/locale/fallback/enum.LocaleFallbackPriority.html) for more information.
 */
class LocaleFallbackPriority {
public:
    enum Value {
        Language = 0,
        Region = 1,
    };

    LocaleFallbackPriority(): value(Value::Language) {}

    // Implicit conversions between enum and ::Value
    constexpr LocaleFallbackPriority(Value v) : value(v) {}
    constexpr operator Value() const { return value; }
    // Prevent usage as boolean value
    explicit operator bool() const = delete;

    inline icu4x::capi::LocaleFallbackPriority AsFFI() const;
    inline static icu4x::LocaleFallbackPriority FromFFI(icu4x::capi::LocaleFallbackPriority c_enum);
private:
    Value value;
};

} // namespace
#endif // ICU4X_LocaleFallbackPriority_D_HPP

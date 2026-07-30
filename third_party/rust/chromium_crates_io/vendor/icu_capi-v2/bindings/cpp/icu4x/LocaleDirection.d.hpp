#ifndef ICU4X_LocaleDirection_D_HPP
#define ICU4X_LocaleDirection_D_HPP

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
    enum LocaleDirection {
      LocaleDirection_LeftToRight = 0,
      LocaleDirection_RightToLeft = 1,
      LocaleDirection_Unknown = 2,
    };

    typedef struct LocaleDirection_option {union { LocaleDirection ok; }; bool is_ok; } LocaleDirection_option;
} // namespace capi
} // namespace

namespace icu4x {
/**
 * See the [Rust documentation for `Direction`](https://docs.rs/icu/2.2.0/icu/locale/enum.Direction.html) for more information.
 */
class LocaleDirection {
public:
    enum Value {
        LeftToRight = 0,
        RightToLeft = 1,
        Unknown = 2,
    };

    LocaleDirection(): value(Value::Unknown) {}

    // Implicit conversions between enum and ::Value
    constexpr LocaleDirection(Value v) : value(v) {}
    constexpr operator Value() const { return value; }
    // Prevent usage as boolean value
    explicit operator bool() const = delete;

    inline icu4x::capi::LocaleDirection AsFFI() const;
    inline static icu4x::LocaleDirection FromFFI(icu4x::capi::LocaleDirection c_enum);
private:
    Value value;
};

} // namespace
#endif // ICU4X_LocaleDirection_D_HPP

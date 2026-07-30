#ifndef ICU4X_CollatorStrength_D_HPP
#define ICU4X_CollatorStrength_D_HPP

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
    enum CollatorStrength {
      CollatorStrength_Primary = 0,
      CollatorStrength_Secondary = 1,
      CollatorStrength_Tertiary = 2,
      CollatorStrength_Quaternary = 3,
      CollatorStrength_Identical = 4,
    };

    typedef struct CollatorStrength_option {union { CollatorStrength ok; }; bool is_ok; } CollatorStrength_option;
} // namespace capi
} // namespace

namespace icu4x {
/**
 * See the [Rust documentation for `Strength`](https://docs.rs/icu/2.2.0/icu/collator/options/enum.Strength.html) for more information.
 */
class CollatorStrength {
public:
    enum Value {
        Primary = 0,
        Secondary = 1,
        Tertiary = 2,
        Quaternary = 3,
        Identical = 4,
    };

    CollatorStrength(): value(Value::Primary) {}

    // Implicit conversions between enum and ::Value
    constexpr CollatorStrength(Value v) : value(v) {}
    constexpr operator Value() const { return value; }
    // Prevent usage as boolean value
    explicit operator bool() const = delete;

    inline icu4x::capi::CollatorStrength AsFFI() const;
    inline static icu4x::CollatorStrength FromFFI(icu4x::capi::CollatorStrength c_enum);
private:
    Value value;
};

} // namespace
#endif // ICU4X_CollatorStrength_D_HPP

#ifndef ICU4X_CollatorAlternateHandling_D_HPP
#define ICU4X_CollatorAlternateHandling_D_HPP

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
    enum CollatorAlternateHandling {
      CollatorAlternateHandling_NonIgnorable = 0,
      CollatorAlternateHandling_Shifted = 1,
    };

    typedef struct CollatorAlternateHandling_option {union { CollatorAlternateHandling ok; }; bool is_ok; } CollatorAlternateHandling_option;
} // namespace capi
} // namespace

namespace icu4x {
/**
 * See the [Rust documentation for `AlternateHandling`](https://docs.rs/icu/2.2.0/icu/collator/options/enum.AlternateHandling.html) for more information.
 */
class CollatorAlternateHandling {
public:
    enum Value {
        NonIgnorable = 0,
        Shifted = 1,
    };

    CollatorAlternateHandling(): value(Value::NonIgnorable) {}

    // Implicit conversions between enum and ::Value
    constexpr CollatorAlternateHandling(Value v) : value(v) {}
    constexpr operator Value() const { return value; }
    // Prevent usage as boolean value
    explicit operator bool() const = delete;

    inline icu4x::capi::CollatorAlternateHandling AsFFI() const;
    inline static icu4x::CollatorAlternateHandling FromFFI(icu4x::capi::CollatorAlternateHandling c_enum);
private:
    Value value;
};

} // namespace
#endif // ICU4X_CollatorAlternateHandling_D_HPP

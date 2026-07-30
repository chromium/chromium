#ifndef ICU4X_CollatorNumericOrdering_D_HPP
#define ICU4X_CollatorNumericOrdering_D_HPP

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
    enum CollatorNumericOrdering {
      CollatorNumericOrdering_Off = 0,
      CollatorNumericOrdering_On = 1,
    };

    typedef struct CollatorNumericOrdering_option {union { CollatorNumericOrdering ok; }; bool is_ok; } CollatorNumericOrdering_option;
} // namespace capi
} // namespace

namespace icu4x {
/**
 * See the [Rust documentation for `CollationNumericOrdering`](https://docs.rs/icu/2.2.0/icu/collator/preferences/enum.CollationNumericOrdering.html) for more information.
 */
class CollatorNumericOrdering {
public:
    enum Value {
        Off = 0,
        On = 1,
    };

    CollatorNumericOrdering(): value(Value::Off) {}

    // Implicit conversions between enum and ::Value
    constexpr CollatorNumericOrdering(Value v) : value(v) {}
    constexpr operator Value() const { return value; }
    // Prevent usage as boolean value
    explicit operator bool() const = delete;

    inline icu4x::capi::CollatorNumericOrdering AsFFI() const;
    inline static icu4x::CollatorNumericOrdering FromFFI(icu4x::capi::CollatorNumericOrdering c_enum);
private:
    Value value;
};

} // namespace
#endif // ICU4X_CollatorNumericOrdering_D_HPP

#ifndef ICU4X_CollatorCaseFirst_D_HPP
#define ICU4X_CollatorCaseFirst_D_HPP

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
    enum CollatorCaseFirst {
      CollatorCaseFirst_Off = 0,
      CollatorCaseFirst_Lower = 1,
      CollatorCaseFirst_Upper = 2,
    };

    typedef struct CollatorCaseFirst_option {union { CollatorCaseFirst ok; }; bool is_ok; } CollatorCaseFirst_option;
} // namespace capi
} // namespace

namespace icu4x {
/**
 * See the [Rust documentation for `CollationCaseFirst`](https://docs.rs/icu/2.2.0/icu/collator/preferences/enum.CollationCaseFirst.html) for more information.
 */
class CollatorCaseFirst {
public:
    enum Value {
        Off = 0,
        Lower = 1,
        Upper = 2,
    };

    CollatorCaseFirst(): value(Value::Off) {}

    // Implicit conversions between enum and ::Value
    constexpr CollatorCaseFirst(Value v) : value(v) {}
    constexpr operator Value() const { return value; }
    // Prevent usage as boolean value
    explicit operator bool() const = delete;

    inline icu4x::capi::CollatorCaseFirst AsFFI() const;
    inline static icu4x::CollatorCaseFirst FromFFI(icu4x::capi::CollatorCaseFirst c_enum);
private:
    Value value;
};

} // namespace
#endif // ICU4X_CollatorCaseFirst_D_HPP

#ifndef ICU4X_DecimalGroupingStrategy_D_HPP
#define ICU4X_DecimalGroupingStrategy_D_HPP

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
    enum DecimalGroupingStrategy {
      DecimalGroupingStrategy_Auto = 0,
      DecimalGroupingStrategy_Never = 1,
      DecimalGroupingStrategy_Always = 2,
      DecimalGroupingStrategy_Min2 = 3,
    };

    typedef struct DecimalGroupingStrategy_option {union { DecimalGroupingStrategy ok; }; bool is_ok; } DecimalGroupingStrategy_option;
} // namespace capi
} // namespace

namespace icu4x {
/**
 * See the [Rust documentation for `GroupingStrategy`](https://docs.rs/icu/2.2.0/icu/decimal/options/enum.GroupingStrategy.html) for more information.
 */
class DecimalGroupingStrategy {
public:
    enum Value {
        Auto = 0,
        Never = 1,
        Always = 2,
        Min2 = 3,
    };

    DecimalGroupingStrategy(): value(Value::Auto) {}

    // Implicit conversions between enum and ::Value
    constexpr DecimalGroupingStrategy(Value v) : value(v) {}
    constexpr operator Value() const { return value; }
    // Prevent usage as boolean value
    explicit operator bool() const = delete;

    inline icu4x::capi::DecimalGroupingStrategy AsFFI() const;
    inline static icu4x::DecimalGroupingStrategy FromFFI(icu4x::capi::DecimalGroupingStrategy c_enum);
private:
    Value value;
};

} // namespace
#endif // ICU4X_DecimalGroupingStrategy_D_HPP

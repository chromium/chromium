#ifndef ICU4X_YearStyle_D_HPP
#define ICU4X_YearStyle_D_HPP

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
    enum YearStyle {
      YearStyle_Auto = 0,
      YearStyle_Full = 1,
      YearStyle_WithEra = 2,
      YearStyle_NoEra = 3,
    };

    typedef struct YearStyle_option {union { YearStyle ok; }; bool is_ok; } YearStyle_option;
} // namespace capi
} // namespace

namespace icu4x {
/**
 * See the [Rust documentation for `YearStyle`](https://docs.rs/icu/2.2.0/icu/datetime/options/enum.YearStyle.html) for more information.
 */
class YearStyle {
public:
    enum Value {
        Auto = 0,
        Full = 1,
        WithEra = 2,
        NoEra = 3,
    };

    YearStyle(): value(Value::Auto) {}

    // Implicit conversions between enum and ::Value
    constexpr YearStyle(Value v) : value(v) {}
    constexpr operator Value() const { return value; }
    // Prevent usage as boolean value
    explicit operator bool() const = delete;

    inline icu4x::capi::YearStyle AsFFI() const;
    inline static icu4x::YearStyle FromFFI(icu4x::capi::YearStyle c_enum);
private:
    Value value;
};

} // namespace
#endif // ICU4X_YearStyle_D_HPP

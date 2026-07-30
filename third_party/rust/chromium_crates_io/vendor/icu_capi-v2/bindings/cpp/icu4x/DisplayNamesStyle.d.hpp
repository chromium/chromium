#ifndef ICU4X_DisplayNamesStyle_D_HPP
#define ICU4X_DisplayNamesStyle_D_HPP

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
    enum DisplayNamesStyle {
      DisplayNamesStyle_Narrow = 0,
      DisplayNamesStyle_Short = 1,
      DisplayNamesStyle_Long = 2,
      DisplayNamesStyle_Menu = 3,
    };

    typedef struct DisplayNamesStyle_option {union { DisplayNamesStyle ok; }; bool is_ok; } DisplayNamesStyle_option;
} // namespace capi
} // namespace

namespace icu4x {
/**
 * 🚧 This API is unstable and may experience breaking changes outside major releases.
 *
 * See the [Rust documentation for `Style`](https://docs.rs/icu/2.2.0/icu/experimental/displaynames/enum.Style.html) for more information.
 */
class DisplayNamesStyle {
public:
    enum Value {
        Narrow = 0,
        Short = 1,
        Long = 2,
        Menu = 3,
    };

    DisplayNamesStyle(): value(Value::Narrow) {}

    // Implicit conversions between enum and ::Value
    constexpr DisplayNamesStyle(Value v) : value(v) {}
    constexpr operator Value() const { return value; }
    // Prevent usage as boolean value
    explicit operator bool() const = delete;

    inline icu4x::capi::DisplayNamesStyle AsFFI() const;
    inline static icu4x::DisplayNamesStyle FromFFI(icu4x::capi::DisplayNamesStyle c_enum);
private:
    Value value;
};

} // namespace
#endif // ICU4X_DisplayNamesStyle_D_HPP

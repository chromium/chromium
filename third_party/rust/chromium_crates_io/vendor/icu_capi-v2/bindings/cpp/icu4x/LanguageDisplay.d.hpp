#ifndef ICU4X_LanguageDisplay_D_HPP
#define ICU4X_LanguageDisplay_D_HPP

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
    enum LanguageDisplay {
      LanguageDisplay_Dialect = 0,
      LanguageDisplay_Standard = 1,
    };

    typedef struct LanguageDisplay_option {union { LanguageDisplay ok; }; bool is_ok; } LanguageDisplay_option;
} // namespace capi
} // namespace

namespace icu4x {
/**
 * 🚧 This API is unstable and may experience breaking changes outside major releases.
 *
 * See the [Rust documentation for `LanguageDisplay`](https://docs.rs/icu/2.2.0/icu/experimental/displaynames/enum.LanguageDisplay.html) for more information.
 */
class LanguageDisplay {
public:
    enum Value {
        Dialect = 0,
        Standard = 1,
    };

    LanguageDisplay(): value(Value::Dialect) {}

    // Implicit conversions between enum and ::Value
    constexpr LanguageDisplay(Value v) : value(v) {}
    constexpr operator Value() const { return value; }
    // Prevent usage as boolean value
    explicit operator bool() const = delete;

    inline icu4x::capi::LanguageDisplay AsFFI() const;
    inline static icu4x::LanguageDisplay FromFFI(icu4x::capi::LanguageDisplay c_enum);
private:
    Value value;
};

} // namespace
#endif // ICU4X_LanguageDisplay_D_HPP

#ifndef ICU4X_LanguageDisplayUnstable_D_HPP
#define ICU4X_LanguageDisplayUnstable_D_HPP

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
    enum LanguageDisplayUnstable {
      LanguageDisplayUnstable_Dialect = 0,
      LanguageDisplayUnstable_Standard = 1,
    };

    typedef struct LanguageDisplayUnstable_option {union { LanguageDisplayUnstable ok; }; bool is_ok; } LanguageDisplayUnstable_option;
} // namespace capi
} // namespace

namespace icu4x {
/**
 * See the [Rust documentation for `LanguageDisplay`](https://docs.rs/icu/2.4.0/icu/locale/names/enum.LanguageDisplay.html) for more information.
 *
 * 🚧 This API is unstable and may experience breaking changes outside major releases.
 */
class LanguageDisplayUnstable {
public:
    enum Value {
        Dialect = 0,
        Standard = 1,
    };

    LanguageDisplayUnstable(): value(Value::Dialect) {}

    // Implicit conversions between enum and ::Value
    constexpr LanguageDisplayUnstable(Value v) : value(v) {}
    constexpr operator Value() const { return value; }
    // Prevent usage as boolean value
    explicit operator bool() const = delete;

    inline icu4x::capi::LanguageDisplayUnstable AsFFI() const;
    inline static icu4x::LanguageDisplayUnstable FromFFI(icu4x::capi::LanguageDisplayUnstable c_enum);
private:
    Value value;
};

} // namespace
#endif // ICU4X_LanguageDisplayUnstable_D_HPP

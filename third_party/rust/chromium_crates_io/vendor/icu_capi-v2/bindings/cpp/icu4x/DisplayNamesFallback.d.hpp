#ifndef ICU4X_DisplayNamesFallback_D_HPP
#define ICU4X_DisplayNamesFallback_D_HPP

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
    enum DisplayNamesFallback {
      DisplayNamesFallback_Code = 0,
      DisplayNamesFallback_None = 1,
    };

    typedef struct DisplayNamesFallback_option {union { DisplayNamesFallback ok; }; bool is_ok; } DisplayNamesFallback_option;
} // namespace capi
} // namespace

namespace icu4x {
/**
 * 🚧 This API is unstable and may experience breaking changes outside major releases.
 *
 * See the [Rust documentation for `Fallback`](https://docs.rs/icu/2.2.0/icu/experimental/displaynames/enum.Fallback.html) for more information.
 */
class DisplayNamesFallback {
public:
    enum Value {
        Code = 0,
        None = 1,
    };

    DisplayNamesFallback(): value(Value::Code) {}

    // Implicit conversions between enum and ::Value
    constexpr DisplayNamesFallback(Value v) : value(v) {}
    constexpr operator Value() const { return value; }
    // Prevent usage as boolean value
    explicit operator bool() const = delete;

    inline icu4x::capi::DisplayNamesFallback AsFFI() const;
    inline static icu4x::DisplayNamesFallback FromFFI(icu4x::capi::DisplayNamesFallback c_enum);
private:
    Value value;
};

} // namespace
#endif // ICU4X_DisplayNamesFallback_D_HPP

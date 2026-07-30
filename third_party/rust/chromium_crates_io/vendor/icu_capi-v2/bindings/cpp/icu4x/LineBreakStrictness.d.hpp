#ifndef ICU4X_LineBreakStrictness_D_HPP
#define ICU4X_LineBreakStrictness_D_HPP

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
    enum LineBreakStrictness {
      LineBreakStrictness_Loose = 0,
      LineBreakStrictness_Normal = 1,
      LineBreakStrictness_Strict = 2,
      LineBreakStrictness_Anywhere = 3,
    };

    typedef struct LineBreakStrictness_option {union { LineBreakStrictness ok; }; bool is_ok; } LineBreakStrictness_option;
} // namespace capi
} // namespace

namespace icu4x {
/**
 * See the [Rust documentation for `LineBreakStrictness`](https://docs.rs/icu/2.2.0/icu/segmenter/options/enum.LineBreakStrictness.html) for more information.
 */
class LineBreakStrictness {
public:
    enum Value {
        Loose = 0,
        Normal = 1,
        Strict = 2,
        Anywhere = 3,
    };

    LineBreakStrictness(): value(Value::Strict) {}

    // Implicit conversions between enum and ::Value
    constexpr LineBreakStrictness(Value v) : value(v) {}
    constexpr operator Value() const { return value; }
    // Prevent usage as boolean value
    explicit operator bool() const = delete;

    inline icu4x::capi::LineBreakStrictness AsFFI() const;
    inline static icu4x::LineBreakStrictness FromFFI(icu4x::capi::LineBreakStrictness c_enum);
private:
    Value value;
};

} // namespace
#endif // ICU4X_LineBreakStrictness_D_HPP

#ifndef ICU4X_LineBreakWordOption_D_HPP
#define ICU4X_LineBreakWordOption_D_HPP

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
    enum LineBreakWordOption {
      LineBreakWordOption_Normal = 0,
      LineBreakWordOption_BreakAll = 1,
      LineBreakWordOption_KeepAll = 2,
    };

    typedef struct LineBreakWordOption_option {union { LineBreakWordOption ok; }; bool is_ok; } LineBreakWordOption_option;
} // namespace capi
} // namespace

namespace icu4x {
/**
 * See the [Rust documentation for `LineBreakWordOption`](https://docs.rs/icu/2.2.0/icu/segmenter/options/enum.LineBreakWordOption.html) for more information.
 */
class LineBreakWordOption {
public:
    enum Value {
        Normal = 0,
        BreakAll = 1,
        KeepAll = 2,
    };

    LineBreakWordOption(): value(Value::Normal) {}

    // Implicit conversions between enum and ::Value
    constexpr LineBreakWordOption(Value v) : value(v) {}
    constexpr operator Value() const { return value; }
    // Prevent usage as boolean value
    explicit operator bool() const = delete;

    inline icu4x::capi::LineBreakWordOption AsFFI() const;
    inline static icu4x::LineBreakWordOption FromFFI(icu4x::capi::LineBreakWordOption c_enum);
private:
    Value value;
};

} // namespace
#endif // ICU4X_LineBreakWordOption_D_HPP

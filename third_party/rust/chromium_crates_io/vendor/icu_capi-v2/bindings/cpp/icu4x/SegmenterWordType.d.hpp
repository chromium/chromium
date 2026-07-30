#ifndef ICU4X_SegmenterWordType_D_HPP
#define ICU4X_SegmenterWordType_D_HPP

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
    enum SegmenterWordType {
      SegmenterWordType_None = 0,
      SegmenterWordType_Number = 1,
      SegmenterWordType_Letter = 2,
    };

    typedef struct SegmenterWordType_option {union { SegmenterWordType ok; }; bool is_ok; } SegmenterWordType_option;
} // namespace capi
} // namespace

namespace icu4x {
/**
 * See the [Rust documentation for `WordType`](https://docs.rs/icu/2.2.0/icu/segmenter/options/enum.WordType.html) for more information.
 */
class SegmenterWordType {
public:
    enum Value {
        None = 0,
        Number = 1,
        Letter = 2,
    };

    SegmenterWordType(): value(Value::None) {}

    // Implicit conversions between enum and ::Value
    constexpr SegmenterWordType(Value v) : value(v) {}
    constexpr operator Value() const { return value; }
    // Prevent usage as boolean value
    explicit operator bool() const = delete;

  /**
   * See the [Rust documentation for `is_word_like`](https://docs.rs/icu/2.2.0/icu/segmenter/options/enum.WordType.html#method.is_word_like) for more information.
   */
  inline bool is_word_like() const;

    inline icu4x::capi::SegmenterWordType AsFFI() const;
    inline static icu4x::SegmenterWordType FromFFI(icu4x::capi::SegmenterWordType c_enum);
private:
    Value value;
};

} // namespace
#endif // ICU4X_SegmenterWordType_D_HPP

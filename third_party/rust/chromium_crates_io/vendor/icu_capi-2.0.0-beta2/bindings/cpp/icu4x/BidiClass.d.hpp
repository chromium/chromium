#ifndef icu4x_BidiClass_D_HPP
#define icu4x_BidiClass_D_HPP

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include "../diplomat_runtime.hpp"

namespace icu4x {
class BidiClass;
}


namespace icu4x {
namespace capi {
    enum BidiClass {
      BidiClass_LeftToRight = 0,
      BidiClass_RightToLeft = 1,
      BidiClass_EuropeanNumber = 2,
      BidiClass_EuropeanSeparator = 3,
      BidiClass_EuropeanTerminator = 4,
      BidiClass_ArabicNumber = 5,
      BidiClass_CommonSeparator = 6,
      BidiClass_ParagraphSeparator = 7,
      BidiClass_SegmentSeparator = 8,
      BidiClass_WhiteSpace = 9,
      BidiClass_OtherNeutral = 10,
      BidiClass_LeftToRightEmbedding = 11,
      BidiClass_LeftToRightOverride = 12,
      BidiClass_ArabicLetter = 13,
      BidiClass_RightToLeftEmbedding = 14,
      BidiClass_RightToLeftOverride = 15,
      BidiClass_PopDirectionalFormat = 16,
      BidiClass_NonspacingMark = 17,
      BidiClass_BoundaryNeutral = 18,
      BidiClass_FirstStrongIsolate = 19,
      BidiClass_LeftToRightIsolate = 20,
      BidiClass_RightToLeftIsolate = 21,
      BidiClass_PopDirectionalIsolate = 22,
    };
    
    typedef struct BidiClass_option {union { BidiClass ok; }; bool is_ok; } BidiClass_option;
} // namespace capi
} // namespace

namespace icu4x {
class BidiClass {
public:
  enum Value {
    LeftToRight = 0,
    RightToLeft = 1,
    EuropeanNumber = 2,
    EuropeanSeparator = 3,
    EuropeanTerminator = 4,
    ArabicNumber = 5,
    CommonSeparator = 6,
    ParagraphSeparator = 7,
    SegmentSeparator = 8,
    WhiteSpace = 9,
    OtherNeutral = 10,
    LeftToRightEmbedding = 11,
    LeftToRightOverride = 12,
    ArabicLetter = 13,
    RightToLeftEmbedding = 14,
    RightToLeftOverride = 15,
    PopDirectionalFormat = 16,
    NonspacingMark = 17,
    BoundaryNeutral = 18,
    FirstStrongIsolate = 19,
    LeftToRightIsolate = 20,
    RightToLeftIsolate = 21,
    PopDirectionalIsolate = 22,
  };

  BidiClass() = default;
  // Implicit conversions between enum and ::Value
  constexpr BidiClass(Value v) : value(v) {}
  constexpr operator Value() const { return value; }
  // Prevent usage as boolean value
  explicit operator bool() const = delete;

  inline static icu4x::BidiClass for_char(char32_t ch);

  inline std::optional<std::string_view> long_name();

  inline std::optional<std::string_view> short_name();

  inline uint8_t to_integer_value();

  inline static std::optional<icu4x::BidiClass> from_integer_value(uint8_t other);

  inline icu4x::capi::BidiClass AsFFI() const;
  inline static icu4x::BidiClass FromFFI(icu4x::capi::BidiClass c_enum);
private:
    Value value;
};

} // namespace
#endif // icu4x_BidiClass_D_HPP

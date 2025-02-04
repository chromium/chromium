#ifndef icu4x_WordBreak_D_HPP
#define icu4x_WordBreak_D_HPP

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <optional>
#include "../diplomat_runtime.hpp"

namespace icu4x {
class WordBreak;
}


namespace icu4x {
namespace capi {
    enum WordBreak {
      WordBreak_Other = 0,
      WordBreak_ALetter = 1,
      WordBreak_Format = 2,
      WordBreak_Katakana = 3,
      WordBreak_MidLetter = 4,
      WordBreak_MidNum = 5,
      WordBreak_Numeric = 6,
      WordBreak_ExtendNumLet = 7,
      WordBreak_CR = 8,
      WordBreak_Extend = 9,
      WordBreak_LF = 10,
      WordBreak_MidNumLet = 11,
      WordBreak_Newline = 12,
      WordBreak_RegionalIndicator = 13,
      WordBreak_HebrewLetter = 14,
      WordBreak_SingleQuote = 15,
      WordBreak_DoubleQuote = 16,
      WordBreak_EBase = 17,
      WordBreak_EBaseGAZ = 18,
      WordBreak_EModifier = 19,
      WordBreak_GlueAfterZwj = 20,
      WordBreak_ZWJ = 21,
      WordBreak_WSegSpace = 22,
    };
    
    typedef struct WordBreak_option {union { WordBreak ok; }; bool is_ok; } WordBreak_option;
} // namespace capi
} // namespace

namespace icu4x {
class WordBreak {
public:
  enum Value {
    Other = 0,
    ALetter = 1,
    Format = 2,
    Katakana = 3,
    MidLetter = 4,
    MidNum = 5,
    Numeric = 6,
    ExtendNumLet = 7,
    CR = 8,
    Extend = 9,
    LF = 10,
    MidNumLet = 11,
    Newline = 12,
    RegionalIndicator = 13,
    HebrewLetter = 14,
    SingleQuote = 15,
    DoubleQuote = 16,
    EBase = 17,
    EBaseGAZ = 18,
    EModifier = 19,
    GlueAfterZwj = 20,
    ZWJ = 21,
    WSegSpace = 22,
  };

  WordBreak() = default;
  // Implicit conversions between enum and ::Value
  constexpr WordBreak(Value v) : value(v) {}
  constexpr operator Value() const { return value; }
  // Prevent usage as boolean value
  explicit operator bool() const = delete;

  inline uint8_t to_integer();

  inline static std::optional<icu4x::WordBreak> from_integer(uint8_t other);

  inline icu4x::capi::WordBreak AsFFI() const;
  inline static icu4x::WordBreak FromFFI(icu4x::capi::WordBreak c_enum);
private:
    Value value;
};

} // namespace
#endif // icu4x_WordBreak_D_HPP

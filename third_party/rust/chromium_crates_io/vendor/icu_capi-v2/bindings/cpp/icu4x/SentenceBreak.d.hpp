#ifndef icu4x_SentenceBreak_D_HPP
#define icu4x_SentenceBreak_D_HPP

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include "../diplomat_runtime.hpp"

namespace icu4x {
class SentenceBreak;
}


namespace icu4x {
namespace capi {
    enum SentenceBreak {
      SentenceBreak_Other = 0,
      SentenceBreak_ATerm = 1,
      SentenceBreak_Close = 2,
      SentenceBreak_Format = 3,
      SentenceBreak_Lower = 4,
      SentenceBreak_Numeric = 5,
      SentenceBreak_OLetter = 6,
      SentenceBreak_Sep = 7,
      SentenceBreak_Sp = 8,
      SentenceBreak_STerm = 9,
      SentenceBreak_Upper = 10,
      SentenceBreak_CR = 11,
      SentenceBreak_Extend = 12,
      SentenceBreak_LF = 13,
      SentenceBreak_SContinue = 14,
    };
    
    typedef struct SentenceBreak_option {union { SentenceBreak ok; }; bool is_ok; } SentenceBreak_option;
} // namespace capi
} // namespace

namespace icu4x {
class SentenceBreak {
public:
  enum Value {
    Other = 0,
    ATerm = 1,
    Close = 2,
    Format = 3,
    Lower = 4,
    Numeric = 5,
    OLetter = 6,
    Sep = 7,
    Sp = 8,
    STerm = 9,
    Upper = 10,
    CR = 11,
    Extend = 12,
    LF = 13,
    SContinue = 14,
  };

  SentenceBreak() = default;
  // Implicit conversions between enum and ::Value
  constexpr SentenceBreak(Value v) : value(v) {}
  constexpr operator Value() const { return value; }
  // Prevent usage as boolean value
  explicit operator bool() const = delete;

  inline static icu4x::SentenceBreak for_char(char32_t ch);

  inline std::optional<std::string_view> long_name();

  inline std::optional<std::string_view> short_name();

  inline uint8_t to_integer_value();

  inline static std::optional<icu4x::SentenceBreak> from_integer_value(uint8_t other);

  inline icu4x::capi::SentenceBreak AsFFI() const;
  inline static icu4x::SentenceBreak FromFFI(icu4x::capi::SentenceBreak c_enum);
private:
    Value value;
};

} // namespace
#endif // icu4x_SentenceBreak_D_HPP

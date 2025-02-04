#ifndef icu4x_CanonicalCombiningClass_D_HPP
#define icu4x_CanonicalCombiningClass_D_HPP

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <optional>
#include "../diplomat_runtime.hpp"

namespace icu4x {
class CanonicalCombiningClass;
}


namespace icu4x {
namespace capi {
    enum CanonicalCombiningClass {
      CanonicalCombiningClass_NotReordered = 0,
      CanonicalCombiningClass_Overlay = 1,
      CanonicalCombiningClass_HanReading = 6,
      CanonicalCombiningClass_Nukta = 7,
      CanonicalCombiningClass_KanaVoicing = 8,
      CanonicalCombiningClass_Virama = 9,
      CanonicalCombiningClass_CCC10 = 10,
      CanonicalCombiningClass_CCC11 = 11,
      CanonicalCombiningClass_CCC12 = 12,
      CanonicalCombiningClass_CCC13 = 13,
      CanonicalCombiningClass_CCC14 = 14,
      CanonicalCombiningClass_CCC15 = 15,
      CanonicalCombiningClass_CCC16 = 16,
      CanonicalCombiningClass_CCC17 = 17,
      CanonicalCombiningClass_CCC18 = 18,
      CanonicalCombiningClass_CCC19 = 19,
      CanonicalCombiningClass_CCC20 = 20,
      CanonicalCombiningClass_CCC21 = 21,
      CanonicalCombiningClass_CCC22 = 22,
      CanonicalCombiningClass_CCC23 = 23,
      CanonicalCombiningClass_CCC24 = 24,
      CanonicalCombiningClass_CCC25 = 25,
      CanonicalCombiningClass_CCC26 = 26,
      CanonicalCombiningClass_CCC27 = 27,
      CanonicalCombiningClass_CCC28 = 28,
      CanonicalCombiningClass_CCC29 = 29,
      CanonicalCombiningClass_CCC30 = 30,
      CanonicalCombiningClass_CCC31 = 31,
      CanonicalCombiningClass_CCC32 = 32,
      CanonicalCombiningClass_CCC33 = 33,
      CanonicalCombiningClass_CCC34 = 34,
      CanonicalCombiningClass_CCC35 = 35,
      CanonicalCombiningClass_CCC36 = 36,
      CanonicalCombiningClass_CCC84 = 84,
      CanonicalCombiningClass_CCC91 = 91,
      CanonicalCombiningClass_CCC103 = 103,
      CanonicalCombiningClass_CCC107 = 107,
      CanonicalCombiningClass_CCC118 = 118,
      CanonicalCombiningClass_CCC122 = 122,
      CanonicalCombiningClass_CCC129 = 129,
      CanonicalCombiningClass_CCC130 = 130,
      CanonicalCombiningClass_CCC132 = 132,
      CanonicalCombiningClass_CCC133 = 133,
      CanonicalCombiningClass_AttachedBelowLeft = 200,
      CanonicalCombiningClass_AttachedBelow = 202,
      CanonicalCombiningClass_AttachedAbove = 214,
      CanonicalCombiningClass_AttachedAboveRight = 216,
      CanonicalCombiningClass_BelowLeft = 218,
      CanonicalCombiningClass_Below = 220,
      CanonicalCombiningClass_BelowRight = 222,
      CanonicalCombiningClass_Left = 224,
      CanonicalCombiningClass_Right = 226,
      CanonicalCombiningClass_AboveLeft = 228,
      CanonicalCombiningClass_Above = 230,
      CanonicalCombiningClass_AboveRight = 232,
      CanonicalCombiningClass_DoubleBelow = 233,
      CanonicalCombiningClass_DoubleAbove = 234,
      CanonicalCombiningClass_IotaSubscript = 240,
    };
    
    typedef struct CanonicalCombiningClass_option {union { CanonicalCombiningClass ok; }; bool is_ok; } CanonicalCombiningClass_option;
} // namespace capi
} // namespace

namespace icu4x {
class CanonicalCombiningClass {
public:
  enum Value {
    NotReordered = 0,
    Overlay = 1,
    HanReading = 6,
    Nukta = 7,
    KanaVoicing = 8,
    Virama = 9,
    CCC10 = 10,
    CCC11 = 11,
    CCC12 = 12,
    CCC13 = 13,
    CCC14 = 14,
    CCC15 = 15,
    CCC16 = 16,
    CCC17 = 17,
    CCC18 = 18,
    CCC19 = 19,
    CCC20 = 20,
    CCC21 = 21,
    CCC22 = 22,
    CCC23 = 23,
    CCC24 = 24,
    CCC25 = 25,
    CCC26 = 26,
    CCC27 = 27,
    CCC28 = 28,
    CCC29 = 29,
    CCC30 = 30,
    CCC31 = 31,
    CCC32 = 32,
    CCC33 = 33,
    CCC34 = 34,
    CCC35 = 35,
    CCC36 = 36,
    CCC84 = 84,
    CCC91 = 91,
    CCC103 = 103,
    CCC107 = 107,
    CCC118 = 118,
    CCC122 = 122,
    CCC129 = 129,
    CCC130 = 130,
    CCC132 = 132,
    CCC133 = 133,
    AttachedBelowLeft = 200,
    AttachedBelow = 202,
    AttachedAbove = 214,
    AttachedAboveRight = 216,
    BelowLeft = 218,
    Below = 220,
    BelowRight = 222,
    Left = 224,
    Right = 226,
    AboveLeft = 228,
    Above = 230,
    AboveRight = 232,
    DoubleBelow = 233,
    DoubleAbove = 234,
    IotaSubscript = 240,
  };

  CanonicalCombiningClass() = default;
  // Implicit conversions between enum and ::Value
  constexpr CanonicalCombiningClass(Value v) : value(v) {}
  constexpr operator Value() const { return value; }
  // Prevent usage as boolean value
  explicit operator bool() const = delete;

  inline uint8_t to_integer();

  inline static std::optional<icu4x::CanonicalCombiningClass> from_integer(uint8_t other);

  inline icu4x::capi::CanonicalCombiningClass AsFFI() const;
  inline static icu4x::CanonicalCombiningClass FromFFI(icu4x::capi::CanonicalCombiningClass c_enum);
private:
    Value value;
};

} // namespace
#endif // icu4x_CanonicalCombiningClass_D_HPP

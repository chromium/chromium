#ifndef icu4x_IndicSyllabicCategory_D_HPP
#define icu4x_IndicSyllabicCategory_D_HPP

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <optional>
#include "../diplomat_runtime.hpp"

namespace icu4x {
class IndicSyllabicCategory;
}


namespace icu4x {
namespace capi {
    enum IndicSyllabicCategory {
      IndicSyllabicCategory_Other = 0,
      IndicSyllabicCategory_Avagraha = 1,
      IndicSyllabicCategory_Bindu = 2,
      IndicSyllabicCategory_BrahmiJoiningNumber = 3,
      IndicSyllabicCategory_CantillationMark = 4,
      IndicSyllabicCategory_Consonant = 5,
      IndicSyllabicCategory_ConsonantDead = 6,
      IndicSyllabicCategory_ConsonantFinal = 7,
      IndicSyllabicCategory_ConsonantHeadLetter = 8,
      IndicSyllabicCategory_ConsonantInitialPostfixed = 9,
      IndicSyllabicCategory_ConsonantKiller = 10,
      IndicSyllabicCategory_ConsonantMedial = 11,
      IndicSyllabicCategory_ConsonantPlaceholder = 12,
      IndicSyllabicCategory_ConsonantPrecedingRepha = 13,
      IndicSyllabicCategory_ConsonantPrefixed = 14,
      IndicSyllabicCategory_ConsonantSucceedingRepha = 15,
      IndicSyllabicCategory_ConsonantSubjoined = 16,
      IndicSyllabicCategory_ConsonantWithStacker = 17,
      IndicSyllabicCategory_GeminationMark = 18,
      IndicSyllabicCategory_InvisibleStacker = 19,
      IndicSyllabicCategory_Joiner = 20,
      IndicSyllabicCategory_ModifyingLetter = 21,
      IndicSyllabicCategory_NonJoiner = 22,
      IndicSyllabicCategory_Nukta = 23,
      IndicSyllabicCategory_Number = 24,
      IndicSyllabicCategory_NumberJoiner = 25,
      IndicSyllabicCategory_PureKiller = 26,
      IndicSyllabicCategory_RegisterShifter = 27,
      IndicSyllabicCategory_SyllableModifier = 28,
      IndicSyllabicCategory_ToneLetter = 29,
      IndicSyllabicCategory_ToneMark = 30,
      IndicSyllabicCategory_Virama = 31,
      IndicSyllabicCategory_Visarga = 32,
      IndicSyllabicCategory_Vowel = 33,
      IndicSyllabicCategory_VowelDependent = 34,
      IndicSyllabicCategory_VowelIndependent = 35,
      IndicSyllabicCategory_ReorderingKiller = 36,
    };
    
    typedef struct IndicSyllabicCategory_option {union { IndicSyllabicCategory ok; }; bool is_ok; } IndicSyllabicCategory_option;
} // namespace capi
} // namespace

namespace icu4x {
class IndicSyllabicCategory {
public:
  enum Value {
    Other = 0,
    Avagraha = 1,
    Bindu = 2,
    BrahmiJoiningNumber = 3,
    CantillationMark = 4,
    Consonant = 5,
    ConsonantDead = 6,
    ConsonantFinal = 7,
    ConsonantHeadLetter = 8,
    ConsonantInitialPostfixed = 9,
    ConsonantKiller = 10,
    ConsonantMedial = 11,
    ConsonantPlaceholder = 12,
    ConsonantPrecedingRepha = 13,
    ConsonantPrefixed = 14,
    ConsonantSucceedingRepha = 15,
    ConsonantSubjoined = 16,
    ConsonantWithStacker = 17,
    GeminationMark = 18,
    InvisibleStacker = 19,
    Joiner = 20,
    ModifyingLetter = 21,
    NonJoiner = 22,
    Nukta = 23,
    Number = 24,
    NumberJoiner = 25,
    PureKiller = 26,
    RegisterShifter = 27,
    SyllableModifier = 28,
    ToneLetter = 29,
    ToneMark = 30,
    Virama = 31,
    Visarga = 32,
    Vowel = 33,
    VowelDependent = 34,
    VowelIndependent = 35,
    ReorderingKiller = 36,
  };

  IndicSyllabicCategory() = default;
  // Implicit conversions between enum and ::Value
  constexpr IndicSyllabicCategory(Value v) : value(v) {}
  constexpr operator Value() const { return value; }
  // Prevent usage as boolean value
  explicit operator bool() const = delete;

  inline uint8_t to_integer();

  inline static std::optional<icu4x::IndicSyllabicCategory> from_integer(uint8_t other);

  inline icu4x::capi::IndicSyllabicCategory AsFFI() const;
  inline static icu4x::IndicSyllabicCategory FromFFI(icu4x::capi::IndicSyllabicCategory c_enum);
private:
    Value value;
};

} // namespace
#endif // icu4x_IndicSyllabicCategory_D_HPP

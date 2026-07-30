#ifndef ICU4X_IndicSyllabicCategory_D_HPP
#define ICU4X_IndicSyllabicCategory_D_HPP

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
class IndicSyllabicCategory;
} // namespace icu4x



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
      IndicSyllabicCategory_ConsonantSubjoined = 15,
      IndicSyllabicCategory_ConsonantSucceedingRepha = 16,
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
/**
 * See the [Rust documentation for `IndicSyllabicCategory`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.IndicSyllabicCategory.html) for more information.
 */
class IndicSyllabicCategory {
public:
    enum Value {
        /**
         * See the [Rust documentation for `Other`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.IndicSyllabicCategory.html#associatedconstant.Other) for more information.
         */
        Other = 0,
        /**
         * See the [Rust documentation for `Avagraha`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.IndicSyllabicCategory.html#associatedconstant.Avagraha) for more information.
         */
        Avagraha = 1,
        /**
         * See the [Rust documentation for `Bindu`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.IndicSyllabicCategory.html#associatedconstant.Bindu) for more information.
         */
        Bindu = 2,
        /**
         * See the [Rust documentation for `BrahmiJoiningNumber`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.IndicSyllabicCategory.html#associatedconstant.BrahmiJoiningNumber) for more information.
         */
        BrahmiJoiningNumber = 3,
        /**
         * See the [Rust documentation for `CantillationMark`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.IndicSyllabicCategory.html#associatedconstant.CantillationMark) for more information.
         */
        CantillationMark = 4,
        /**
         * See the [Rust documentation for `Consonant`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.IndicSyllabicCategory.html#associatedconstant.Consonant) for more information.
         */
        Consonant = 5,
        /**
         * See the [Rust documentation for `ConsonantDead`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.IndicSyllabicCategory.html#associatedconstant.ConsonantDead) for more information.
         */
        ConsonantDead = 6,
        /**
         * See the [Rust documentation for `ConsonantFinal`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.IndicSyllabicCategory.html#associatedconstant.ConsonantFinal) for more information.
         */
        ConsonantFinal = 7,
        /**
         * See the [Rust documentation for `ConsonantHeadLetter`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.IndicSyllabicCategory.html#associatedconstant.ConsonantHeadLetter) for more information.
         */
        ConsonantHeadLetter = 8,
        /**
         * See the [Rust documentation for `ConsonantInitialPostfixed`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.IndicSyllabicCategory.html#associatedconstant.ConsonantInitialPostfixed) for more information.
         */
        ConsonantInitialPostfixed = 9,
        /**
         * See the [Rust documentation for `ConsonantKiller`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.IndicSyllabicCategory.html#associatedconstant.ConsonantKiller) for more information.
         */
        ConsonantKiller = 10,
        /**
         * See the [Rust documentation for `ConsonantMedial`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.IndicSyllabicCategory.html#associatedconstant.ConsonantMedial) for more information.
         */
        ConsonantMedial = 11,
        /**
         * See the [Rust documentation for `ConsonantPlaceholder`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.IndicSyllabicCategory.html#associatedconstant.ConsonantPlaceholder) for more information.
         */
        ConsonantPlaceholder = 12,
        /**
         * See the [Rust documentation for `ConsonantPrecedingRepha`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.IndicSyllabicCategory.html#associatedconstant.ConsonantPrecedingRepha) for more information.
         */
        ConsonantPrecedingRepha = 13,
        /**
         * See the [Rust documentation for `ConsonantPrefixed`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.IndicSyllabicCategory.html#associatedconstant.ConsonantPrefixed) for more information.
         */
        ConsonantPrefixed = 14,
        /**
         * See the [Rust documentation for `ConsonantSubjoined`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.IndicSyllabicCategory.html#associatedconstant.ConsonantSubjoined) for more information.
         */
        ConsonantSubjoined = 15,
        /**
         * See the [Rust documentation for `ConsonantSucceedingRepha`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.IndicSyllabicCategory.html#associatedconstant.ConsonantSucceedingRepha) for more information.
         */
        ConsonantSucceedingRepha = 16,
        /**
         * See the [Rust documentation for `ConsonantWithStacker`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.IndicSyllabicCategory.html#associatedconstant.ConsonantWithStacker) for more information.
         */
        ConsonantWithStacker = 17,
        /**
         * See the [Rust documentation for `GeminationMark`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.IndicSyllabicCategory.html#associatedconstant.GeminationMark) for more information.
         */
        GeminationMark = 18,
        /**
         * See the [Rust documentation for `InvisibleStacker`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.IndicSyllabicCategory.html#associatedconstant.InvisibleStacker) for more information.
         */
        InvisibleStacker = 19,
        /**
         * See the [Rust documentation for `Joiner`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.IndicSyllabicCategory.html#associatedconstant.Joiner) for more information.
         */
        Joiner = 20,
        /**
         * See the [Rust documentation for `ModifyingLetter`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.IndicSyllabicCategory.html#associatedconstant.ModifyingLetter) for more information.
         */
        ModifyingLetter = 21,
        /**
         * See the [Rust documentation for `NonJoiner`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.IndicSyllabicCategory.html#associatedconstant.NonJoiner) for more information.
         */
        NonJoiner = 22,
        /**
         * See the [Rust documentation for `Nukta`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.IndicSyllabicCategory.html#associatedconstant.Nukta) for more information.
         */
        Nukta = 23,
        /**
         * See the [Rust documentation for `Number`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.IndicSyllabicCategory.html#associatedconstant.Number) for more information.
         */
        Number = 24,
        /**
         * See the [Rust documentation for `NumberJoiner`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.IndicSyllabicCategory.html#associatedconstant.NumberJoiner) for more information.
         */
        NumberJoiner = 25,
        /**
         * See the [Rust documentation for `PureKiller`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.IndicSyllabicCategory.html#associatedconstant.PureKiller) for more information.
         */
        PureKiller = 26,
        /**
         * See the [Rust documentation for `RegisterShifter`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.IndicSyllabicCategory.html#associatedconstant.RegisterShifter) for more information.
         */
        RegisterShifter = 27,
        /**
         * See the [Rust documentation for `SyllableModifier`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.IndicSyllabicCategory.html#associatedconstant.SyllableModifier) for more information.
         */
        SyllableModifier = 28,
        /**
         * See the [Rust documentation for `ToneLetter`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.IndicSyllabicCategory.html#associatedconstant.ToneLetter) for more information.
         */
        ToneLetter = 29,
        /**
         * See the [Rust documentation for `ToneMark`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.IndicSyllabicCategory.html#associatedconstant.ToneMark) for more information.
         */
        ToneMark = 30,
        /**
         * See the [Rust documentation for `Virama`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.IndicSyllabicCategory.html#associatedconstant.Virama) for more information.
         */
        Virama = 31,
        /**
         * See the [Rust documentation for `Visarga`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.IndicSyllabicCategory.html#associatedconstant.Visarga) for more information.
         */
        Visarga = 32,
        /**
         * See the [Rust documentation for `Vowel`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.IndicSyllabicCategory.html#associatedconstant.Vowel) for more information.
         */
        Vowel = 33,
        /**
         * See the [Rust documentation for `VowelDependent`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.IndicSyllabicCategory.html#associatedconstant.VowelDependent) for more information.
         */
        VowelDependent = 34,
        /**
         * See the [Rust documentation for `VowelIndependent`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.IndicSyllabicCategory.html#associatedconstant.VowelIndependent) for more information.
         */
        VowelIndependent = 35,
        /**
         * See the [Rust documentation for `ReorderingKiller`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.IndicSyllabicCategory.html#associatedconstant.ReorderingKiller) for more information.
         */
        ReorderingKiller = 36,
    };

    IndicSyllabicCategory(): value(Value::Other) {}

    // Implicit conversions between enum and ::Value
    constexpr IndicSyllabicCategory(Value v) : value(v) {}
    constexpr operator Value() const { return value; }
    // Prevent usage as boolean value
    explicit operator bool() const = delete;

  /**
   * See the [Rust documentation for `for_char`](https://docs.rs/icu/2.2.0/icu/properties/props/trait.EnumeratedProperty.html#tymethod.for_char) for more information.
   */
  inline static icu4x::IndicSyllabicCategory for_char(char32_t ch);

  /**
   * Get the "long" name of this property value (returns empty if property value is unknown)
   *
   * See the [Rust documentation for `get`](https://docs.rs/icu/2.2.0/icu/properties/struct.PropertyNamesLongBorrowed.html#method.get) for more information.
   */
  inline std::optional<std::string_view> long_name() const;

  /**
   * Get the "short" name of this property value (returns empty if property value is unknown)
   *
   * See the [Rust documentation for `get`](https://docs.rs/icu/2.2.0/icu/properties/struct.PropertyNamesShortBorrowed.html#method.get) for more information.
   */
  inline std::optional<std::string_view> short_name() const;

  /**
   * Convert to an integer value usable with ICU4C and `CodePointMapData`
   *
   * See the [Rust documentation for `to_icu4c_value`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.IndicSyllabicCategory.html#method.to_icu4c_value) for more information.
   */
  inline uint8_t to_integer_value() const;

  /**
   * Convert from an integer value from ICU4C or `CodePointMapData`
   *
   * See the [Rust documentation for `from_icu4c_value`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.IndicSyllabicCategory.html#method.from_icu4c_value) for more information.
   */
  inline static std::optional<icu4x::IndicSyllabicCategory> from_integer_value(uint8_t other);

  inline static std::optional<icu4x::IndicSyllabicCategory> try_from_str(std::string_view s);

    inline icu4x::capi::IndicSyllabicCategory AsFFI() const;
    inline static icu4x::IndicSyllabicCategory FromFFI(icu4x::capi::IndicSyllabicCategory c_enum);
private:
    Value value;
};

} // namespace
#endif // ICU4X_IndicSyllabicCategory_D_HPP

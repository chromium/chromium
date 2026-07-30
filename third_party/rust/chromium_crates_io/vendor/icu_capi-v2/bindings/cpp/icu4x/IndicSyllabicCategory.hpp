#ifndef ICU4X_IndicSyllabicCategory_HPP
#define ICU4X_IndicSyllabicCategory_HPP

#include "IndicSyllabicCategory.d.hpp"

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
    extern "C" {

    icu4x::capi::IndicSyllabicCategory icu4x_IndicSyllabicCategory_for_char_mv1(char32_t ch);

    typedef struct icu4x_IndicSyllabicCategory_long_name_mv1_result {union {icu4x::diplomat::capi::DiplomatStringView ok; }; bool is_ok;} icu4x_IndicSyllabicCategory_long_name_mv1_result;
    icu4x_IndicSyllabicCategory_long_name_mv1_result icu4x_IndicSyllabicCategory_long_name_mv1(icu4x::capi::IndicSyllabicCategory self);

    typedef struct icu4x_IndicSyllabicCategory_short_name_mv1_result {union {icu4x::diplomat::capi::DiplomatStringView ok; }; bool is_ok;} icu4x_IndicSyllabicCategory_short_name_mv1_result;
    icu4x_IndicSyllabicCategory_short_name_mv1_result icu4x_IndicSyllabicCategory_short_name_mv1(icu4x::capi::IndicSyllabicCategory self);

    uint8_t icu4x_IndicSyllabicCategory_to_integer_value_mv1(icu4x::capi::IndicSyllabicCategory self);

    typedef struct icu4x_IndicSyllabicCategory_from_integer_value_mv1_result {union {icu4x::capi::IndicSyllabicCategory ok; }; bool is_ok;} icu4x_IndicSyllabicCategory_from_integer_value_mv1_result;
    icu4x_IndicSyllabicCategory_from_integer_value_mv1_result icu4x_IndicSyllabicCategory_from_integer_value_mv1(uint8_t other);

    typedef struct icu4x_IndicSyllabicCategory_try_from_str_mv1_result {union {icu4x::capi::IndicSyllabicCategory ok; }; bool is_ok;} icu4x_IndicSyllabicCategory_try_from_str_mv1_result;
    icu4x_IndicSyllabicCategory_try_from_str_mv1_result icu4x_IndicSyllabicCategory_try_from_str_mv1(icu4x::diplomat::capi::DiplomatStringView s);

    } // extern "C"
} // namespace capi
} // namespace

inline icu4x::capi::IndicSyllabicCategory icu4x::IndicSyllabicCategory::AsFFI() const {
    return static_cast<icu4x::capi::IndicSyllabicCategory>(value);
}

inline icu4x::IndicSyllabicCategory icu4x::IndicSyllabicCategory::FromFFI(icu4x::capi::IndicSyllabicCategory c_enum) {
    switch (c_enum) {
        case icu4x::capi::IndicSyllabicCategory_Other:
        case icu4x::capi::IndicSyllabicCategory_Avagraha:
        case icu4x::capi::IndicSyllabicCategory_Bindu:
        case icu4x::capi::IndicSyllabicCategory_BrahmiJoiningNumber:
        case icu4x::capi::IndicSyllabicCategory_CantillationMark:
        case icu4x::capi::IndicSyllabicCategory_Consonant:
        case icu4x::capi::IndicSyllabicCategory_ConsonantDead:
        case icu4x::capi::IndicSyllabicCategory_ConsonantFinal:
        case icu4x::capi::IndicSyllabicCategory_ConsonantHeadLetter:
        case icu4x::capi::IndicSyllabicCategory_ConsonantInitialPostfixed:
        case icu4x::capi::IndicSyllabicCategory_ConsonantKiller:
        case icu4x::capi::IndicSyllabicCategory_ConsonantMedial:
        case icu4x::capi::IndicSyllabicCategory_ConsonantPlaceholder:
        case icu4x::capi::IndicSyllabicCategory_ConsonantPrecedingRepha:
        case icu4x::capi::IndicSyllabicCategory_ConsonantPrefixed:
        case icu4x::capi::IndicSyllabicCategory_ConsonantSubjoined:
        case icu4x::capi::IndicSyllabicCategory_ConsonantSucceedingRepha:
        case icu4x::capi::IndicSyllabicCategory_ConsonantWithStacker:
        case icu4x::capi::IndicSyllabicCategory_GeminationMark:
        case icu4x::capi::IndicSyllabicCategory_InvisibleStacker:
        case icu4x::capi::IndicSyllabicCategory_Joiner:
        case icu4x::capi::IndicSyllabicCategory_ModifyingLetter:
        case icu4x::capi::IndicSyllabicCategory_NonJoiner:
        case icu4x::capi::IndicSyllabicCategory_Nukta:
        case icu4x::capi::IndicSyllabicCategory_Number:
        case icu4x::capi::IndicSyllabicCategory_NumberJoiner:
        case icu4x::capi::IndicSyllabicCategory_PureKiller:
        case icu4x::capi::IndicSyllabicCategory_RegisterShifter:
        case icu4x::capi::IndicSyllabicCategory_SyllableModifier:
        case icu4x::capi::IndicSyllabicCategory_ToneLetter:
        case icu4x::capi::IndicSyllabicCategory_ToneMark:
        case icu4x::capi::IndicSyllabicCategory_Virama:
        case icu4x::capi::IndicSyllabicCategory_Visarga:
        case icu4x::capi::IndicSyllabicCategory_Vowel:
        case icu4x::capi::IndicSyllabicCategory_VowelDependent:
        case icu4x::capi::IndicSyllabicCategory_VowelIndependent:
        case icu4x::capi::IndicSyllabicCategory_ReorderingKiller:
            return static_cast<icu4x::IndicSyllabicCategory::Value>(c_enum);
        default:
            std::abort();
    }
}

inline icu4x::IndicSyllabicCategory icu4x::IndicSyllabicCategory::for_char(char32_t ch) {
    auto result = icu4x::capi::icu4x_IndicSyllabicCategory_for_char_mv1(ch);
    return icu4x::IndicSyllabicCategory::FromFFI(result);
}

inline std::optional<std::string_view> icu4x::IndicSyllabicCategory::long_name() const {
    auto result = icu4x::capi::icu4x_IndicSyllabicCategory_long_name_mv1(this->AsFFI());
    return result.is_ok ? std::optional<std::string_view>(std::string_view(result.ok.data, result.ok.len)) : std::nullopt;
}

inline std::optional<std::string_view> icu4x::IndicSyllabicCategory::short_name() const {
    auto result = icu4x::capi::icu4x_IndicSyllabicCategory_short_name_mv1(this->AsFFI());
    return result.is_ok ? std::optional<std::string_view>(std::string_view(result.ok.data, result.ok.len)) : std::nullopt;
}

inline uint8_t icu4x::IndicSyllabicCategory::to_integer_value() const {
    auto result = icu4x::capi::icu4x_IndicSyllabicCategory_to_integer_value_mv1(this->AsFFI());
    return result;
}

inline std::optional<icu4x::IndicSyllabicCategory> icu4x::IndicSyllabicCategory::from_integer_value(uint8_t other) {
    auto result = icu4x::capi::icu4x_IndicSyllabicCategory_from_integer_value_mv1(other);
    return result.is_ok ? std::optional<icu4x::IndicSyllabicCategory>(icu4x::IndicSyllabicCategory::FromFFI(result.ok)) : std::nullopt;
}

inline std::optional<icu4x::IndicSyllabicCategory> icu4x::IndicSyllabicCategory::try_from_str(std::string_view s) {
    auto result = icu4x::capi::icu4x_IndicSyllabicCategory_try_from_str_mv1({s.data(), s.size()});
    return result.is_ok ? std::optional<icu4x::IndicSyllabicCategory>(icu4x::IndicSyllabicCategory::FromFFI(result.ok)) : std::nullopt;
}
#endif // ICU4X_IndicSyllabicCategory_HPP

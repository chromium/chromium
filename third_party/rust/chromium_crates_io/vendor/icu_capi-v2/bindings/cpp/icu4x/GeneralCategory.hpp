#ifndef ICU4X_GeneralCategory_HPP
#define ICU4X_GeneralCategory_HPP

#include "GeneralCategory.d.hpp"

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include <cstdlib>
#include "GeneralCategoryGroup.hpp"
#include "diplomat_runtime.hpp"


namespace icu4x {
namespace capi {
    extern "C" {

    icu4x::capi::GeneralCategory icu4x_GeneralCategory_for_char_mv1(char32_t ch);

    typedef struct icu4x_GeneralCategory_long_name_mv1_result {union {icu4x::diplomat::capi::DiplomatStringView ok; }; bool is_ok;} icu4x_GeneralCategory_long_name_mv1_result;
    icu4x_GeneralCategory_long_name_mv1_result icu4x_GeneralCategory_long_name_mv1(icu4x::capi::GeneralCategory self);

    typedef struct icu4x_GeneralCategory_short_name_mv1_result {union {icu4x::diplomat::capi::DiplomatStringView ok; }; bool is_ok;} icu4x_GeneralCategory_short_name_mv1_result;
    icu4x_GeneralCategory_short_name_mv1_result icu4x_GeneralCategory_short_name_mv1(icu4x::capi::GeneralCategory self);

    uint8_t icu4x_GeneralCategory_to_integer_value_mv1(icu4x::capi::GeneralCategory self);

    typedef struct icu4x_GeneralCategory_from_integer_value_mv1_result {union {icu4x::capi::GeneralCategory ok; }; bool is_ok;} icu4x_GeneralCategory_from_integer_value_mv1_result;
    icu4x_GeneralCategory_from_integer_value_mv1_result icu4x_GeneralCategory_from_integer_value_mv1(uint8_t other);

    typedef struct icu4x_GeneralCategory_try_from_str_mv1_result {union {icu4x::capi::GeneralCategory ok; }; bool is_ok;} icu4x_GeneralCategory_try_from_str_mv1_result;
    icu4x_GeneralCategory_try_from_str_mv1_result icu4x_GeneralCategory_try_from_str_mv1(icu4x::diplomat::capi::DiplomatStringView s);

    icu4x::capi::GeneralCategoryGroup icu4x_GeneralCategory_to_group_mv1(icu4x::capi::GeneralCategory self);

    } // extern "C"
} // namespace capi
} // namespace

inline icu4x::capi::GeneralCategory icu4x::GeneralCategory::AsFFI() const {
    return static_cast<icu4x::capi::GeneralCategory>(value);
}

inline icu4x::GeneralCategory icu4x::GeneralCategory::FromFFI(icu4x::capi::GeneralCategory c_enum) {
    switch (c_enum) {
        case icu4x::capi::GeneralCategory_Unassigned:
        case icu4x::capi::GeneralCategory_UppercaseLetter:
        case icu4x::capi::GeneralCategory_LowercaseLetter:
        case icu4x::capi::GeneralCategory_TitlecaseLetter:
        case icu4x::capi::GeneralCategory_ModifierLetter:
        case icu4x::capi::GeneralCategory_OtherLetter:
        case icu4x::capi::GeneralCategory_NonspacingMark:
        case icu4x::capi::GeneralCategory_SpacingMark:
        case icu4x::capi::GeneralCategory_EnclosingMark:
        case icu4x::capi::GeneralCategory_DecimalNumber:
        case icu4x::capi::GeneralCategory_LetterNumber:
        case icu4x::capi::GeneralCategory_OtherNumber:
        case icu4x::capi::GeneralCategory_SpaceSeparator:
        case icu4x::capi::GeneralCategory_LineSeparator:
        case icu4x::capi::GeneralCategory_ParagraphSeparator:
        case icu4x::capi::GeneralCategory_Control:
        case icu4x::capi::GeneralCategory_Format:
        case icu4x::capi::GeneralCategory_PrivateUse:
        case icu4x::capi::GeneralCategory_Surrogate:
        case icu4x::capi::GeneralCategory_DashPunctuation:
        case icu4x::capi::GeneralCategory_OpenPunctuation:
        case icu4x::capi::GeneralCategory_ClosePunctuation:
        case icu4x::capi::GeneralCategory_ConnectorPunctuation:
        case icu4x::capi::GeneralCategory_InitialPunctuation:
        case icu4x::capi::GeneralCategory_FinalPunctuation:
        case icu4x::capi::GeneralCategory_OtherPunctuation:
        case icu4x::capi::GeneralCategory_MathSymbol:
        case icu4x::capi::GeneralCategory_CurrencySymbol:
        case icu4x::capi::GeneralCategory_ModifierSymbol:
        case icu4x::capi::GeneralCategory_OtherSymbol:
            return static_cast<icu4x::GeneralCategory::Value>(c_enum);
        default:
            std::abort();
    }
}

inline icu4x::GeneralCategory icu4x::GeneralCategory::for_char(char32_t ch) {
    auto result = icu4x::capi::icu4x_GeneralCategory_for_char_mv1(ch);
    return icu4x::GeneralCategory::FromFFI(result);
}

inline std::optional<std::string_view> icu4x::GeneralCategory::long_name() const {
    auto result = icu4x::capi::icu4x_GeneralCategory_long_name_mv1(this->AsFFI());
    return result.is_ok ? std::optional<std::string_view>(std::string_view(result.ok.data, result.ok.len)) : std::nullopt;
}

inline std::optional<std::string_view> icu4x::GeneralCategory::short_name() const {
    auto result = icu4x::capi::icu4x_GeneralCategory_short_name_mv1(this->AsFFI());
    return result.is_ok ? std::optional<std::string_view>(std::string_view(result.ok.data, result.ok.len)) : std::nullopt;
}

inline uint8_t icu4x::GeneralCategory::to_integer_value() const {
    auto result = icu4x::capi::icu4x_GeneralCategory_to_integer_value_mv1(this->AsFFI());
    return result;
}

inline std::optional<icu4x::GeneralCategory> icu4x::GeneralCategory::from_integer_value(uint8_t other) {
    auto result = icu4x::capi::icu4x_GeneralCategory_from_integer_value_mv1(other);
    return result.is_ok ? std::optional<icu4x::GeneralCategory>(icu4x::GeneralCategory::FromFFI(result.ok)) : std::nullopt;
}

inline std::optional<icu4x::GeneralCategory> icu4x::GeneralCategory::try_from_str(std::string_view s) {
    auto result = icu4x::capi::icu4x_GeneralCategory_try_from_str_mv1({s.data(), s.size()});
    return result.is_ok ? std::optional<icu4x::GeneralCategory>(icu4x::GeneralCategory::FromFFI(result.ok)) : std::nullopt;
}

inline icu4x::GeneralCategoryGroup icu4x::GeneralCategory::to_group() const {
    auto result = icu4x::capi::icu4x_GeneralCategory_to_group_mv1(this->AsFFI());
    return icu4x::GeneralCategoryGroup::FromFFI(result);
}
#endif // ICU4X_GeneralCategory_HPP

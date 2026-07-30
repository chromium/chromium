#ifndef ICU4X_HangulSyllableType_HPP
#define ICU4X_HangulSyllableType_HPP

#include "HangulSyllableType.d.hpp"

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

    icu4x::capi::HangulSyllableType icu4x_HangulSyllableType_for_char_mv1(char32_t ch);

    typedef struct icu4x_HangulSyllableType_long_name_mv1_result {union {icu4x::diplomat::capi::DiplomatStringView ok; }; bool is_ok;} icu4x_HangulSyllableType_long_name_mv1_result;
    icu4x_HangulSyllableType_long_name_mv1_result icu4x_HangulSyllableType_long_name_mv1(icu4x::capi::HangulSyllableType self);

    typedef struct icu4x_HangulSyllableType_short_name_mv1_result {union {icu4x::diplomat::capi::DiplomatStringView ok; }; bool is_ok;} icu4x_HangulSyllableType_short_name_mv1_result;
    icu4x_HangulSyllableType_short_name_mv1_result icu4x_HangulSyllableType_short_name_mv1(icu4x::capi::HangulSyllableType self);

    uint8_t icu4x_HangulSyllableType_to_integer_value_mv1(icu4x::capi::HangulSyllableType self);

    typedef struct icu4x_HangulSyllableType_from_integer_value_mv1_result {union {icu4x::capi::HangulSyllableType ok; }; bool is_ok;} icu4x_HangulSyllableType_from_integer_value_mv1_result;
    icu4x_HangulSyllableType_from_integer_value_mv1_result icu4x_HangulSyllableType_from_integer_value_mv1(uint8_t other);

    typedef struct icu4x_HangulSyllableType_try_from_str_mv1_result {union {icu4x::capi::HangulSyllableType ok; }; bool is_ok;} icu4x_HangulSyllableType_try_from_str_mv1_result;
    icu4x_HangulSyllableType_try_from_str_mv1_result icu4x_HangulSyllableType_try_from_str_mv1(icu4x::diplomat::capi::DiplomatStringView s);

    } // extern "C"
} // namespace capi
} // namespace

inline icu4x::capi::HangulSyllableType icu4x::HangulSyllableType::AsFFI() const {
    return static_cast<icu4x::capi::HangulSyllableType>(value);
}

inline icu4x::HangulSyllableType icu4x::HangulSyllableType::FromFFI(icu4x::capi::HangulSyllableType c_enum) {
    switch (c_enum) {
        case icu4x::capi::HangulSyllableType_NotApplicable:
        case icu4x::capi::HangulSyllableType_LeadingJamo:
        case icu4x::capi::HangulSyllableType_VowelJamo:
        case icu4x::capi::HangulSyllableType_TrailingJamo:
        case icu4x::capi::HangulSyllableType_LeadingVowelSyllable:
        case icu4x::capi::HangulSyllableType_LeadingVowelTrailingSyllable:
            return static_cast<icu4x::HangulSyllableType::Value>(c_enum);
        default:
            std::abort();
    }
}

inline icu4x::HangulSyllableType icu4x::HangulSyllableType::for_char(char32_t ch) {
    auto result = icu4x::capi::icu4x_HangulSyllableType_for_char_mv1(ch);
    return icu4x::HangulSyllableType::FromFFI(result);
}

inline std::optional<std::string_view> icu4x::HangulSyllableType::long_name() const {
    auto result = icu4x::capi::icu4x_HangulSyllableType_long_name_mv1(this->AsFFI());
    return result.is_ok ? std::optional<std::string_view>(std::string_view(result.ok.data, result.ok.len)) : std::nullopt;
}

inline std::optional<std::string_view> icu4x::HangulSyllableType::short_name() const {
    auto result = icu4x::capi::icu4x_HangulSyllableType_short_name_mv1(this->AsFFI());
    return result.is_ok ? std::optional<std::string_view>(std::string_view(result.ok.data, result.ok.len)) : std::nullopt;
}

inline uint8_t icu4x::HangulSyllableType::to_integer_value() const {
    auto result = icu4x::capi::icu4x_HangulSyllableType_to_integer_value_mv1(this->AsFFI());
    return result;
}

inline std::optional<icu4x::HangulSyllableType> icu4x::HangulSyllableType::from_integer_value(uint8_t other) {
    auto result = icu4x::capi::icu4x_HangulSyllableType_from_integer_value_mv1(other);
    return result.is_ok ? std::optional<icu4x::HangulSyllableType>(icu4x::HangulSyllableType::FromFFI(result.ok)) : std::nullopt;
}

inline std::optional<icu4x::HangulSyllableType> icu4x::HangulSyllableType::try_from_str(std::string_view s) {
    auto result = icu4x::capi::icu4x_HangulSyllableType_try_from_str_mv1({s.data(), s.size()});
    return result.is_ok ? std::optional<icu4x::HangulSyllableType>(icu4x::HangulSyllableType::FromFFI(result.ok)) : std::nullopt;
}
#endif // ICU4X_HangulSyllableType_HPP

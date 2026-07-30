#ifndef ICU4X_WordBreak_HPP
#define ICU4X_WordBreak_HPP

#include "WordBreak.d.hpp"

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

    icu4x::capi::WordBreak icu4x_WordBreak_for_char_mv1(char32_t ch);

    typedef struct icu4x_WordBreak_long_name_mv1_result {union {icu4x::diplomat::capi::DiplomatStringView ok; }; bool is_ok;} icu4x_WordBreak_long_name_mv1_result;
    icu4x_WordBreak_long_name_mv1_result icu4x_WordBreak_long_name_mv1(icu4x::capi::WordBreak self);

    typedef struct icu4x_WordBreak_short_name_mv1_result {union {icu4x::diplomat::capi::DiplomatStringView ok; }; bool is_ok;} icu4x_WordBreak_short_name_mv1_result;
    icu4x_WordBreak_short_name_mv1_result icu4x_WordBreak_short_name_mv1(icu4x::capi::WordBreak self);

    uint8_t icu4x_WordBreak_to_integer_value_mv1(icu4x::capi::WordBreak self);

    typedef struct icu4x_WordBreak_from_integer_value_mv1_result {union {icu4x::capi::WordBreak ok; }; bool is_ok;} icu4x_WordBreak_from_integer_value_mv1_result;
    icu4x_WordBreak_from_integer_value_mv1_result icu4x_WordBreak_from_integer_value_mv1(uint8_t other);

    typedef struct icu4x_WordBreak_try_from_str_mv1_result {union {icu4x::capi::WordBreak ok; }; bool is_ok;} icu4x_WordBreak_try_from_str_mv1_result;
    icu4x_WordBreak_try_from_str_mv1_result icu4x_WordBreak_try_from_str_mv1(icu4x::diplomat::capi::DiplomatStringView s);

    } // extern "C"
} // namespace capi
} // namespace

inline icu4x::capi::WordBreak icu4x::WordBreak::AsFFI() const {
    return static_cast<icu4x::capi::WordBreak>(value);
}

inline icu4x::WordBreak icu4x::WordBreak::FromFFI(icu4x::capi::WordBreak c_enum) {
    switch (c_enum) {
        case icu4x::capi::WordBreak_Other:
        case icu4x::capi::WordBreak_ALetter:
        case icu4x::capi::WordBreak_Format:
        case icu4x::capi::WordBreak_Katakana:
        case icu4x::capi::WordBreak_MidLetter:
        case icu4x::capi::WordBreak_MidNum:
        case icu4x::capi::WordBreak_Numeric:
        case icu4x::capi::WordBreak_ExtendNumLet:
        case icu4x::capi::WordBreak_CR:
        case icu4x::capi::WordBreak_Extend:
        case icu4x::capi::WordBreak_LF:
        case icu4x::capi::WordBreak_MidNumLet:
        case icu4x::capi::WordBreak_Newline:
        case icu4x::capi::WordBreak_RegionalIndicator:
        case icu4x::capi::WordBreak_HebrewLetter:
        case icu4x::capi::WordBreak_SingleQuote:
        case icu4x::capi::WordBreak_DoubleQuote:
        case icu4x::capi::WordBreak_EBase:
        case icu4x::capi::WordBreak_EBaseGAZ:
        case icu4x::capi::WordBreak_EModifier:
        case icu4x::capi::WordBreak_GlueAfterZwj:
        case icu4x::capi::WordBreak_ZWJ:
        case icu4x::capi::WordBreak_WSegSpace:
            return static_cast<icu4x::WordBreak::Value>(c_enum);
        default:
            std::abort();
    }
}

inline icu4x::WordBreak icu4x::WordBreak::for_char(char32_t ch) {
    auto result = icu4x::capi::icu4x_WordBreak_for_char_mv1(ch);
    return icu4x::WordBreak::FromFFI(result);
}

inline std::optional<std::string_view> icu4x::WordBreak::long_name() const {
    auto result = icu4x::capi::icu4x_WordBreak_long_name_mv1(this->AsFFI());
    return result.is_ok ? std::optional<std::string_view>(std::string_view(result.ok.data, result.ok.len)) : std::nullopt;
}

inline std::optional<std::string_view> icu4x::WordBreak::short_name() const {
    auto result = icu4x::capi::icu4x_WordBreak_short_name_mv1(this->AsFFI());
    return result.is_ok ? std::optional<std::string_view>(std::string_view(result.ok.data, result.ok.len)) : std::nullopt;
}

inline uint8_t icu4x::WordBreak::to_integer_value() const {
    auto result = icu4x::capi::icu4x_WordBreak_to_integer_value_mv1(this->AsFFI());
    return result;
}

inline std::optional<icu4x::WordBreak> icu4x::WordBreak::from_integer_value(uint8_t other) {
    auto result = icu4x::capi::icu4x_WordBreak_from_integer_value_mv1(other);
    return result.is_ok ? std::optional<icu4x::WordBreak>(icu4x::WordBreak::FromFFI(result.ok)) : std::nullopt;
}

inline std::optional<icu4x::WordBreak> icu4x::WordBreak::try_from_str(std::string_view s) {
    auto result = icu4x::capi::icu4x_WordBreak_try_from_str_mv1({s.data(), s.size()});
    return result.is_ok ? std::optional<icu4x::WordBreak>(icu4x::WordBreak::FromFFI(result.ok)) : std::nullopt;
}
#endif // ICU4X_WordBreak_HPP

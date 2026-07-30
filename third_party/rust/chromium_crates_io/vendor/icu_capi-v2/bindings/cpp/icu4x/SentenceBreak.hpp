#ifndef ICU4X_SentenceBreak_HPP
#define ICU4X_SentenceBreak_HPP

#include "SentenceBreak.d.hpp"

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

    icu4x::capi::SentenceBreak icu4x_SentenceBreak_for_char_mv1(char32_t ch);

    typedef struct icu4x_SentenceBreak_long_name_mv1_result {union {icu4x::diplomat::capi::DiplomatStringView ok; }; bool is_ok;} icu4x_SentenceBreak_long_name_mv1_result;
    icu4x_SentenceBreak_long_name_mv1_result icu4x_SentenceBreak_long_name_mv1(icu4x::capi::SentenceBreak self);

    typedef struct icu4x_SentenceBreak_short_name_mv1_result {union {icu4x::diplomat::capi::DiplomatStringView ok; }; bool is_ok;} icu4x_SentenceBreak_short_name_mv1_result;
    icu4x_SentenceBreak_short_name_mv1_result icu4x_SentenceBreak_short_name_mv1(icu4x::capi::SentenceBreak self);

    uint8_t icu4x_SentenceBreak_to_integer_value_mv1(icu4x::capi::SentenceBreak self);

    typedef struct icu4x_SentenceBreak_from_integer_value_mv1_result {union {icu4x::capi::SentenceBreak ok; }; bool is_ok;} icu4x_SentenceBreak_from_integer_value_mv1_result;
    icu4x_SentenceBreak_from_integer_value_mv1_result icu4x_SentenceBreak_from_integer_value_mv1(uint8_t other);

    typedef struct icu4x_SentenceBreak_try_from_str_mv1_result {union {icu4x::capi::SentenceBreak ok; }; bool is_ok;} icu4x_SentenceBreak_try_from_str_mv1_result;
    icu4x_SentenceBreak_try_from_str_mv1_result icu4x_SentenceBreak_try_from_str_mv1(icu4x::diplomat::capi::DiplomatStringView s);

    } // extern "C"
} // namespace capi
} // namespace

inline icu4x::capi::SentenceBreak icu4x::SentenceBreak::AsFFI() const {
    return static_cast<icu4x::capi::SentenceBreak>(value);
}

inline icu4x::SentenceBreak icu4x::SentenceBreak::FromFFI(icu4x::capi::SentenceBreak c_enum) {
    switch (c_enum) {
        case icu4x::capi::SentenceBreak_Other:
        case icu4x::capi::SentenceBreak_ATerm:
        case icu4x::capi::SentenceBreak_Close:
        case icu4x::capi::SentenceBreak_Format:
        case icu4x::capi::SentenceBreak_Lower:
        case icu4x::capi::SentenceBreak_Numeric:
        case icu4x::capi::SentenceBreak_OLetter:
        case icu4x::capi::SentenceBreak_Sep:
        case icu4x::capi::SentenceBreak_Sp:
        case icu4x::capi::SentenceBreak_STerm:
        case icu4x::capi::SentenceBreak_Upper:
        case icu4x::capi::SentenceBreak_CR:
        case icu4x::capi::SentenceBreak_Extend:
        case icu4x::capi::SentenceBreak_LF:
        case icu4x::capi::SentenceBreak_SContinue:
            return static_cast<icu4x::SentenceBreak::Value>(c_enum);
        default:
            std::abort();
    }
}

inline icu4x::SentenceBreak icu4x::SentenceBreak::for_char(char32_t ch) {
    auto result = icu4x::capi::icu4x_SentenceBreak_for_char_mv1(ch);
    return icu4x::SentenceBreak::FromFFI(result);
}

inline std::optional<std::string_view> icu4x::SentenceBreak::long_name() const {
    auto result = icu4x::capi::icu4x_SentenceBreak_long_name_mv1(this->AsFFI());
    return result.is_ok ? std::optional<std::string_view>(std::string_view(result.ok.data, result.ok.len)) : std::nullopt;
}

inline std::optional<std::string_view> icu4x::SentenceBreak::short_name() const {
    auto result = icu4x::capi::icu4x_SentenceBreak_short_name_mv1(this->AsFFI());
    return result.is_ok ? std::optional<std::string_view>(std::string_view(result.ok.data, result.ok.len)) : std::nullopt;
}

inline uint8_t icu4x::SentenceBreak::to_integer_value() const {
    auto result = icu4x::capi::icu4x_SentenceBreak_to_integer_value_mv1(this->AsFFI());
    return result;
}

inline std::optional<icu4x::SentenceBreak> icu4x::SentenceBreak::from_integer_value(uint8_t other) {
    auto result = icu4x::capi::icu4x_SentenceBreak_from_integer_value_mv1(other);
    return result.is_ok ? std::optional<icu4x::SentenceBreak>(icu4x::SentenceBreak::FromFFI(result.ok)) : std::nullopt;
}

inline std::optional<icu4x::SentenceBreak> icu4x::SentenceBreak::try_from_str(std::string_view s) {
    auto result = icu4x::capi::icu4x_SentenceBreak_try_from_str_mv1({s.data(), s.size()});
    return result.is_ok ? std::optional<icu4x::SentenceBreak>(icu4x::SentenceBreak::FromFFI(result.ok)) : std::nullopt;
}
#endif // ICU4X_SentenceBreak_HPP

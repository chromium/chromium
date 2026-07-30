#ifndef ICU4X_EastAsianWidth_HPP
#define ICU4X_EastAsianWidth_HPP

#include "EastAsianWidth.d.hpp"

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

    icu4x::capi::EastAsianWidth icu4x_EastAsianWidth_for_char_mv1(char32_t ch);

    typedef struct icu4x_EastAsianWidth_long_name_mv1_result {union {icu4x::diplomat::capi::DiplomatStringView ok; }; bool is_ok;} icu4x_EastAsianWidth_long_name_mv1_result;
    icu4x_EastAsianWidth_long_name_mv1_result icu4x_EastAsianWidth_long_name_mv1(icu4x::capi::EastAsianWidth self);

    typedef struct icu4x_EastAsianWidth_short_name_mv1_result {union {icu4x::diplomat::capi::DiplomatStringView ok; }; bool is_ok;} icu4x_EastAsianWidth_short_name_mv1_result;
    icu4x_EastAsianWidth_short_name_mv1_result icu4x_EastAsianWidth_short_name_mv1(icu4x::capi::EastAsianWidth self);

    uint8_t icu4x_EastAsianWidth_to_integer_value_mv1(icu4x::capi::EastAsianWidth self);

    typedef struct icu4x_EastAsianWidth_from_integer_value_mv1_result {union {icu4x::capi::EastAsianWidth ok; }; bool is_ok;} icu4x_EastAsianWidth_from_integer_value_mv1_result;
    icu4x_EastAsianWidth_from_integer_value_mv1_result icu4x_EastAsianWidth_from_integer_value_mv1(uint8_t other);

    typedef struct icu4x_EastAsianWidth_try_from_str_mv1_result {union {icu4x::capi::EastAsianWidth ok; }; bool is_ok;} icu4x_EastAsianWidth_try_from_str_mv1_result;
    icu4x_EastAsianWidth_try_from_str_mv1_result icu4x_EastAsianWidth_try_from_str_mv1(icu4x::diplomat::capi::DiplomatStringView s);

    } // extern "C"
} // namespace capi
} // namespace

inline icu4x::capi::EastAsianWidth icu4x::EastAsianWidth::AsFFI() const {
    return static_cast<icu4x::capi::EastAsianWidth>(value);
}

inline icu4x::EastAsianWidth icu4x::EastAsianWidth::FromFFI(icu4x::capi::EastAsianWidth c_enum) {
    switch (c_enum) {
        case icu4x::capi::EastAsianWidth_Neutral:
        case icu4x::capi::EastAsianWidth_Ambiguous:
        case icu4x::capi::EastAsianWidth_Halfwidth:
        case icu4x::capi::EastAsianWidth_Fullwidth:
        case icu4x::capi::EastAsianWidth_Narrow:
        case icu4x::capi::EastAsianWidth_Wide:
            return static_cast<icu4x::EastAsianWidth::Value>(c_enum);
        default:
            std::abort();
    }
}

inline icu4x::EastAsianWidth icu4x::EastAsianWidth::for_char(char32_t ch) {
    auto result = icu4x::capi::icu4x_EastAsianWidth_for_char_mv1(ch);
    return icu4x::EastAsianWidth::FromFFI(result);
}

inline std::optional<std::string_view> icu4x::EastAsianWidth::long_name() const {
    auto result = icu4x::capi::icu4x_EastAsianWidth_long_name_mv1(this->AsFFI());
    return result.is_ok ? std::optional<std::string_view>(std::string_view(result.ok.data, result.ok.len)) : std::nullopt;
}

inline std::optional<std::string_view> icu4x::EastAsianWidth::short_name() const {
    auto result = icu4x::capi::icu4x_EastAsianWidth_short_name_mv1(this->AsFFI());
    return result.is_ok ? std::optional<std::string_view>(std::string_view(result.ok.data, result.ok.len)) : std::nullopt;
}

inline uint8_t icu4x::EastAsianWidth::to_integer_value() const {
    auto result = icu4x::capi::icu4x_EastAsianWidth_to_integer_value_mv1(this->AsFFI());
    return result;
}

inline std::optional<icu4x::EastAsianWidth> icu4x::EastAsianWidth::from_integer_value(uint8_t other) {
    auto result = icu4x::capi::icu4x_EastAsianWidth_from_integer_value_mv1(other);
    return result.is_ok ? std::optional<icu4x::EastAsianWidth>(icu4x::EastAsianWidth::FromFFI(result.ok)) : std::nullopt;
}

inline std::optional<icu4x::EastAsianWidth> icu4x::EastAsianWidth::try_from_str(std::string_view s) {
    auto result = icu4x::capi::icu4x_EastAsianWidth_try_from_str_mv1({s.data(), s.size()});
    return result.is_ok ? std::optional<icu4x::EastAsianWidth>(icu4x::EastAsianWidth::FromFFI(result.ok)) : std::nullopt;
}
#endif // ICU4X_EastAsianWidth_HPP

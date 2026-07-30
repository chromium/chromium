#ifndef ICU4X_GraphemeClusterBreak_HPP
#define ICU4X_GraphemeClusterBreak_HPP

#include "GraphemeClusterBreak.d.hpp"

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

    icu4x::capi::GraphemeClusterBreak icu4x_GraphemeClusterBreak_for_char_mv1(char32_t ch);

    typedef struct icu4x_GraphemeClusterBreak_long_name_mv1_result {union {icu4x::diplomat::capi::DiplomatStringView ok; }; bool is_ok;} icu4x_GraphemeClusterBreak_long_name_mv1_result;
    icu4x_GraphemeClusterBreak_long_name_mv1_result icu4x_GraphemeClusterBreak_long_name_mv1(icu4x::capi::GraphemeClusterBreak self);

    typedef struct icu4x_GraphemeClusterBreak_short_name_mv1_result {union {icu4x::diplomat::capi::DiplomatStringView ok; }; bool is_ok;} icu4x_GraphemeClusterBreak_short_name_mv1_result;
    icu4x_GraphemeClusterBreak_short_name_mv1_result icu4x_GraphemeClusterBreak_short_name_mv1(icu4x::capi::GraphemeClusterBreak self);

    uint8_t icu4x_GraphemeClusterBreak_to_integer_value_mv1(icu4x::capi::GraphemeClusterBreak self);

    typedef struct icu4x_GraphemeClusterBreak_from_integer_value_mv1_result {union {icu4x::capi::GraphemeClusterBreak ok; }; bool is_ok;} icu4x_GraphemeClusterBreak_from_integer_value_mv1_result;
    icu4x_GraphemeClusterBreak_from_integer_value_mv1_result icu4x_GraphemeClusterBreak_from_integer_value_mv1(uint8_t other);

    typedef struct icu4x_GraphemeClusterBreak_try_from_str_mv1_result {union {icu4x::capi::GraphemeClusterBreak ok; }; bool is_ok;} icu4x_GraphemeClusterBreak_try_from_str_mv1_result;
    icu4x_GraphemeClusterBreak_try_from_str_mv1_result icu4x_GraphemeClusterBreak_try_from_str_mv1(icu4x::diplomat::capi::DiplomatStringView s);

    } // extern "C"
} // namespace capi
} // namespace

inline icu4x::capi::GraphemeClusterBreak icu4x::GraphemeClusterBreak::AsFFI() const {
    return static_cast<icu4x::capi::GraphemeClusterBreak>(value);
}

inline icu4x::GraphemeClusterBreak icu4x::GraphemeClusterBreak::FromFFI(icu4x::capi::GraphemeClusterBreak c_enum) {
    switch (c_enum) {
        case icu4x::capi::GraphemeClusterBreak_Other:
        case icu4x::capi::GraphemeClusterBreak_Control:
        case icu4x::capi::GraphemeClusterBreak_CR:
        case icu4x::capi::GraphemeClusterBreak_Extend:
        case icu4x::capi::GraphemeClusterBreak_L:
        case icu4x::capi::GraphemeClusterBreak_LF:
        case icu4x::capi::GraphemeClusterBreak_LV:
        case icu4x::capi::GraphemeClusterBreak_LVT:
        case icu4x::capi::GraphemeClusterBreak_T:
        case icu4x::capi::GraphemeClusterBreak_V:
        case icu4x::capi::GraphemeClusterBreak_SpacingMark:
        case icu4x::capi::GraphemeClusterBreak_Prepend:
        case icu4x::capi::GraphemeClusterBreak_RegionalIndicator:
        case icu4x::capi::GraphemeClusterBreak_EBase:
        case icu4x::capi::GraphemeClusterBreak_EBaseGAZ:
        case icu4x::capi::GraphemeClusterBreak_EModifier:
        case icu4x::capi::GraphemeClusterBreak_GlueAfterZwj:
        case icu4x::capi::GraphemeClusterBreak_ZWJ:
            return static_cast<icu4x::GraphemeClusterBreak::Value>(c_enum);
        default:
            std::abort();
    }
}

inline icu4x::GraphemeClusterBreak icu4x::GraphemeClusterBreak::for_char(char32_t ch) {
    auto result = icu4x::capi::icu4x_GraphemeClusterBreak_for_char_mv1(ch);
    return icu4x::GraphemeClusterBreak::FromFFI(result);
}

inline std::optional<std::string_view> icu4x::GraphemeClusterBreak::long_name() const {
    auto result = icu4x::capi::icu4x_GraphemeClusterBreak_long_name_mv1(this->AsFFI());
    return result.is_ok ? std::optional<std::string_view>(std::string_view(result.ok.data, result.ok.len)) : std::nullopt;
}

inline std::optional<std::string_view> icu4x::GraphemeClusterBreak::short_name() const {
    auto result = icu4x::capi::icu4x_GraphemeClusterBreak_short_name_mv1(this->AsFFI());
    return result.is_ok ? std::optional<std::string_view>(std::string_view(result.ok.data, result.ok.len)) : std::nullopt;
}

inline uint8_t icu4x::GraphemeClusterBreak::to_integer_value() const {
    auto result = icu4x::capi::icu4x_GraphemeClusterBreak_to_integer_value_mv1(this->AsFFI());
    return result;
}

inline std::optional<icu4x::GraphemeClusterBreak> icu4x::GraphemeClusterBreak::from_integer_value(uint8_t other) {
    auto result = icu4x::capi::icu4x_GraphemeClusterBreak_from_integer_value_mv1(other);
    return result.is_ok ? std::optional<icu4x::GraphemeClusterBreak>(icu4x::GraphemeClusterBreak::FromFFI(result.ok)) : std::nullopt;
}

inline std::optional<icu4x::GraphemeClusterBreak> icu4x::GraphemeClusterBreak::try_from_str(std::string_view s) {
    auto result = icu4x::capi::icu4x_GraphemeClusterBreak_try_from_str_mv1({s.data(), s.size()});
    return result.is_ok ? std::optional<icu4x::GraphemeClusterBreak>(icu4x::GraphemeClusterBreak::FromFFI(result.ok)) : std::nullopt;
}
#endif // ICU4X_GraphemeClusterBreak_HPP

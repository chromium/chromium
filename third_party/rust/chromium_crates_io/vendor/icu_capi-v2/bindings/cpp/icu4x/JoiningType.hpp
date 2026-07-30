#ifndef ICU4X_JoiningType_HPP
#define ICU4X_JoiningType_HPP

#include "JoiningType.d.hpp"

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

    icu4x::capi::JoiningType icu4x_JoiningType_for_char_mv1(char32_t ch);

    typedef struct icu4x_JoiningType_long_name_mv1_result {union {icu4x::diplomat::capi::DiplomatStringView ok; }; bool is_ok;} icu4x_JoiningType_long_name_mv1_result;
    icu4x_JoiningType_long_name_mv1_result icu4x_JoiningType_long_name_mv1(icu4x::capi::JoiningType self);

    typedef struct icu4x_JoiningType_short_name_mv1_result {union {icu4x::diplomat::capi::DiplomatStringView ok; }; bool is_ok;} icu4x_JoiningType_short_name_mv1_result;
    icu4x_JoiningType_short_name_mv1_result icu4x_JoiningType_short_name_mv1(icu4x::capi::JoiningType self);

    uint8_t icu4x_JoiningType_to_integer_value_mv1(icu4x::capi::JoiningType self);

    typedef struct icu4x_JoiningType_from_integer_value_mv1_result {union {icu4x::capi::JoiningType ok; }; bool is_ok;} icu4x_JoiningType_from_integer_value_mv1_result;
    icu4x_JoiningType_from_integer_value_mv1_result icu4x_JoiningType_from_integer_value_mv1(uint8_t other);

    typedef struct icu4x_JoiningType_try_from_str_mv1_result {union {icu4x::capi::JoiningType ok; }; bool is_ok;} icu4x_JoiningType_try_from_str_mv1_result;
    icu4x_JoiningType_try_from_str_mv1_result icu4x_JoiningType_try_from_str_mv1(icu4x::diplomat::capi::DiplomatStringView s);

    } // extern "C"
} // namespace capi
} // namespace

inline icu4x::capi::JoiningType icu4x::JoiningType::AsFFI() const {
    return static_cast<icu4x::capi::JoiningType>(value);
}

inline icu4x::JoiningType icu4x::JoiningType::FromFFI(icu4x::capi::JoiningType c_enum) {
    switch (c_enum) {
        case icu4x::capi::JoiningType_NonJoining:
        case icu4x::capi::JoiningType_JoinCausing:
        case icu4x::capi::JoiningType_DualJoining:
        case icu4x::capi::JoiningType_LeftJoining:
        case icu4x::capi::JoiningType_RightJoining:
        case icu4x::capi::JoiningType_Transparent:
            return static_cast<icu4x::JoiningType::Value>(c_enum);
        default:
            std::abort();
    }
}

inline icu4x::JoiningType icu4x::JoiningType::for_char(char32_t ch) {
    auto result = icu4x::capi::icu4x_JoiningType_for_char_mv1(ch);
    return icu4x::JoiningType::FromFFI(result);
}

inline std::optional<std::string_view> icu4x::JoiningType::long_name() const {
    auto result = icu4x::capi::icu4x_JoiningType_long_name_mv1(this->AsFFI());
    return result.is_ok ? std::optional<std::string_view>(std::string_view(result.ok.data, result.ok.len)) : std::nullopt;
}

inline std::optional<std::string_view> icu4x::JoiningType::short_name() const {
    auto result = icu4x::capi::icu4x_JoiningType_short_name_mv1(this->AsFFI());
    return result.is_ok ? std::optional<std::string_view>(std::string_view(result.ok.data, result.ok.len)) : std::nullopt;
}

inline uint8_t icu4x::JoiningType::to_integer_value() const {
    auto result = icu4x::capi::icu4x_JoiningType_to_integer_value_mv1(this->AsFFI());
    return result;
}

inline std::optional<icu4x::JoiningType> icu4x::JoiningType::from_integer_value(uint8_t other) {
    auto result = icu4x::capi::icu4x_JoiningType_from_integer_value_mv1(other);
    return result.is_ok ? std::optional<icu4x::JoiningType>(icu4x::JoiningType::FromFFI(result.ok)) : std::nullopt;
}

inline std::optional<icu4x::JoiningType> icu4x::JoiningType::try_from_str(std::string_view s) {
    auto result = icu4x::capi::icu4x_JoiningType_try_from_str_mv1({s.data(), s.size()});
    return result.is_ok ? std::optional<icu4x::JoiningType>(icu4x::JoiningType::FromFFI(result.ok)) : std::nullopt;
}
#endif // ICU4X_JoiningType_HPP

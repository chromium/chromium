#ifndef ICU4X_BidiClass_HPP
#define ICU4X_BidiClass_HPP

#include "BidiClass.d.hpp"

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

    icu4x::capi::BidiClass icu4x_BidiClass_for_char_mv1(char32_t ch);

    typedef struct icu4x_BidiClass_long_name_mv1_result {union {icu4x::diplomat::capi::DiplomatStringView ok; }; bool is_ok;} icu4x_BidiClass_long_name_mv1_result;
    icu4x_BidiClass_long_name_mv1_result icu4x_BidiClass_long_name_mv1(icu4x::capi::BidiClass self);

    typedef struct icu4x_BidiClass_short_name_mv1_result {union {icu4x::diplomat::capi::DiplomatStringView ok; }; bool is_ok;} icu4x_BidiClass_short_name_mv1_result;
    icu4x_BidiClass_short_name_mv1_result icu4x_BidiClass_short_name_mv1(icu4x::capi::BidiClass self);

    uint8_t icu4x_BidiClass_to_integer_value_mv1(icu4x::capi::BidiClass self);

    typedef struct icu4x_BidiClass_from_integer_value_mv1_result {union {icu4x::capi::BidiClass ok; }; bool is_ok;} icu4x_BidiClass_from_integer_value_mv1_result;
    icu4x_BidiClass_from_integer_value_mv1_result icu4x_BidiClass_from_integer_value_mv1(uint8_t other);

    typedef struct icu4x_BidiClass_try_from_str_mv1_result {union {icu4x::capi::BidiClass ok; }; bool is_ok;} icu4x_BidiClass_try_from_str_mv1_result;
    icu4x_BidiClass_try_from_str_mv1_result icu4x_BidiClass_try_from_str_mv1(icu4x::diplomat::capi::DiplomatStringView s);

    } // extern "C"
} // namespace capi
} // namespace

inline icu4x::capi::BidiClass icu4x::BidiClass::AsFFI() const {
    return static_cast<icu4x::capi::BidiClass>(value);
}

inline icu4x::BidiClass icu4x::BidiClass::FromFFI(icu4x::capi::BidiClass c_enum) {
    switch (c_enum) {
        case icu4x::capi::BidiClass_LeftToRight:
        case icu4x::capi::BidiClass_RightToLeft:
        case icu4x::capi::BidiClass_EuropeanNumber:
        case icu4x::capi::BidiClass_EuropeanSeparator:
        case icu4x::capi::BidiClass_EuropeanTerminator:
        case icu4x::capi::BidiClass_ArabicNumber:
        case icu4x::capi::BidiClass_CommonSeparator:
        case icu4x::capi::BidiClass_ParagraphSeparator:
        case icu4x::capi::BidiClass_SegmentSeparator:
        case icu4x::capi::BidiClass_WhiteSpace:
        case icu4x::capi::BidiClass_OtherNeutral:
        case icu4x::capi::BidiClass_LeftToRightEmbedding:
        case icu4x::capi::BidiClass_LeftToRightOverride:
        case icu4x::capi::BidiClass_ArabicLetter:
        case icu4x::capi::BidiClass_RightToLeftEmbedding:
        case icu4x::capi::BidiClass_RightToLeftOverride:
        case icu4x::capi::BidiClass_PopDirectionalFormat:
        case icu4x::capi::BidiClass_NonspacingMark:
        case icu4x::capi::BidiClass_BoundaryNeutral:
        case icu4x::capi::BidiClass_FirstStrongIsolate:
        case icu4x::capi::BidiClass_LeftToRightIsolate:
        case icu4x::capi::BidiClass_RightToLeftIsolate:
        case icu4x::capi::BidiClass_PopDirectionalIsolate:
            return static_cast<icu4x::BidiClass::Value>(c_enum);
        default:
            std::abort();
    }
}

inline icu4x::BidiClass icu4x::BidiClass::for_char(char32_t ch) {
    auto result = icu4x::capi::icu4x_BidiClass_for_char_mv1(ch);
    return icu4x::BidiClass::FromFFI(result);
}

inline std::optional<std::string_view> icu4x::BidiClass::long_name() const {
    auto result = icu4x::capi::icu4x_BidiClass_long_name_mv1(this->AsFFI());
    return result.is_ok ? std::optional<std::string_view>(std::string_view(result.ok.data, result.ok.len)) : std::nullopt;
}

inline std::optional<std::string_view> icu4x::BidiClass::short_name() const {
    auto result = icu4x::capi::icu4x_BidiClass_short_name_mv1(this->AsFFI());
    return result.is_ok ? std::optional<std::string_view>(std::string_view(result.ok.data, result.ok.len)) : std::nullopt;
}

inline uint8_t icu4x::BidiClass::to_integer_value() const {
    auto result = icu4x::capi::icu4x_BidiClass_to_integer_value_mv1(this->AsFFI());
    return result;
}

inline std::optional<icu4x::BidiClass> icu4x::BidiClass::from_integer_value(uint8_t other) {
    auto result = icu4x::capi::icu4x_BidiClass_from_integer_value_mv1(other);
    return result.is_ok ? std::optional<icu4x::BidiClass>(icu4x::BidiClass::FromFFI(result.ok)) : std::nullopt;
}

inline std::optional<icu4x::BidiClass> icu4x::BidiClass::try_from_str(std::string_view s) {
    auto result = icu4x::capi::icu4x_BidiClass_try_from_str_mv1({s.data(), s.size()});
    return result.is_ok ? std::optional<icu4x::BidiClass>(icu4x::BidiClass::FromFFI(result.ok)) : std::nullopt;
}
#endif // ICU4X_BidiClass_HPP

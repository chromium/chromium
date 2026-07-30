#ifndef ICU4X_CanonicalCombiningClass_HPP
#define ICU4X_CanonicalCombiningClass_HPP

#include "CanonicalCombiningClass.d.hpp"

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

    icu4x::capi::CanonicalCombiningClass icu4x_CanonicalCombiningClass_for_char_mv1(char32_t ch);

    typedef struct icu4x_CanonicalCombiningClass_long_name_mv1_result {union {icu4x::diplomat::capi::DiplomatStringView ok; }; bool is_ok;} icu4x_CanonicalCombiningClass_long_name_mv1_result;
    icu4x_CanonicalCombiningClass_long_name_mv1_result icu4x_CanonicalCombiningClass_long_name_mv1(icu4x::capi::CanonicalCombiningClass self);

    typedef struct icu4x_CanonicalCombiningClass_short_name_mv1_result {union {icu4x::diplomat::capi::DiplomatStringView ok; }; bool is_ok;} icu4x_CanonicalCombiningClass_short_name_mv1_result;
    icu4x_CanonicalCombiningClass_short_name_mv1_result icu4x_CanonicalCombiningClass_short_name_mv1(icu4x::capi::CanonicalCombiningClass self);

    uint8_t icu4x_CanonicalCombiningClass_to_integer_value_mv1(icu4x::capi::CanonicalCombiningClass self);

    typedef struct icu4x_CanonicalCombiningClass_from_integer_value_mv1_result {union {icu4x::capi::CanonicalCombiningClass ok; }; bool is_ok;} icu4x_CanonicalCombiningClass_from_integer_value_mv1_result;
    icu4x_CanonicalCombiningClass_from_integer_value_mv1_result icu4x_CanonicalCombiningClass_from_integer_value_mv1(uint8_t other);

    typedef struct icu4x_CanonicalCombiningClass_try_from_str_mv1_result {union {icu4x::capi::CanonicalCombiningClass ok; }; bool is_ok;} icu4x_CanonicalCombiningClass_try_from_str_mv1_result;
    icu4x_CanonicalCombiningClass_try_from_str_mv1_result icu4x_CanonicalCombiningClass_try_from_str_mv1(icu4x::diplomat::capi::DiplomatStringView s);

    } // extern "C"
} // namespace capi
} // namespace

inline icu4x::capi::CanonicalCombiningClass icu4x::CanonicalCombiningClass::AsFFI() const {
    return static_cast<icu4x::capi::CanonicalCombiningClass>(value);
}

inline icu4x::CanonicalCombiningClass icu4x::CanonicalCombiningClass::FromFFI(icu4x::capi::CanonicalCombiningClass c_enum) {
    switch (c_enum) {
        case icu4x::capi::CanonicalCombiningClass_NotReordered:
        case icu4x::capi::CanonicalCombiningClass_Overlay:
        case icu4x::capi::CanonicalCombiningClass_HanReading:
        case icu4x::capi::CanonicalCombiningClass_Nukta:
        case icu4x::capi::CanonicalCombiningClass_KanaVoicing:
        case icu4x::capi::CanonicalCombiningClass_Virama:
        case icu4x::capi::CanonicalCombiningClass_CCC10:
        case icu4x::capi::CanonicalCombiningClass_CCC11:
        case icu4x::capi::CanonicalCombiningClass_CCC12:
        case icu4x::capi::CanonicalCombiningClass_CCC13:
        case icu4x::capi::CanonicalCombiningClass_CCC14:
        case icu4x::capi::CanonicalCombiningClass_CCC15:
        case icu4x::capi::CanonicalCombiningClass_CCC16:
        case icu4x::capi::CanonicalCombiningClass_CCC17:
        case icu4x::capi::CanonicalCombiningClass_CCC18:
        case icu4x::capi::CanonicalCombiningClass_CCC19:
        case icu4x::capi::CanonicalCombiningClass_CCC20:
        case icu4x::capi::CanonicalCombiningClass_CCC21:
        case icu4x::capi::CanonicalCombiningClass_CCC22:
        case icu4x::capi::CanonicalCombiningClass_CCC23:
        case icu4x::capi::CanonicalCombiningClass_CCC24:
        case icu4x::capi::CanonicalCombiningClass_CCC25:
        case icu4x::capi::CanonicalCombiningClass_CCC26:
        case icu4x::capi::CanonicalCombiningClass_CCC27:
        case icu4x::capi::CanonicalCombiningClass_CCC28:
        case icu4x::capi::CanonicalCombiningClass_CCC29:
        case icu4x::capi::CanonicalCombiningClass_CCC30:
        case icu4x::capi::CanonicalCombiningClass_CCC31:
        case icu4x::capi::CanonicalCombiningClass_CCC32:
        case icu4x::capi::CanonicalCombiningClass_CCC33:
        case icu4x::capi::CanonicalCombiningClass_CCC34:
        case icu4x::capi::CanonicalCombiningClass_CCC35:
        case icu4x::capi::CanonicalCombiningClass_CCC36:
        case icu4x::capi::CanonicalCombiningClass_CCC84:
        case icu4x::capi::CanonicalCombiningClass_CCC91:
        case icu4x::capi::CanonicalCombiningClass_CCC103:
        case icu4x::capi::CanonicalCombiningClass_CCC107:
        case icu4x::capi::CanonicalCombiningClass_CCC118:
        case icu4x::capi::CanonicalCombiningClass_CCC122:
        case icu4x::capi::CanonicalCombiningClass_CCC129:
        case icu4x::capi::CanonicalCombiningClass_CCC130:
        case icu4x::capi::CanonicalCombiningClass_CCC132:
        case icu4x::capi::CanonicalCombiningClass_CCC133:
        case icu4x::capi::CanonicalCombiningClass_AttachedBelowLeft:
        case icu4x::capi::CanonicalCombiningClass_AttachedBelow:
        case icu4x::capi::CanonicalCombiningClass_AttachedAbove:
        case icu4x::capi::CanonicalCombiningClass_AttachedAboveRight:
        case icu4x::capi::CanonicalCombiningClass_BelowLeft:
        case icu4x::capi::CanonicalCombiningClass_Below:
        case icu4x::capi::CanonicalCombiningClass_BelowRight:
        case icu4x::capi::CanonicalCombiningClass_Left:
        case icu4x::capi::CanonicalCombiningClass_Right:
        case icu4x::capi::CanonicalCombiningClass_AboveLeft:
        case icu4x::capi::CanonicalCombiningClass_Above:
        case icu4x::capi::CanonicalCombiningClass_AboveRight:
        case icu4x::capi::CanonicalCombiningClass_DoubleBelow:
        case icu4x::capi::CanonicalCombiningClass_DoubleAbove:
        case icu4x::capi::CanonicalCombiningClass_IotaSubscript:
            return static_cast<icu4x::CanonicalCombiningClass::Value>(c_enum);
        default:
            std::abort();
    }
}

inline icu4x::CanonicalCombiningClass icu4x::CanonicalCombiningClass::for_char(char32_t ch) {
    auto result = icu4x::capi::icu4x_CanonicalCombiningClass_for_char_mv1(ch);
    return icu4x::CanonicalCombiningClass::FromFFI(result);
}

inline std::optional<std::string_view> icu4x::CanonicalCombiningClass::long_name() const {
    auto result = icu4x::capi::icu4x_CanonicalCombiningClass_long_name_mv1(this->AsFFI());
    return result.is_ok ? std::optional<std::string_view>(std::string_view(result.ok.data, result.ok.len)) : std::nullopt;
}

inline std::optional<std::string_view> icu4x::CanonicalCombiningClass::short_name() const {
    auto result = icu4x::capi::icu4x_CanonicalCombiningClass_short_name_mv1(this->AsFFI());
    return result.is_ok ? std::optional<std::string_view>(std::string_view(result.ok.data, result.ok.len)) : std::nullopt;
}

inline uint8_t icu4x::CanonicalCombiningClass::to_integer_value() const {
    auto result = icu4x::capi::icu4x_CanonicalCombiningClass_to_integer_value_mv1(this->AsFFI());
    return result;
}

inline std::optional<icu4x::CanonicalCombiningClass> icu4x::CanonicalCombiningClass::from_integer_value(uint8_t other) {
    auto result = icu4x::capi::icu4x_CanonicalCombiningClass_from_integer_value_mv1(other);
    return result.is_ok ? std::optional<icu4x::CanonicalCombiningClass>(icu4x::CanonicalCombiningClass::FromFFI(result.ok)) : std::nullopt;
}

inline std::optional<icu4x::CanonicalCombiningClass> icu4x::CanonicalCombiningClass::try_from_str(std::string_view s) {
    auto result = icu4x::capi::icu4x_CanonicalCombiningClass_try_from_str_mv1({s.data(), s.size()});
    return result.is_ok ? std::optional<icu4x::CanonicalCombiningClass>(icu4x::CanonicalCombiningClass::FromFFI(result.ok)) : std::nullopt;
}
#endif // ICU4X_CanonicalCombiningClass_HPP

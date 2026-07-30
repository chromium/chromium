#ifndef ICU4X_JoiningGroup_HPP
#define ICU4X_JoiningGroup_HPP

#include "JoiningGroup.d.hpp"

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

    icu4x::capi::JoiningGroup icu4x_JoiningGroup_for_char_mv1(char32_t ch);

    typedef struct icu4x_JoiningGroup_long_name_mv1_result {union {icu4x::diplomat::capi::DiplomatStringView ok; }; bool is_ok;} icu4x_JoiningGroup_long_name_mv1_result;
    icu4x_JoiningGroup_long_name_mv1_result icu4x_JoiningGroup_long_name_mv1(icu4x::capi::JoiningGroup self);

    typedef struct icu4x_JoiningGroup_short_name_mv1_result {union {icu4x::diplomat::capi::DiplomatStringView ok; }; bool is_ok;} icu4x_JoiningGroup_short_name_mv1_result;
    icu4x_JoiningGroup_short_name_mv1_result icu4x_JoiningGroup_short_name_mv1(icu4x::capi::JoiningGroup self);

    uint8_t icu4x_JoiningGroup_to_integer_value_mv1(icu4x::capi::JoiningGroup self);

    typedef struct icu4x_JoiningGroup_from_integer_value_mv1_result {union {icu4x::capi::JoiningGroup ok; }; bool is_ok;} icu4x_JoiningGroup_from_integer_value_mv1_result;
    icu4x_JoiningGroup_from_integer_value_mv1_result icu4x_JoiningGroup_from_integer_value_mv1(uint8_t other);

    typedef struct icu4x_JoiningGroup_try_from_str_mv1_result {union {icu4x::capi::JoiningGroup ok; }; bool is_ok;} icu4x_JoiningGroup_try_from_str_mv1_result;
    icu4x_JoiningGroup_try_from_str_mv1_result icu4x_JoiningGroup_try_from_str_mv1(icu4x::diplomat::capi::DiplomatStringView s);

    } // extern "C"
} // namespace capi
} // namespace

inline icu4x::capi::JoiningGroup icu4x::JoiningGroup::AsFFI() const {
    return static_cast<icu4x::capi::JoiningGroup>(value);
}

inline icu4x::JoiningGroup icu4x::JoiningGroup::FromFFI(icu4x::capi::JoiningGroup c_enum) {
    switch (c_enum) {
        case icu4x::capi::JoiningGroup_NoJoiningGroup:
        case icu4x::capi::JoiningGroup_Ain:
        case icu4x::capi::JoiningGroup_Alaph:
        case icu4x::capi::JoiningGroup_Alef:
        case icu4x::capi::JoiningGroup_Beh:
        case icu4x::capi::JoiningGroup_Beth:
        case icu4x::capi::JoiningGroup_Dal:
        case icu4x::capi::JoiningGroup_DalathRish:
        case icu4x::capi::JoiningGroup_E:
        case icu4x::capi::JoiningGroup_Feh:
        case icu4x::capi::JoiningGroup_FinalSemkath:
        case icu4x::capi::JoiningGroup_Gaf:
        case icu4x::capi::JoiningGroup_Gamal:
        case icu4x::capi::JoiningGroup_Hah:
        case icu4x::capi::JoiningGroup_TehMarbutaGoal:
        case icu4x::capi::JoiningGroup_He:
        case icu4x::capi::JoiningGroup_Heh:
        case icu4x::capi::JoiningGroup_HehGoal:
        case icu4x::capi::JoiningGroup_Heth:
        case icu4x::capi::JoiningGroup_Kaf:
        case icu4x::capi::JoiningGroup_Kaph:
        case icu4x::capi::JoiningGroup_KnottedHeh:
        case icu4x::capi::JoiningGroup_Lam:
        case icu4x::capi::JoiningGroup_Lamadh:
        case icu4x::capi::JoiningGroup_Meem:
        case icu4x::capi::JoiningGroup_Mim:
        case icu4x::capi::JoiningGroup_Noon:
        case icu4x::capi::JoiningGroup_Nun:
        case icu4x::capi::JoiningGroup_Pe:
        case icu4x::capi::JoiningGroup_Qaf:
        case icu4x::capi::JoiningGroup_Qaph:
        case icu4x::capi::JoiningGroup_Reh:
        case icu4x::capi::JoiningGroup_ReversedPe:
        case icu4x::capi::JoiningGroup_Sad:
        case icu4x::capi::JoiningGroup_Sadhe:
        case icu4x::capi::JoiningGroup_Seen:
        case icu4x::capi::JoiningGroup_Semkath:
        case icu4x::capi::JoiningGroup_Shin:
        case icu4x::capi::JoiningGroup_SwashKaf:
        case icu4x::capi::JoiningGroup_SyriacWaw:
        case icu4x::capi::JoiningGroup_Tah:
        case icu4x::capi::JoiningGroup_Taw:
        case icu4x::capi::JoiningGroup_TehMarbuta:
        case icu4x::capi::JoiningGroup_Teth:
        case icu4x::capi::JoiningGroup_Waw:
        case icu4x::capi::JoiningGroup_Yeh:
        case icu4x::capi::JoiningGroup_YehBarree:
        case icu4x::capi::JoiningGroup_YehWithTail:
        case icu4x::capi::JoiningGroup_Yudh:
        case icu4x::capi::JoiningGroup_YudhHe:
        case icu4x::capi::JoiningGroup_Zain:
        case icu4x::capi::JoiningGroup_Fe:
        case icu4x::capi::JoiningGroup_Khaph:
        case icu4x::capi::JoiningGroup_Zhain:
        case icu4x::capi::JoiningGroup_BurushaskiYehBarree:
        case icu4x::capi::JoiningGroup_FarsiYeh:
        case icu4x::capi::JoiningGroup_Nya:
        case icu4x::capi::JoiningGroup_RohingyaYeh:
        case icu4x::capi::JoiningGroup_ManichaeanAleph:
        case icu4x::capi::JoiningGroup_ManichaeanAyin:
        case icu4x::capi::JoiningGroup_ManichaeanBeth:
        case icu4x::capi::JoiningGroup_ManichaeanDaleth:
        case icu4x::capi::JoiningGroup_ManichaeanDhamedh:
        case icu4x::capi::JoiningGroup_ManichaeanFive:
        case icu4x::capi::JoiningGroup_ManichaeanGimel:
        case icu4x::capi::JoiningGroup_ManichaeanHeth:
        case icu4x::capi::JoiningGroup_ManichaeanHundred:
        case icu4x::capi::JoiningGroup_ManichaeanKaph:
        case icu4x::capi::JoiningGroup_ManichaeanLamedh:
        case icu4x::capi::JoiningGroup_ManichaeanMem:
        case icu4x::capi::JoiningGroup_ManichaeanNun:
        case icu4x::capi::JoiningGroup_ManichaeanOne:
        case icu4x::capi::JoiningGroup_ManichaeanPe:
        case icu4x::capi::JoiningGroup_ManichaeanQoph:
        case icu4x::capi::JoiningGroup_ManichaeanResh:
        case icu4x::capi::JoiningGroup_ManichaeanSadhe:
        case icu4x::capi::JoiningGroup_ManichaeanSamekh:
        case icu4x::capi::JoiningGroup_ManichaeanTaw:
        case icu4x::capi::JoiningGroup_ManichaeanTen:
        case icu4x::capi::JoiningGroup_ManichaeanTeth:
        case icu4x::capi::JoiningGroup_ManichaeanThamedh:
        case icu4x::capi::JoiningGroup_ManichaeanTwenty:
        case icu4x::capi::JoiningGroup_ManichaeanWaw:
        case icu4x::capi::JoiningGroup_ManichaeanYodh:
        case icu4x::capi::JoiningGroup_ManichaeanZayin:
        case icu4x::capi::JoiningGroup_StraightWaw:
        case icu4x::capi::JoiningGroup_AfricanFeh:
        case icu4x::capi::JoiningGroup_AfricanNoon:
        case icu4x::capi::JoiningGroup_AfricanQaf:
        case icu4x::capi::JoiningGroup_MalayalamBha:
        case icu4x::capi::JoiningGroup_MalayalamJa:
        case icu4x::capi::JoiningGroup_MalayalamLla:
        case icu4x::capi::JoiningGroup_MalayalamLlla:
        case icu4x::capi::JoiningGroup_MalayalamNga:
        case icu4x::capi::JoiningGroup_MalayalamNna:
        case icu4x::capi::JoiningGroup_MalayalamNnna:
        case icu4x::capi::JoiningGroup_MalayalamNya:
        case icu4x::capi::JoiningGroup_MalayalamRa:
        case icu4x::capi::JoiningGroup_MalayalamSsa:
        case icu4x::capi::JoiningGroup_MalayalamTta:
        case icu4x::capi::JoiningGroup_HanifiRohingyaKinnaYa:
        case icu4x::capi::JoiningGroup_HanifiRohingyaPa:
        case icu4x::capi::JoiningGroup_ThinYeh:
        case icu4x::capi::JoiningGroup_VerticalTail:
        case icu4x::capi::JoiningGroup_KashmiriYeh:
        case icu4x::capi::JoiningGroup_ThinNoon:
            return static_cast<icu4x::JoiningGroup::Value>(c_enum);
        default:
            std::abort();
    }
}

inline icu4x::JoiningGroup icu4x::JoiningGroup::for_char(char32_t ch) {
    auto result = icu4x::capi::icu4x_JoiningGroup_for_char_mv1(ch);
    return icu4x::JoiningGroup::FromFFI(result);
}

inline std::optional<std::string_view> icu4x::JoiningGroup::long_name() const {
    auto result = icu4x::capi::icu4x_JoiningGroup_long_name_mv1(this->AsFFI());
    return result.is_ok ? std::optional<std::string_view>(std::string_view(result.ok.data, result.ok.len)) : std::nullopt;
}

inline std::optional<std::string_view> icu4x::JoiningGroup::short_name() const {
    auto result = icu4x::capi::icu4x_JoiningGroup_short_name_mv1(this->AsFFI());
    return result.is_ok ? std::optional<std::string_view>(std::string_view(result.ok.data, result.ok.len)) : std::nullopt;
}

inline uint8_t icu4x::JoiningGroup::to_integer_value() const {
    auto result = icu4x::capi::icu4x_JoiningGroup_to_integer_value_mv1(this->AsFFI());
    return result;
}

inline std::optional<icu4x::JoiningGroup> icu4x::JoiningGroup::from_integer_value(uint8_t other) {
    auto result = icu4x::capi::icu4x_JoiningGroup_from_integer_value_mv1(other);
    return result.is_ok ? std::optional<icu4x::JoiningGroup>(icu4x::JoiningGroup::FromFFI(result.ok)) : std::nullopt;
}

inline std::optional<icu4x::JoiningGroup> icu4x::JoiningGroup::try_from_str(std::string_view s) {
    auto result = icu4x::capi::icu4x_JoiningGroup_try_from_str_mv1({s.data(), s.size()});
    return result.is_ok ? std::optional<icu4x::JoiningGroup>(icu4x::JoiningGroup::FromFFI(result.ok)) : std::nullopt;
}
#endif // ICU4X_JoiningGroup_HPP

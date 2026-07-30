#ifndef ICU4X_Script_HPP
#define ICU4X_Script_HPP

#include "Script.d.hpp"

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

    icu4x::capi::Script icu4x_Script_for_char_mv1(char32_t ch);

    typedef struct icu4x_Script_long_name_mv1_result {union {icu4x::diplomat::capi::DiplomatStringView ok; }; bool is_ok;} icu4x_Script_long_name_mv1_result;
    icu4x_Script_long_name_mv1_result icu4x_Script_long_name_mv1(icu4x::capi::Script self);

    typedef struct icu4x_Script_short_name_mv1_result {union {icu4x::diplomat::capi::DiplomatStringView ok; }; bool is_ok;} icu4x_Script_short_name_mv1_result;
    icu4x_Script_short_name_mv1_result icu4x_Script_short_name_mv1(icu4x::capi::Script self);

    uint16_t icu4x_Script_to_integer_value_mv1(icu4x::capi::Script self);

    typedef struct icu4x_Script_from_integer_value_mv1_result {union {icu4x::capi::Script ok; }; bool is_ok;} icu4x_Script_from_integer_value_mv1_result;
    icu4x_Script_from_integer_value_mv1_result icu4x_Script_from_integer_value_mv1(uint16_t other);

    typedef struct icu4x_Script_try_from_str_mv1_result {union {icu4x::capi::Script ok; }; bool is_ok;} icu4x_Script_try_from_str_mv1_result;
    icu4x_Script_try_from_str_mv1_result icu4x_Script_try_from_str_mv1(icu4x::diplomat::capi::DiplomatStringView s);

    } // extern "C"
} // namespace capi
} // namespace

inline icu4x::capi::Script icu4x::Script::AsFFI() const {
    return static_cast<icu4x::capi::Script>(value);
}

inline icu4x::Script icu4x::Script::FromFFI(icu4x::capi::Script c_enum) {
    switch (c_enum) {
        case icu4x::capi::Script_Adlam:
        case icu4x::capi::Script_Ahom:
        case icu4x::capi::Script_AnatolianHieroglyphs:
        case icu4x::capi::Script_Arabic:
        case icu4x::capi::Script_Armenian:
        case icu4x::capi::Script_Avestan:
        case icu4x::capi::Script_Balinese:
        case icu4x::capi::Script_Bamum:
        case icu4x::capi::Script_BassaVah:
        case icu4x::capi::Script_Batak:
        case icu4x::capi::Script_Bengali:
        case icu4x::capi::Script_BeriaErfe:
        case icu4x::capi::Script_Bhaiksuki:
        case icu4x::capi::Script_Bopomofo:
        case icu4x::capi::Script_Brahmi:
        case icu4x::capi::Script_Braille:
        case icu4x::capi::Script_Buginese:
        case icu4x::capi::Script_Buhid:
        case icu4x::capi::Script_CanadianAboriginal:
        case icu4x::capi::Script_Carian:
        case icu4x::capi::Script_CaucasianAlbanian:
        case icu4x::capi::Script_Chakma:
        case icu4x::capi::Script_Cham:
        case icu4x::capi::Script_Cherokee:
        case icu4x::capi::Script_Chisoi:
        case icu4x::capi::Script_Chorasmian:
        case icu4x::capi::Script_Common:
        case icu4x::capi::Script_Coptic:
        case icu4x::capi::Script_Cuneiform:
        case icu4x::capi::Script_Cypriot:
        case icu4x::capi::Script_CyproMinoan:
        case icu4x::capi::Script_Cyrillic:
        case icu4x::capi::Script_Deseret:
        case icu4x::capi::Script_Devanagari:
        case icu4x::capi::Script_DivesAkuru:
        case icu4x::capi::Script_Dogra:
        case icu4x::capi::Script_Duployan:
        case icu4x::capi::Script_EgyptianHieroglyphs:
        case icu4x::capi::Script_Elbasan:
        case icu4x::capi::Script_Elymaic:
        case icu4x::capi::Script_Ethiopian:
        case icu4x::capi::Script_Garay:
        case icu4x::capi::Script_Georgian:
        case icu4x::capi::Script_Glagolitic:
        case icu4x::capi::Script_Gothic:
        case icu4x::capi::Script_Grantha:
        case icu4x::capi::Script_Greek:
        case icu4x::capi::Script_Gujarati:
        case icu4x::capi::Script_GunjalaGondi:
        case icu4x::capi::Script_Gurmukhi:
        case icu4x::capi::Script_GurungKhema:
        case icu4x::capi::Script_Han:
        case icu4x::capi::Script_Hangul:
        case icu4x::capi::Script_HanifiRohingya:
        case icu4x::capi::Script_Hanunoo:
        case icu4x::capi::Script_Hatran:
        case icu4x::capi::Script_Hebrew:
        case icu4x::capi::Script_Hiragana:
        case icu4x::capi::Script_ImperialAramaic:
        case icu4x::capi::Script_Inherited:
        case icu4x::capi::Script_InscriptionalPahlavi:
        case icu4x::capi::Script_InscriptionalParthian:
        case icu4x::capi::Script_Javanese:
        case icu4x::capi::Script_Kaithi:
        case icu4x::capi::Script_Kannada:
        case icu4x::capi::Script_Katakana:
        case icu4x::capi::Script_Kawi:
        case icu4x::capi::Script_KayahLi:
        case icu4x::capi::Script_Kharoshthi:
        case icu4x::capi::Script_KhitanSmallScript:
        case icu4x::capi::Script_Khmer:
        case icu4x::capi::Script_Khojki:
        case icu4x::capi::Script_Khudawadi:
        case icu4x::capi::Script_KiratRai:
        case icu4x::capi::Script_Lao:
        case icu4x::capi::Script_Latin:
        case icu4x::capi::Script_Lepcha:
        case icu4x::capi::Script_Limbu:
        case icu4x::capi::Script_LinearA:
        case icu4x::capi::Script_LinearB:
        case icu4x::capi::Script_Lisu:
        case icu4x::capi::Script_Lycian:
        case icu4x::capi::Script_Lydian:
        case icu4x::capi::Script_Mahajani:
        case icu4x::capi::Script_Makasar:
        case icu4x::capi::Script_Malayalam:
        case icu4x::capi::Script_Mandaic:
        case icu4x::capi::Script_Manichaean:
        case icu4x::capi::Script_Marchen:
        case icu4x::capi::Script_MasaramGondi:
        case icu4x::capi::Script_Medefaidrin:
        case icu4x::capi::Script_MeeteiMayek:
        case icu4x::capi::Script_MendeKikakui:
        case icu4x::capi::Script_MeroiticCursive:
        case icu4x::capi::Script_MeroiticHieroglyphs:
        case icu4x::capi::Script_Miao:
        case icu4x::capi::Script_Modi:
        case icu4x::capi::Script_Mongolian:
        case icu4x::capi::Script_Mro:
        case icu4x::capi::Script_Multani:
        case icu4x::capi::Script_Myanmar:
        case icu4x::capi::Script_Nabataean:
        case icu4x::capi::Script_NagMundari:
        case icu4x::capi::Script_Nandinagari:
        case icu4x::capi::Script_Nastaliq:
        case icu4x::capi::Script_NewTaiLue:
        case icu4x::capi::Script_Newa:
        case icu4x::capi::Script_Nko:
        case icu4x::capi::Script_Nushu:
        case icu4x::capi::Script_NyiakengPuachueHmong:
        case icu4x::capi::Script_Ogham:
        case icu4x::capi::Script_OlChiki:
        case icu4x::capi::Script_OlOnal:
        case icu4x::capi::Script_OldHungarian:
        case icu4x::capi::Script_OldItalic:
        case icu4x::capi::Script_OldNorthArabian:
        case icu4x::capi::Script_OldPermic:
        case icu4x::capi::Script_OldPersian:
        case icu4x::capi::Script_OldSogdian:
        case icu4x::capi::Script_OldSouthArabian:
        case icu4x::capi::Script_OldTurkic:
        case icu4x::capi::Script_OldUyghur:
        case icu4x::capi::Script_Oriya:
        case icu4x::capi::Script_Osage:
        case icu4x::capi::Script_Osmanya:
        case icu4x::capi::Script_PahawhHmong:
        case icu4x::capi::Script_Palmyrene:
        case icu4x::capi::Script_PauCinHau:
        case icu4x::capi::Script_PhagsPa:
        case icu4x::capi::Script_Phoenician:
        case icu4x::capi::Script_PsalterPahlavi:
        case icu4x::capi::Script_Rejang:
        case icu4x::capi::Script_Runic:
        case icu4x::capi::Script_Samaritan:
        case icu4x::capi::Script_Saurashtra:
        case icu4x::capi::Script_Sharada:
        case icu4x::capi::Script_Shavian:
        case icu4x::capi::Script_Siddham:
        case icu4x::capi::Script_Sidetic:
        case icu4x::capi::Script_SignWriting:
        case icu4x::capi::Script_Sinhala:
        case icu4x::capi::Script_Sogdian:
        case icu4x::capi::Script_SoraSompeng:
        case icu4x::capi::Script_Soyombo:
        case icu4x::capi::Script_Sundanese:
        case icu4x::capi::Script_Sunuwar:
        case icu4x::capi::Script_SylotiNagri:
        case icu4x::capi::Script_Syriac:
        case icu4x::capi::Script_Tagalog:
        case icu4x::capi::Script_Tagbanwa:
        case icu4x::capi::Script_TaiLe:
        case icu4x::capi::Script_TaiTham:
        case icu4x::capi::Script_TaiViet:
        case icu4x::capi::Script_TaiYo:
        case icu4x::capi::Script_Takri:
        case icu4x::capi::Script_Tamil:
        case icu4x::capi::Script_Tangsa:
        case icu4x::capi::Script_Tangut:
        case icu4x::capi::Script_Telugu:
        case icu4x::capi::Script_Thaana:
        case icu4x::capi::Script_Thai:
        case icu4x::capi::Script_Tibetan:
        case icu4x::capi::Script_Tifinagh:
        case icu4x::capi::Script_Tirhuta:
        case icu4x::capi::Script_Todhri:
        case icu4x::capi::Script_TolongSiki:
        case icu4x::capi::Script_Toto:
        case icu4x::capi::Script_TuluTigalari:
        case icu4x::capi::Script_Ugaritic:
        case icu4x::capi::Script_Unknown:
        case icu4x::capi::Script_Vai:
        case icu4x::capi::Script_Vithkuqi:
        case icu4x::capi::Script_Wancho:
        case icu4x::capi::Script_WarangCiti:
        case icu4x::capi::Script_Yezidi:
        case icu4x::capi::Script_Yi:
        case icu4x::capi::Script_ZanabazarSquare:
            return static_cast<icu4x::Script::Value>(c_enum);
        default:
            std::abort();
    }
}

inline icu4x::Script icu4x::Script::for_char(char32_t ch) {
    auto result = icu4x::capi::icu4x_Script_for_char_mv1(ch);
    return icu4x::Script::FromFFI(result);
}

inline std::optional<std::string_view> icu4x::Script::long_name() const {
    auto result = icu4x::capi::icu4x_Script_long_name_mv1(this->AsFFI());
    return result.is_ok ? std::optional<std::string_view>(std::string_view(result.ok.data, result.ok.len)) : std::nullopt;
}

inline std::optional<std::string_view> icu4x::Script::short_name() const {
    auto result = icu4x::capi::icu4x_Script_short_name_mv1(this->AsFFI());
    return result.is_ok ? std::optional<std::string_view>(std::string_view(result.ok.data, result.ok.len)) : std::nullopt;
}

inline uint16_t icu4x::Script::to_integer_value() const {
    auto result = icu4x::capi::icu4x_Script_to_integer_value_mv1(this->AsFFI());
    return result;
}

inline std::optional<icu4x::Script> icu4x::Script::from_integer_value(uint16_t other) {
    auto result = icu4x::capi::icu4x_Script_from_integer_value_mv1(other);
    return result.is_ok ? std::optional<icu4x::Script>(icu4x::Script::FromFFI(result.ok)) : std::nullopt;
}

inline std::optional<icu4x::Script> icu4x::Script::try_from_str(std::string_view s) {
    auto result = icu4x::capi::icu4x_Script_try_from_str_mv1({s.data(), s.size()});
    return result.is_ok ? std::optional<icu4x::Script>(icu4x::Script::FromFFI(result.ok)) : std::nullopt;
}
#endif // ICU4X_Script_HPP

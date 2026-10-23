#ifndef ICU4X_Block_HPP
#define ICU4X_Block_HPP

#include "Block.d.hpp"

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

    icu4x::capi::Block icu4x_Block_for_char_mv1(char32_t ch);

    typedef struct icu4x_Block_long_name_mv1_result {union {icu4x::diplomat::capi::DiplomatStringView ok; }; bool is_ok;} icu4x_Block_long_name_mv1_result;
    icu4x_Block_long_name_mv1_result icu4x_Block_long_name_mv1(icu4x::capi::Block self);

    typedef struct icu4x_Block_short_name_mv1_result {union {icu4x::diplomat::capi::DiplomatStringView ok; }; bool is_ok;} icu4x_Block_short_name_mv1_result;
    icu4x_Block_short_name_mv1_result icu4x_Block_short_name_mv1(icu4x::capi::Block self);

    uint16_t icu4x_Block_to_integer_value_mv1(icu4x::capi::Block self);

    typedef struct icu4x_Block_from_integer_value_mv1_result {union {icu4x::capi::Block ok; }; bool is_ok;} icu4x_Block_from_integer_value_mv1_result;
    icu4x_Block_from_integer_value_mv1_result icu4x_Block_from_integer_value_mv1(uint16_t other);

    typedef struct icu4x_Block_try_from_str_mv1_result {union {icu4x::capi::Block ok; }; bool is_ok;} icu4x_Block_try_from_str_mv1_result;
    icu4x_Block_try_from_str_mv1_result icu4x_Block_try_from_str_mv1(icu4x::diplomat::capi::DiplomatStringView s);

    } // extern "C"
} // namespace capi
} // namespace

inline icu4x::capi::Block icu4x::Block::AsFFI() const {
    return static_cast<icu4x::capi::Block>(value);
}

inline icu4x::Block icu4x::Block::FromFFI(icu4x::capi::Block c_enum) {
    switch (c_enum) {
        case icu4x::capi::Block_NoBlock:
        case icu4x::capi::Block_BasicLatin:
        case icu4x::capi::Block_Latin1Supplement:
        case icu4x::capi::Block_LatinExtendedA:
        case icu4x::capi::Block_LatinExtendedB:
        case icu4x::capi::Block_IPAExtensions:
        case icu4x::capi::Block_SpacingModifierLetters:
        case icu4x::capi::Block_CombiningDiacriticalMarks:
        case icu4x::capi::Block_GreekAndCoptic:
        case icu4x::capi::Block_Cyrillic:
        case icu4x::capi::Block_Armenian:
        case icu4x::capi::Block_Hebrew:
        case icu4x::capi::Block_Arabic:
        case icu4x::capi::Block_Syriac:
        case icu4x::capi::Block_Thaana:
        case icu4x::capi::Block_Devanagari:
        case icu4x::capi::Block_Bengali:
        case icu4x::capi::Block_Gurmukhi:
        case icu4x::capi::Block_Gujarati:
        case icu4x::capi::Block_Oriya:
        case icu4x::capi::Block_Tamil:
        case icu4x::capi::Block_Telugu:
        case icu4x::capi::Block_Kannada:
        case icu4x::capi::Block_Malayalam:
        case icu4x::capi::Block_Sinhala:
        case icu4x::capi::Block_Thai:
        case icu4x::capi::Block_Lao:
        case icu4x::capi::Block_Tibetan:
        case icu4x::capi::Block_Myanmar:
        case icu4x::capi::Block_Georgian:
        case icu4x::capi::Block_HangulJamo:
        case icu4x::capi::Block_Ethiopic:
        case icu4x::capi::Block_Cherokee:
        case icu4x::capi::Block_UnifiedCanadianAboriginalSyllabics:
        case icu4x::capi::Block_Ogham:
        case icu4x::capi::Block_Runic:
        case icu4x::capi::Block_Khmer:
        case icu4x::capi::Block_Mongolian:
        case icu4x::capi::Block_LatinExtendedAdditional:
        case icu4x::capi::Block_GreekExtended:
        case icu4x::capi::Block_GeneralPunctuation:
        case icu4x::capi::Block_SuperscriptsAndSubscripts:
        case icu4x::capi::Block_CurrencySymbols:
        case icu4x::capi::Block_CombiningDiacriticalMarksForSymbols:
        case icu4x::capi::Block_LetterlikeSymbols:
        case icu4x::capi::Block_NumberForms:
        case icu4x::capi::Block_Arrows:
        case icu4x::capi::Block_MathematicalOperators:
        case icu4x::capi::Block_MiscellaneousTechnical:
        case icu4x::capi::Block_ControlPictures:
        case icu4x::capi::Block_OpticalCharacterRecognition:
        case icu4x::capi::Block_EnclosedAlphanumerics:
        case icu4x::capi::Block_BoxDrawing:
        case icu4x::capi::Block_BlockElements:
        case icu4x::capi::Block_GeometricShapes:
        case icu4x::capi::Block_MiscellaneousSymbols:
        case icu4x::capi::Block_Dingbats:
        case icu4x::capi::Block_BraillePatterns:
        case icu4x::capi::Block_CJKRadicalsSupplement:
        case icu4x::capi::Block_KangxiRadicals:
        case icu4x::capi::Block_IdeographicDescriptionCharacters:
        case icu4x::capi::Block_CJKSymbolsAndPunctuation:
        case icu4x::capi::Block_Hiragana:
        case icu4x::capi::Block_Katakana:
        case icu4x::capi::Block_Bopomofo:
        case icu4x::capi::Block_HangulCompatibilityJamo:
        case icu4x::capi::Block_Kanbun:
        case icu4x::capi::Block_BopomofoExtended:
        case icu4x::capi::Block_EnclosedCJKLettersAndMonths:
        case icu4x::capi::Block_CJKCompatibility:
        case icu4x::capi::Block_CJKUnifiedIdeographsExtensionA:
        case icu4x::capi::Block_CJKUnifiedIdeographs:
        case icu4x::capi::Block_YiSyllables:
        case icu4x::capi::Block_YiRadicals:
        case icu4x::capi::Block_HangulSyllables:
        case icu4x::capi::Block_HighSurrogates:
        case icu4x::capi::Block_HighPrivateUseSurrogates:
        case icu4x::capi::Block_LowSurrogates:
        case icu4x::capi::Block_PrivateUseArea:
        case icu4x::capi::Block_CJKCompatibilityIdeographs:
        case icu4x::capi::Block_AlphabeticPresentationForms:
        case icu4x::capi::Block_ArabicPresentationFormsA:
        case icu4x::capi::Block_CombiningHalfMarks:
        case icu4x::capi::Block_CJKCompatibilityForms:
        case icu4x::capi::Block_SmallFormVariants:
        case icu4x::capi::Block_ArabicPresentationFormsB:
        case icu4x::capi::Block_Specials:
        case icu4x::capi::Block_HalfwidthAndFullwidthForms:
        case icu4x::capi::Block_OldItalic:
        case icu4x::capi::Block_Gothic:
        case icu4x::capi::Block_Deseret:
        case icu4x::capi::Block_ByzantineMusicalSymbols:
        case icu4x::capi::Block_MusicalSymbols:
        case icu4x::capi::Block_MathematicalAlphanumericSymbols:
        case icu4x::capi::Block_CJKUnifiedIdeographsExtensionB:
        case icu4x::capi::Block_CJKCompatibilityIdeographsSupplement:
        case icu4x::capi::Block_Tags:
        case icu4x::capi::Block_CyrillicSupplement:
        case icu4x::capi::Block_Tagalog:
        case icu4x::capi::Block_Hanunoo:
        case icu4x::capi::Block_Buhid:
        case icu4x::capi::Block_Tagbanwa:
        case icu4x::capi::Block_MiscellaneousMathematicalSymbolsA:
        case icu4x::capi::Block_SupplementalArrowsA:
        case icu4x::capi::Block_SupplementalArrowsB:
        case icu4x::capi::Block_MiscellaneousMathematicalSymbolsB:
        case icu4x::capi::Block_SupplementalMathematicalOperators:
        case icu4x::capi::Block_KatakanaPhoneticExtensions:
        case icu4x::capi::Block_VariationSelectors:
        case icu4x::capi::Block_SupplementaryPrivateUseAreaA:
        case icu4x::capi::Block_SupplementaryPrivateUseAreaB:
        case icu4x::capi::Block_Limbu:
        case icu4x::capi::Block_TaiLe:
        case icu4x::capi::Block_KhmerSymbols:
        case icu4x::capi::Block_PhoneticExtensions:
        case icu4x::capi::Block_MiscellaneousSymbolsAndArrows:
        case icu4x::capi::Block_YijingHexagramSymbols:
        case icu4x::capi::Block_LinearBSyllabary:
        case icu4x::capi::Block_LinearBIdeograms:
        case icu4x::capi::Block_AegeanNumbers:
        case icu4x::capi::Block_Ugaritic:
        case icu4x::capi::Block_Shavian:
        case icu4x::capi::Block_Osmanya:
        case icu4x::capi::Block_CypriotSyllabary:
        case icu4x::capi::Block_TaiXuanJingSymbols:
        case icu4x::capi::Block_VariationSelectorsSupplement:
        case icu4x::capi::Block_AncientGreekMusicalNotation:
        case icu4x::capi::Block_AncientGreekNumbers:
        case icu4x::capi::Block_ArabicSupplement:
        case icu4x::capi::Block_Buginese:
        case icu4x::capi::Block_CJKStrokes:
        case icu4x::capi::Block_CombiningDiacriticalMarksSupplement:
        case icu4x::capi::Block_Coptic:
        case icu4x::capi::Block_EthiopicExtended:
        case icu4x::capi::Block_EthiopicSupplement:
        case icu4x::capi::Block_GeorgianSupplement:
        case icu4x::capi::Block_Glagolitic:
        case icu4x::capi::Block_Kharoshthi:
        case icu4x::capi::Block_ModifierToneLetters:
        case icu4x::capi::Block_NewTaiLue:
        case icu4x::capi::Block_OldPersian:
        case icu4x::capi::Block_PhoneticExtensionsSupplement:
        case icu4x::capi::Block_SupplementalPunctuation:
        case icu4x::capi::Block_SylotiNagri:
        case icu4x::capi::Block_Tifinagh:
        case icu4x::capi::Block_VerticalForms:
        case icu4x::capi::Block_NKo:
        case icu4x::capi::Block_Balinese:
        case icu4x::capi::Block_LatinExtendedC:
        case icu4x::capi::Block_LatinExtendedD:
        case icu4x::capi::Block_PhagsPa:
        case icu4x::capi::Block_Phoenician:
        case icu4x::capi::Block_Cuneiform:
        case icu4x::capi::Block_CuneiformNumbersAndPunctuation:
        case icu4x::capi::Block_CountingRodNumerals:
        case icu4x::capi::Block_Sundanese:
        case icu4x::capi::Block_Lepcha:
        case icu4x::capi::Block_OlChiki:
        case icu4x::capi::Block_CyrillicExtendedA:
        case icu4x::capi::Block_Vai:
        case icu4x::capi::Block_CyrillicExtendedB:
        case icu4x::capi::Block_Saurashtra:
        case icu4x::capi::Block_KayahLi:
        case icu4x::capi::Block_Rejang:
        case icu4x::capi::Block_Cham:
        case icu4x::capi::Block_AncientSymbols:
        case icu4x::capi::Block_PhaistosDisc:
        case icu4x::capi::Block_Lycian:
        case icu4x::capi::Block_Carian:
        case icu4x::capi::Block_Lydian:
        case icu4x::capi::Block_MahjongTiles:
        case icu4x::capi::Block_DominoTiles:
        case icu4x::capi::Block_Samaritan:
        case icu4x::capi::Block_UnifiedCanadianAboriginalSyllabicsExtended:
        case icu4x::capi::Block_TaiTham:
        case icu4x::capi::Block_VedicExtensions:
        case icu4x::capi::Block_Lisu:
        case icu4x::capi::Block_Bamum:
        case icu4x::capi::Block_CommonIndicNumberForms:
        case icu4x::capi::Block_DevanagariExtended:
        case icu4x::capi::Block_HangulJamoExtendedA:
        case icu4x::capi::Block_Javanese:
        case icu4x::capi::Block_MyanmarExtendedA:
        case icu4x::capi::Block_TaiViet:
        case icu4x::capi::Block_MeeteiMayek:
        case icu4x::capi::Block_HangulJamoExtendedB:
        case icu4x::capi::Block_ImperialAramaic:
        case icu4x::capi::Block_OldSouthArabian:
        case icu4x::capi::Block_Avestan:
        case icu4x::capi::Block_InscriptionalParthian:
        case icu4x::capi::Block_InscriptionalPahlavi:
        case icu4x::capi::Block_OldTurkic:
        case icu4x::capi::Block_RumiNumeralSymbols:
        case icu4x::capi::Block_Kaithi:
        case icu4x::capi::Block_EgyptianHieroglyphs:
        case icu4x::capi::Block_EnclosedAlphanumericSupplement:
        case icu4x::capi::Block_EnclosedIdeographicSupplement:
        case icu4x::capi::Block_CJKUnifiedIdeographsExtensionC:
        case icu4x::capi::Block_Mandaic:
        case icu4x::capi::Block_Batak:
        case icu4x::capi::Block_EthiopicExtendedA:
        case icu4x::capi::Block_Brahmi:
        case icu4x::capi::Block_BamumSupplement:
        case icu4x::capi::Block_KanaSupplement:
        case icu4x::capi::Block_PlayingCards:
        case icu4x::capi::Block_MiscellaneousSymbolsAndPictographs:
        case icu4x::capi::Block_Emoticons:
        case icu4x::capi::Block_TransportAndMapSymbols:
        case icu4x::capi::Block_AlchemicalSymbols:
        case icu4x::capi::Block_CJKUnifiedIdeographsExtensionD:
        case icu4x::capi::Block_ArabicExtendedA:
        case icu4x::capi::Block_ArabicMathematicalAlphabeticSymbols:
        case icu4x::capi::Block_Chakma:
        case icu4x::capi::Block_MeeteiMayekExtensions:
        case icu4x::capi::Block_MeroiticCursive:
        case icu4x::capi::Block_MeroiticHieroglyphs:
        case icu4x::capi::Block_Miao:
        case icu4x::capi::Block_Sharada:
        case icu4x::capi::Block_SoraSompeng:
        case icu4x::capi::Block_SundaneseSupplement:
        case icu4x::capi::Block_Takri:
        case icu4x::capi::Block_BassaVah:
        case icu4x::capi::Block_CaucasianAlbanian:
        case icu4x::capi::Block_CopticEpactNumbers:
        case icu4x::capi::Block_CombiningDiacriticalMarksExtended:
        case icu4x::capi::Block_Duployan:
        case icu4x::capi::Block_Elbasan:
        case icu4x::capi::Block_GeometricShapesExtended:
        case icu4x::capi::Block_Grantha:
        case icu4x::capi::Block_Khojki:
        case icu4x::capi::Block_Khudawadi:
        case icu4x::capi::Block_LatinExtendedE:
        case icu4x::capi::Block_LinearA:
        case icu4x::capi::Block_Mahajani:
        case icu4x::capi::Block_Manichaean:
        case icu4x::capi::Block_MendeKikakui:
        case icu4x::capi::Block_Modi:
        case icu4x::capi::Block_Mro:
        case icu4x::capi::Block_MyanmarExtendedB:
        case icu4x::capi::Block_Nabataean:
        case icu4x::capi::Block_OldNorthArabian:
        case icu4x::capi::Block_OldPermic:
        case icu4x::capi::Block_OrnamentalDingbats:
        case icu4x::capi::Block_PahawhHmong:
        case icu4x::capi::Block_Palmyrene:
        case icu4x::capi::Block_PauCinHau:
        case icu4x::capi::Block_PsalterPahlavi:
        case icu4x::capi::Block_ShorthandFormatControls:
        case icu4x::capi::Block_Siddham:
        case icu4x::capi::Block_SinhalaArchaicNumbers:
        case icu4x::capi::Block_SupplementalArrowsC:
        case icu4x::capi::Block_Tirhuta:
        case icu4x::capi::Block_WarangCiti:
        case icu4x::capi::Block_Ahom:
        case icu4x::capi::Block_AnatolianHieroglyphs:
        case icu4x::capi::Block_CherokeeSupplement:
        case icu4x::capi::Block_CJKUnifiedIdeographsExtensionE:
        case icu4x::capi::Block_EarlyDynasticCuneiform:
        case icu4x::capi::Block_Hatran:
        case icu4x::capi::Block_Multani:
        case icu4x::capi::Block_OldHungarian:
        case icu4x::capi::Block_SupplementalSymbolsAndPictographs:
        case icu4x::capi::Block_SuttonSignWriting:
        case icu4x::capi::Block_Adlam:
        case icu4x::capi::Block_Bhaiksuki:
        case icu4x::capi::Block_CyrillicExtendedC:
        case icu4x::capi::Block_GlagoliticSupplement:
        case icu4x::capi::Block_IdeographicSymbolsAndPunctuation:
        case icu4x::capi::Block_Marchen:
        case icu4x::capi::Block_MongolianSupplement:
        case icu4x::capi::Block_Newa:
        case icu4x::capi::Block_Osage:
        case icu4x::capi::Block_Tangut:
        case icu4x::capi::Block_TangutComponents:
        case icu4x::capi::Block_CJKUnifiedIdeographsExtensionF:
        case icu4x::capi::Block_KanaExtendedA:
        case icu4x::capi::Block_MasaramGondi:
        case icu4x::capi::Block_Nushu:
        case icu4x::capi::Block_Soyombo:
        case icu4x::capi::Block_SyriacSupplement:
        case icu4x::capi::Block_ZanabazarSquare:
        case icu4x::capi::Block_ChessSymbols:
        case icu4x::capi::Block_Dogra:
        case icu4x::capi::Block_GeorgianExtended:
        case icu4x::capi::Block_GunjalaGondi:
        case icu4x::capi::Block_HanifiRohingya:
        case icu4x::capi::Block_IndicSiyaqNumbers:
        case icu4x::capi::Block_Makasar:
        case icu4x::capi::Block_MayanNumerals:
        case icu4x::capi::Block_Medefaidrin:
        case icu4x::capi::Block_OldSogdian:
        case icu4x::capi::Block_Sogdian:
        case icu4x::capi::Block_EgyptianHieroglyphFormatControls:
        case icu4x::capi::Block_Elymaic:
        case icu4x::capi::Block_Nandinagari:
        case icu4x::capi::Block_NyiakengPuachueHmong:
        case icu4x::capi::Block_OttomanSiyaqNumbers:
        case icu4x::capi::Block_SmallKanaExtension:
        case icu4x::capi::Block_SymbolsAndPictographsExtendedA:
        case icu4x::capi::Block_TamilSupplement:
        case icu4x::capi::Block_Wancho:
        case icu4x::capi::Block_Chorasmian:
        case icu4x::capi::Block_CJKUnifiedIdeographsExtensionG:
        case icu4x::capi::Block_DivesAkuru:
        case icu4x::capi::Block_KhitanSmallScript:
        case icu4x::capi::Block_LisuSupplement:
        case icu4x::capi::Block_SymbolsForLegacyComputing:
        case icu4x::capi::Block_TangutSupplement:
        case icu4x::capi::Block_Yezidi:
        case icu4x::capi::Block_ArabicExtendedB:
        case icu4x::capi::Block_CyproMinoan:
        case icu4x::capi::Block_EthiopicExtendedB:
        case icu4x::capi::Block_KanaExtendedB:
        case icu4x::capi::Block_LatinExtendedF:
        case icu4x::capi::Block_LatinExtendedG:
        case icu4x::capi::Block_OldUyghur:
        case icu4x::capi::Block_Tangsa:
        case icu4x::capi::Block_Toto:
        case icu4x::capi::Block_UnifiedCanadianAboriginalSyllabicsExtendedA:
        case icu4x::capi::Block_Vithkuqi:
        case icu4x::capi::Block_ZnamennyMusicalNotation:
        case icu4x::capi::Block_ArabicExtendedC:
        case icu4x::capi::Block_CJKUnifiedIdeographsExtensionH:
        case icu4x::capi::Block_CyrillicExtendedD:
        case icu4x::capi::Block_DevanagariExtendedA:
        case icu4x::capi::Block_KaktovikNumerals:
        case icu4x::capi::Block_Kawi:
        case icu4x::capi::Block_NagMundari:
        case icu4x::capi::Block_CJKUnifiedIdeographsExtensionI:
        case icu4x::capi::Block_EgyptianHieroglyphsExtendedA:
        case icu4x::capi::Block_Garay:
        case icu4x::capi::Block_GurungKhema:
        case icu4x::capi::Block_KiratRai:
        case icu4x::capi::Block_MyanmarExtendedC:
        case icu4x::capi::Block_OlOnal:
        case icu4x::capi::Block_Sunuwar:
        case icu4x::capi::Block_SymbolsForLegacyComputingSupplement:
        case icu4x::capi::Block_Todhri:
        case icu4x::capi::Block_TuluTigalari:
        case icu4x::capi::Block_BeriaErfe:
        case icu4x::capi::Block_CJKUnifiedIdeographsExtensionJ:
        case icu4x::capi::Block_MiscellaneousSymbolsSupplement:
        case icu4x::capi::Block_SharadaSupplement:
        case icu4x::capi::Block_Sidetic:
        case icu4x::capi::Block_TaiYo:
        case icu4x::capi::Block_TangutComponentsSupplement:
        case icu4x::capi::Block_TolongSiki:
        case icu4x::capi::Block_ArchaicCuneiformNumerals:
        case icu4x::capi::Block_BengaliSupplement:
        case icu4x::capi::Block_Jurchen:
        case icu4x::capi::Block_JurchenRadicals:
        case icu4x::capi::Block_MiscellaneousSymbolsAndArrowsExtended:
        case icu4x::capi::Block_MusicalSymbolsSupplement:
        case icu4x::capi::Block_Seal:
            return static_cast<icu4x::Block::Value>(c_enum);
        default:
            std::abort();
    }
}

inline icu4x::Block icu4x::Block::for_char(char32_t ch) {
    auto result = icu4x::capi::icu4x_Block_for_char_mv1(ch);
    return icu4x::Block::FromFFI(result);
}

inline std::optional<std::string_view> icu4x::Block::long_name() const {
    auto result = icu4x::capi::icu4x_Block_long_name_mv1(this->AsFFI());
    return result.is_ok ? std::optional<std::string_view>(std::string_view(result.ok.data, result.ok.len)) : std::nullopt;
}

inline std::optional<std::string_view> icu4x::Block::short_name() const {
    auto result = icu4x::capi::icu4x_Block_short_name_mv1(this->AsFFI());
    return result.is_ok ? std::optional<std::string_view>(std::string_view(result.ok.data, result.ok.len)) : std::nullopt;
}

inline uint16_t icu4x::Block::to_integer_value() const {
    auto result = icu4x::capi::icu4x_Block_to_integer_value_mv1(this->AsFFI());
    return result;
}

inline std::optional<icu4x::Block> icu4x::Block::from_integer_value(uint16_t other) {
    auto result = icu4x::capi::icu4x_Block_from_integer_value_mv1(other);
    return result.is_ok ? std::optional<icu4x::Block>(icu4x::Block::FromFFI(result.ok)) : std::nullopt;
}

inline std::optional<icu4x::Block> icu4x::Block::try_from_str(std::string_view s) {
    auto result = icu4x::capi::icu4x_Block_try_from_str_mv1({s.data(), s.size()});
    return result.is_ok ? std::optional<icu4x::Block>(icu4x::Block::FromFFI(result.ok)) : std::nullopt;
}
#endif // ICU4X_Block_HPP

#ifndef ICU4X_Block_D_HPP
#define ICU4X_Block_D_HPP

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
class Block;
} // namespace icu4x



namespace icu4x {
namespace capi {
    enum Block {
      Block_NoBlock = 0,
      Block_BasicLatin = 1,
      Block_Latin1Supplement = 2,
      Block_LatinExtendedA = 3,
      Block_LatinExtendedB = 4,
      Block_IPAExtensions = 5,
      Block_SpacingModifierLetters = 6,
      Block_CombiningDiacriticalMarks = 7,
      Block_GreekAndCoptic = 8,
      Block_Cyrillic = 9,
      Block_Armenian = 10,
      Block_Hebrew = 11,
      Block_Arabic = 12,
      Block_Syriac = 13,
      Block_Thaana = 14,
      Block_Devanagari = 15,
      Block_Bengali = 16,
      Block_Gurmukhi = 17,
      Block_Gujarati = 18,
      Block_Oriya = 19,
      Block_Tamil = 20,
      Block_Telugu = 21,
      Block_Kannada = 22,
      Block_Malayalam = 23,
      Block_Sinhala = 24,
      Block_Thai = 25,
      Block_Lao = 26,
      Block_Tibetan = 27,
      Block_Myanmar = 28,
      Block_Georgian = 29,
      Block_HangulJamo = 30,
      Block_Ethiopic = 31,
      Block_Cherokee = 32,
      Block_UnifiedCanadianAboriginalSyllabics = 33,
      Block_Ogham = 34,
      Block_Runic = 35,
      Block_Khmer = 36,
      Block_Mongolian = 37,
      Block_LatinExtendedAdditional = 38,
      Block_GreekExtended = 39,
      Block_GeneralPunctuation = 40,
      Block_SuperscriptsAndSubscripts = 41,
      Block_CurrencySymbols = 42,
      Block_CombiningDiacriticalMarksForSymbols = 43,
      Block_LetterlikeSymbols = 44,
      Block_NumberForms = 45,
      Block_Arrows = 46,
      Block_MathematicalOperators = 47,
      Block_MiscellaneousTechnical = 48,
      Block_ControlPictures = 49,
      Block_OpticalCharacterRecognition = 50,
      Block_EnclosedAlphanumerics = 51,
      Block_BoxDrawing = 52,
      Block_BlockElements = 53,
      Block_GeometricShapes = 54,
      Block_MiscellaneousSymbols = 55,
      Block_Dingbats = 56,
      Block_BraillePatterns = 57,
      Block_CJKRadicalsSupplement = 58,
      Block_KangxiRadicals = 59,
      Block_IdeographicDescriptionCharacters = 60,
      Block_CJKSymbolsAndPunctuation = 61,
      Block_Hiragana = 62,
      Block_Katakana = 63,
      Block_Bopomofo = 64,
      Block_HangulCompatibilityJamo = 65,
      Block_Kanbun = 66,
      Block_BopomofoExtended = 67,
      Block_EnclosedCJKLettersAndMonths = 68,
      Block_CJKCompatibility = 69,
      Block_CJKUnifiedIdeographsExtensionA = 70,
      Block_CJKUnifiedIdeographs = 71,
      Block_YiSyllables = 72,
      Block_YiRadicals = 73,
      Block_HangulSyllables = 74,
      Block_HighSurrogates = 75,
      Block_HighPrivateUseSurrogates = 76,
      Block_LowSurrogates = 77,
      Block_PrivateUseArea = 78,
      Block_CJKCompatibilityIdeographs = 79,
      Block_AlphabeticPresentationForms = 80,
      Block_ArabicPresentationFormsA = 81,
      Block_CombiningHalfMarks = 82,
      Block_CJKCompatibilityForms = 83,
      Block_SmallFormVariants = 84,
      Block_ArabicPresentationFormsB = 85,
      Block_Specials = 86,
      Block_HalfwidthAndFullwidthForms = 87,
      Block_OldItalic = 88,
      Block_Gothic = 89,
      Block_Deseret = 90,
      Block_ByzantineMusicalSymbols = 91,
      Block_MusicalSymbols = 92,
      Block_MathematicalAlphanumericSymbols = 93,
      Block_CJKUnifiedIdeographsExtensionB = 94,
      Block_CJKCompatibilityIdeographsSupplement = 95,
      Block_Tags = 96,
      Block_CyrillicSupplement = 97,
      Block_Tagalog = 98,
      Block_Hanunoo = 99,
      Block_Buhid = 100,
      Block_Tagbanwa = 101,
      Block_MiscellaneousMathematicalSymbolsA = 102,
      Block_SupplementalArrowsA = 103,
      Block_SupplementalArrowsB = 104,
      Block_MiscellaneousMathematicalSymbolsB = 105,
      Block_SupplementalMathematicalOperators = 106,
      Block_KatakanaPhoneticExtensions = 107,
      Block_VariationSelectors = 108,
      Block_SupplementaryPrivateUseAreaA = 109,
      Block_SupplementaryPrivateUseAreaB = 110,
      Block_Limbu = 111,
      Block_TaiLe = 112,
      Block_KhmerSymbols = 113,
      Block_PhoneticExtensions = 114,
      Block_MiscellaneousSymbolsAndArrows = 115,
      Block_YijingHexagramSymbols = 116,
      Block_LinearBSyllabary = 117,
      Block_LinearBIdeograms = 118,
      Block_AegeanNumbers = 119,
      Block_Ugaritic = 120,
      Block_Shavian = 121,
      Block_Osmanya = 122,
      Block_CypriotSyllabary = 123,
      Block_TaiXuanJingSymbols = 124,
      Block_VariationSelectorsSupplement = 125,
      Block_AncientGreekMusicalNotation = 126,
      Block_AncientGreekNumbers = 127,
      Block_ArabicSupplement = 128,
      Block_Buginese = 129,
      Block_CJKStrokes = 130,
      Block_CombiningDiacriticalMarksSupplement = 131,
      Block_Coptic = 132,
      Block_EthiopicExtended = 133,
      Block_EthiopicSupplement = 134,
      Block_GeorgianSupplement = 135,
      Block_Glagolitic = 136,
      Block_Kharoshthi = 137,
      Block_ModifierToneLetters = 138,
      Block_NewTaiLue = 139,
      Block_OldPersian = 140,
      Block_PhoneticExtensionsSupplement = 141,
      Block_SupplementalPunctuation = 142,
      Block_SylotiNagri = 143,
      Block_Tifinagh = 144,
      Block_VerticalForms = 145,
      Block_NKo = 146,
      Block_Balinese = 147,
      Block_LatinExtendedC = 148,
      Block_LatinExtendedD = 149,
      Block_PhagsPa = 150,
      Block_Phoenician = 151,
      Block_Cuneiform = 152,
      Block_CuneiformNumbersAndPunctuation = 153,
      Block_CountingRodNumerals = 154,
      Block_Sundanese = 155,
      Block_Lepcha = 156,
      Block_OlChiki = 157,
      Block_CyrillicExtendedA = 158,
      Block_Vai = 159,
      Block_CyrillicExtendedB = 160,
      Block_Saurashtra = 161,
      Block_KayahLi = 162,
      Block_Rejang = 163,
      Block_Cham = 164,
      Block_AncientSymbols = 165,
      Block_PhaistosDisc = 166,
      Block_Lycian = 167,
      Block_Carian = 168,
      Block_Lydian = 169,
      Block_MahjongTiles = 170,
      Block_DominoTiles = 171,
      Block_Samaritan = 172,
      Block_UnifiedCanadianAboriginalSyllabicsExtended = 173,
      Block_TaiTham = 174,
      Block_VedicExtensions = 175,
      Block_Lisu = 176,
      Block_Bamum = 177,
      Block_CommonIndicNumberForms = 178,
      Block_DevanagariExtended = 179,
      Block_HangulJamoExtendedA = 180,
      Block_Javanese = 181,
      Block_MyanmarExtendedA = 182,
      Block_TaiViet = 183,
      Block_MeeteiMayek = 184,
      Block_HangulJamoExtendedB = 185,
      Block_ImperialAramaic = 186,
      Block_OldSouthArabian = 187,
      Block_Avestan = 188,
      Block_InscriptionalParthian = 189,
      Block_InscriptionalPahlavi = 190,
      Block_OldTurkic = 191,
      Block_RumiNumeralSymbols = 192,
      Block_Kaithi = 193,
      Block_EgyptianHieroglyphs = 194,
      Block_EnclosedAlphanumericSupplement = 195,
      Block_EnclosedIdeographicSupplement = 196,
      Block_CJKUnifiedIdeographsExtensionC = 197,
      Block_Mandaic = 198,
      Block_Batak = 199,
      Block_EthiopicExtendedA = 200,
      Block_Brahmi = 201,
      Block_BamumSupplement = 202,
      Block_KanaSupplement = 203,
      Block_PlayingCards = 204,
      Block_MiscellaneousSymbolsAndPictographs = 205,
      Block_Emoticons = 206,
      Block_TransportAndMapSymbols = 207,
      Block_AlchemicalSymbols = 208,
      Block_CJKUnifiedIdeographsExtensionD = 209,
      Block_ArabicExtendedA = 210,
      Block_ArabicMathematicalAlphabeticSymbols = 211,
      Block_Chakma = 212,
      Block_MeeteiMayekExtensions = 213,
      Block_MeroiticCursive = 214,
      Block_MeroiticHieroglyphs = 215,
      Block_Miao = 216,
      Block_Sharada = 217,
      Block_SoraSompeng = 218,
      Block_SundaneseSupplement = 219,
      Block_Takri = 220,
      Block_BassaVah = 221,
      Block_CaucasianAlbanian = 222,
      Block_CopticEpactNumbers = 223,
      Block_CombiningDiacriticalMarksExtended = 224,
      Block_Duployan = 225,
      Block_Elbasan = 226,
      Block_GeometricShapesExtended = 227,
      Block_Grantha = 228,
      Block_Khojki = 229,
      Block_Khudawadi = 230,
      Block_LatinExtendedE = 231,
      Block_LinearA = 232,
      Block_Mahajani = 233,
      Block_Manichaean = 234,
      Block_MendeKikakui = 235,
      Block_Modi = 236,
      Block_Mro = 237,
      Block_MyanmarExtendedB = 238,
      Block_Nabataean = 239,
      Block_OldNorthArabian = 240,
      Block_OldPermic = 241,
      Block_OrnamentalDingbats = 242,
      Block_PahawhHmong = 243,
      Block_Palmyrene = 244,
      Block_PauCinHau = 245,
      Block_PsalterPahlavi = 246,
      Block_ShorthandFormatControls = 247,
      Block_Siddham = 248,
      Block_SinhalaArchaicNumbers = 249,
      Block_SupplementalArrowsC = 250,
      Block_Tirhuta = 251,
      Block_WarangCiti = 252,
      Block_Ahom = 253,
      Block_AnatolianHieroglyphs = 254,
      Block_CherokeeSupplement = 255,
      Block_CJKUnifiedIdeographsExtensionE = 256,
      Block_EarlyDynasticCuneiform = 257,
      Block_Hatran = 258,
      Block_Multani = 259,
      Block_OldHungarian = 260,
      Block_SupplementalSymbolsAndPictographs = 261,
      Block_SuttonSignWriting = 262,
      Block_Adlam = 263,
      Block_Bhaiksuki = 264,
      Block_CyrillicExtendedC = 265,
      Block_GlagoliticSupplement = 266,
      Block_IdeographicSymbolsAndPunctuation = 267,
      Block_Marchen = 268,
      Block_MongolianSupplement = 269,
      Block_Newa = 270,
      Block_Osage = 271,
      Block_Tangut = 272,
      Block_TangutComponents = 273,
      Block_CJKUnifiedIdeographsExtensionF = 274,
      Block_KanaExtendedA = 275,
      Block_MasaramGondi = 276,
      Block_Nushu = 277,
      Block_Soyombo = 278,
      Block_SyriacSupplement = 279,
      Block_ZanabazarSquare = 280,
      Block_ChessSymbols = 281,
      Block_Dogra = 282,
      Block_GeorgianExtended = 283,
      Block_GunjalaGondi = 284,
      Block_HanifiRohingya = 285,
      Block_IndicSiyaqNumbers = 286,
      Block_Makasar = 287,
      Block_MayanNumerals = 288,
      Block_Medefaidrin = 289,
      Block_OldSogdian = 290,
      Block_Sogdian = 291,
      Block_EgyptianHieroglyphFormatControls = 292,
      Block_Elymaic = 293,
      Block_Nandinagari = 294,
      Block_NyiakengPuachueHmong = 295,
      Block_OttomanSiyaqNumbers = 296,
      Block_SmallKanaExtension = 297,
      Block_SymbolsAndPictographsExtendedA = 298,
      Block_TamilSupplement = 299,
      Block_Wancho = 300,
      Block_Chorasmian = 301,
      Block_CJKUnifiedIdeographsExtensionG = 302,
      Block_DivesAkuru = 303,
      Block_KhitanSmallScript = 304,
      Block_LisuSupplement = 305,
      Block_SymbolsForLegacyComputing = 306,
      Block_TangutSupplement = 307,
      Block_Yezidi = 308,
      Block_ArabicExtendedB = 309,
      Block_CyproMinoan = 310,
      Block_EthiopicExtendedB = 311,
      Block_KanaExtendedB = 312,
      Block_LatinExtendedF = 313,
      Block_LatinExtendedG = 314,
      Block_OldUyghur = 315,
      Block_Tangsa = 316,
      Block_Toto = 317,
      Block_UnifiedCanadianAboriginalSyllabicsExtendedA = 318,
      Block_Vithkuqi = 319,
      Block_ZnamennyMusicalNotation = 320,
      Block_ArabicExtendedC = 321,
      Block_CJKUnifiedIdeographsExtensionH = 322,
      Block_CyrillicExtendedD = 323,
      Block_DevanagariExtendedA = 324,
      Block_KaktovikNumerals = 325,
      Block_Kawi = 326,
      Block_NagMundari = 327,
      Block_CJKUnifiedIdeographsExtensionI = 328,
      Block_EgyptianHieroglyphsExtendedA = 329,
      Block_Garay = 330,
      Block_GurungKhema = 331,
      Block_KiratRai = 332,
      Block_MyanmarExtendedC = 333,
      Block_OlOnal = 334,
      Block_Sunuwar = 335,
      Block_SymbolsForLegacyComputingSupplement = 336,
      Block_Todhri = 337,
      Block_TuluTigalari = 338,
      Block_BeriaErfe = 339,
      Block_CJKUnifiedIdeographsExtensionJ = 340,
      Block_MiscellaneousSymbolsSupplement = 341,
      Block_SharadaSupplement = 342,
      Block_Sidetic = 343,
      Block_TaiYo = 344,
      Block_TangutComponentsSupplement = 345,
      Block_TolongSiki = 346,
      Block_ArchaicCuneiformNumerals = 347,
      Block_BengaliSupplement = 348,
      Block_Jurchen = 349,
      Block_JurchenRadicals = 350,
      Block_MiscellaneousSymbolsAndArrowsExtended = 351,
      Block_MusicalSymbolsSupplement = 352,
      Block_Seal = 353,
    };

    typedef struct Block_option {union { Block ok; }; bool is_ok; } Block_option;
} // namespace capi
} // namespace

namespace icu4x {
/**
 * See the [Rust documentation for `Block`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html) for more information.
 */
class Block {
public:
    enum Value {
        /**
         * See the [Rust documentation for `NoBlock`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.NoBlock) for more information.
         */
        NoBlock = 0,
        /**
         * See the [Rust documentation for `BasicLatin`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.BasicLatin) for more information.
         */
        BasicLatin = 1,
        /**
         * See the [Rust documentation for `Latin1Supplement`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.Latin1Supplement) for more information.
         */
        Latin1Supplement = 2,
        /**
         * See the [Rust documentation for `LatinExtendedA`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.LatinExtendedA) for more information.
         */
        LatinExtendedA = 3,
        /**
         * See the [Rust documentation for `LatinExtendedB`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.LatinExtendedB) for more information.
         */
        LatinExtendedB = 4,
        /**
         * See the [Rust documentation for `IPAExtensions`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.IPAExtensions) for more information.
         */
        IPAExtensions = 5,
        /**
         * See the [Rust documentation for `SpacingModifierLetters`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.SpacingModifierLetters) for more information.
         */
        SpacingModifierLetters = 6,
        /**
         * See the [Rust documentation for `CombiningDiacriticalMarks`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.CombiningDiacriticalMarks) for more information.
         */
        CombiningDiacriticalMarks = 7,
        /**
         * See the [Rust documentation for `GreekAndCoptic`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.GreekAndCoptic) for more information.
         */
        GreekAndCoptic = 8,
        /**
         * See the [Rust documentation for `Cyrillic`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.Cyrillic) for more information.
         */
        Cyrillic = 9,
        /**
         * See the [Rust documentation for `Armenian`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.Armenian) for more information.
         */
        Armenian = 10,
        /**
         * See the [Rust documentation for `Hebrew`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.Hebrew) for more information.
         */
        Hebrew = 11,
        /**
         * See the [Rust documentation for `Arabic`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.Arabic) for more information.
         */
        Arabic = 12,
        /**
         * See the [Rust documentation for `Syriac`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.Syriac) for more information.
         */
        Syriac = 13,
        /**
         * See the [Rust documentation for `Thaana`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.Thaana) for more information.
         */
        Thaana = 14,
        /**
         * See the [Rust documentation for `Devanagari`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.Devanagari) for more information.
         */
        Devanagari = 15,
        /**
         * See the [Rust documentation for `Bengali`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.Bengali) for more information.
         */
        Bengali = 16,
        /**
         * See the [Rust documentation for `Gurmukhi`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.Gurmukhi) for more information.
         */
        Gurmukhi = 17,
        /**
         * See the [Rust documentation for `Gujarati`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.Gujarati) for more information.
         */
        Gujarati = 18,
        /**
         * See the [Rust documentation for `Oriya`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.Oriya) for more information.
         */
        Oriya = 19,
        /**
         * See the [Rust documentation for `Tamil`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.Tamil) for more information.
         */
        Tamil = 20,
        /**
         * See the [Rust documentation for `Telugu`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.Telugu) for more information.
         */
        Telugu = 21,
        /**
         * See the [Rust documentation for `Kannada`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.Kannada) for more information.
         */
        Kannada = 22,
        /**
         * See the [Rust documentation for `Malayalam`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.Malayalam) for more information.
         */
        Malayalam = 23,
        /**
         * See the [Rust documentation for `Sinhala`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.Sinhala) for more information.
         */
        Sinhala = 24,
        /**
         * See the [Rust documentation for `Thai`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.Thai) for more information.
         */
        Thai = 25,
        /**
         * See the [Rust documentation for `Lao`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.Lao) for more information.
         */
        Lao = 26,
        /**
         * See the [Rust documentation for `Tibetan`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.Tibetan) for more information.
         */
        Tibetan = 27,
        /**
         * See the [Rust documentation for `Myanmar`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.Myanmar) for more information.
         */
        Myanmar = 28,
        /**
         * See the [Rust documentation for `Georgian`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.Georgian) for more information.
         */
        Georgian = 29,
        /**
         * See the [Rust documentation for `HangulJamo`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.HangulJamo) for more information.
         */
        HangulJamo = 30,
        /**
         * See the [Rust documentation for `Ethiopic`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.Ethiopic) for more information.
         */
        Ethiopic = 31,
        /**
         * See the [Rust documentation for `Cherokee`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.Cherokee) for more information.
         */
        Cherokee = 32,
        /**
         * See the [Rust documentation for `UnifiedCanadianAboriginalSyllabics`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.UnifiedCanadianAboriginalSyllabics) for more information.
         */
        UnifiedCanadianAboriginalSyllabics = 33,
        /**
         * See the [Rust documentation for `Ogham`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.Ogham) for more information.
         */
        Ogham = 34,
        /**
         * See the [Rust documentation for `Runic`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.Runic) for more information.
         */
        Runic = 35,
        /**
         * See the [Rust documentation for `Khmer`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.Khmer) for more information.
         */
        Khmer = 36,
        /**
         * See the [Rust documentation for `Mongolian`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.Mongolian) for more information.
         */
        Mongolian = 37,
        /**
         * See the [Rust documentation for `LatinExtendedAdditional`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.LatinExtendedAdditional) for more information.
         */
        LatinExtendedAdditional = 38,
        /**
         * See the [Rust documentation for `GreekExtended`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.GreekExtended) for more information.
         */
        GreekExtended = 39,
        /**
         * See the [Rust documentation for `GeneralPunctuation`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.GeneralPunctuation) for more information.
         */
        GeneralPunctuation = 40,
        /**
         * See the [Rust documentation for `SuperscriptsAndSubscripts`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.SuperscriptsAndSubscripts) for more information.
         */
        SuperscriptsAndSubscripts = 41,
        /**
         * See the [Rust documentation for `CurrencySymbols`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.CurrencySymbols) for more information.
         */
        CurrencySymbols = 42,
        /**
         * See the [Rust documentation for `CombiningDiacriticalMarksForSymbols`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.CombiningDiacriticalMarksForSymbols) for more information.
         */
        CombiningDiacriticalMarksForSymbols = 43,
        /**
         * See the [Rust documentation for `LetterlikeSymbols`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.LetterlikeSymbols) for more information.
         */
        LetterlikeSymbols = 44,
        /**
         * See the [Rust documentation for `NumberForms`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.NumberForms) for more information.
         */
        NumberForms = 45,
        /**
         * See the [Rust documentation for `Arrows`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.Arrows) for more information.
         */
        Arrows = 46,
        /**
         * See the [Rust documentation for `MathematicalOperators`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.MathematicalOperators) for more information.
         */
        MathematicalOperators = 47,
        /**
         * See the [Rust documentation for `MiscellaneousTechnical`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.MiscellaneousTechnical) for more information.
         */
        MiscellaneousTechnical = 48,
        /**
         * See the [Rust documentation for `ControlPictures`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.ControlPictures) for more information.
         */
        ControlPictures = 49,
        /**
         * See the [Rust documentation for `OpticalCharacterRecognition`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.OpticalCharacterRecognition) for more information.
         */
        OpticalCharacterRecognition = 50,
        /**
         * See the [Rust documentation for `EnclosedAlphanumerics`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.EnclosedAlphanumerics) for more information.
         */
        EnclosedAlphanumerics = 51,
        /**
         * See the [Rust documentation for `BoxDrawing`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.BoxDrawing) for more information.
         */
        BoxDrawing = 52,
        /**
         * See the [Rust documentation for `BlockElements`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.BlockElements) for more information.
         */
        BlockElements = 53,
        /**
         * See the [Rust documentation for `GeometricShapes`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.GeometricShapes) for more information.
         */
        GeometricShapes = 54,
        /**
         * See the [Rust documentation for `MiscellaneousSymbols`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.MiscellaneousSymbols) for more information.
         */
        MiscellaneousSymbols = 55,
        /**
         * See the [Rust documentation for `Dingbats`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.Dingbats) for more information.
         */
        Dingbats = 56,
        /**
         * See the [Rust documentation for `BraillePatterns`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.BraillePatterns) for more information.
         */
        BraillePatterns = 57,
        /**
         * See the [Rust documentation for `CJKRadicalsSupplement`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.CJKRadicalsSupplement) for more information.
         */
        CJKRadicalsSupplement = 58,
        /**
         * See the [Rust documentation for `KangxiRadicals`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.KangxiRadicals) for more information.
         */
        KangxiRadicals = 59,
        /**
         * See the [Rust documentation for `IdeographicDescriptionCharacters`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.IdeographicDescriptionCharacters) for more information.
         */
        IdeographicDescriptionCharacters = 60,
        /**
         * See the [Rust documentation for `CJKSymbolsAndPunctuation`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.CJKSymbolsAndPunctuation) for more information.
         */
        CJKSymbolsAndPunctuation = 61,
        /**
         * See the [Rust documentation for `Hiragana`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.Hiragana) for more information.
         */
        Hiragana = 62,
        /**
         * See the [Rust documentation for `Katakana`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.Katakana) for more information.
         */
        Katakana = 63,
        /**
         * See the [Rust documentation for `Bopomofo`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.Bopomofo) for more information.
         */
        Bopomofo = 64,
        /**
         * See the [Rust documentation for `HangulCompatibilityJamo`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.HangulCompatibilityJamo) for more information.
         */
        HangulCompatibilityJamo = 65,
        /**
         * See the [Rust documentation for `Kanbun`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.Kanbun) for more information.
         */
        Kanbun = 66,
        /**
         * See the [Rust documentation for `BopomofoExtended`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.BopomofoExtended) for more information.
         */
        BopomofoExtended = 67,
        /**
         * See the [Rust documentation for `EnclosedCJKLettersAndMonths`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.EnclosedCJKLettersAndMonths) for more information.
         */
        EnclosedCJKLettersAndMonths = 68,
        /**
         * See the [Rust documentation for `CJKCompatibility`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.CJKCompatibility) for more information.
         */
        CJKCompatibility = 69,
        /**
         * See the [Rust documentation for `CJKUnifiedIdeographsExtensionA`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.CJKUnifiedIdeographsExtensionA) for more information.
         */
        CJKUnifiedIdeographsExtensionA = 70,
        /**
         * See the [Rust documentation for `CJKUnifiedIdeographs`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.CJKUnifiedIdeographs) for more information.
         */
        CJKUnifiedIdeographs = 71,
        /**
         * See the [Rust documentation for `YiSyllables`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.YiSyllables) for more information.
         */
        YiSyllables = 72,
        /**
         * See the [Rust documentation for `YiRadicals`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.YiRadicals) for more information.
         */
        YiRadicals = 73,
        /**
         * See the [Rust documentation for `HangulSyllables`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.HangulSyllables) for more information.
         */
        HangulSyllables = 74,
        /**
         * See the [Rust documentation for `HighSurrogates`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.HighSurrogates) for more information.
         */
        HighSurrogates = 75,
        /**
         * See the [Rust documentation for `HighPrivateUseSurrogates`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.HighPrivateUseSurrogates) for more information.
         */
        HighPrivateUseSurrogates = 76,
        /**
         * See the [Rust documentation for `LowSurrogates`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.LowSurrogates) for more information.
         */
        LowSurrogates = 77,
        /**
         * See the [Rust documentation for `PrivateUseArea`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.PrivateUseArea) for more information.
         */
        PrivateUseArea = 78,
        /**
         * See the [Rust documentation for `CJKCompatibilityIdeographs`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.CJKCompatibilityIdeographs) for more information.
         */
        CJKCompatibilityIdeographs = 79,
        /**
         * See the [Rust documentation for `AlphabeticPresentationForms`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.AlphabeticPresentationForms) for more information.
         */
        AlphabeticPresentationForms = 80,
        /**
         * See the [Rust documentation for `ArabicPresentationFormsA`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.ArabicPresentationFormsA) for more information.
         */
        ArabicPresentationFormsA = 81,
        /**
         * See the [Rust documentation for `CombiningHalfMarks`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.CombiningHalfMarks) for more information.
         */
        CombiningHalfMarks = 82,
        /**
         * See the [Rust documentation for `CJKCompatibilityForms`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.CJKCompatibilityForms) for more information.
         */
        CJKCompatibilityForms = 83,
        /**
         * See the [Rust documentation for `SmallFormVariants`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.SmallFormVariants) for more information.
         */
        SmallFormVariants = 84,
        /**
         * See the [Rust documentation for `ArabicPresentationFormsB`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.ArabicPresentationFormsB) for more information.
         */
        ArabicPresentationFormsB = 85,
        /**
         * See the [Rust documentation for `Specials`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.Specials) for more information.
         */
        Specials = 86,
        /**
         * See the [Rust documentation for `HalfwidthAndFullwidthForms`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.HalfwidthAndFullwidthForms) for more information.
         */
        HalfwidthAndFullwidthForms = 87,
        /**
         * See the [Rust documentation for `OldItalic`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.OldItalic) for more information.
         */
        OldItalic = 88,
        /**
         * See the [Rust documentation for `Gothic`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.Gothic) for more information.
         */
        Gothic = 89,
        /**
         * See the [Rust documentation for `Deseret`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.Deseret) for more information.
         */
        Deseret = 90,
        /**
         * See the [Rust documentation for `ByzantineMusicalSymbols`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.ByzantineMusicalSymbols) for more information.
         */
        ByzantineMusicalSymbols = 91,
        /**
         * See the [Rust documentation for `MusicalSymbols`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.MusicalSymbols) for more information.
         */
        MusicalSymbols = 92,
        /**
         * See the [Rust documentation for `MathematicalAlphanumericSymbols`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.MathematicalAlphanumericSymbols) for more information.
         */
        MathematicalAlphanumericSymbols = 93,
        /**
         * See the [Rust documentation for `CJKUnifiedIdeographsExtensionB`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.CJKUnifiedIdeographsExtensionB) for more information.
         */
        CJKUnifiedIdeographsExtensionB = 94,
        /**
         * See the [Rust documentation for `CJKCompatibilityIdeographsSupplement`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.CJKCompatibilityIdeographsSupplement) for more information.
         */
        CJKCompatibilityIdeographsSupplement = 95,
        /**
         * See the [Rust documentation for `Tags`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.Tags) for more information.
         */
        Tags = 96,
        /**
         * See the [Rust documentation for `CyrillicSupplement`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.CyrillicSupplement) for more information.
         */
        CyrillicSupplement = 97,
        /**
         * See the [Rust documentation for `Tagalog`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.Tagalog) for more information.
         */
        Tagalog = 98,
        /**
         * See the [Rust documentation for `Hanunoo`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.Hanunoo) for more information.
         */
        Hanunoo = 99,
        /**
         * See the [Rust documentation for `Buhid`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.Buhid) for more information.
         */
        Buhid = 100,
        /**
         * See the [Rust documentation for `Tagbanwa`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.Tagbanwa) for more information.
         */
        Tagbanwa = 101,
        /**
         * See the [Rust documentation for `MiscellaneousMathematicalSymbolsA`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.MiscellaneousMathematicalSymbolsA) for more information.
         */
        MiscellaneousMathematicalSymbolsA = 102,
        /**
         * See the [Rust documentation for `SupplementalArrowsA`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.SupplementalArrowsA) for more information.
         */
        SupplementalArrowsA = 103,
        /**
         * See the [Rust documentation for `SupplementalArrowsB`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.SupplementalArrowsB) for more information.
         */
        SupplementalArrowsB = 104,
        /**
         * See the [Rust documentation for `MiscellaneousMathematicalSymbolsB`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.MiscellaneousMathematicalSymbolsB) for more information.
         */
        MiscellaneousMathematicalSymbolsB = 105,
        /**
         * See the [Rust documentation for `SupplementalMathematicalOperators`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.SupplementalMathematicalOperators) for more information.
         */
        SupplementalMathematicalOperators = 106,
        /**
         * See the [Rust documentation for `KatakanaPhoneticExtensions`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.KatakanaPhoneticExtensions) for more information.
         */
        KatakanaPhoneticExtensions = 107,
        /**
         * See the [Rust documentation for `VariationSelectors`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.VariationSelectors) for more information.
         */
        VariationSelectors = 108,
        /**
         * See the [Rust documentation for `SupplementaryPrivateUseAreaA`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.SupplementaryPrivateUseAreaA) for more information.
         */
        SupplementaryPrivateUseAreaA = 109,
        /**
         * See the [Rust documentation for `SupplementaryPrivateUseAreaB`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.SupplementaryPrivateUseAreaB) for more information.
         */
        SupplementaryPrivateUseAreaB = 110,
        /**
         * See the [Rust documentation for `Limbu`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.Limbu) for more information.
         */
        Limbu = 111,
        /**
         * See the [Rust documentation for `TaiLe`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.TaiLe) for more information.
         */
        TaiLe = 112,
        /**
         * See the [Rust documentation for `KhmerSymbols`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.KhmerSymbols) for more information.
         */
        KhmerSymbols = 113,
        /**
         * See the [Rust documentation for `PhoneticExtensions`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.PhoneticExtensions) for more information.
         */
        PhoneticExtensions = 114,
        /**
         * See the [Rust documentation for `MiscellaneousSymbolsAndArrows`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.MiscellaneousSymbolsAndArrows) for more information.
         */
        MiscellaneousSymbolsAndArrows = 115,
        /**
         * See the [Rust documentation for `YijingHexagramSymbols`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.YijingHexagramSymbols) for more information.
         */
        YijingHexagramSymbols = 116,
        /**
         * See the [Rust documentation for `LinearBSyllabary`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.LinearBSyllabary) for more information.
         */
        LinearBSyllabary = 117,
        /**
         * See the [Rust documentation for `LinearBIdeograms`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.LinearBIdeograms) for more information.
         */
        LinearBIdeograms = 118,
        /**
         * See the [Rust documentation for `AegeanNumbers`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.AegeanNumbers) for more information.
         */
        AegeanNumbers = 119,
        /**
         * See the [Rust documentation for `Ugaritic`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.Ugaritic) for more information.
         */
        Ugaritic = 120,
        /**
         * See the [Rust documentation for `Shavian`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.Shavian) for more information.
         */
        Shavian = 121,
        /**
         * See the [Rust documentation for `Osmanya`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.Osmanya) for more information.
         */
        Osmanya = 122,
        /**
         * See the [Rust documentation for `CypriotSyllabary`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.CypriotSyllabary) for more information.
         */
        CypriotSyllabary = 123,
        /**
         * See the [Rust documentation for `TaiXuanJingSymbols`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.TaiXuanJingSymbols) for more information.
         */
        TaiXuanJingSymbols = 124,
        /**
         * See the [Rust documentation for `VariationSelectorsSupplement`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.VariationSelectorsSupplement) for more information.
         */
        VariationSelectorsSupplement = 125,
        /**
         * See the [Rust documentation for `AncientGreekMusicalNotation`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.AncientGreekMusicalNotation) for more information.
         */
        AncientGreekMusicalNotation = 126,
        /**
         * See the [Rust documentation for `AncientGreekNumbers`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.AncientGreekNumbers) for more information.
         */
        AncientGreekNumbers = 127,
        /**
         * See the [Rust documentation for `ArabicSupplement`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.ArabicSupplement) for more information.
         */
        ArabicSupplement = 128,
        /**
         * See the [Rust documentation for `Buginese`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.Buginese) for more information.
         */
        Buginese = 129,
        /**
         * See the [Rust documentation for `CJKStrokes`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.CJKStrokes) for more information.
         */
        CJKStrokes = 130,
        /**
         * See the [Rust documentation for `CombiningDiacriticalMarksSupplement`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.CombiningDiacriticalMarksSupplement) for more information.
         */
        CombiningDiacriticalMarksSupplement = 131,
        /**
         * See the [Rust documentation for `Coptic`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.Coptic) for more information.
         */
        Coptic = 132,
        /**
         * See the [Rust documentation for `EthiopicExtended`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.EthiopicExtended) for more information.
         */
        EthiopicExtended = 133,
        /**
         * See the [Rust documentation for `EthiopicSupplement`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.EthiopicSupplement) for more information.
         */
        EthiopicSupplement = 134,
        /**
         * See the [Rust documentation for `GeorgianSupplement`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.GeorgianSupplement) for more information.
         */
        GeorgianSupplement = 135,
        /**
         * See the [Rust documentation for `Glagolitic`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.Glagolitic) for more information.
         */
        Glagolitic = 136,
        /**
         * See the [Rust documentation for `Kharoshthi`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.Kharoshthi) for more information.
         */
        Kharoshthi = 137,
        /**
         * See the [Rust documentation for `ModifierToneLetters`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.ModifierToneLetters) for more information.
         */
        ModifierToneLetters = 138,
        /**
         * See the [Rust documentation for `NewTaiLue`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.NewTaiLue) for more information.
         */
        NewTaiLue = 139,
        /**
         * See the [Rust documentation for `OldPersian`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.OldPersian) for more information.
         */
        OldPersian = 140,
        /**
         * See the [Rust documentation for `PhoneticExtensionsSupplement`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.PhoneticExtensionsSupplement) for more information.
         */
        PhoneticExtensionsSupplement = 141,
        /**
         * See the [Rust documentation for `SupplementalPunctuation`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.SupplementalPunctuation) for more information.
         */
        SupplementalPunctuation = 142,
        /**
         * See the [Rust documentation for `SylotiNagri`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.SylotiNagri) for more information.
         */
        SylotiNagri = 143,
        /**
         * See the [Rust documentation for `Tifinagh`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.Tifinagh) for more information.
         */
        Tifinagh = 144,
        /**
         * See the [Rust documentation for `VerticalForms`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.VerticalForms) for more information.
         */
        VerticalForms = 145,
        /**
         * See the [Rust documentation for `NKo`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.NKo) for more information.
         */
        NKo = 146,
        /**
         * See the [Rust documentation for `Balinese`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.Balinese) for more information.
         */
        Balinese = 147,
        /**
         * See the [Rust documentation for `LatinExtendedC`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.LatinExtendedC) for more information.
         */
        LatinExtendedC = 148,
        /**
         * See the [Rust documentation for `LatinExtendedD`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.LatinExtendedD) for more information.
         */
        LatinExtendedD = 149,
        /**
         * See the [Rust documentation for `PhagsPa`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.PhagsPa) for more information.
         */
        PhagsPa = 150,
        /**
         * See the [Rust documentation for `Phoenician`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.Phoenician) for more information.
         */
        Phoenician = 151,
        /**
         * See the [Rust documentation for `Cuneiform`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.Cuneiform) for more information.
         */
        Cuneiform = 152,
        /**
         * See the [Rust documentation for `CuneiformNumbersAndPunctuation`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.CuneiformNumbersAndPunctuation) for more information.
         */
        CuneiformNumbersAndPunctuation = 153,
        /**
         * See the [Rust documentation for `CountingRodNumerals`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.CountingRodNumerals) for more information.
         */
        CountingRodNumerals = 154,
        /**
         * See the [Rust documentation for `Sundanese`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.Sundanese) for more information.
         */
        Sundanese = 155,
        /**
         * See the [Rust documentation for `Lepcha`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.Lepcha) for more information.
         */
        Lepcha = 156,
        /**
         * See the [Rust documentation for `OlChiki`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.OlChiki) for more information.
         */
        OlChiki = 157,
        /**
         * See the [Rust documentation for `CyrillicExtendedA`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.CyrillicExtendedA) for more information.
         */
        CyrillicExtendedA = 158,
        /**
         * See the [Rust documentation for `Vai`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.Vai) for more information.
         */
        Vai = 159,
        /**
         * See the [Rust documentation for `CyrillicExtendedB`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.CyrillicExtendedB) for more information.
         */
        CyrillicExtendedB = 160,
        /**
         * See the [Rust documentation for `Saurashtra`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.Saurashtra) for more information.
         */
        Saurashtra = 161,
        /**
         * See the [Rust documentation for `KayahLi`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.KayahLi) for more information.
         */
        KayahLi = 162,
        /**
         * See the [Rust documentation for `Rejang`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.Rejang) for more information.
         */
        Rejang = 163,
        /**
         * See the [Rust documentation for `Cham`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.Cham) for more information.
         */
        Cham = 164,
        /**
         * See the [Rust documentation for `AncientSymbols`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.AncientSymbols) for more information.
         */
        AncientSymbols = 165,
        /**
         * See the [Rust documentation for `PhaistosDisc`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.PhaistosDisc) for more information.
         */
        PhaistosDisc = 166,
        /**
         * See the [Rust documentation for `Lycian`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.Lycian) for more information.
         */
        Lycian = 167,
        /**
         * See the [Rust documentation for `Carian`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.Carian) for more information.
         */
        Carian = 168,
        /**
         * See the [Rust documentation for `Lydian`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.Lydian) for more information.
         */
        Lydian = 169,
        /**
         * See the [Rust documentation for `MahjongTiles`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.MahjongTiles) for more information.
         */
        MahjongTiles = 170,
        /**
         * See the [Rust documentation for `DominoTiles`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.DominoTiles) for more information.
         */
        DominoTiles = 171,
        /**
         * See the [Rust documentation for `Samaritan`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.Samaritan) for more information.
         */
        Samaritan = 172,
        /**
         * See the [Rust documentation for `UnifiedCanadianAboriginalSyllabicsExtended`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.UnifiedCanadianAboriginalSyllabicsExtended) for more information.
         */
        UnifiedCanadianAboriginalSyllabicsExtended = 173,
        /**
         * See the [Rust documentation for `TaiTham`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.TaiTham) for more information.
         */
        TaiTham = 174,
        /**
         * See the [Rust documentation for `VedicExtensions`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.VedicExtensions) for more information.
         */
        VedicExtensions = 175,
        /**
         * See the [Rust documentation for `Lisu`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.Lisu) for more information.
         */
        Lisu = 176,
        /**
         * See the [Rust documentation for `Bamum`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.Bamum) for more information.
         */
        Bamum = 177,
        /**
         * See the [Rust documentation for `CommonIndicNumberForms`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.CommonIndicNumberForms) for more information.
         */
        CommonIndicNumberForms = 178,
        /**
         * See the [Rust documentation for `DevanagariExtended`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.DevanagariExtended) for more information.
         */
        DevanagariExtended = 179,
        /**
         * See the [Rust documentation for `HangulJamoExtendedA`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.HangulJamoExtendedA) for more information.
         */
        HangulJamoExtendedA = 180,
        /**
         * See the [Rust documentation for `Javanese`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.Javanese) for more information.
         */
        Javanese = 181,
        /**
         * See the [Rust documentation for `MyanmarExtendedA`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.MyanmarExtendedA) for more information.
         */
        MyanmarExtendedA = 182,
        /**
         * See the [Rust documentation for `TaiViet`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.TaiViet) for more information.
         */
        TaiViet = 183,
        /**
         * See the [Rust documentation for `MeeteiMayek`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.MeeteiMayek) for more information.
         */
        MeeteiMayek = 184,
        /**
         * See the [Rust documentation for `HangulJamoExtendedB`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.HangulJamoExtendedB) for more information.
         */
        HangulJamoExtendedB = 185,
        /**
         * See the [Rust documentation for `ImperialAramaic`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.ImperialAramaic) for more information.
         */
        ImperialAramaic = 186,
        /**
         * See the [Rust documentation for `OldSouthArabian`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.OldSouthArabian) for more information.
         */
        OldSouthArabian = 187,
        /**
         * See the [Rust documentation for `Avestan`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.Avestan) for more information.
         */
        Avestan = 188,
        /**
         * See the [Rust documentation for `InscriptionalParthian`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.InscriptionalParthian) for more information.
         */
        InscriptionalParthian = 189,
        /**
         * See the [Rust documentation for `InscriptionalPahlavi`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.InscriptionalPahlavi) for more information.
         */
        InscriptionalPahlavi = 190,
        /**
         * See the [Rust documentation for `OldTurkic`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.OldTurkic) for more information.
         */
        OldTurkic = 191,
        /**
         * See the [Rust documentation for `RumiNumeralSymbols`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.RumiNumeralSymbols) for more information.
         */
        RumiNumeralSymbols = 192,
        /**
         * See the [Rust documentation for `Kaithi`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.Kaithi) for more information.
         */
        Kaithi = 193,
        /**
         * See the [Rust documentation for `EgyptianHieroglyphs`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.EgyptianHieroglyphs) for more information.
         */
        EgyptianHieroglyphs = 194,
        /**
         * See the [Rust documentation for `EnclosedAlphanumericSupplement`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.EnclosedAlphanumericSupplement) for more information.
         */
        EnclosedAlphanumericSupplement = 195,
        /**
         * See the [Rust documentation for `EnclosedIdeographicSupplement`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.EnclosedIdeographicSupplement) for more information.
         */
        EnclosedIdeographicSupplement = 196,
        /**
         * See the [Rust documentation for `CJKUnifiedIdeographsExtensionC`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.CJKUnifiedIdeographsExtensionC) for more information.
         */
        CJKUnifiedIdeographsExtensionC = 197,
        /**
         * See the [Rust documentation for `Mandaic`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.Mandaic) for more information.
         */
        Mandaic = 198,
        /**
         * See the [Rust documentation for `Batak`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.Batak) for more information.
         */
        Batak = 199,
        /**
         * See the [Rust documentation for `EthiopicExtendedA`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.EthiopicExtendedA) for more information.
         */
        EthiopicExtendedA = 200,
        /**
         * See the [Rust documentation for `Brahmi`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.Brahmi) for more information.
         */
        Brahmi = 201,
        /**
         * See the [Rust documentation for `BamumSupplement`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.BamumSupplement) for more information.
         */
        BamumSupplement = 202,
        /**
         * See the [Rust documentation for `KanaSupplement`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.KanaSupplement) for more information.
         */
        KanaSupplement = 203,
        /**
         * See the [Rust documentation for `PlayingCards`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.PlayingCards) for more information.
         */
        PlayingCards = 204,
        /**
         * See the [Rust documentation for `MiscellaneousSymbolsAndPictographs`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.MiscellaneousSymbolsAndPictographs) for more information.
         */
        MiscellaneousSymbolsAndPictographs = 205,
        /**
         * See the [Rust documentation for `Emoticons`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.Emoticons) for more information.
         */
        Emoticons = 206,
        /**
         * See the [Rust documentation for `TransportAndMapSymbols`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.TransportAndMapSymbols) for more information.
         */
        TransportAndMapSymbols = 207,
        /**
         * See the [Rust documentation for `AlchemicalSymbols`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.AlchemicalSymbols) for more information.
         */
        AlchemicalSymbols = 208,
        /**
         * See the [Rust documentation for `CJKUnifiedIdeographsExtensionD`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.CJKUnifiedIdeographsExtensionD) for more information.
         */
        CJKUnifiedIdeographsExtensionD = 209,
        /**
         * See the [Rust documentation for `ArabicExtendedA`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.ArabicExtendedA) for more information.
         */
        ArabicExtendedA = 210,
        /**
         * See the [Rust documentation for `ArabicMathematicalAlphabeticSymbols`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.ArabicMathematicalAlphabeticSymbols) for more information.
         */
        ArabicMathematicalAlphabeticSymbols = 211,
        /**
         * See the [Rust documentation for `Chakma`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.Chakma) for more information.
         */
        Chakma = 212,
        /**
         * See the [Rust documentation for `MeeteiMayekExtensions`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.MeeteiMayekExtensions) for more information.
         */
        MeeteiMayekExtensions = 213,
        /**
         * See the [Rust documentation for `MeroiticCursive`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.MeroiticCursive) for more information.
         */
        MeroiticCursive = 214,
        /**
         * See the [Rust documentation for `MeroiticHieroglyphs`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.MeroiticHieroglyphs) for more information.
         */
        MeroiticHieroglyphs = 215,
        /**
         * See the [Rust documentation for `Miao`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.Miao) for more information.
         */
        Miao = 216,
        /**
         * See the [Rust documentation for `Sharada`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.Sharada) for more information.
         */
        Sharada = 217,
        /**
         * See the [Rust documentation for `SoraSompeng`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.SoraSompeng) for more information.
         */
        SoraSompeng = 218,
        /**
         * See the [Rust documentation for `SundaneseSupplement`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.SundaneseSupplement) for more information.
         */
        SundaneseSupplement = 219,
        /**
         * See the [Rust documentation for `Takri`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.Takri) for more information.
         */
        Takri = 220,
        /**
         * See the [Rust documentation for `BassaVah`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.BassaVah) for more information.
         */
        BassaVah = 221,
        /**
         * See the [Rust documentation for `CaucasianAlbanian`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.CaucasianAlbanian) for more information.
         */
        CaucasianAlbanian = 222,
        /**
         * See the [Rust documentation for `CopticEpactNumbers`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.CopticEpactNumbers) for more information.
         */
        CopticEpactNumbers = 223,
        /**
         * See the [Rust documentation for `CombiningDiacriticalMarksExtended`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.CombiningDiacriticalMarksExtended) for more information.
         */
        CombiningDiacriticalMarksExtended = 224,
        /**
         * See the [Rust documentation for `Duployan`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.Duployan) for more information.
         */
        Duployan = 225,
        /**
         * See the [Rust documentation for `Elbasan`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.Elbasan) for more information.
         */
        Elbasan = 226,
        /**
         * See the [Rust documentation for `GeometricShapesExtended`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.GeometricShapesExtended) for more information.
         */
        GeometricShapesExtended = 227,
        /**
         * See the [Rust documentation for `Grantha`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.Grantha) for more information.
         */
        Grantha = 228,
        /**
         * See the [Rust documentation for `Khojki`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.Khojki) for more information.
         */
        Khojki = 229,
        /**
         * See the [Rust documentation for `Khudawadi`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.Khudawadi) for more information.
         */
        Khudawadi = 230,
        /**
         * See the [Rust documentation for `LatinExtendedE`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.LatinExtendedE) for more information.
         */
        LatinExtendedE = 231,
        /**
         * See the [Rust documentation for `LinearA`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.LinearA) for more information.
         */
        LinearA = 232,
        /**
         * See the [Rust documentation for `Mahajani`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.Mahajani) for more information.
         */
        Mahajani = 233,
        /**
         * See the [Rust documentation for `Manichaean`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.Manichaean) for more information.
         */
        Manichaean = 234,
        /**
         * See the [Rust documentation for `MendeKikakui`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.MendeKikakui) for more information.
         */
        MendeKikakui = 235,
        /**
         * See the [Rust documentation for `Modi`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.Modi) for more information.
         */
        Modi = 236,
        /**
         * See the [Rust documentation for `Mro`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.Mro) for more information.
         */
        Mro = 237,
        /**
         * See the [Rust documentation for `MyanmarExtendedB`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.MyanmarExtendedB) for more information.
         */
        MyanmarExtendedB = 238,
        /**
         * See the [Rust documentation for `Nabataean`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.Nabataean) for more information.
         */
        Nabataean = 239,
        /**
         * See the [Rust documentation for `OldNorthArabian`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.OldNorthArabian) for more information.
         */
        OldNorthArabian = 240,
        /**
         * See the [Rust documentation for `OldPermic`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.OldPermic) for more information.
         */
        OldPermic = 241,
        /**
         * See the [Rust documentation for `OrnamentalDingbats`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.OrnamentalDingbats) for more information.
         */
        OrnamentalDingbats = 242,
        /**
         * See the [Rust documentation for `PahawhHmong`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.PahawhHmong) for more information.
         */
        PahawhHmong = 243,
        /**
         * See the [Rust documentation for `Palmyrene`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.Palmyrene) for more information.
         */
        Palmyrene = 244,
        /**
         * See the [Rust documentation for `PauCinHau`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.PauCinHau) for more information.
         */
        PauCinHau = 245,
        /**
         * See the [Rust documentation for `PsalterPahlavi`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.PsalterPahlavi) for more information.
         */
        PsalterPahlavi = 246,
        /**
         * See the [Rust documentation for `ShorthandFormatControls`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.ShorthandFormatControls) for more information.
         */
        ShorthandFormatControls = 247,
        /**
         * See the [Rust documentation for `Siddham`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.Siddham) for more information.
         */
        Siddham = 248,
        /**
         * See the [Rust documentation for `SinhalaArchaicNumbers`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.SinhalaArchaicNumbers) for more information.
         */
        SinhalaArchaicNumbers = 249,
        /**
         * See the [Rust documentation for `SupplementalArrowsC`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.SupplementalArrowsC) for more information.
         */
        SupplementalArrowsC = 250,
        /**
         * See the [Rust documentation for `Tirhuta`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.Tirhuta) for more information.
         */
        Tirhuta = 251,
        /**
         * See the [Rust documentation for `WarangCiti`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.WarangCiti) for more information.
         */
        WarangCiti = 252,
        /**
         * See the [Rust documentation for `Ahom`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.Ahom) for more information.
         */
        Ahom = 253,
        /**
         * See the [Rust documentation for `AnatolianHieroglyphs`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.AnatolianHieroglyphs) for more information.
         */
        AnatolianHieroglyphs = 254,
        /**
         * See the [Rust documentation for `CherokeeSupplement`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.CherokeeSupplement) for more information.
         */
        CherokeeSupplement = 255,
        /**
         * See the [Rust documentation for `CJKUnifiedIdeographsExtensionE`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.CJKUnifiedIdeographsExtensionE) for more information.
         */
        CJKUnifiedIdeographsExtensionE = 256,
        /**
         * See the [Rust documentation for `EarlyDynasticCuneiform`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.EarlyDynasticCuneiform) for more information.
         */
        EarlyDynasticCuneiform = 257,
        /**
         * See the [Rust documentation for `Hatran`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.Hatran) for more information.
         */
        Hatran = 258,
        /**
         * See the [Rust documentation for `Multani`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.Multani) for more information.
         */
        Multani = 259,
        /**
         * See the [Rust documentation for `OldHungarian`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.OldHungarian) for more information.
         */
        OldHungarian = 260,
        /**
         * See the [Rust documentation for `SupplementalSymbolsAndPictographs`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.SupplementalSymbolsAndPictographs) for more information.
         */
        SupplementalSymbolsAndPictographs = 261,
        /**
         * See the [Rust documentation for `SuttonSignWriting`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.SuttonSignWriting) for more information.
         */
        SuttonSignWriting = 262,
        /**
         * See the [Rust documentation for `Adlam`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.Adlam) for more information.
         */
        Adlam = 263,
        /**
         * See the [Rust documentation for `Bhaiksuki`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.Bhaiksuki) for more information.
         */
        Bhaiksuki = 264,
        /**
         * See the [Rust documentation for `CyrillicExtendedC`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.CyrillicExtendedC) for more information.
         */
        CyrillicExtendedC = 265,
        /**
         * See the [Rust documentation for `GlagoliticSupplement`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.GlagoliticSupplement) for more information.
         */
        GlagoliticSupplement = 266,
        /**
         * See the [Rust documentation for `IdeographicSymbolsAndPunctuation`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.IdeographicSymbolsAndPunctuation) for more information.
         */
        IdeographicSymbolsAndPunctuation = 267,
        /**
         * See the [Rust documentation for `Marchen`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.Marchen) for more information.
         */
        Marchen = 268,
        /**
         * See the [Rust documentation for `MongolianSupplement`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.MongolianSupplement) for more information.
         */
        MongolianSupplement = 269,
        /**
         * See the [Rust documentation for `Newa`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.Newa) for more information.
         */
        Newa = 270,
        /**
         * See the [Rust documentation for `Osage`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.Osage) for more information.
         */
        Osage = 271,
        /**
         * See the [Rust documentation for `Tangut`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.Tangut) for more information.
         */
        Tangut = 272,
        /**
         * See the [Rust documentation for `TangutComponents`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.TangutComponents) for more information.
         */
        TangutComponents = 273,
        /**
         * See the [Rust documentation for `CJKUnifiedIdeographsExtensionF`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.CJKUnifiedIdeographsExtensionF) for more information.
         */
        CJKUnifiedIdeographsExtensionF = 274,
        /**
         * See the [Rust documentation for `KanaExtendedA`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.KanaExtendedA) for more information.
         */
        KanaExtendedA = 275,
        /**
         * See the [Rust documentation for `MasaramGondi`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.MasaramGondi) for more information.
         */
        MasaramGondi = 276,
        /**
         * See the [Rust documentation for `Nushu`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.Nushu) for more information.
         */
        Nushu = 277,
        /**
         * See the [Rust documentation for `Soyombo`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.Soyombo) for more information.
         */
        Soyombo = 278,
        /**
         * See the [Rust documentation for `SyriacSupplement`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.SyriacSupplement) for more information.
         */
        SyriacSupplement = 279,
        /**
         * See the [Rust documentation for `ZanabazarSquare`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.ZanabazarSquare) for more information.
         */
        ZanabazarSquare = 280,
        /**
         * See the [Rust documentation for `ChessSymbols`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.ChessSymbols) for more information.
         */
        ChessSymbols = 281,
        /**
         * See the [Rust documentation for `Dogra`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.Dogra) for more information.
         */
        Dogra = 282,
        /**
         * See the [Rust documentation for `GeorgianExtended`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.GeorgianExtended) for more information.
         */
        GeorgianExtended = 283,
        /**
         * See the [Rust documentation for `GunjalaGondi`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.GunjalaGondi) for more information.
         */
        GunjalaGondi = 284,
        /**
         * See the [Rust documentation for `HanifiRohingya`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.HanifiRohingya) for more information.
         */
        HanifiRohingya = 285,
        /**
         * See the [Rust documentation for `IndicSiyaqNumbers`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.IndicSiyaqNumbers) for more information.
         */
        IndicSiyaqNumbers = 286,
        /**
         * See the [Rust documentation for `Makasar`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.Makasar) for more information.
         */
        Makasar = 287,
        /**
         * See the [Rust documentation for `MayanNumerals`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.MayanNumerals) for more information.
         */
        MayanNumerals = 288,
        /**
         * See the [Rust documentation for `Medefaidrin`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.Medefaidrin) for more information.
         */
        Medefaidrin = 289,
        /**
         * See the [Rust documentation for `OldSogdian`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.OldSogdian) for more information.
         */
        OldSogdian = 290,
        /**
         * See the [Rust documentation for `Sogdian`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.Sogdian) for more information.
         */
        Sogdian = 291,
        /**
         * See the [Rust documentation for `EgyptianHieroglyphFormatControls`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.EgyptianHieroglyphFormatControls) for more information.
         */
        EgyptianHieroglyphFormatControls = 292,
        /**
         * See the [Rust documentation for `Elymaic`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.Elymaic) for more information.
         */
        Elymaic = 293,
        /**
         * See the [Rust documentation for `Nandinagari`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.Nandinagari) for more information.
         */
        Nandinagari = 294,
        /**
         * See the [Rust documentation for `NyiakengPuachueHmong`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.NyiakengPuachueHmong) for more information.
         */
        NyiakengPuachueHmong = 295,
        /**
         * See the [Rust documentation for `OttomanSiyaqNumbers`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.OttomanSiyaqNumbers) for more information.
         */
        OttomanSiyaqNumbers = 296,
        /**
         * See the [Rust documentation for `SmallKanaExtension`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.SmallKanaExtension) for more information.
         */
        SmallKanaExtension = 297,
        /**
         * See the [Rust documentation for `SymbolsAndPictographsExtendedA`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.SymbolsAndPictographsExtendedA) for more information.
         */
        SymbolsAndPictographsExtendedA = 298,
        /**
         * See the [Rust documentation for `TamilSupplement`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.TamilSupplement) for more information.
         */
        TamilSupplement = 299,
        /**
         * See the [Rust documentation for `Wancho`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.Wancho) for more information.
         */
        Wancho = 300,
        /**
         * See the [Rust documentation for `Chorasmian`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.Chorasmian) for more information.
         */
        Chorasmian = 301,
        /**
         * See the [Rust documentation for `CJKUnifiedIdeographsExtensionG`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.CJKUnifiedIdeographsExtensionG) for more information.
         */
        CJKUnifiedIdeographsExtensionG = 302,
        /**
         * See the [Rust documentation for `DivesAkuru`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.DivesAkuru) for more information.
         */
        DivesAkuru = 303,
        /**
         * See the [Rust documentation for `KhitanSmallScript`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.KhitanSmallScript) for more information.
         */
        KhitanSmallScript = 304,
        /**
         * See the [Rust documentation for `LisuSupplement`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.LisuSupplement) for more information.
         */
        LisuSupplement = 305,
        /**
         * See the [Rust documentation for `SymbolsForLegacyComputing`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.SymbolsForLegacyComputing) for more information.
         */
        SymbolsForLegacyComputing = 306,
        /**
         * See the [Rust documentation for `TangutSupplement`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.TangutSupplement) for more information.
         */
        TangutSupplement = 307,
        /**
         * See the [Rust documentation for `Yezidi`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.Yezidi) for more information.
         */
        Yezidi = 308,
        /**
         * See the [Rust documentation for `ArabicExtendedB`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.ArabicExtendedB) for more information.
         */
        ArabicExtendedB = 309,
        /**
         * See the [Rust documentation for `CyproMinoan`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.CyproMinoan) for more information.
         */
        CyproMinoan = 310,
        /**
         * See the [Rust documentation for `EthiopicExtendedB`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.EthiopicExtendedB) for more information.
         */
        EthiopicExtendedB = 311,
        /**
         * See the [Rust documentation for `KanaExtendedB`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.KanaExtendedB) for more information.
         */
        KanaExtendedB = 312,
        /**
         * See the [Rust documentation for `LatinExtendedF`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.LatinExtendedF) for more information.
         */
        LatinExtendedF = 313,
        /**
         * See the [Rust documentation for `LatinExtendedG`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.LatinExtendedG) for more information.
         */
        LatinExtendedG = 314,
        /**
         * See the [Rust documentation for `OldUyghur`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.OldUyghur) for more information.
         */
        OldUyghur = 315,
        /**
         * See the [Rust documentation for `Tangsa`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.Tangsa) for more information.
         */
        Tangsa = 316,
        /**
         * See the [Rust documentation for `Toto`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.Toto) for more information.
         */
        Toto = 317,
        /**
         * See the [Rust documentation for `UnifiedCanadianAboriginalSyllabicsExtendedA`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.UnifiedCanadianAboriginalSyllabicsExtendedA) for more information.
         */
        UnifiedCanadianAboriginalSyllabicsExtendedA = 318,
        /**
         * See the [Rust documentation for `Vithkuqi`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.Vithkuqi) for more information.
         */
        Vithkuqi = 319,
        /**
         * See the [Rust documentation for `ZnamennyMusicalNotation`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.ZnamennyMusicalNotation) for more information.
         */
        ZnamennyMusicalNotation = 320,
        /**
         * See the [Rust documentation for `ArabicExtendedC`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.ArabicExtendedC) for more information.
         */
        ArabicExtendedC = 321,
        /**
         * See the [Rust documentation for `CJKUnifiedIdeographsExtensionH`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.CJKUnifiedIdeographsExtensionH) for more information.
         */
        CJKUnifiedIdeographsExtensionH = 322,
        /**
         * See the [Rust documentation for `CyrillicExtendedD`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.CyrillicExtendedD) for more information.
         */
        CyrillicExtendedD = 323,
        /**
         * See the [Rust documentation for `DevanagariExtendedA`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.DevanagariExtendedA) for more information.
         */
        DevanagariExtendedA = 324,
        /**
         * See the [Rust documentation for `KaktovikNumerals`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.KaktovikNumerals) for more information.
         */
        KaktovikNumerals = 325,
        /**
         * See the [Rust documentation for `Kawi`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.Kawi) for more information.
         */
        Kawi = 326,
        /**
         * See the [Rust documentation for `NagMundari`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.NagMundari) for more information.
         */
        NagMundari = 327,
        /**
         * See the [Rust documentation for `CJKUnifiedIdeographsExtensionI`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.CJKUnifiedIdeographsExtensionI) for more information.
         */
        CJKUnifiedIdeographsExtensionI = 328,
        /**
         * See the [Rust documentation for `EgyptianHieroglyphsExtendedA`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.EgyptianHieroglyphsExtendedA) for more information.
         */
        EgyptianHieroglyphsExtendedA = 329,
        /**
         * See the [Rust documentation for `Garay`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.Garay) for more information.
         */
        Garay = 330,
        /**
         * See the [Rust documentation for `GurungKhema`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.GurungKhema) for more information.
         */
        GurungKhema = 331,
        /**
         * See the [Rust documentation for `KiratRai`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.KiratRai) for more information.
         */
        KiratRai = 332,
        /**
         * See the [Rust documentation for `MyanmarExtendedC`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.MyanmarExtendedC) for more information.
         */
        MyanmarExtendedC = 333,
        /**
         * See the [Rust documentation for `OlOnal`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.OlOnal) for more information.
         */
        OlOnal = 334,
        /**
         * See the [Rust documentation for `Sunuwar`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.Sunuwar) for more information.
         */
        Sunuwar = 335,
        /**
         * See the [Rust documentation for `SymbolsForLegacyComputingSupplement`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.SymbolsForLegacyComputingSupplement) for more information.
         */
        SymbolsForLegacyComputingSupplement = 336,
        /**
         * See the [Rust documentation for `Todhri`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.Todhri) for more information.
         */
        Todhri = 337,
        /**
         * See the [Rust documentation for `TuluTigalari`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.TuluTigalari) for more information.
         */
        TuluTigalari = 338,
        /**
         * See the [Rust documentation for `BeriaErfe`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.BeriaErfe) for more information.
         */
        BeriaErfe = 339,
        /**
         * See the [Rust documentation for `CJKUnifiedIdeographsExtensionJ`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.CJKUnifiedIdeographsExtensionJ) for more information.
         */
        CJKUnifiedIdeographsExtensionJ = 340,
        /**
         * See the [Rust documentation for `MiscellaneousSymbolsSupplement`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.MiscellaneousSymbolsSupplement) for more information.
         */
        MiscellaneousSymbolsSupplement = 341,
        /**
         * See the [Rust documentation for `SharadaSupplement`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.SharadaSupplement) for more information.
         */
        SharadaSupplement = 342,
        /**
         * See the [Rust documentation for `Sidetic`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.Sidetic) for more information.
         */
        Sidetic = 343,
        /**
         * See the [Rust documentation for `TaiYo`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.TaiYo) for more information.
         */
        TaiYo = 344,
        /**
         * See the [Rust documentation for `TangutComponentsSupplement`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.TangutComponentsSupplement) for more information.
         */
        TangutComponentsSupplement = 345,
        /**
         * See the [Rust documentation for `TolongSiki`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.TolongSiki) for more information.
         */
        TolongSiki = 346,
        /**
         * See the [Rust documentation for `ArchaicCuneiformNumerals`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.ArchaicCuneiformNumerals) for more information.
         */
        ArchaicCuneiformNumerals = 347,
        /**
         * See the [Rust documentation for `BengaliSupplement`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.BengaliSupplement) for more information.
         */
        BengaliSupplement = 348,
        /**
         * See the [Rust documentation for `Jurchen`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.Jurchen) for more information.
         */
        Jurchen = 349,
        /**
         * See the [Rust documentation for `JurchenRadicals`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.JurchenRadicals) for more information.
         */
        JurchenRadicals = 350,
        /**
         * See the [Rust documentation for `MiscellaneousSymbolsAndArrowsExtended`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.MiscellaneousSymbolsAndArrowsExtended) for more information.
         */
        MiscellaneousSymbolsAndArrowsExtended = 351,
        /**
         * See the [Rust documentation for `MusicalSymbolsSupplement`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.MusicalSymbolsSupplement) for more information.
         */
        MusicalSymbolsSupplement = 352,
        /**
         * See the [Rust documentation for `Seal`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#associatedconstant.Seal) for more information.
         */
        Seal = 353,
    };

    Block(): value(Value::NoBlock) {}

    // Implicit conversions between enum and ::Value
    constexpr Block(Value v) : value(v) {}
    constexpr operator Value() const { return value; }
    // Prevent usage as boolean value
    explicit operator bool() const = delete;

  /**
   * See the [Rust documentation for `for_char`](https://docs.rs/icu/2.4.0/icu/properties/props/trait.EnumeratedProperty.html#tymethod.for_char) for more information.
   */
  inline static icu4x::Block for_char(char32_t ch);

  /**
   * Get the "long" name of this property value (returns empty if property value is unknown)
   *
   * See the [Rust documentation for `get`](https://docs.rs/icu/2.4.0/icu/properties/struct.PropertyNamesLongBorrowed.html#method.get) for more information.
   */
  inline std::optional<std::string_view> long_name() const;

  /**
   * Get the "short" name of this property value (returns empty if property value is unknown)
   *
   * See the [Rust documentation for `get`](https://docs.rs/icu/2.4.0/icu/properties/struct.PropertyNamesShortBorrowed.html#method.get) for more information.
   */
  inline std::optional<std::string_view> short_name() const;

  /**
   * Convert to an integer value usable with ICU4C and `CodePointMapData`
   *
   * See the [Rust documentation for `to_icu4c_value`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#method.to_icu4c_value) for more information.
   */
  inline uint16_t to_integer_value() const;

  /**
   * Convert from an integer value from ICU4C or `CodePointMapData`
   *
   * See the [Rust documentation for `from_icu4c_value`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Block.html#method.from_icu4c_value) for more information.
   */
  inline static std::optional<icu4x::Block> from_integer_value(uint16_t other);

  /**
   * Creates a `Block` from a string.
   *
   * Short names, long names, and aliases are supported, and matching is case-insensitive.
   */
  inline static std::optional<icu4x::Block> try_from_str(std::string_view s);

    inline icu4x::capi::Block AsFFI() const;
    inline static icu4x::Block FromFFI(icu4x::capi::Block c_enum);
private:
    Value value;
};

} // namespace
#endif // ICU4X_Block_D_HPP

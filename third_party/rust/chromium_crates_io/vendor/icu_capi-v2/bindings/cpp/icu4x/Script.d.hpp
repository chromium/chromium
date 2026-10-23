#ifndef ICU4X_Script_D_HPP
#define ICU4X_Script_D_HPP

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
class Script;
} // namespace icu4x



namespace icu4x {
namespace capi {
    enum Script {
      Script_Common = 0,
      Script_Inherited = 1,
      Script_Arabic = 2,
      Script_Armenian = 3,
      Script_Bengali = 4,
      Script_Bopomofo = 5,
      Script_Cherokee = 6,
      Script_Coptic = 7,
      Script_Cyrillic = 8,
      Script_Deseret = 9,
      Script_Devanagari = 10,
      Script_Ethiopian = 11,
      Script_Georgian = 12,
      Script_Gothic = 13,
      Script_Greek = 14,
      Script_Gujarati = 15,
      Script_Gurmukhi = 16,
      Script_Han = 17,
      Script_Hangul = 18,
      Script_Hebrew = 19,
      Script_Hiragana = 20,
      Script_Kannada = 21,
      Script_Katakana = 22,
      Script_Khmer = 23,
      Script_Lao = 24,
      Script_Latin = 25,
      Script_Malayalam = 26,
      Script_Mongolian = 27,
      Script_Myanmar = 28,
      Script_Ogham = 29,
      Script_OldItalic = 30,
      Script_Oriya = 31,
      Script_Runic = 32,
      Script_Sinhala = 33,
      Script_Syriac = 34,
      Script_Tamil = 35,
      Script_Telugu = 36,
      Script_Thaana = 37,
      Script_Thai = 38,
      Script_Tibetan = 39,
      Script_CanadianAboriginal = 40,
      Script_Yi = 41,
      Script_Tagalog = 42,
      Script_Hanunoo = 43,
      Script_Buhid = 44,
      Script_Tagbanwa = 45,
      Script_Braille = 46,
      Script_Cypriot = 47,
      Script_Limbu = 48,
      Script_LinearB = 49,
      Script_Osmanya = 50,
      Script_Shavian = 51,
      Script_TaiLe = 52,
      Script_Ugaritic = 53,
      Script_KatakanaOrHiragana = 54,
      Script_Buginese = 55,
      Script_Glagolitic = 56,
      Script_Kharoshthi = 57,
      Script_SylotiNagri = 58,
      Script_NewTaiLue = 59,
      Script_Tifinagh = 60,
      Script_OldPersian = 61,
      Script_Balinese = 62,
      Script_Batak = 63,
      Script_BlisSymbols = 64,
      Script_Brahmi = 65,
      Script_Cham = 66,
      Script_Cirth = 67,
      Script_OldChurchSlavonicCyrillic = 68,
      Script_DemoticEgyptian = 69,
      Script_HieraticEgyptian = 70,
      Script_EgyptianHieroglyphs = 71,
      Script_Khutsuri = 72,
      Script_SimplifiedHan = 73,
      Script_TraditionalHan = 74,
      Script_PahawhHmong = 75,
      Script_OldHungarian = 76,
      Script_HarappanIndus = 77,
      Script_Javanese = 78,
      Script_KayahLi = 79,
      Script_LatinFraktur = 80,
      Script_LatinGaelic = 81,
      Script_Lepcha = 82,
      Script_LinearA = 83,
      Script_Mandaic = 84,
      Script_MayanHieroglyphs = 85,
      Script_MeroiticHieroglyphs = 86,
      Script_Nko = 87,
      Script_OldTurkic = 88,
      Script_OldPermic = 89,
      Script_PhagsPa = 90,
      Script_Phoenician = 91,
      Script_Miao = 92,
      Script_Rongorongo = 93,
      Script_Sarati = 94,
      Script_EstrangeloSyriac = 95,
      Script_WesternSyriac = 96,
      Script_EasternSyriac = 97,
      Script_Tengwar = 98,
      Script_Vai = 99,
      Script_VisibleSpeech = 100,
      Script_Cuneiform = 101,
      Script_UnwrittenLanguages = 102,
      Script_Unknown = 103,
      Script_Carian = 104,
      Script_Japanese = 105,
      Script_TaiTham = 106,
      Script_Lycian = 107,
      Script_Lydian = 108,
      Script_OlChiki = 109,
      Script_Rejang = 110,
      Script_Saurashtra = 111,
      Script_SignWriting = 112,
      Script_Sundanese = 113,
      Script_Moon = 114,
      Script_MeeteiMayek = 115,
      Script_ImperialAramaic = 116,
      Script_Avestan = 117,
      Script_Chakma = 118,
      Script_Korean = 119,
      Script_Kaithi = 120,
      Script_Manichaean = 121,
      Script_InscriptionalPahlavi = 122,
      Script_PsalterPahlavi = 123,
      Script_BookPahlavi = 124,
      Script_InscriptionalParthian = 125,
      Script_Samaritan = 126,
      Script_TaiViet = 127,
      Script_MathematicalNotation = 128,
      Script_Symbols = 129,
      Script_Bamum = 130,
      Script_Lisu = 131,
      Script_NakhiGeba = 132,
      Script_OldSouthArabian = 133,
      Script_BassaVah = 134,
      Script_Duployan = 135,
      Script_Elbasan = 136,
      Script_Grantha = 137,
      Script_Kpelle = 138,
      Script_Loma = 139,
      Script_MendeKikakui = 140,
      Script_MeroiticCursive = 141,
      Script_OldNorthArabian = 142,
      Script_Nabataean = 143,
      Script_Palmyrene = 144,
      Script_Khudawadi = 145,
      Script_WarangCiti = 146,
      Script_Afaka = 147,
      Script_Jurchen = 148,
      Script_Mro = 149,
      Script_Nushu = 150,
      Script_Sharada = 151,
      Script_SoraSompeng = 152,
      Script_Takri = 153,
      Script_Tangut = 154,
      Script_Woleai = 155,
      Script_AnatolianHieroglyphs = 156,
      Script_Khojki = 157,
      Script_Tirhuta = 158,
      Script_CaucasianAlbanian = 159,
      Script_Mahajani = 160,
      Script_Ahom = 161,
      Script_Hatran = 162,
      Script_Modi = 163,
      Script_Multani = 164,
      Script_PauCinHau = 165,
      Script_Siddham = 166,
      Script_Adlam = 167,
      Script_Bhaiksuki = 168,
      Script_Marchen = 169,
      Script_Newa = 170,
      Script_Osage = 171,
      Script_HanWithBopomofo = 172,
      Script_Jamo = 173,
      Script_SymbolsEmoji = 174,
      Script_MasaramGondi = 175,
      Script_Soyombo = 176,
      Script_ZanabazarSquare = 177,
      Script_Dogra = 178,
      Script_GunjalaGondi = 179,
      Script_Makasar = 180,
      Script_Medefaidrin = 181,
      Script_HanifiRohingya = 182,
      Script_Sogdian = 183,
      Script_OldSogdian = 184,
      Script_Elymaic = 185,
      Script_NyiakengPuachueHmong = 186,
      Script_Nandinagari = 187,
      Script_Wancho = 188,
      Script_Chorasmian = 189,
      Script_DivesAkuru = 190,
      Script_KhitanSmallScript = 191,
      Script_Yezidi = 192,
      Script_CyproMinoan = 193,
      Script_OldUyghur = 194,
      Script_Tangsa = 195,
      Script_Toto = 196,
      Script_Vithkuqi = 197,
      Script_Kawi = 198,
      Script_NagMundari = 199,
      Script_Nastaliq = 200,
      Script_Garay = 201,
      Script_GurungKhema = 202,
      Script_KiratRai = 203,
      Script_OlOnal = 204,
      Script_Sunuwar = 205,
      Script_Todhri = 206,
      Script_TuluTigalari = 207,
      Script_BeriaErfe = 208,
      Script_Sidetic = 209,
      Script_TaiYo = 210,
      Script_TolongSiki = 211,
      Script_TraditionalHanWithLatin = 212,
      Script_ProtoCuneiform = 213,
      Script_Seal = 214,
      Script_Chisoi = 254,
    };

    typedef struct Script_option {union { Script ok; }; bool is_ok; } Script_option;
} // namespace capi
} // namespace

namespace icu4x {
/**
 * See the [Rust documentation for `Script`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html) for more information.
 */
class Script {
public:
    enum Value {
        /**
         * See the [Rust documentation for `Common`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.Common) for more information.
         */
        Common = 0,
        /**
         * See the [Rust documentation for `Inherited`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.Inherited) for more information.
         */
        Inherited = 1,
        /**
         * See the [Rust documentation for `Arabic`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.Arabic) for more information.
         */
        Arabic = 2,
        /**
         * See the [Rust documentation for `Armenian`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.Armenian) for more information.
         */
        Armenian = 3,
        /**
         * See the [Rust documentation for `Bengali`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.Bengali) for more information.
         */
        Bengali = 4,
        /**
         * See the [Rust documentation for `Bopomofo`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.Bopomofo) for more information.
         */
        Bopomofo = 5,
        /**
         * See the [Rust documentation for `Cherokee`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.Cherokee) for more information.
         */
        Cherokee = 6,
        /**
         * See the [Rust documentation for `Coptic`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.Coptic) for more information.
         */
        Coptic = 7,
        /**
         * See the [Rust documentation for `Cyrillic`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.Cyrillic) for more information.
         */
        Cyrillic = 8,
        /**
         * See the [Rust documentation for `Deseret`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.Deseret) for more information.
         */
        Deseret = 9,
        /**
         * See the [Rust documentation for `Devanagari`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.Devanagari) for more information.
         */
        Devanagari = 10,
        /**
         * See the [Rust documentation for `Ethiopic`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.Ethiopic) for more information.
         */
        Ethiopian = 11,
        /**
         * See the [Rust documentation for `Georgian`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.Georgian) for more information.
         */
        Georgian = 12,
        /**
         * See the [Rust documentation for `Gothic`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.Gothic) for more information.
         */
        Gothic = 13,
        /**
         * See the [Rust documentation for `Greek`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.Greek) for more information.
         */
        Greek = 14,
        /**
         * See the [Rust documentation for `Gujarati`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.Gujarati) for more information.
         */
        Gujarati = 15,
        /**
         * See the [Rust documentation for `Gurmukhi`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.Gurmukhi) for more information.
         */
        Gurmukhi = 16,
        /**
         * See the [Rust documentation for `Han`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.Han) for more information.
         */
        Han = 17,
        /**
         * See the [Rust documentation for `Hangul`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.Hangul) for more information.
         */
        Hangul = 18,
        /**
         * See the [Rust documentation for `Hebrew`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.Hebrew) for more information.
         */
        Hebrew = 19,
        /**
         * See the [Rust documentation for `Hiragana`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.Hiragana) for more information.
         */
        Hiragana = 20,
        /**
         * See the [Rust documentation for `Kannada`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.Kannada) for more information.
         */
        Kannada = 21,
        /**
         * See the [Rust documentation for `Katakana`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.Katakana) for more information.
         */
        Katakana = 22,
        /**
         * See the [Rust documentation for `Khmer`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.Khmer) for more information.
         */
        Khmer = 23,
        /**
         * See the [Rust documentation for `Lao`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.Lao) for more information.
         */
        Lao = 24,
        /**
         * See the [Rust documentation for `Latin`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.Latin) for more information.
         */
        Latin = 25,
        /**
         * See the [Rust documentation for `Malayalam`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.Malayalam) for more information.
         */
        Malayalam = 26,
        /**
         * See the [Rust documentation for `Mongolian`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.Mongolian) for more information.
         */
        Mongolian = 27,
        /**
         * See the [Rust documentation for `Myanmar`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.Myanmar) for more information.
         */
        Myanmar = 28,
        /**
         * See the [Rust documentation for `Ogham`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.Ogham) for more information.
         */
        Ogham = 29,
        /**
         * See the [Rust documentation for `OldItalic`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.OldItalic) for more information.
         */
        OldItalic = 30,
        /**
         * See the [Rust documentation for `Oriya`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.Oriya) for more information.
         */
        Oriya = 31,
        /**
         * See the [Rust documentation for `Runic`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.Runic) for more information.
         */
        Runic = 32,
        /**
         * See the [Rust documentation for `Sinhala`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.Sinhala) for more information.
         */
        Sinhala = 33,
        /**
         * See the [Rust documentation for `Syriac`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.Syriac) for more information.
         */
        Syriac = 34,
        /**
         * See the [Rust documentation for `Tamil`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.Tamil) for more information.
         */
        Tamil = 35,
        /**
         * See the [Rust documentation for `Telugu`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.Telugu) for more information.
         */
        Telugu = 36,
        /**
         * See the [Rust documentation for `Thaana`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.Thaana) for more information.
         */
        Thaana = 37,
        /**
         * See the [Rust documentation for `Thai`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.Thai) for more information.
         */
        Thai = 38,
        /**
         * See the [Rust documentation for `Tibetan`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.Tibetan) for more information.
         */
        Tibetan = 39,
        /**
         * See the [Rust documentation for `CanadianAboriginal`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.CanadianAboriginal) for more information.
         */
        CanadianAboriginal = 40,
        /**
         * See the [Rust documentation for `Yi`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.Yi) for more information.
         */
        Yi = 41,
        /**
         * See the [Rust documentation for `Tagalog`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.Tagalog) for more information.
         */
        Tagalog = 42,
        /**
         * See the [Rust documentation for `Hanunoo`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.Hanunoo) for more information.
         */
        Hanunoo = 43,
        /**
         * See the [Rust documentation for `Buhid`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.Buhid) for more information.
         */
        Buhid = 44,
        /**
         * See the [Rust documentation for `Tagbanwa`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.Tagbanwa) for more information.
         */
        Tagbanwa = 45,
        /**
         * See the [Rust documentation for `Braille`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.Braille) for more information.
         */
        Braille = 46,
        /**
         * See the [Rust documentation for `Cypriot`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.Cypriot) for more information.
         */
        Cypriot = 47,
        /**
         * See the [Rust documentation for `Limbu`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.Limbu) for more information.
         */
        Limbu = 48,
        /**
         * See the [Rust documentation for `LinearB`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.LinearB) for more information.
         */
        LinearB = 49,
        /**
         * See the [Rust documentation for `Osmanya`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.Osmanya) for more information.
         */
        Osmanya = 50,
        /**
         * See the [Rust documentation for `Shavian`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.Shavian) for more information.
         */
        Shavian = 51,
        /**
         * See the [Rust documentation for `TaiLe`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.TaiLe) for more information.
         */
        TaiLe = 52,
        /**
         * See the [Rust documentation for `Ugaritic`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.Ugaritic) for more information.
         */
        Ugaritic = 53,
        /**
         * See the [Rust documentation for `KatakanaOrHiragana`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.KatakanaOrHiragana) for more information.
         */
        KatakanaOrHiragana = 54,
        /**
         * See the [Rust documentation for `Buginese`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.Buginese) for more information.
         */
        Buginese = 55,
        /**
         * See the [Rust documentation for `Glagolitic`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.Glagolitic) for more information.
         */
        Glagolitic = 56,
        /**
         * See the [Rust documentation for `Kharoshthi`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.Kharoshthi) for more information.
         */
        Kharoshthi = 57,
        /**
         * See the [Rust documentation for `SylotiNagri`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.SylotiNagri) for more information.
         */
        SylotiNagri = 58,
        /**
         * See the [Rust documentation for `NewTaiLue`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.NewTaiLue) for more information.
         */
        NewTaiLue = 59,
        /**
         * See the [Rust documentation for `Tifinagh`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.Tifinagh) for more information.
         */
        Tifinagh = 60,
        /**
         * See the [Rust documentation for `OldPersian`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.OldPersian) for more information.
         */
        OldPersian = 61,
        /**
         * See the [Rust documentation for `Balinese`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.Balinese) for more information.
         */
        Balinese = 62,
        /**
         * See the [Rust documentation for `Batak`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.Batak) for more information.
         */
        Batak = 63,
        /**
         * See the [Rust documentation for `Blissymbols`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.Blissymbols) for more information.
         */
        BlisSymbols = 64,
        /**
         * See the [Rust documentation for `Brahmi`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.Brahmi) for more information.
         */
        Brahmi = 65,
        /**
         * See the [Rust documentation for `Cham`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.Cham) for more information.
         */
        Cham = 66,
        /**
         * See the [Rust documentation for `Cirth`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.Cirth) for more information.
         */
        Cirth = 67,
        /**
         * See the [Rust documentation for `OldChurchSlavonicCyrillic`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.OldChurchSlavonicCyrillic) for more information.
         */
        OldChurchSlavonicCyrillic = 68,
        /**
         * See the [Rust documentation for `DemoticEgyptian`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.DemoticEgyptian) for more information.
         */
        DemoticEgyptian = 69,
        /**
         * See the [Rust documentation for `HieraticEgyptian`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.HieraticEgyptian) for more information.
         */
        HieraticEgyptian = 70,
        /**
         * See the [Rust documentation for `EgyptianHieroglyphs`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.EgyptianHieroglyphs) for more information.
         */
        EgyptianHieroglyphs = 71,
        /**
         * See the [Rust documentation for `Khutsuri`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.Khutsuri) for more information.
         */
        Khutsuri = 72,
        /**
         * See the [Rust documentation for `SimplifiedHan`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.SimplifiedHan) for more information.
         */
        SimplifiedHan = 73,
        /**
         * See the [Rust documentation for `TraditionalHan`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.TraditionalHan) for more information.
         */
        TraditionalHan = 74,
        /**
         * See the [Rust documentation for `PahawhHmong`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.PahawhHmong) for more information.
         */
        PahawhHmong = 75,
        /**
         * See the [Rust documentation for `OldHungarian`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.OldHungarian) for more information.
         */
        OldHungarian = 76,
        /**
         * See the [Rust documentation for `HarappanIndus`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.HarappanIndus) for more information.
         */
        HarappanIndus = 77,
        /**
         * See the [Rust documentation for `Javanese`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.Javanese) for more information.
         */
        Javanese = 78,
        /**
         * See the [Rust documentation for `KayahLi`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.KayahLi) for more information.
         */
        KayahLi = 79,
        /**
         * See the [Rust documentation for `LatinFraktur`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.LatinFraktur) for more information.
         */
        LatinFraktur = 80,
        /**
         * See the [Rust documentation for `LatinGaelic`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.LatinGaelic) for more information.
         */
        LatinGaelic = 81,
        /**
         * See the [Rust documentation for `Lepcha`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.Lepcha) for more information.
         */
        Lepcha = 82,
        /**
         * See the [Rust documentation for `LinearA`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.LinearA) for more information.
         */
        LinearA = 83,
        /**
         * See the [Rust documentation for `Mandaic`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.Mandaic) for more information.
         */
        Mandaic = 84,
        /**
         * See the [Rust documentation for `MayanHieroglyphs`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.MayanHieroglyphs) for more information.
         */
        MayanHieroglyphs = 85,
        /**
         * See the [Rust documentation for `MeroiticHieroglyphs`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.MeroiticHieroglyphs) for more information.
         */
        MeroiticHieroglyphs = 86,
        /**
         * See the [Rust documentation for `Nko`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.Nko) for more information.
         */
        Nko = 87,
        /**
         * See the [Rust documentation for `OldTurkic`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.OldTurkic) for more information.
         */
        OldTurkic = 88,
        /**
         * See the [Rust documentation for `OldPermic`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.OldPermic) for more information.
         */
        OldPermic = 89,
        /**
         * See the [Rust documentation for `PhagsPa`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.PhagsPa) for more information.
         */
        PhagsPa = 90,
        /**
         * See the [Rust documentation for `Phoenician`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.Phoenician) for more information.
         */
        Phoenician = 91,
        /**
         * See the [Rust documentation for `Miao`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.Miao) for more information.
         */
        Miao = 92,
        /**
         * See the [Rust documentation for `Rongorongo`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.Rongorongo) for more information.
         */
        Rongorongo = 93,
        /**
         * See the [Rust documentation for `Sarati`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.Sarati) for more information.
         */
        Sarati = 94,
        /**
         * See the [Rust documentation for `EstrangeloSyriac`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.EstrangeloSyriac) for more information.
         */
        EstrangeloSyriac = 95,
        /**
         * See the [Rust documentation for `WesternSyriac`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.WesternSyriac) for more information.
         */
        WesternSyriac = 96,
        /**
         * See the [Rust documentation for `EasternSyriac`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.EasternSyriac) for more information.
         */
        EasternSyriac = 97,
        /**
         * See the [Rust documentation for `Tengwar`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.Tengwar) for more information.
         */
        Tengwar = 98,
        /**
         * See the [Rust documentation for `Vai`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.Vai) for more information.
         */
        Vai = 99,
        /**
         * See the [Rust documentation for `VisibleSpeech`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.VisibleSpeech) for more information.
         */
        VisibleSpeech = 100,
        /**
         * See the [Rust documentation for `Cuneiform`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.Cuneiform) for more information.
         */
        Cuneiform = 101,
        /**
         * See the [Rust documentation for `UnwrittenLanguages`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.UnwrittenLanguages) for more information.
         */
        UnwrittenLanguages = 102,
        /**
         * See the [Rust documentation for `Unknown`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.Unknown) for more information.
         */
        Unknown = 103,
        /**
         * See the [Rust documentation for `Carian`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.Carian) for more information.
         */
        Carian = 104,
        /**
         * See the [Rust documentation for `Japanese`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.Japanese) for more information.
         */
        Japanese = 105,
        /**
         * See the [Rust documentation for `TaiTham`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.TaiTham) for more information.
         */
        TaiTham = 106,
        /**
         * See the [Rust documentation for `Lycian`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.Lycian) for more information.
         */
        Lycian = 107,
        /**
         * See the [Rust documentation for `Lydian`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.Lydian) for more information.
         */
        Lydian = 108,
        /**
         * See the [Rust documentation for `OlChiki`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.OlChiki) for more information.
         */
        OlChiki = 109,
        /**
         * See the [Rust documentation for `Rejang`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.Rejang) for more information.
         */
        Rejang = 110,
        /**
         * See the [Rust documentation for `Saurashtra`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.Saurashtra) for more information.
         */
        Saurashtra = 111,
        /**
         * See the [Rust documentation for `SignWriting`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.SignWriting) for more information.
         */
        SignWriting = 112,
        /**
         * See the [Rust documentation for `Sundanese`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.Sundanese) for more information.
         */
        Sundanese = 113,
        /**
         * See the [Rust documentation for `Moon`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.Moon) for more information.
         */
        Moon = 114,
        /**
         * See the [Rust documentation for `MeeteiMayek`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.MeeteiMayek) for more information.
         */
        MeeteiMayek = 115,
        /**
         * See the [Rust documentation for `ImperialAramaic`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.ImperialAramaic) for more information.
         */
        ImperialAramaic = 116,
        /**
         * See the [Rust documentation for `Avestan`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.Avestan) for more information.
         */
        Avestan = 117,
        /**
         * See the [Rust documentation for `Chakma`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.Chakma) for more information.
         */
        Chakma = 118,
        /**
         * See the [Rust documentation for `Korean`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.Korean) for more information.
         */
        Korean = 119,
        /**
         * See the [Rust documentation for `Kaithi`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.Kaithi) for more information.
         */
        Kaithi = 120,
        /**
         * See the [Rust documentation for `Manichaean`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.Manichaean) for more information.
         */
        Manichaean = 121,
        /**
         * See the [Rust documentation for `InscriptionalPahlavi`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.InscriptionalPahlavi) for more information.
         */
        InscriptionalPahlavi = 122,
        /**
         * See the [Rust documentation for `PsalterPahlavi`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.PsalterPahlavi) for more information.
         */
        PsalterPahlavi = 123,
        /**
         * See the [Rust documentation for `BookPahlavi`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.BookPahlavi) for more information.
         */
        BookPahlavi = 124,
        /**
         * See the [Rust documentation for `InscriptionalParthian`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.InscriptionalParthian) for more information.
         */
        InscriptionalParthian = 125,
        /**
         * See the [Rust documentation for `Samaritan`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.Samaritan) for more information.
         */
        Samaritan = 126,
        /**
         * See the [Rust documentation for `TaiViet`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.TaiViet) for more information.
         */
        TaiViet = 127,
        /**
         * See the [Rust documentation for `MathematicalNotation`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.MathematicalNotation) for more information.
         */
        MathematicalNotation = 128,
        /**
         * See the [Rust documentation for `Symbols`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.Symbols) for more information.
         */
        Symbols = 129,
        /**
         * See the [Rust documentation for `Bamum`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.Bamum) for more information.
         */
        Bamum = 130,
        /**
         * See the [Rust documentation for `Lisu`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.Lisu) for more information.
         */
        Lisu = 131,
        /**
         * See the [Rust documentation for `NakhiGeba`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.NakhiGeba) for more information.
         */
        NakhiGeba = 132,
        /**
         * See the [Rust documentation for `OldSouthArabian`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.OldSouthArabian) for more information.
         */
        OldSouthArabian = 133,
        /**
         * See the [Rust documentation for `BassaVah`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.BassaVah) for more information.
         */
        BassaVah = 134,
        /**
         * See the [Rust documentation for `Duployan`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.Duployan) for more information.
         */
        Duployan = 135,
        /**
         * See the [Rust documentation for `Elbasan`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.Elbasan) for more information.
         */
        Elbasan = 136,
        /**
         * See the [Rust documentation for `Grantha`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.Grantha) for more information.
         */
        Grantha = 137,
        /**
         * See the [Rust documentation for `Kpelle`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.Kpelle) for more information.
         */
        Kpelle = 138,
        /**
         * See the [Rust documentation for `Loma`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.Loma) for more information.
         */
        Loma = 139,
        /**
         * See the [Rust documentation for `MendeKikakui`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.MendeKikakui) for more information.
         */
        MendeKikakui = 140,
        /**
         * See the [Rust documentation for `MeroiticCursive`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.MeroiticCursive) for more information.
         */
        MeroiticCursive = 141,
        /**
         * See the [Rust documentation for `OldNorthArabian`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.OldNorthArabian) for more information.
         */
        OldNorthArabian = 142,
        /**
         * See the [Rust documentation for `Nabataean`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.Nabataean) for more information.
         */
        Nabataean = 143,
        /**
         * See the [Rust documentation for `Palmyrene`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.Palmyrene) for more information.
         */
        Palmyrene = 144,
        /**
         * See the [Rust documentation for `Khudawadi`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.Khudawadi) for more information.
         */
        Khudawadi = 145,
        /**
         * See the [Rust documentation for `WarangCiti`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.WarangCiti) for more information.
         */
        WarangCiti = 146,
        /**
         * See the [Rust documentation for `Afaka`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.Afaka) for more information.
         */
        Afaka = 147,
        /**
         * See the [Rust documentation for `Jurchen`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.Jurchen) for more information.
         */
        Jurchen = 148,
        /**
         * See the [Rust documentation for `Mro`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.Mro) for more information.
         */
        Mro = 149,
        /**
         * See the [Rust documentation for `Nushu`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.Nushu) for more information.
         */
        Nushu = 150,
        /**
         * See the [Rust documentation for `Sharada`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.Sharada) for more information.
         */
        Sharada = 151,
        /**
         * See the [Rust documentation for `SoraSompeng`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.SoraSompeng) for more information.
         */
        SoraSompeng = 152,
        /**
         * See the [Rust documentation for `Takri`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.Takri) for more information.
         */
        Takri = 153,
        /**
         * See the [Rust documentation for `Tangut`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.Tangut) for more information.
         */
        Tangut = 154,
        /**
         * See the [Rust documentation for `Woleai`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.Woleai) for more information.
         */
        Woleai = 155,
        /**
         * See the [Rust documentation for `AnatolianHieroglyphs`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.AnatolianHieroglyphs) for more information.
         */
        AnatolianHieroglyphs = 156,
        /**
         * See the [Rust documentation for `Khojki`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.Khojki) for more information.
         */
        Khojki = 157,
        /**
         * See the [Rust documentation for `Tirhuta`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.Tirhuta) for more information.
         */
        Tirhuta = 158,
        /**
         * See the [Rust documentation for `CaucasianAlbanian`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.CaucasianAlbanian) for more information.
         */
        CaucasianAlbanian = 159,
        /**
         * See the [Rust documentation for `Mahajani`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.Mahajani) for more information.
         */
        Mahajani = 160,
        /**
         * See the [Rust documentation for `Ahom`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.Ahom) for more information.
         */
        Ahom = 161,
        /**
         * See the [Rust documentation for `Hatran`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.Hatran) for more information.
         */
        Hatran = 162,
        /**
         * See the [Rust documentation for `Modi`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.Modi) for more information.
         */
        Modi = 163,
        /**
         * See the [Rust documentation for `Multani`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.Multani) for more information.
         */
        Multani = 164,
        /**
         * See the [Rust documentation for `PauCinHau`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.PauCinHau) for more information.
         */
        PauCinHau = 165,
        /**
         * See the [Rust documentation for `Siddham`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.Siddham) for more information.
         */
        Siddham = 166,
        /**
         * See the [Rust documentation for `Adlam`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.Adlam) for more information.
         */
        Adlam = 167,
        /**
         * See the [Rust documentation for `Bhaiksuki`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.Bhaiksuki) for more information.
         */
        Bhaiksuki = 168,
        /**
         * See the [Rust documentation for `Marchen`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.Marchen) for more information.
         */
        Marchen = 169,
        /**
         * See the [Rust documentation for `Newa`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.Newa) for more information.
         */
        Newa = 170,
        /**
         * See the [Rust documentation for `Osage`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.Osage) for more information.
         */
        Osage = 171,
        /**
         * See the [Rust documentation for `HanWithBopomofo`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.HanWithBopomofo) for more information.
         */
        HanWithBopomofo = 172,
        /**
         * See the [Rust documentation for `Jamo`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.Jamo) for more information.
         */
        Jamo = 173,
        /**
         * See the [Rust documentation for `SymbolsEmoji`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.SymbolsEmoji) for more information.
         */
        SymbolsEmoji = 174,
        /**
         * See the [Rust documentation for `MasaramGondi`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.MasaramGondi) for more information.
         */
        MasaramGondi = 175,
        /**
         * See the [Rust documentation for `Soyombo`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.Soyombo) for more information.
         */
        Soyombo = 176,
        /**
         * See the [Rust documentation for `ZanabazarSquare`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.ZanabazarSquare) for more information.
         */
        ZanabazarSquare = 177,
        /**
         * See the [Rust documentation for `Dogra`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.Dogra) for more information.
         */
        Dogra = 178,
        /**
         * See the [Rust documentation for `GunjalaGondi`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.GunjalaGondi) for more information.
         */
        GunjalaGondi = 179,
        /**
         * See the [Rust documentation for `Makasar`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.Makasar) for more information.
         */
        Makasar = 180,
        /**
         * See the [Rust documentation for `Medefaidrin`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.Medefaidrin) for more information.
         */
        Medefaidrin = 181,
        /**
         * See the [Rust documentation for `HanifiRohingya`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.HanifiRohingya) for more information.
         */
        HanifiRohingya = 182,
        /**
         * See the [Rust documentation for `Sogdian`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.Sogdian) for more information.
         */
        Sogdian = 183,
        /**
         * See the [Rust documentation for `OldSogdian`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.OldSogdian) for more information.
         */
        OldSogdian = 184,
        /**
         * See the [Rust documentation for `Elymaic`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.Elymaic) for more information.
         */
        Elymaic = 185,
        /**
         * See the [Rust documentation for `NyiakengPuachueHmong`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.NyiakengPuachueHmong) for more information.
         */
        NyiakengPuachueHmong = 186,
        /**
         * See the [Rust documentation for `Nandinagari`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.Nandinagari) for more information.
         */
        Nandinagari = 187,
        /**
         * See the [Rust documentation for `Wancho`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.Wancho) for more information.
         */
        Wancho = 188,
        /**
         * See the [Rust documentation for `Chorasmian`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.Chorasmian) for more information.
         */
        Chorasmian = 189,
        /**
         * See the [Rust documentation for `DivesAkuru`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.DivesAkuru) for more information.
         */
        DivesAkuru = 190,
        /**
         * See the [Rust documentation for `KhitanSmallScript`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.KhitanSmallScript) for more information.
         */
        KhitanSmallScript = 191,
        /**
         * See the [Rust documentation for `Yezidi`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.Yezidi) for more information.
         */
        Yezidi = 192,
        /**
         * See the [Rust documentation for `CyproMinoan`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.CyproMinoan) for more information.
         */
        CyproMinoan = 193,
        /**
         * See the [Rust documentation for `OldUyghur`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.OldUyghur) for more information.
         */
        OldUyghur = 194,
        /**
         * See the [Rust documentation for `Tangsa`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.Tangsa) for more information.
         */
        Tangsa = 195,
        /**
         * See the [Rust documentation for `Toto`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.Toto) for more information.
         */
        Toto = 196,
        /**
         * See the [Rust documentation for `Vithkuqi`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.Vithkuqi) for more information.
         */
        Vithkuqi = 197,
        /**
         * See the [Rust documentation for `Kawi`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.Kawi) for more information.
         */
        Kawi = 198,
        /**
         * See the [Rust documentation for `NagMundari`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.NagMundari) for more information.
         */
        NagMundari = 199,
        /**
         * See the [Rust documentation for `ArabicNastaliq`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.ArabicNastaliq) for more information.
         */
        Nastaliq = 200,
        /**
         * See the [Rust documentation for `Garay`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.Garay) for more information.
         */
        Garay = 201,
        /**
         * See the [Rust documentation for `GurungKhema`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.GurungKhema) for more information.
         */
        GurungKhema = 202,
        /**
         * See the [Rust documentation for `KiratRai`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.KiratRai) for more information.
         */
        KiratRai = 203,
        /**
         * See the [Rust documentation for `OlOnal`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.OlOnal) for more information.
         */
        OlOnal = 204,
        /**
         * See the [Rust documentation for `Sunuwar`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.Sunuwar) for more information.
         */
        Sunuwar = 205,
        /**
         * See the [Rust documentation for `Todhri`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.Todhri) for more information.
         */
        Todhri = 206,
        /**
         * See the [Rust documentation for `TuluTigalari`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.TuluTigalari) for more information.
         */
        TuluTigalari = 207,
        /**
         * See the [Rust documentation for `BeriaErfe`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.BeriaErfe) for more information.
         */
        BeriaErfe = 208,
        /**
         * See the [Rust documentation for `Sidetic`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.Sidetic) for more information.
         */
        Sidetic = 209,
        /**
         * See the [Rust documentation for `TaiYo`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.TaiYo) for more information.
         */
        TaiYo = 210,
        /**
         * See the [Rust documentation for `TolongSiki`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.TolongSiki) for more information.
         */
        TolongSiki = 211,
        /**
         * See the [Rust documentation for `TraditionalHanWithLatin`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.TraditionalHanWithLatin) for more information.
         */
        TraditionalHanWithLatin = 212,
        /**
         * See the [Rust documentation for `ProtoCuneiform`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.ProtoCuneiform) for more information.
         */
        ProtoCuneiform = 213,
        /**
         * See the [Rust documentation for `Seal`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.Seal) for more information.
         */
        Seal = 214,
        /**
         * See the [Rust documentation for `Chisoi`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#associatedconstant.Chisoi) for more information.
         */
        Chisoi = 254,
    };

    Script(): value(Value::Unknown) {}

    // Implicit conversions between enum and ::Value
    constexpr Script(Value v) : value(v) {}
    constexpr operator Value() const { return value; }
    // Prevent usage as boolean value
    explicit operator bool() const = delete;

  /**
   * See the [Rust documentation for `for_char`](https://docs.rs/icu/2.4.0/icu/properties/props/trait.EnumeratedProperty.html#tymethod.for_char) for more information.
   */
  inline static icu4x::Script for_char(char32_t ch);

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
   * See the [Rust documentation for `to_icu4c_value`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#method.to_icu4c_value) for more information.
   */
  inline uint16_t to_integer_value() const;

  /**
   * Convert from an integer value from ICU4C or `CodePointMapData`
   *
   * See the [Rust documentation for `from_icu4c_value`](https://docs.rs/icu/2.4.0/icu/properties/props/struct.Script.html#method.from_icu4c_value) for more information.
   */
  inline static std::optional<icu4x::Script> from_integer_value(uint16_t other);

  /**
   * Creates a `Script` from a string.
   *
   * Short names, long names, and aliases are supported, and matching is case-insensitive.
   */
  inline static std::optional<icu4x::Script> try_from_str(std::string_view s);

    inline icu4x::capi::Script AsFFI() const;
    inline static icu4x::Script FromFFI(icu4x::capi::Script c_enum);
private:
    Value value;
};

} // namespace
#endif // ICU4X_Script_D_HPP

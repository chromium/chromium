// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

#[diplomat::bridge]
#[diplomat::abi_rename = "icu4x_{0}_mv1"]
#[diplomat::attr(auto, namespace = "icu4x")]
pub mod ffi {
    use icu_properties::props;

    #[cfg(feature = "compiled_data")]
    use diplomat_runtime::DiplomatChar;

    #[diplomat::rust_link(icu::properties::props::BidiClass, Struct)]
    #[diplomat::enum_convert(icu_properties::props::BidiClass, needs_wildcard)]
    pub enum BidiClass {
        #[diplomat::rust_link(icu::properties::props::BidiClass::LeftToRight, EnumVariant)]
        LeftToRight = 0,
        #[diplomat::rust_link(icu::properties::props::BidiClass::RightToLeft, EnumVariant)]
        RightToLeft = 1,
        #[diplomat::rust_link(icu::properties::props::BidiClass::EuropeanNumber, EnumVariant)]
        EuropeanNumber = 2,
        #[diplomat::rust_link(icu::properties::props::BidiClass::EuropeanSeparator, EnumVariant)]
        EuropeanSeparator = 3,
        #[diplomat::rust_link(icu::properties::props::BidiClass::EuropeanTerminator, EnumVariant)]
        EuropeanTerminator = 4,
        #[diplomat::rust_link(icu::properties::props::BidiClass::ArabicNumber, EnumVariant)]
        ArabicNumber = 5,
        #[diplomat::rust_link(icu::properties::props::BidiClass::CommonSeparator, EnumVariant)]
        CommonSeparator = 6,
        #[diplomat::rust_link(icu::properties::props::BidiClass::ParagraphSeparator, EnumVariant)]
        ParagraphSeparator = 7,
        #[diplomat::rust_link(icu::properties::props::BidiClass::SegmentSeparator, EnumVariant)]
        SegmentSeparator = 8,
        #[diplomat::rust_link(icu::properties::props::BidiClass::WhiteSpace, EnumVariant)]
        WhiteSpace = 9,
        #[diplomat::rust_link(icu::properties::props::BidiClass::OtherNeutral, EnumVariant)]
        OtherNeutral = 10,
        #[diplomat::rust_link(
            icu::properties::props::BidiClass::LeftToRightEmbedding,
            EnumVariant
        )]
        LeftToRightEmbedding = 11,
        #[diplomat::rust_link(icu::properties::props::BidiClass::LeftToRightOverride, EnumVariant)]
        LeftToRightOverride = 12,
        #[diplomat::rust_link(icu::properties::props::BidiClass::ArabicLetter, EnumVariant)]
        ArabicLetter = 13,
        #[diplomat::rust_link(
            icu::properties::props::BidiClass::RightToLeftEmbedding,
            EnumVariant
        )]
        RightToLeftEmbedding = 14,
        #[diplomat::rust_link(icu::properties::props::BidiClass::RightToLeftOverride, EnumVariant)]
        RightToLeftOverride = 15,
        #[diplomat::rust_link(
            icu::properties::props::BidiClass::PopDirectionalFormat,
            EnumVariant
        )]
        PopDirectionalFormat = 16,
        #[diplomat::rust_link(icu::properties::props::BidiClass::NonspacingMark, EnumVariant)]
        NonspacingMark = 17,
        #[diplomat::rust_link(icu::properties::props::BidiClass::BoundaryNeutral, EnumVariant)]
        BoundaryNeutral = 18,
        #[diplomat::rust_link(icu::properties::props::BidiClass::FirstStrongIsolate, EnumVariant)]
        FirstStrongIsolate = 19,
        #[diplomat::rust_link(icu::properties::props::BidiClass::LeftToRightIsolate, EnumVariant)]
        LeftToRightIsolate = 20,
        #[diplomat::rust_link(icu::properties::props::BidiClass::RightToLeftIsolate, EnumVariant)]
        RightToLeftIsolate = 21,
        #[diplomat::rust_link(
            icu::properties::props::BidiClass::PopDirectionalIsolate,
            EnumVariant
        )]
        PopDirectionalIsolate = 22,
    }

    impl BidiClass {
        #[diplomat::rust_link(icu::properties::props::EnumeratedProperty::for_char, FnInTrait)]
        #[cfg(feature = "compiled_data")]
        pub fn for_char(ch: DiplomatChar) -> Self {
            icu_properties::CodePointMapData::<props::BidiClass>::new()
                .get32(ch)
                .into()
        }

        #[diplomat::rust_link(icu::properties::PropertyNamesLongBorrowed::get, FnInStruct)]
        #[diplomat::rust_link(icu::properties::PropertyNamesLong, Struct, hidden)]
        #[diplomat::rust_link(icu::properties::PropertyNamesLongBorrowed, Struct, hidden)]
        #[diplomat::rust_link(icu::properties::PropertyNamesLong::new, FnInStruct, hidden)]
        #[diplomat::rust_link(icu::properties::PropertyNamesLongBorrowed::new, FnInStruct, hidden)]
        #[diplomat::rust_link(
            icu::properties::props::NamedEnumeratedProperty::long_name,
            FnInTrait,
            hidden
        )]
        #[cfg(feature = "compiled_data")]
        /// Get the "long" name of this property value (returns empty if property value is unknown)
        pub fn long_name(self) -> Option<&'static str> {
            icu_properties::PropertyNamesLongBorrowed::<props::BidiClass>::new().get(self.into())
        }

        #[diplomat::rust_link(icu::properties::PropertyNamesShortBorrowed::get, FnInStruct)]
        #[diplomat::rust_link(icu::properties::PropertyNamesShort, Struct, hidden)]
        #[diplomat::rust_link(icu::properties::PropertyNamesShortBorrowed, Struct, hidden)]
        #[diplomat::rust_link(icu::properties::PropertyNamesShort::new, FnInStruct, hidden)]
        #[diplomat::rust_link(icu::properties::PropertyNamesShortBorrowed::new, FnInStruct, hidden)]
        #[diplomat::rust_link(
            icu::properties::props::NamedEnumeratedProperty::short_name,
            FnInTrait,
            hidden
        )]
        #[cfg(feature = "compiled_data")]
        /// Get the "short" name of this property value (returns empty if property value is unknown)
        pub fn short_name(self) -> Option<&'static str> {
            icu_properties::PropertyNamesShortBorrowed::<props::BidiClass>::new().get(self.into())
        }
        #[diplomat::rust_link(icu::properties::props::BidiClass::to_icu4c_value, FnInStruct)]
        /// Convert to an integer value usable with ICU4C and CodePointMapData
        pub fn to_integer_value(self) -> u8 {
            self as u8
        }
        #[diplomat::rust_link(icu::properties::props::BidiClass::from_icu4c_value, FnInStruct)]
        /// Convert from an integer value from ICU4C or CodePointMapData
        pub fn from_integer_value(other: u8) -> Option<Self> {
            Some(match other {
                0 => Self::LeftToRight,
                1 => Self::RightToLeft,
                2 => Self::EuropeanNumber,
                3 => Self::EuropeanSeparator,
                4 => Self::EuropeanTerminator,
                5 => Self::ArabicNumber,
                6 => Self::CommonSeparator,
                7 => Self::ParagraphSeparator,
                8 => Self::SegmentSeparator,
                9 => Self::WhiteSpace,
                10 => Self::OtherNeutral,
                11 => Self::LeftToRightEmbedding,
                12 => Self::LeftToRightOverride,
                13 => Self::ArabicLetter,
                14 => Self::RightToLeftEmbedding,
                15 => Self::RightToLeftOverride,
                16 => Self::PopDirectionalFormat,
                17 => Self::NonspacingMark,
                18 => Self::BoundaryNeutral,
                19 => Self::FirstStrongIsolate,
                20 => Self::LeftToRightIsolate,
                21 => Self::RightToLeftIsolate,
                22 => Self::PopDirectionalIsolate,
                _ => return None,
            })
        }
    }

    #[diplomat::rust_link(icu::properties::props::Script, Struct)]
    #[diplomat::enum_convert(icu_properties::props::Script, needs_wildcard)]
    pub enum Script {
        #[diplomat::rust_link(icu::properties::props::Script::Adlam, EnumVariant)]
        Adlam = 167,
        #[diplomat::rust_link(icu::properties::props::Script::Ahom, EnumVariant)]
        Ahom = 161,
        #[diplomat::rust_link(icu::properties::props::Script::AnatolianHieroglyphs, EnumVariant)]
        AnatolianHieroglyphs = 156,
        #[diplomat::rust_link(icu::properties::props::Script::Arabic, EnumVariant)]
        Arabic = 2,
        #[diplomat::rust_link(icu::properties::props::Script::Armenian, EnumVariant)]
        Armenian = 3,
        #[diplomat::rust_link(icu::properties::props::Script::Avestan, EnumVariant)]
        Avestan = 117,
        #[diplomat::rust_link(icu::properties::props::Script::Balinese, EnumVariant)]
        Balinese = 62,
        #[diplomat::rust_link(icu::properties::props::Script::Bamum, EnumVariant)]
        Bamum = 130,
        #[diplomat::rust_link(icu::properties::props::Script::BassaVah, EnumVariant)]
        BassaVah = 134,
        #[diplomat::rust_link(icu::properties::props::Script::Batak, EnumVariant)]
        Batak = 63,
        #[diplomat::rust_link(icu::properties::props::Script::Bengali, EnumVariant)]
        Bengali = 4,
        #[diplomat::rust_link(icu::properties::props::Script::Bhaiksuki, EnumVariant)]
        Bhaiksuki = 168,
        #[diplomat::rust_link(icu::properties::props::Script::Bopomofo, EnumVariant)]
        Bopomofo = 5,
        #[diplomat::rust_link(icu::properties::props::Script::Brahmi, EnumVariant)]
        Brahmi = 65,
        #[diplomat::rust_link(icu::properties::props::Script::Braille, EnumVariant)]
        Braille = 46,
        #[diplomat::rust_link(icu::properties::props::Script::Buginese, EnumVariant)]
        Buginese = 55,
        #[diplomat::rust_link(icu::properties::props::Script::Buhid, EnumVariant)]
        Buhid = 44,
        #[diplomat::rust_link(icu::properties::props::Script::CanadianAboriginal, EnumVariant)]
        CanadianAboriginal = 40,
        #[diplomat::rust_link(icu::properties::props::Script::Carian, EnumVariant)]
        Carian = 104,
        #[diplomat::rust_link(icu::properties::props::Script::CaucasianAlbanian, EnumVariant)]
        CaucasianAlbanian = 159,
        #[diplomat::rust_link(icu::properties::props::Script::Chakma, EnumVariant)]
        Chakma = 118,
        #[diplomat::rust_link(icu::properties::props::Script::Cham, EnumVariant)]
        Cham = 66,
        #[diplomat::rust_link(icu::properties::props::Script::Cherokee, EnumVariant)]
        Cherokee = 6,
        #[diplomat::rust_link(icu::properties::props::Script::Chorasmian, EnumVariant)]
        Chorasmian = 189,
        #[diplomat::rust_link(icu::properties::props::Script::Common, EnumVariant)]
        Common = 0,
        #[diplomat::rust_link(icu::properties::props::Script::Coptic, EnumVariant)]
        Coptic = 7,
        #[diplomat::rust_link(icu::properties::props::Script::Cuneiform, EnumVariant)]
        Cuneiform = 101,
        #[diplomat::rust_link(icu::properties::props::Script::Cypriot, EnumVariant)]
        Cypriot = 47,
        #[diplomat::rust_link(icu::properties::props::Script::CyproMinoan, EnumVariant)]
        CyproMinoan = 193,
        #[diplomat::rust_link(icu::properties::props::Script::Cyrillic, EnumVariant)]
        Cyrillic = 8,
        #[diplomat::rust_link(icu::properties::props::Script::Deseret, EnumVariant)]
        Deseret = 9,
        #[diplomat::rust_link(icu::properties::props::Script::Devanagari, EnumVariant)]
        Devanagari = 10,
        #[diplomat::rust_link(icu::properties::props::Script::DivesAkuru, EnumVariant)]
        DivesAkuru = 190,
        #[diplomat::rust_link(icu::properties::props::Script::Dogra, EnumVariant)]
        Dogra = 178,
        #[diplomat::rust_link(icu::properties::props::Script::Duployan, EnumVariant)]
        Duployan = 135,
        #[diplomat::rust_link(icu::properties::props::Script::EgyptianHieroglyphs, EnumVariant)]
        EgyptianHieroglyphs = 71,
        #[diplomat::rust_link(icu::properties::props::Script::Elbasan, EnumVariant)]
        Elbasan = 136,
        #[diplomat::rust_link(icu::properties::props::Script::Elymaic, EnumVariant)]
        Elymaic = 185,
        #[diplomat::rust_link(icu::properties::props::Script::Ethiopian, EnumVariant)]
        Ethiopian = 11,
        #[diplomat::rust_link(icu::properties::props::Script::Georgian, EnumVariant)]
        Georgian = 12,
        #[diplomat::rust_link(icu::properties::props::Script::Glagolitic, EnumVariant)]
        Glagolitic = 56,
        #[diplomat::rust_link(icu::properties::props::Script::Gothic, EnumVariant)]
        Gothic = 13,
        #[diplomat::rust_link(icu::properties::props::Script::Grantha, EnumVariant)]
        Grantha = 137,
        #[diplomat::rust_link(icu::properties::props::Script::Greek, EnumVariant)]
        Greek = 14,
        #[diplomat::rust_link(icu::properties::props::Script::Gujarati, EnumVariant)]
        Gujarati = 15,
        #[diplomat::rust_link(icu::properties::props::Script::GunjalaGondi, EnumVariant)]
        GunjalaGondi = 179,
        #[diplomat::rust_link(icu::properties::props::Script::Gurmukhi, EnumVariant)]
        Gurmukhi = 16,
        #[diplomat::rust_link(icu::properties::props::Script::Han, EnumVariant)]
        Han = 17,
        #[diplomat::rust_link(icu::properties::props::Script::Hangul, EnumVariant)]
        Hangul = 18,
        #[diplomat::rust_link(icu::properties::props::Script::HanifiRohingya, EnumVariant)]
        HanifiRohingya = 182,
        #[diplomat::rust_link(icu::properties::props::Script::Hanunoo, EnumVariant)]
        Hanunoo = 43,
        #[diplomat::rust_link(icu::properties::props::Script::Hatran, EnumVariant)]
        Hatran = 162,
        #[diplomat::rust_link(icu::properties::props::Script::Hebrew, EnumVariant)]
        Hebrew = 19,
        #[diplomat::rust_link(icu::properties::props::Script::Hiragana, EnumVariant)]
        Hiragana = 20,
        #[diplomat::rust_link(icu::properties::props::Script::ImperialAramaic, EnumVariant)]
        ImperialAramaic = 116,
        #[diplomat::rust_link(icu::properties::props::Script::Inherited, EnumVariant)]
        Inherited = 1,
        #[diplomat::rust_link(icu::properties::props::Script::InscriptionalPahlavi, EnumVariant)]
        InscriptionalPahlavi = 122,
        #[diplomat::rust_link(icu::properties::props::Script::InscriptionalParthian, EnumVariant)]
        InscriptionalParthian = 125,
        #[diplomat::rust_link(icu::properties::props::Script::Javanese, EnumVariant)]
        Javanese = 78,
        #[diplomat::rust_link(icu::properties::props::Script::Kaithi, EnumVariant)]
        Kaithi = 120,
        #[diplomat::rust_link(icu::properties::props::Script::Kannada, EnumVariant)]
        Kannada = 21,
        #[diplomat::rust_link(icu::properties::props::Script::Katakana, EnumVariant)]
        Katakana = 22,
        #[diplomat::rust_link(icu::properties::props::Script::Kawi, EnumVariant)]
        Kawi = 198,
        #[diplomat::rust_link(icu::properties::props::Script::KayahLi, EnumVariant)]
        KayahLi = 79,
        #[diplomat::rust_link(icu::properties::props::Script::Kharoshthi, EnumVariant)]
        Kharoshthi = 57,
        #[diplomat::rust_link(icu::properties::props::Script::KhitanSmallScript, EnumVariant)]
        KhitanSmallScript = 191,
        #[diplomat::rust_link(icu::properties::props::Script::Khmer, EnumVariant)]
        Khmer = 23,
        #[diplomat::rust_link(icu::properties::props::Script::Khojki, EnumVariant)]
        Khojki = 157,
        #[diplomat::rust_link(icu::properties::props::Script::Khudawadi, EnumVariant)]
        Khudawadi = 145,
        #[diplomat::rust_link(icu::properties::props::Script::Lao, EnumVariant)]
        Lao = 24,
        #[diplomat::rust_link(icu::properties::props::Script::Latin, EnumVariant)]
        Latin = 25,
        #[diplomat::rust_link(icu::properties::props::Script::Lepcha, EnumVariant)]
        Lepcha = 82,
        #[diplomat::rust_link(icu::properties::props::Script::Limbu, EnumVariant)]
        Limbu = 48,
        #[diplomat::rust_link(icu::properties::props::Script::LinearA, EnumVariant)]
        LinearA = 83,
        #[diplomat::rust_link(icu::properties::props::Script::LinearB, EnumVariant)]
        LinearB = 49,
        #[diplomat::rust_link(icu::properties::props::Script::Lisu, EnumVariant)]
        Lisu = 131,
        #[diplomat::rust_link(icu::properties::props::Script::Lycian, EnumVariant)]
        Lycian = 107,
        #[diplomat::rust_link(icu::properties::props::Script::Lydian, EnumVariant)]
        Lydian = 108,
        #[diplomat::rust_link(icu::properties::props::Script::Mahajani, EnumVariant)]
        Mahajani = 160,
        #[diplomat::rust_link(icu::properties::props::Script::Makasar, EnumVariant)]
        Makasar = 180,
        #[diplomat::rust_link(icu::properties::props::Script::Malayalam, EnumVariant)]
        Malayalam = 26,
        #[diplomat::rust_link(icu::properties::props::Script::Mandaic, EnumVariant)]
        Mandaic = 84,
        #[diplomat::rust_link(icu::properties::props::Script::Manichaean, EnumVariant)]
        Manichaean = 121,
        #[diplomat::rust_link(icu::properties::props::Script::Marchen, EnumVariant)]
        Marchen = 169,
        #[diplomat::rust_link(icu::properties::props::Script::MasaramGondi, EnumVariant)]
        MasaramGondi = 175,
        #[diplomat::rust_link(icu::properties::props::Script::Medefaidrin, EnumVariant)]
        Medefaidrin = 181,
        #[diplomat::rust_link(icu::properties::props::Script::MeeteiMayek, EnumVariant)]
        MeeteiMayek = 115,
        #[diplomat::rust_link(icu::properties::props::Script::MendeKikakui, EnumVariant)]
        MendeKikakui = 140,
        #[diplomat::rust_link(icu::properties::props::Script::MeroiticCursive, EnumVariant)]
        MeroiticCursive = 141,
        #[diplomat::rust_link(icu::properties::props::Script::MeroiticHieroglyphs, EnumVariant)]
        MeroiticHieroglyphs = 86,
        #[diplomat::rust_link(icu::properties::props::Script::Miao, EnumVariant)]
        Miao = 92,
        #[diplomat::rust_link(icu::properties::props::Script::Modi, EnumVariant)]
        Modi = 163,
        #[diplomat::rust_link(icu::properties::props::Script::Mongolian, EnumVariant)]
        Mongolian = 27,
        #[diplomat::rust_link(icu::properties::props::Script::Mro, EnumVariant)]
        Mro = 149,
        #[diplomat::rust_link(icu::properties::props::Script::Multani, EnumVariant)]
        Multani = 164,
        #[diplomat::rust_link(icu::properties::props::Script::Myanmar, EnumVariant)]
        Myanmar = 28,
        #[diplomat::rust_link(icu::properties::props::Script::Nabataean, EnumVariant)]
        Nabataean = 143,
        #[diplomat::rust_link(icu::properties::props::Script::NagMundari, EnumVariant)]
        NagMundari = 199,
        #[diplomat::rust_link(icu::properties::props::Script::Nandinagari, EnumVariant)]
        Nandinagari = 187,
        #[diplomat::rust_link(icu::properties::props::Script::Nastaliq, EnumVariant)]
        Nastaliq = 200,
        #[diplomat::rust_link(icu::properties::props::Script::NewTaiLue, EnumVariant)]
        NewTaiLue = 59,
        #[diplomat::rust_link(icu::properties::props::Script::Newa, EnumVariant)]
        Newa = 170,
        #[diplomat::rust_link(icu::properties::props::Script::Nko, EnumVariant)]
        Nko = 87,
        #[diplomat::rust_link(icu::properties::props::Script::Nushu, EnumVariant)]
        Nushu = 150,
        #[diplomat::rust_link(icu::properties::props::Script::NyiakengPuachueHmong, EnumVariant)]
        NyiakengPuachueHmong = 186,
        #[diplomat::rust_link(icu::properties::props::Script::Ogham, EnumVariant)]
        Ogham = 29,
        #[diplomat::rust_link(icu::properties::props::Script::OlChiki, EnumVariant)]
        OlChiki = 109,
        #[diplomat::rust_link(icu::properties::props::Script::OldHungarian, EnumVariant)]
        OldHungarian = 76,
        #[diplomat::rust_link(icu::properties::props::Script::OldItalic, EnumVariant)]
        OldItalic = 30,
        #[diplomat::rust_link(icu::properties::props::Script::OldNorthArabian, EnumVariant)]
        OldNorthArabian = 142,
        #[diplomat::rust_link(icu::properties::props::Script::OldPermic, EnumVariant)]
        OldPermic = 89,
        #[diplomat::rust_link(icu::properties::props::Script::OldPersian, EnumVariant)]
        OldPersian = 61,
        #[diplomat::rust_link(icu::properties::props::Script::OldSogdian, EnumVariant)]
        OldSogdian = 184,
        #[diplomat::rust_link(icu::properties::props::Script::OldSouthArabian, EnumVariant)]
        OldSouthArabian = 133,
        #[diplomat::rust_link(icu::properties::props::Script::OldTurkic, EnumVariant)]
        OldTurkic = 88,
        #[diplomat::rust_link(icu::properties::props::Script::OldUyghur, EnumVariant)]
        OldUyghur = 194,
        #[diplomat::rust_link(icu::properties::props::Script::Oriya, EnumVariant)]
        Oriya = 31,
        #[diplomat::rust_link(icu::properties::props::Script::Osage, EnumVariant)]
        Osage = 171,
        #[diplomat::rust_link(icu::properties::props::Script::Osmanya, EnumVariant)]
        Osmanya = 50,
        #[diplomat::rust_link(icu::properties::props::Script::PahawhHmong, EnumVariant)]
        PahawhHmong = 75,
        #[diplomat::rust_link(icu::properties::props::Script::Palmyrene, EnumVariant)]
        Palmyrene = 144,
        #[diplomat::rust_link(icu::properties::props::Script::PauCinHau, EnumVariant)]
        PauCinHau = 165,
        #[diplomat::rust_link(icu::properties::props::Script::PhagsPa, EnumVariant)]
        PhagsPa = 90,
        #[diplomat::rust_link(icu::properties::props::Script::Phoenician, EnumVariant)]
        Phoenician = 91,
        #[diplomat::rust_link(icu::properties::props::Script::PsalterPahlavi, EnumVariant)]
        PsalterPahlavi = 123,
        #[diplomat::rust_link(icu::properties::props::Script::Rejang, EnumVariant)]
        Rejang = 110,
        #[diplomat::rust_link(icu::properties::props::Script::Runic, EnumVariant)]
        Runic = 32,
        #[diplomat::rust_link(icu::properties::props::Script::Samaritan, EnumVariant)]
        Samaritan = 126,
        #[diplomat::rust_link(icu::properties::props::Script::Saurashtra, EnumVariant)]
        Saurashtra = 111,
        #[diplomat::rust_link(icu::properties::props::Script::Sharada, EnumVariant)]
        Sharada = 151,
        #[diplomat::rust_link(icu::properties::props::Script::Shavian, EnumVariant)]
        Shavian = 51,
        #[diplomat::rust_link(icu::properties::props::Script::Siddham, EnumVariant)]
        Siddham = 166,
        #[diplomat::rust_link(icu::properties::props::Script::SignWriting, EnumVariant)]
        SignWriting = 112,
        #[diplomat::rust_link(icu::properties::props::Script::Sinhala, EnumVariant)]
        Sinhala = 33,
        #[diplomat::rust_link(icu::properties::props::Script::Sogdian, EnumVariant)]
        Sogdian = 183,
        #[diplomat::rust_link(icu::properties::props::Script::SoraSompeng, EnumVariant)]
        SoraSompeng = 152,
        #[diplomat::rust_link(icu::properties::props::Script::Soyombo, EnumVariant)]
        Soyombo = 176,
        #[diplomat::rust_link(icu::properties::props::Script::Sundanese, EnumVariant)]
        Sundanese = 113,
        #[diplomat::rust_link(icu::properties::props::Script::SylotiNagri, EnumVariant)]
        SylotiNagri = 58,
        #[diplomat::rust_link(icu::properties::props::Script::Syriac, EnumVariant)]
        Syriac = 34,
        #[diplomat::rust_link(icu::properties::props::Script::Tagalog, EnumVariant)]
        Tagalog = 42,
        #[diplomat::rust_link(icu::properties::props::Script::Tagbanwa, EnumVariant)]
        Tagbanwa = 45,
        #[diplomat::rust_link(icu::properties::props::Script::TaiLe, EnumVariant)]
        TaiLe = 52,
        #[diplomat::rust_link(icu::properties::props::Script::TaiTham, EnumVariant)]
        TaiTham = 106,
        #[diplomat::rust_link(icu::properties::props::Script::TaiViet, EnumVariant)]
        TaiViet = 127,
        #[diplomat::rust_link(icu::properties::props::Script::Takri, EnumVariant)]
        Takri = 153,
        #[diplomat::rust_link(icu::properties::props::Script::Tamil, EnumVariant)]
        Tamil = 35,
        #[diplomat::rust_link(icu::properties::props::Script::Tangsa, EnumVariant)]
        Tangsa = 195,
        #[diplomat::rust_link(icu::properties::props::Script::Tangut, EnumVariant)]
        Tangut = 154,
        #[diplomat::rust_link(icu::properties::props::Script::Telugu, EnumVariant)]
        Telugu = 36,
        #[diplomat::rust_link(icu::properties::props::Script::Thaana, EnumVariant)]
        Thaana = 37,
        #[diplomat::rust_link(icu::properties::props::Script::Thai, EnumVariant)]
        Thai = 38,
        #[diplomat::rust_link(icu::properties::props::Script::Tibetan, EnumVariant)]
        Tibetan = 39,
        #[diplomat::rust_link(icu::properties::props::Script::Tifinagh, EnumVariant)]
        Tifinagh = 60,
        #[diplomat::rust_link(icu::properties::props::Script::Tirhuta, EnumVariant)]
        Tirhuta = 158,
        #[diplomat::rust_link(icu::properties::props::Script::Toto, EnumVariant)]
        Toto = 196,
        #[diplomat::rust_link(icu::properties::props::Script::Ugaritic, EnumVariant)]
        Ugaritic = 53,
        #[diplomat::rust_link(icu::properties::props::Script::Unknown, EnumVariant)]
        Unknown = 103,
        #[diplomat::rust_link(icu::properties::props::Script::Vai, EnumVariant)]
        Vai = 99,
        #[diplomat::rust_link(icu::properties::props::Script::Vithkuqi, EnumVariant)]
        Vithkuqi = 197,
        #[diplomat::rust_link(icu::properties::props::Script::Wancho, EnumVariant)]
        Wancho = 188,
        #[diplomat::rust_link(icu::properties::props::Script::WarangCiti, EnumVariant)]
        WarangCiti = 146,
        #[diplomat::rust_link(icu::properties::props::Script::Yezidi, EnumVariant)]
        Yezidi = 192,
        #[diplomat::rust_link(icu::properties::props::Script::Yi, EnumVariant)]
        Yi = 41,
        #[diplomat::rust_link(icu::properties::props::Script::ZanabazarSquare, EnumVariant)]
        ZanabazarSquare = 177,
    }

    impl Script {
        #[diplomat::rust_link(icu::properties::props::EnumeratedProperty::for_char, FnInTrait)]
        #[cfg(feature = "compiled_data")]
        pub fn for_char(ch: DiplomatChar) -> Self {
            icu_properties::CodePointMapData::<props::Script>::new()
                .get32(ch)
                .into()
        }

        #[diplomat::rust_link(icu::properties::PropertyNamesLongBorrowed::get, FnInStruct)]
        #[cfg(feature = "compiled_data")]
        /// Get the "long" name of this property value (returns empty if property value is unknown)
        pub fn long_name(self) -> Option<&'static str> {
            icu_properties::PropertyNamesLongBorrowed::<props::Script>::new().get(self.into())
        }

        #[diplomat::rust_link(icu::properties::PropertyNamesShortBorrowed::get, FnInStruct)]
        #[diplomat::rust_link(
            icu::properties::PropertyNamesShortBorrowed::get_locale_script,
            FnInStruct,
            hidden
        )]
        #[cfg(feature = "compiled_data")]
        /// Get the "short" name of this property value (returns empty if property value is unknown)
        pub fn short_name(self) -> Option<&'static str> {
            icu_properties::PropertyNamesShortBorrowed::<props::Script>::new().get(self.into())
        }
        #[diplomat::rust_link(icu::properties::props::Script::to_icu4c_value, FnInStruct)]
        /// Convert to an integer value usable with ICU4C and CodePointMapData
        pub fn to_integer_value(self) -> u16 {
            self as u16
        }
        #[diplomat::rust_link(icu::properties::props::Script::from_icu4c_value, FnInStruct)]
        /// Convert from an integer value from ICU4C or CodePointMapData
        pub fn from_integer_value(other: u16) -> Option<Self> {
            Some(match other {
                167 => Self::Adlam,
                161 => Self::Ahom,
                156 => Self::AnatolianHieroglyphs,
                2 => Self::Arabic,
                3 => Self::Armenian,
                117 => Self::Avestan,
                62 => Self::Balinese,
                130 => Self::Bamum,
                134 => Self::BassaVah,
                63 => Self::Batak,
                4 => Self::Bengali,
                168 => Self::Bhaiksuki,
                5 => Self::Bopomofo,
                65 => Self::Brahmi,
                46 => Self::Braille,
                55 => Self::Buginese,
                44 => Self::Buhid,
                40 => Self::CanadianAboriginal,
                104 => Self::Carian,
                159 => Self::CaucasianAlbanian,
                118 => Self::Chakma,
                66 => Self::Cham,
                6 => Self::Cherokee,
                189 => Self::Chorasmian,
                0 => Self::Common,
                7 => Self::Coptic,
                101 => Self::Cuneiform,
                47 => Self::Cypriot,
                193 => Self::CyproMinoan,
                8 => Self::Cyrillic,
                9 => Self::Deseret,
                10 => Self::Devanagari,
                190 => Self::DivesAkuru,
                178 => Self::Dogra,
                135 => Self::Duployan,
                71 => Self::EgyptianHieroglyphs,
                136 => Self::Elbasan,
                185 => Self::Elymaic,
                11 => Self::Ethiopian,
                12 => Self::Georgian,
                56 => Self::Glagolitic,
                13 => Self::Gothic,
                137 => Self::Grantha,
                14 => Self::Greek,
                15 => Self::Gujarati,
                179 => Self::GunjalaGondi,
                16 => Self::Gurmukhi,
                17 => Self::Han,
                18 => Self::Hangul,
                182 => Self::HanifiRohingya,
                43 => Self::Hanunoo,
                162 => Self::Hatran,
                19 => Self::Hebrew,
                20 => Self::Hiragana,
                116 => Self::ImperialAramaic,
                1 => Self::Inherited,
                122 => Self::InscriptionalPahlavi,
                125 => Self::InscriptionalParthian,
                78 => Self::Javanese,
                120 => Self::Kaithi,
                21 => Self::Kannada,
                22 => Self::Katakana,
                198 => Self::Kawi,
                79 => Self::KayahLi,
                57 => Self::Kharoshthi,
                191 => Self::KhitanSmallScript,
                23 => Self::Khmer,
                157 => Self::Khojki,
                145 => Self::Khudawadi,
                24 => Self::Lao,
                25 => Self::Latin,
                82 => Self::Lepcha,
                48 => Self::Limbu,
                83 => Self::LinearA,
                49 => Self::LinearB,
                131 => Self::Lisu,
                107 => Self::Lycian,
                108 => Self::Lydian,
                160 => Self::Mahajani,
                180 => Self::Makasar,
                26 => Self::Malayalam,
                84 => Self::Mandaic,
                121 => Self::Manichaean,
                169 => Self::Marchen,
                175 => Self::MasaramGondi,
                181 => Self::Medefaidrin,
                115 => Self::MeeteiMayek,
                140 => Self::MendeKikakui,
                141 => Self::MeroiticCursive,
                86 => Self::MeroiticHieroglyphs,
                92 => Self::Miao,
                163 => Self::Modi,
                27 => Self::Mongolian,
                149 => Self::Mro,
                164 => Self::Multani,
                28 => Self::Myanmar,
                143 => Self::Nabataean,
                199 => Self::NagMundari,
                187 => Self::Nandinagari,
                200 => Self::Nastaliq,
                59 => Self::NewTaiLue,
                170 => Self::Newa,
                87 => Self::Nko,
                150 => Self::Nushu,
                186 => Self::NyiakengPuachueHmong,
                29 => Self::Ogham,
                109 => Self::OlChiki,
                76 => Self::OldHungarian,
                30 => Self::OldItalic,
                142 => Self::OldNorthArabian,
                89 => Self::OldPermic,
                61 => Self::OldPersian,
                184 => Self::OldSogdian,
                133 => Self::OldSouthArabian,
                88 => Self::OldTurkic,
                194 => Self::OldUyghur,
                31 => Self::Oriya,
                171 => Self::Osage,
                50 => Self::Osmanya,
                75 => Self::PahawhHmong,
                144 => Self::Palmyrene,
                165 => Self::PauCinHau,
                90 => Self::PhagsPa,
                91 => Self::Phoenician,
                123 => Self::PsalterPahlavi,
                110 => Self::Rejang,
                32 => Self::Runic,
                126 => Self::Samaritan,
                111 => Self::Saurashtra,
                151 => Self::Sharada,
                51 => Self::Shavian,
                166 => Self::Siddham,
                112 => Self::SignWriting,
                33 => Self::Sinhala,
                183 => Self::Sogdian,
                152 => Self::SoraSompeng,
                176 => Self::Soyombo,
                113 => Self::Sundanese,
                58 => Self::SylotiNagri,
                34 => Self::Syriac,
                42 => Self::Tagalog,
                45 => Self::Tagbanwa,
                52 => Self::TaiLe,
                106 => Self::TaiTham,
                127 => Self::TaiViet,
                153 => Self::Takri,
                35 => Self::Tamil,
                195 => Self::Tangsa,
                154 => Self::Tangut,
                36 => Self::Telugu,
                37 => Self::Thaana,
                38 => Self::Thai,
                39 => Self::Tibetan,
                60 => Self::Tifinagh,
                158 => Self::Tirhuta,
                196 => Self::Toto,
                53 => Self::Ugaritic,
                103 => Self::Unknown,
                99 => Self::Vai,
                197 => Self::Vithkuqi,
                188 => Self::Wancho,
                146 => Self::WarangCiti,
                192 => Self::Yezidi,
                41 => Self::Yi,
                177 => Self::ZanabazarSquare,
                _ => return None,
            })
        }
    }

    #[diplomat::rust_link(icu::properties::props::HangulSyllableType, Struct)]
    #[diplomat::enum_convert(icu_properties::props::HangulSyllableType, needs_wildcard)]
    pub enum HangulSyllableType {
        #[diplomat::rust_link(
            icu::properties::props::HangulSyllableType::NotApplicable,
            EnumVariant
        )]
        NotApplicable = 0,
        #[diplomat::rust_link(
            icu::properties::props::HangulSyllableType::LeadingJamo,
            EnumVariant
        )]
        LeadingJamo = 1,
        #[diplomat::rust_link(icu::properties::props::HangulSyllableType::VowelJamo, EnumVariant)]
        VowelJamo = 2,
        #[diplomat::rust_link(
            icu::properties::props::HangulSyllableType::TrailingJamo,
            EnumVariant
        )]
        TrailingJamo = 3,
        #[diplomat::rust_link(
            icu::properties::props::HangulSyllableType::LeadingVowelSyllable,
            EnumVariant
        )]
        LeadingVowelSyllable = 4,
        #[diplomat::rust_link(
            icu::properties::props::HangulSyllableType::LeadingVowelTrailingSyllable,
            EnumVariant
        )]
        LeadingVowelTrailingSyllable = 5,
    }

    impl HangulSyllableType {
        #[diplomat::rust_link(icu::properties::props::EnumeratedProperty::for_char, FnInTrait)]
        #[cfg(feature = "compiled_data")]
        pub fn for_char(ch: DiplomatChar) -> Self {
            icu_properties::CodePointMapData::<props::HangulSyllableType>::new()
                .get32(ch)
                .into()
        }
        #[diplomat::rust_link(
            icu::properties::props::HangulSyllableType::to_icu4c_value,
            FnInStruct
        )]
        /// Convert to an integer value usable with ICU4C and CodePointMapData
        pub fn to_integer_value(self) -> u8 {
            self as u8
        }
        #[diplomat::rust_link(
            icu::properties::props::HangulSyllableType::from_icu4c_value,
            FnInStruct
        )]
        /// Convert from an integer value from ICU4C or CodePointMapData
        pub fn from_integer_value(other: u8) -> Option<Self> {
            Some(match other {
                0 => Self::NotApplicable,
                1 => Self::LeadingJamo,
                2 => Self::VowelJamo,
                3 => Self::TrailingJamo,
                4 => Self::LeadingVowelSyllable,
                5 => Self::LeadingVowelTrailingSyllable,
                _ => return None,
            })
        }
    }

    #[diplomat::rust_link(icu::properties::props::EastAsianWidth, Struct)]
    #[diplomat::enum_convert(icu_properties::props::EastAsianWidth, needs_wildcard)]
    pub enum EastAsianWidth {
        #[diplomat::rust_link(icu::properties::props::EastAsianWidth::Neutral, EnumVariant)]
        Neutral = 0,
        #[diplomat::rust_link(icu::properties::props::EastAsianWidth::Ambiguous, EnumVariant)]
        Ambiguous = 1,
        #[diplomat::rust_link(icu::properties::props::EastAsianWidth::Halfwidth, EnumVariant)]
        Halfwidth = 2,
        #[diplomat::rust_link(icu::properties::props::EastAsianWidth::Fullwidth, EnumVariant)]
        Fullwidth = 3,
        #[diplomat::rust_link(icu::properties::props::EastAsianWidth::Narrow, EnumVariant)]
        Narrow = 4,
        #[diplomat::rust_link(icu::properties::props::EastAsianWidth::Wide, EnumVariant)]
        Wide = 5,
    }

    impl EastAsianWidth {
        #[diplomat::rust_link(icu::properties::props::EnumeratedProperty::for_char, FnInTrait)]
        #[cfg(feature = "compiled_data")]
        pub fn for_char(ch: DiplomatChar) -> Self {
            icu_properties::CodePointMapData::<props::EastAsianWidth>::new()
                .get32(ch)
                .into()
        }

        #[diplomat::rust_link(icu::properties::PropertyNamesLongBorrowed::get, FnInStruct)]
        #[cfg(feature = "compiled_data")]
        /// Get the "long" name of this property value (returns empty if property value is unknown)
        pub fn long_name(self) -> Option<&'static str> {
            icu_properties::PropertyNamesLongBorrowed::<props::EastAsianWidth>::new()
                .get(self.into())
        }

        #[diplomat::rust_link(icu::properties::PropertyNamesShortBorrowed::get, FnInStruct)]
        #[cfg(feature = "compiled_data")]
        /// Get the "short" name of this property value (returns empty if property value is unknown)
        pub fn short_name(self) -> Option<&'static str> {
            icu_properties::PropertyNamesShortBorrowed::<props::EastAsianWidth>::new()
                .get(self.into())
        }
        #[diplomat::rust_link(icu::properties::props::EastAsianWidth::to_icu4c_value, FnInStruct)]
        /// Convert to an integer value usable with ICU4C and CodePointMapData
        pub fn to_integer_value(self) -> u8 {
            self as u8
        }
        #[diplomat::rust_link(icu::properties::props::EastAsianWidth::from_icu4c_value, FnInStruct)]
        /// Convert from an integer value from ICU4C or CodePointMapData
        pub fn from_integer_value(other: u8) -> Option<Self> {
            Some(match other {
                0 => Self::Neutral,
                1 => Self::Ambiguous,
                2 => Self::Halfwidth,
                3 => Self::Fullwidth,
                4 => Self::Narrow,
                5 => Self::Wide,
                _ => return None,
            })
        }
    }

    #[diplomat::rust_link(icu::properties::props::LineBreak, Struct)]
    #[diplomat::enum_convert(icu_properties::props::LineBreak, needs_wildcard)]
    pub enum LineBreak {
        #[diplomat::rust_link(icu::props::LineBreak::Unknown, EnumVariant)]
        Unknown = 0,
        #[diplomat::rust_link(icu::props::LineBreak::Ambiguous, EnumVariant)]
        Ambiguous = 1,
        #[diplomat::rust_link(icu::props::LineBreak::Alphabetic, EnumVariant)]
        Alphabetic = 2,
        #[diplomat::rust_link(icu::props::LineBreak::BreakBoth, EnumVariant)]
        BreakBoth = 3,
        #[diplomat::rust_link(icu::props::LineBreak::BreakAfter, EnumVariant)]
        BreakAfter = 4,
        #[diplomat::rust_link(icu::props::LineBreak::BreakBefore, EnumVariant)]
        BreakBefore = 5,
        #[diplomat::rust_link(icu::props::LineBreak::MandatoryBreak, EnumVariant)]
        MandatoryBreak = 6,
        #[diplomat::rust_link(icu::props::LineBreak::ContingentBreak, EnumVariant)]
        ContingentBreak = 7,
        #[diplomat::rust_link(icu::props::LineBreak::ClosePunctuation, EnumVariant)]
        ClosePunctuation = 8,
        #[diplomat::rust_link(icu::props::LineBreak::CombiningMark, EnumVariant)]
        CombiningMark = 9,
        #[diplomat::rust_link(icu::props::LineBreak::CarriageReturn, EnumVariant)]
        CarriageReturn = 10,
        #[diplomat::rust_link(icu::props::LineBreak::Exclamation, EnumVariant)]
        Exclamation = 11,
        #[diplomat::rust_link(icu::props::LineBreak::Glue, EnumVariant)]
        Glue = 12,
        #[diplomat::rust_link(icu::props::LineBreak::Hyphen, EnumVariant)]
        Hyphen = 13,
        #[diplomat::rust_link(icu::props::LineBreak::Ideographic, EnumVariant)]
        Ideographic = 14,
        #[diplomat::rust_link(icu::props::LineBreak::Inseparable, EnumVariant)]
        Inseparable = 15,
        #[diplomat::rust_link(icu::props::LineBreak::InfixNumeric, EnumVariant)]
        InfixNumeric = 16,
        #[diplomat::rust_link(icu::props::LineBreak::LineFeed, EnumVariant)]
        LineFeed = 17,
        #[diplomat::rust_link(icu::props::LineBreak::Nonstarter, EnumVariant)]
        Nonstarter = 18,
        #[diplomat::rust_link(icu::props::LineBreak::Numeric, EnumVariant)]
        Numeric = 19,
        #[diplomat::rust_link(icu::props::LineBreak::OpenPunctuation, EnumVariant)]
        OpenPunctuation = 20,
        #[diplomat::rust_link(icu::props::LineBreak::PostfixNumeric, EnumVariant)]
        PostfixNumeric = 21,
        #[diplomat::rust_link(icu::props::LineBreak::PrefixNumeric, EnumVariant)]
        PrefixNumeric = 22,
        #[diplomat::rust_link(icu::props::LineBreak::Quotation, EnumVariant)]
        Quotation = 23,
        #[diplomat::rust_link(icu::props::LineBreak::ComplexContext, EnumVariant)]
        ComplexContext = 24,
        #[diplomat::rust_link(icu::props::LineBreak::Surrogate, EnumVariant)]
        Surrogate = 25,
        #[diplomat::rust_link(icu::props::LineBreak::Space, EnumVariant)]
        Space = 26,
        #[diplomat::rust_link(icu::props::LineBreak::BreakSymbols, EnumVariant)]
        BreakSymbols = 27,
        #[diplomat::rust_link(icu::props::LineBreak::ZWSpace, EnumVariant)]
        ZWSpace = 28,
        #[diplomat::rust_link(icu::props::LineBreak::NextLine, EnumVariant)]
        NextLine = 29,
        #[diplomat::rust_link(icu::props::LineBreak::WordJoiner, EnumVariant)]
        WordJoiner = 30,
        #[diplomat::rust_link(icu::props::LineBreak::H2, EnumVariant)]
        H2 = 31,
        #[diplomat::rust_link(icu::props::LineBreak::H3, EnumVariant)]
        H3 = 32,
        #[diplomat::rust_link(icu::props::LineBreak::JL, EnumVariant)]
        JL = 33,
        #[diplomat::rust_link(icu::props::LineBreak::JT, EnumVariant)]
        JT = 34,
        #[diplomat::rust_link(icu::props::LineBreak::JV, EnumVariant)]
        JV = 35,
        #[diplomat::rust_link(icu::props::LineBreak::CloseParenthesis, EnumVariant)]
        CloseParenthesis = 36,
        #[diplomat::rust_link(icu::props::LineBreak::ConditionalJapaneseStarter, EnumVariant)]
        ConditionalJapaneseStarter = 37,
        #[diplomat::rust_link(icu::props::LineBreak::HebrewLetter, EnumVariant)]
        HebrewLetter = 38,
        #[diplomat::rust_link(icu::props::LineBreak::RegionalIndicator, EnumVariant)]
        RegionalIndicator = 39,
        #[diplomat::rust_link(icu::props::LineBreak::EBase, EnumVariant)]
        EBase = 40,
        #[diplomat::rust_link(icu::props::LineBreak::EModifier, EnumVariant)]
        EModifier = 41,
        #[diplomat::rust_link(icu::props::LineBreak::ZWJ, EnumVariant)]
        ZWJ = 42,
        #[diplomat::rust_link(icu::props::LineBreak::Aksara, EnumVariant)]
        Aksara = 43,
        #[diplomat::rust_link(icu::props::LineBreak::AksaraPrebase, EnumVariant)]
        AksaraPrebase = 44,
        #[diplomat::rust_link(icu::props::LineBreak::AksaraStart, EnumVariant)]
        AksaraStart = 45,
        #[diplomat::rust_link(icu::props::LineBreak::ViramaFinal, EnumVariant)]
        ViramaFinal = 46,
        #[diplomat::rust_link(icu::props::LineBreak::Virama, EnumVariant)]
        Virama = 47,
    }

    impl LineBreak {
        #[diplomat::rust_link(icu::properties::props::EnumeratedProperty::for_char, FnInTrait)]
        #[cfg(feature = "compiled_data")]
        pub fn for_char(ch: DiplomatChar) -> Self {
            icu_properties::CodePointMapData::<props::LineBreak>::new()
                .get32(ch)
                .into()
        }

        #[diplomat::rust_link(icu::properties::PropertyNamesLongBorrowed::get, FnInStruct)]
        #[cfg(feature = "compiled_data")]
        /// Get the "long" name of this property value (returns empty if property value is unknown)
        pub fn long_name(self) -> Option<&'static str> {
            icu_properties::PropertyNamesLongBorrowed::<props::LineBreak>::new().get(self.into())
        }

        #[diplomat::rust_link(icu::properties::PropertyNamesShortBorrowed::get, FnInStruct)]
        #[cfg(feature = "compiled_data")]
        /// Get the "short" name of this property value (returns empty if property value is unknown)
        pub fn short_name(self) -> Option<&'static str> {
            icu_properties::PropertyNamesShortBorrowed::<props::LineBreak>::new().get(self.into())
        }
        #[diplomat::rust_link(icu::properties::props::LineBreak::to_icu4c_value, FnInStruct)]
        /// Convert to an integer value usable with ICU4C and CodePointMapData
        pub fn to_integer_value(self) -> u8 {
            self as u8
        }
        #[diplomat::rust_link(icu::properties::props::LineBreak::from_icu4c_value, FnInStruct)]
        /// Convert from an integer value from ICU4C or CodePointMapData
        pub fn from_integer_value(other: u8) -> Option<Self> {
            Some(match other {
                0 => Self::Unknown,
                1 => Self::Ambiguous,
                2 => Self::Alphabetic,
                3 => Self::BreakBoth,
                4 => Self::BreakAfter,
                5 => Self::BreakBefore,
                6 => Self::MandatoryBreak,
                7 => Self::ContingentBreak,
                8 => Self::ClosePunctuation,
                9 => Self::CombiningMark,
                10 => Self::CarriageReturn,
                11 => Self::Exclamation,
                12 => Self::Glue,
                13 => Self::Hyphen,
                14 => Self::Ideographic,
                15 => Self::Inseparable,
                16 => Self::InfixNumeric,
                17 => Self::LineFeed,
                18 => Self::Nonstarter,
                19 => Self::Numeric,
                20 => Self::OpenPunctuation,
                21 => Self::PostfixNumeric,
                22 => Self::PrefixNumeric,
                23 => Self::Quotation,
                24 => Self::ComplexContext,
                25 => Self::Surrogate,
                26 => Self::Space,
                27 => Self::BreakSymbols,
                28 => Self::ZWSpace,
                29 => Self::NextLine,
                30 => Self::WordJoiner,
                31 => Self::H2,
                32 => Self::H3,
                33 => Self::JL,
                34 => Self::JT,
                35 => Self::JV,
                36 => Self::CloseParenthesis,
                37 => Self::ConditionalJapaneseStarter,
                38 => Self::HebrewLetter,
                39 => Self::RegionalIndicator,
                40 => Self::EBase,
                41 => Self::EModifier,
                42 => Self::ZWJ,
                43 => Self::Aksara,
                44 => Self::AksaraPrebase,
                45 => Self::AksaraStart,
                46 => Self::ViramaFinal,
                47 => Self::Virama,
                _ => return None,
            })
        }
    }

    #[diplomat::rust_link(icu::properties::props::GraphemeClusterBreak, Struct)]
    #[diplomat::enum_convert(icu_properties::props::GraphemeClusterBreak, needs_wildcard)]
    pub enum GraphemeClusterBreak {
        #[diplomat::rust_link(icu::properties::props::LineBreak::Other, EnumVariant)]
        Other = 0,
        #[diplomat::rust_link(icu::properties::props::LineBreak::Control, EnumVariant)]
        Control = 1,
        #[diplomat::rust_link(icu::properties::props::LineBreak::CR, EnumVariant)]
        CR = 2,
        #[diplomat::rust_link(icu::properties::props::LineBreak::Extend, EnumVariant)]
        Extend = 3,
        #[diplomat::rust_link(icu::properties::props::LineBreak::L, EnumVariant)]
        L = 4,
        #[diplomat::rust_link(icu::properties::props::LineBreak::LF, EnumVariant)]
        LF = 5,
        #[diplomat::rust_link(icu::properties::props::LineBreak::LV, EnumVariant)]
        LV = 6,
        #[diplomat::rust_link(icu::properties::props::LineBreak::LVT, EnumVariant)]
        LVT = 7,
        #[diplomat::rust_link(icu::properties::props::LineBreak::T, EnumVariant)]
        T = 8,
        #[diplomat::rust_link(icu::properties::props::LineBreak::V, EnumVariant)]
        V = 9,
        #[diplomat::rust_link(icu::properties::props::LineBreak::SpacingMark, EnumVariant)]
        SpacingMark = 10,
        #[diplomat::rust_link(icu::properties::props::LineBreak::Prepend, EnumVariant)]
        Prepend = 11,
        #[diplomat::rust_link(icu::properties::props::LineBreak::RegionalIndicator, EnumVariant)]
        RegionalIndicator = 12,
        #[diplomat::rust_link(icu::properties::props::LineBreak::EBase, EnumVariant)]
        EBase = 13,
        #[diplomat::rust_link(icu::properties::props::LineBreak::EBaseGAZ, EnumVariant)]
        EBaseGAZ = 14,
        #[diplomat::rust_link(icu::properties::props::LineBreak::EModifier, EnumVariant)]
        EModifier = 15,
        #[diplomat::rust_link(icu::properties::props::LineBreak::GlueAfterZwj, EnumVariant)]
        GlueAfterZwj = 16,
        #[diplomat::rust_link(icu::properties::props::LineBreak::ZWJ, EnumVariant)]
        ZWJ = 17,
    }

    impl GraphemeClusterBreak {
        #[diplomat::rust_link(icu::properties::props::EnumeratedProperty::for_char, FnInTrait)]
        #[cfg(feature = "compiled_data")]
        pub fn for_char(ch: DiplomatChar) -> Self {
            icu_properties::CodePointMapData::<props::GraphemeClusterBreak>::new()
                .get32(ch)
                .into()
        }
        #[diplomat::rust_link(
            icu::properties::props::GraphemeClusterBreak::to_icu4c_value,
            FnInStruct
        )]
        /// Convert to an integer value usable with ICU4C and CodePointMapData
        pub fn to_integer_value(self) -> u8 {
            self as u8
        }
        #[diplomat::rust_link(
            icu::properties::props::GraphemeClusterBreak::from_icu4c_value,
            FnInStruct
        )]
        /// Convert from an integer value from ICU4C or CodePointMapData
        pub fn from_integer_value(other: u8) -> Option<Self> {
            Some(match other {
                0 => Self::Other,
                1 => Self::Control,
                2 => Self::CR,
                3 => Self::Extend,
                4 => Self::L,
                5 => Self::LF,
                6 => Self::LV,
                7 => Self::LVT,
                8 => Self::T,
                9 => Self::V,
                10 => Self::SpacingMark,
                11 => Self::Prepend,
                12 => Self::RegionalIndicator,
                13 => Self::EBase,
                14 => Self::EBaseGAZ,
                15 => Self::EModifier,
                16 => Self::GlueAfterZwj,
                17 => Self::ZWJ,
                _ => return None,
            })
        }
    }

    #[diplomat::rust_link(icu::properties::props::WordBreak, Struct)]
    #[diplomat::enum_convert(icu_properties::props::WordBreak, needs_wildcard)]
    pub enum WordBreak {
        #[diplomat::rust_link(icu::properties::props::WordBreak::Other, EnumVariant)]
        Other = 0,
        #[diplomat::rust_link(icu::properties::props::WordBreak::ALetter, EnumVariant)]
        ALetter = 1,
        #[diplomat::rust_link(icu::properties::props::WordBreak::Format, EnumVariant)]
        Format = 2,
        #[diplomat::rust_link(icu::properties::props::WordBreak::Katakana, EnumVariant)]
        Katakana = 3,
        #[diplomat::rust_link(icu::properties::props::WordBreak::MidLetter, EnumVariant)]
        MidLetter = 4,
        #[diplomat::rust_link(icu::properties::props::WordBreak::MidNum, EnumVariant)]
        MidNum = 5,
        #[diplomat::rust_link(icu::properties::props::WordBreak::Numeric, EnumVariant)]
        Numeric = 6,
        #[diplomat::rust_link(icu::properties::props::WordBreak::ExtendNumLet, EnumVariant)]
        ExtendNumLet = 7,
        #[diplomat::rust_link(icu::properties::props::WordBreak::CR, EnumVariant)]
        CR = 8,
        #[diplomat::rust_link(icu::properties::props::WordBreak::Extend, EnumVariant)]
        Extend = 9,
        #[diplomat::rust_link(icu::properties::props::WordBreak::LF, EnumVariant)]
        LF = 10,
        #[diplomat::rust_link(icu::properties::props::WordBreak::MidNumLet, EnumVariant)]
        MidNumLet = 11,
        #[diplomat::rust_link(icu::properties::props::WordBreak::Newline, EnumVariant)]
        Newline = 12,
        #[diplomat::rust_link(icu::properties::props::WordBreak::RegionalIndicator, EnumVariant)]
        RegionalIndicator = 13,
        #[diplomat::rust_link(icu::properties::props::WordBreak::HebrewLetter, EnumVariant)]
        HebrewLetter = 14,
        #[diplomat::rust_link(icu::properties::props::WordBreak::SingleQuote, EnumVariant)]
        SingleQuote = 15,
        #[diplomat::rust_link(icu::properties::props::WordBreak::DoubleQuote, EnumVariant)]
        DoubleQuote = 16,
        #[diplomat::rust_link(icu::properties::props::WordBreak::EBase, EnumVariant)]
        EBase = 17,
        #[diplomat::rust_link(icu::properties::props::WordBreak::EBaseGAZ, EnumVariant)]
        EBaseGAZ = 18,
        #[diplomat::rust_link(icu::properties::props::WordBreak::EModifier, EnumVariant)]
        EModifier = 19,
        #[diplomat::rust_link(icu::properties::props::WordBreak::GlueAfterZwj, EnumVariant)]
        GlueAfterZwj = 20,
        #[diplomat::rust_link(icu::properties::props::WordBreak::ZWJ, EnumVariant)]
        ZWJ = 21,
        #[diplomat::rust_link(icu::properties::props::WordBreak::WSegSpace, EnumVariant)]
        WSegSpace = 22,
    }

    impl WordBreak {
        #[diplomat::rust_link(icu::properties::props::EnumeratedProperty::for_char, FnInTrait)]
        #[cfg(feature = "compiled_data")]
        pub fn for_char(ch: DiplomatChar) -> Self {
            icu_properties::CodePointMapData::<props::WordBreak>::new()
                .get32(ch)
                .into()
        }

        #[diplomat::rust_link(icu::properties::PropertyNamesLongBorrowed::get, FnInStruct)]
        #[cfg(feature = "compiled_data")]
        /// Get the "long" name of this property value (returns empty if property value is unknown)
        pub fn long_name(self) -> Option<&'static str> {
            icu_properties::PropertyNamesLongBorrowed::<props::WordBreak>::new().get(self.into())
        }

        #[diplomat::rust_link(icu::properties::PropertyNamesShortBorrowed::get, FnInStruct)]
        #[cfg(feature = "compiled_data")]
        /// Get the "short" name of this property value (returns empty if property value is unknown)
        pub fn short_name(self) -> Option<&'static str> {
            icu_properties::PropertyNamesShortBorrowed::<props::WordBreak>::new().get(self.into())
        }
        #[diplomat::rust_link(icu::properties::props::WordBreak::to_icu4c_value, FnInStruct)]
        /// Convert to an integer value usable with ICU4C and CodePointMapData
        pub fn to_integer_value(self) -> u8 {
            self as u8
        }
        #[diplomat::rust_link(icu::properties::props::WordBreak::from_icu4c_value, FnInStruct)]
        /// Convert from an integer value from ICU4C or CodePointMapData
        pub fn from_integer_value(other: u8) -> Option<Self> {
            Some(match other {
                0 => Self::Other,
                1 => Self::ALetter,
                2 => Self::Format,
                3 => Self::Katakana,
                4 => Self::MidLetter,
                5 => Self::MidNum,
                6 => Self::Numeric,
                7 => Self::ExtendNumLet,
                8 => Self::CR,
                9 => Self::Extend,
                10 => Self::LF,
                11 => Self::MidNumLet,
                12 => Self::Newline,
                13 => Self::RegionalIndicator,
                14 => Self::HebrewLetter,
                15 => Self::SingleQuote,
                16 => Self::DoubleQuote,
                17 => Self::EBase,
                18 => Self::EBaseGAZ,
                19 => Self::EModifier,
                20 => Self::GlueAfterZwj,
                21 => Self::ZWJ,
                22 => Self::WSegSpace,
                _ => return None,
            })
        }
    }

    #[diplomat::rust_link(icu::properties::props::SentenceBreak, Struct)]
    #[diplomat::enum_convert(icu_properties::props::SentenceBreak, needs_wildcard)]
    pub enum SentenceBreak {
        #[diplomat::rust_link(icu::properties::props::SentenceBreak::Other, EnumVariant)]
        Other = 0,
        #[diplomat::rust_link(icu::properties::props::SentenceBreak::ATerm, EnumVariant)]
        ATerm = 1,
        #[diplomat::rust_link(icu::properties::props::SentenceBreak::Close, EnumVariant)]
        Close = 2,
        #[diplomat::rust_link(icu::properties::props::SentenceBreak::Format, EnumVariant)]
        Format = 3,
        #[diplomat::rust_link(icu::properties::props::SentenceBreak::Lower, EnumVariant)]
        Lower = 4,
        #[diplomat::rust_link(icu::properties::props::SentenceBreak::Numeric, EnumVariant)]
        Numeric = 5,
        #[diplomat::rust_link(icu::properties::props::SentenceBreak::OLetter, EnumVariant)]
        OLetter = 6,
        #[diplomat::rust_link(icu::properties::props::SentenceBreak::Sep, EnumVariant)]
        Sep = 7,
        #[diplomat::rust_link(icu::properties::props::SentenceBreak::Sp, EnumVariant)]
        Sp = 8,
        #[diplomat::rust_link(icu::properties::props::SentenceBreak::STerm, EnumVariant)]
        STerm = 9,
        #[diplomat::rust_link(icu::properties::props::SentenceBreak::Upper, EnumVariant)]
        Upper = 10,
        #[diplomat::rust_link(icu::properties::props::SentenceBreak::CR, EnumVariant)]
        CR = 11,
        #[diplomat::rust_link(icu::properties::props::SentenceBreak::Extend, EnumVariant)]
        Extend = 12,
        #[diplomat::rust_link(icu::properties::props::SentenceBreak::LF, EnumVariant)]
        LF = 13,
        #[diplomat::rust_link(icu::properties::props::SentenceBreak::SContinue, EnumVariant)]
        SContinue = 14,
    }

    impl SentenceBreak {
        #[diplomat::rust_link(icu::properties::props::EnumeratedProperty::for_char, FnInTrait)]
        #[cfg(feature = "compiled_data")]
        pub fn for_char(ch: DiplomatChar) -> Self {
            icu_properties::CodePointMapData::<props::SentenceBreak>::new()
                .get32(ch)
                .into()
        }

        #[diplomat::rust_link(icu::properties::PropertyNamesLongBorrowed::get, FnInStruct)]
        #[cfg(feature = "compiled_data")]
        /// Get the "long" name of this property value (returns empty if property value is unknown)
        pub fn long_name(self) -> Option<&'static str> {
            icu_properties::PropertyNamesLongBorrowed::<props::SentenceBreak>::new()
                .get(self.into())
        }

        #[diplomat::rust_link(icu::properties::PropertyNamesShortBorrowed::get, FnInStruct)]
        #[cfg(feature = "compiled_data")]
        /// Get the "short" name of this property value (returns empty if property value is unknown)
        pub fn short_name(self) -> Option<&'static str> {
            icu_properties::PropertyNamesShortBorrowed::<props::SentenceBreak>::new()
                .get(self.into())
        }
        #[diplomat::rust_link(icu::properties::props::SentenceBreak::to_icu4c_value, FnInStruct)]
        /// Convert to an integer value usable with ICU4C and CodePointMapData
        pub fn to_integer_value(self) -> u8 {
            self as u8
        }
        #[diplomat::rust_link(icu::properties::props::SentenceBreak::from_icu4c_value, FnInStruct)]
        /// Convert from an integer value from ICU4C or CodePointMapData
        pub fn from_integer_value(other: u8) -> Option<Self> {
            Some(match other {
                0 => Self::Other,
                1 => Self::ATerm,
                2 => Self::Close,
                3 => Self::Format,
                4 => Self::Lower,
                5 => Self::Numeric,
                6 => Self::OLetter,
                7 => Self::Sep,
                8 => Self::Sp,
                9 => Self::STerm,
                10 => Self::Upper,
                11 => Self::CR,
                12 => Self::Extend,
                13 => Self::LF,
                14 => Self::SContinue,
                _ => return None,
            })
        }
    }

    #[diplomat::rust_link(icu::properties::props::CanonicalCombiningClass, Struct)]
    #[diplomat::enum_convert(icu_properties::props::CanonicalCombiningClass, needs_wildcard)]
    pub enum CanonicalCombiningClass {
        #[diplomat::rust_link(
            icu::properties::props::CanonicalCombiningClass::NotReordered,
            EnumVariant
        )]
        NotReordered = 0,
        #[diplomat::rust_link(
            icu::properties::props::CanonicalCombiningClass::Overlay,
            EnumVariant
        )]
        Overlay = 1,
        #[diplomat::rust_link(
            icu::properties::props::CanonicalCombiningClass::HanReading,
            EnumVariant
        )]
        HanReading = 6,
        #[diplomat::rust_link(icu::properties::props::CanonicalCombiningClass::Nukta, EnumVariant)]
        Nukta = 7,
        #[diplomat::rust_link(
            icu::properties::props::CanonicalCombiningClass::KanaVoicing,
            EnumVariant
        )]
        KanaVoicing = 8,
        #[diplomat::rust_link(
            icu::properties::props::CanonicalCombiningClass::Virama,
            EnumVariant
        )]
        Virama = 9,
        #[diplomat::rust_link(icu::properties::props::CanonicalCombiningClass::CCC10, EnumVariant)]
        CCC10 = 10,
        #[diplomat::rust_link(icu::properties::props::CanonicalCombiningClass::CCC11, EnumVariant)]
        CCC11 = 11,
        #[diplomat::rust_link(icu::properties::props::CanonicalCombiningClass::CCC12, EnumVariant)]
        CCC12 = 12,
        #[diplomat::rust_link(icu::properties::props::CanonicalCombiningClass::CCC13, EnumVariant)]
        CCC13 = 13,
        #[diplomat::rust_link(icu::properties::props::CanonicalCombiningClass::CCC14, EnumVariant)]
        CCC14 = 14,
        #[diplomat::rust_link(icu::properties::props::CanonicalCombiningClass::CCC15, EnumVariant)]
        CCC15 = 15,
        #[diplomat::rust_link(icu::properties::props::CanonicalCombiningClass::CCC16, EnumVariant)]
        CCC16 = 16,
        #[diplomat::rust_link(icu::properties::props::CanonicalCombiningClass::CCC17, EnumVariant)]
        CCC17 = 17,
        #[diplomat::rust_link(icu::properties::props::CanonicalCombiningClass::CCC18, EnumVariant)]
        CCC18 = 18,
        #[diplomat::rust_link(icu::properties::props::CanonicalCombiningClass::CCC19, EnumVariant)]
        CCC19 = 19,
        #[diplomat::rust_link(icu::properties::props::CanonicalCombiningClass::CCC20, EnumVariant)]
        CCC20 = 20,
        #[diplomat::rust_link(icu::properties::props::CanonicalCombiningClass::CCC21, EnumVariant)]
        CCC21 = 21,
        #[diplomat::rust_link(icu::properties::props::CanonicalCombiningClass::CCC22, EnumVariant)]
        CCC22 = 22,
        #[diplomat::rust_link(icu::properties::props::CanonicalCombiningClass::CCC23, EnumVariant)]
        CCC23 = 23,
        #[diplomat::rust_link(icu::properties::props::CanonicalCombiningClass::CCC24, EnumVariant)]
        CCC24 = 24,
        #[diplomat::rust_link(icu::properties::props::CanonicalCombiningClass::CCC25, EnumVariant)]
        CCC25 = 25,
        #[diplomat::rust_link(icu::properties::props::CanonicalCombiningClass::CCC26, EnumVariant)]
        CCC26 = 26,
        #[diplomat::rust_link(icu::properties::props::CanonicalCombiningClass::CCC27, EnumVariant)]
        CCC27 = 27,
        #[diplomat::rust_link(icu::properties::props::CanonicalCombiningClass::CCC28, EnumVariant)]
        CCC28 = 28,
        #[diplomat::rust_link(icu::properties::props::CanonicalCombiningClass::CCC29, EnumVariant)]
        CCC29 = 29,
        #[diplomat::rust_link(icu::properties::props::CanonicalCombiningClass::CCC30, EnumVariant)]
        CCC30 = 30,
        #[diplomat::rust_link(icu::properties::props::CanonicalCombiningClass::CCC31, EnumVariant)]
        CCC31 = 31,
        #[diplomat::rust_link(icu::properties::props::CanonicalCombiningClass::CCC32, EnumVariant)]
        CCC32 = 32,
        #[diplomat::rust_link(icu::properties::props::CanonicalCombiningClass::CCC33, EnumVariant)]
        CCC33 = 33,
        #[diplomat::rust_link(icu::properties::props::CanonicalCombiningClass::CCC34, EnumVariant)]
        CCC34 = 34,
        #[diplomat::rust_link(icu::properties::props::CanonicalCombiningClass::CCC35, EnumVariant)]
        CCC35 = 35,
        #[diplomat::rust_link(icu::properties::props::CanonicalCombiningClass::CCC36, EnumVariant)]
        CCC36 = 36,
        #[diplomat::rust_link(icu::properties::props::CanonicalCombiningClass::CCC84, EnumVariant)]
        CCC84 = 84,
        #[diplomat::rust_link(icu::properties::props::CanonicalCombiningClass::CCC91, EnumVariant)]
        CCC91 = 91,
        #[diplomat::rust_link(
            icu::properties::props::CanonicalCombiningClass::CCC103,
            EnumVariant
        )]
        CCC103 = 103,
        #[diplomat::rust_link(
            icu::properties::props::CanonicalCombiningClass::CCC107,
            EnumVariant
        )]
        CCC107 = 107,
        #[diplomat::rust_link(
            icu::properties::props::CanonicalCombiningClass::CCC118,
            EnumVariant
        )]
        CCC118 = 118,
        #[diplomat::rust_link(
            icu::properties::props::CanonicalCombiningClass::CCC122,
            EnumVariant
        )]
        CCC122 = 122,
        #[diplomat::rust_link(
            icu::properties::props::CanonicalCombiningClass::CCC129,
            EnumVariant
        )]
        CCC129 = 129,
        #[diplomat::rust_link(
            icu::properties::props::CanonicalCombiningClass::CCC130,
            EnumVariant
        )]
        CCC130 = 130,
        #[diplomat::rust_link(
            icu::properties::props::CanonicalCombiningClass::CCC132,
            EnumVariant
        )]
        CCC132 = 132,
        #[diplomat::rust_link(
            icu::properties::props::CanonicalCombiningClass::CCC133,
            EnumVariant
        )]
        CCC133 = 133,
        #[diplomat::rust_link(
            icu::properties::props::CanonicalCombiningClass::AttachedBelowLeft,
            EnumVariant
        )]
        AttachedBelowLeft = 200,
        #[diplomat::rust_link(
            icu::properties::props::CanonicalCombiningClass::AttachedBelow,
            EnumVariant
        )]
        AttachedBelow = 202,
        #[diplomat::rust_link(
            icu::properties::props::CanonicalCombiningClass::AttachedAbove,
            EnumVariant
        )]
        AttachedAbove = 214,
        #[diplomat::rust_link(
            icu::properties::props::CanonicalCombiningClass::AttachedAboveRight,
            EnumVariant
        )]
        AttachedAboveRight = 216,
        #[diplomat::rust_link(
            icu::properties::props::CanonicalCombiningClass::BelowLeft,
            EnumVariant
        )]
        BelowLeft = 218,
        #[diplomat::rust_link(icu::properties::props::CanonicalCombiningClass::Below, EnumVariant)]
        Below = 220,
        #[diplomat::rust_link(
            icu::properties::props::CanonicalCombiningClass::BelowRight,
            EnumVariant
        )]
        BelowRight = 222,
        #[diplomat::rust_link(icu::properties::props::CanonicalCombiningClass::Left, EnumVariant)]
        Left = 224,
        #[diplomat::rust_link(icu::properties::props::CanonicalCombiningClass::Right, EnumVariant)]
        Right = 226,
        #[diplomat::rust_link(
            icu::properties::props::CanonicalCombiningClass::AboveLeft,
            EnumVariant
        )]
        AboveLeft = 228,
        #[diplomat::rust_link(icu::properties::props::CanonicalCombiningClass::Above, EnumVariant)]
        Above = 230,
        #[diplomat::rust_link(
            icu::properties::props::CanonicalCombiningClass::AboveRight,
            EnumVariant
        )]
        AboveRight = 232,
        #[diplomat::rust_link(
            icu::properties::props::CanonicalCombiningClass::DoubleBelow,
            EnumVariant
        )]
        DoubleBelow = 233,
        #[diplomat::rust_link(
            icu::properties::props::CanonicalCombiningClass::DoubleAbove,
            EnumVariant
        )]
        DoubleAbove = 234,
        #[diplomat::rust_link(
            icu::properties::props::CanonicalCombiningClass::IotaSubscript,
            EnumVariant
        )]
        IotaSubscript = 240,
    }

    impl CanonicalCombiningClass {
        #[diplomat::rust_link(icu::properties::props::EnumeratedProperty::for_char, FnInTrait)]
        #[cfg(feature = "compiled_data")]
        pub fn for_char(ch: DiplomatChar) -> Self {
            icu_properties::CodePointMapData::<props::CanonicalCombiningClass>::new()
                .get32(ch)
                .into()
        }
        #[diplomat::rust_link(
            icu::properties::props::CanonicalCombiningClass::to_icu4c_value,
            FnInStruct
        )]
        /// Convert to an integer value usable with ICU4C and CodePointMapData
        pub fn to_integer_value(self) -> u8 {
            self as u8
        }
        #[diplomat::rust_link(
            icu::properties::props::CanonicalCombiningClass::from_icu4c_value,
            FnInStruct
        )]
        /// Convert from an integer value from ICU4C or CodePointMapData
        pub fn from_integer_value(other: u8) -> Option<Self> {
            Some(match other {
                0 => Self::NotReordered,
                1 => Self::Overlay,
                6 => Self::HanReading,
                7 => Self::Nukta,
                8 => Self::KanaVoicing,
                9 => Self::Virama,
                10 => Self::CCC10,
                11 => Self::CCC11,
                12 => Self::CCC12,
                13 => Self::CCC13,
                14 => Self::CCC14,
                15 => Self::CCC15,
                16 => Self::CCC16,
                17 => Self::CCC17,
                18 => Self::CCC18,
                19 => Self::CCC19,
                20 => Self::CCC20,
                21 => Self::CCC21,
                22 => Self::CCC22,
                23 => Self::CCC23,
                24 => Self::CCC24,
                25 => Self::CCC25,
                26 => Self::CCC26,
                27 => Self::CCC27,
                28 => Self::CCC28,
                29 => Self::CCC29,
                30 => Self::CCC30,
                31 => Self::CCC31,
                32 => Self::CCC32,
                33 => Self::CCC33,
                34 => Self::CCC34,
                35 => Self::CCC35,
                36 => Self::CCC36,
                84 => Self::CCC84,
                91 => Self::CCC91,
                103 => Self::CCC103,
                107 => Self::CCC107,
                118 => Self::CCC118,
                122 => Self::CCC122,
                129 => Self::CCC129,
                130 => Self::CCC130,
                132 => Self::CCC132,
                133 => Self::CCC133,
                200 => Self::AttachedBelowLeft,
                202 => Self::AttachedBelow,
                214 => Self::AttachedAbove,
                216 => Self::AttachedAboveRight,
                218 => Self::BelowLeft,
                220 => Self::Below,
                222 => Self::BelowRight,
                224 => Self::Left,
                226 => Self::Right,
                228 => Self::AboveLeft,
                230 => Self::Above,
                232 => Self::AboveRight,
                233 => Self::DoubleBelow,
                234 => Self::DoubleAbove,
                240 => Self::IotaSubscript,
                _ => return None,
            })
        }
    }

    #[diplomat::rust_link(icu::properties::props::IndicSyllabicCategory, Struct)]
    #[diplomat::enum_convert(icu_properties::props::IndicSyllabicCategory, needs_wildcard)]
    pub enum IndicSyllabicCategory {
        #[diplomat::rust_link(icu::properties::props::IndicSyllabicCategory::Other, EnumVariant)]
        Other = 0,
        #[diplomat::rust_link(
            icu::properties::props::IndicSyllabicCategory::Avagraha,
            EnumVariant
        )]
        Avagraha = 1,
        #[diplomat::rust_link(icu::properties::props::IndicSyllabicCategory::Bindu, EnumVariant)]
        Bindu = 2,
        #[diplomat::rust_link(
            icu::properties::props::IndicSyllabicCategory::BrahmiJoiningNumber,
            EnumVariant
        )]
        BrahmiJoiningNumber = 3,
        #[diplomat::rust_link(
            icu::properties::props::IndicSyllabicCategory::CantillationMark,
            EnumVariant
        )]
        CantillationMark = 4,
        #[diplomat::rust_link(
            icu::properties::props::IndicSyllabicCategory::Consonant,
            EnumVariant
        )]
        Consonant = 5,
        #[diplomat::rust_link(
            icu::properties::props::IndicSyllabicCategory::ConsonantDead,
            EnumVariant
        )]
        ConsonantDead = 6,
        #[diplomat::rust_link(
            icu::properties::props::IndicSyllabicCategory::ConsonantFinal,
            EnumVariant
        )]
        ConsonantFinal = 7,
        #[diplomat::rust_link(
            icu::properties::props::IndicSyllabicCategory::ConsonantHeadLetter,
            EnumVariant
        )]
        ConsonantHeadLetter = 8,
        #[diplomat::rust_link(
            icu::properties::props::IndicSyllabicCategory::ConsonantInitialPostfixed,
            EnumVariant
        )]
        ConsonantInitialPostfixed = 9,
        #[diplomat::rust_link(
            icu::properties::props::IndicSyllabicCategory::ConsonantKiller,
            EnumVariant
        )]
        ConsonantKiller = 10,
        #[diplomat::rust_link(
            icu::properties::props::IndicSyllabicCategory::ConsonantMedial,
            EnumVariant
        )]
        ConsonantMedial = 11,
        #[diplomat::rust_link(
            icu::properties::props::IndicSyllabicCategory::ConsonantPlaceholder,
            EnumVariant
        )]
        ConsonantPlaceholder = 12,
        #[diplomat::rust_link(
            icu::properties::props::IndicSyllabicCategory::ConsonantPrecedingRepha,
            EnumVariant
        )]
        ConsonantPrecedingRepha = 13,
        #[diplomat::rust_link(
            icu::properties::props::IndicSyllabicCategory::ConsonantPrefixed,
            EnumVariant
        )]
        ConsonantPrefixed = 14,
        #[diplomat::rust_link(
            icu::properties::props::IndicSyllabicCategory::ConsonantSucceedingRepha,
            EnumVariant
        )]
        ConsonantSucceedingRepha = 15,
        #[diplomat::rust_link(
            icu::properties::props::IndicSyllabicCategory::ConsonantSubjoined,
            EnumVariant
        )]
        ConsonantSubjoined = 16,
        #[diplomat::rust_link(
            icu::properties::props::IndicSyllabicCategory::ConsonantWithStacker,
            EnumVariant
        )]
        ConsonantWithStacker = 17,
        #[diplomat::rust_link(
            icu::properties::props::IndicSyllabicCategory::GeminationMark,
            EnumVariant
        )]
        GeminationMark = 18,
        #[diplomat::rust_link(
            icu::properties::props::IndicSyllabicCategory::InvisibleStacker,
            EnumVariant
        )]
        InvisibleStacker = 19,
        #[diplomat::rust_link(icu::properties::props::IndicSyllabicCategory::Joiner, EnumVariant)]
        Joiner = 20,
        #[diplomat::rust_link(
            icu::properties::props::IndicSyllabicCategory::ModifyingLetter,
            EnumVariant
        )]
        ModifyingLetter = 21,
        #[diplomat::rust_link(
            icu::properties::props::IndicSyllabicCategory::NonJoiner,
            EnumVariant
        )]
        NonJoiner = 22,
        #[diplomat::rust_link(icu::properties::props::IndicSyllabicCategory::Nukta, EnumVariant)]
        Nukta = 23,
        #[diplomat::rust_link(icu::properties::props::IndicSyllabicCategory::Number, EnumVariant)]
        Number = 24,
        #[diplomat::rust_link(
            icu::properties::props::IndicSyllabicCategory::NumberJoiner,
            EnumVariant
        )]
        NumberJoiner = 25,
        #[diplomat::rust_link(
            icu::properties::props::IndicSyllabicCategory::PureKiller,
            EnumVariant
        )]
        PureKiller = 26,
        #[diplomat::rust_link(
            icu::properties::props::IndicSyllabicCategory::RegisterShifter,
            EnumVariant
        )]
        RegisterShifter = 27,
        #[diplomat::rust_link(
            icu::properties::props::IndicSyllabicCategory::SyllableModifier,
            EnumVariant
        )]
        SyllableModifier = 28,
        #[diplomat::rust_link(
            icu::properties::props::IndicSyllabicCategory::ToneLetter,
            EnumVariant
        )]
        ToneLetter = 29,
        #[diplomat::rust_link(
            icu::properties::props::IndicSyllabicCategory::ToneMark,
            EnumVariant
        )]
        ToneMark = 30,
        #[diplomat::rust_link(icu::properties::props::IndicSyllabicCategory::Virama, EnumVariant)]
        Virama = 31,
        #[diplomat::rust_link(icu::properties::props::IndicSyllabicCategory::Visarga, EnumVariant)]
        Visarga = 32,
        #[diplomat::rust_link(icu::properties::props::IndicSyllabicCategory::Vowel, EnumVariant)]
        Vowel = 33,
        #[diplomat::rust_link(
            icu::properties::props::IndicSyllabicCategory::VowelDependent,
            EnumVariant
        )]
        VowelDependent = 34,
        #[diplomat::rust_link(
            icu::properties::props::IndicSyllabicCategory::VowelIndependent,
            EnumVariant
        )]
        VowelIndependent = 35,
        #[diplomat::rust_link(
            icu::properties::props::IndicSyllabicCategory::ReorderingKiller,
            EnumVariant
        )]
        ReorderingKiller = 36,
    }

    impl IndicSyllabicCategory {
        #[diplomat::rust_link(icu::properties::props::EnumeratedProperty::for_char, FnInTrait)]
        #[cfg(feature = "compiled_data")]
        pub fn for_char(ch: DiplomatChar) -> Self {
            icu_properties::CodePointMapData::<props::IndicSyllabicCategory>::new()
                .get32(ch)
                .into()
        }
        #[diplomat::rust_link(
            icu::properties::props::IndicSyllabicCategory::to_icu4c_value,
            FnInStruct
        )]
        /// Convert to an integer value usable with ICU4C and CodePointMapData
        pub fn to_integer_value(self) -> u8 {
            self as u8
        }
        #[diplomat::rust_link(
            icu::properties::props::IndicSyllabicCategory::from_icu4c_value,
            FnInStruct
        )]
        /// Convert from an integer value from ICU4C or CodePointMapData
        pub fn from_integer_value(other: u8) -> Option<Self> {
            Some(match other {
                0 => Self::Other,
                1 => Self::Avagraha,
                2 => Self::Bindu,
                3 => Self::BrahmiJoiningNumber,
                4 => Self::CantillationMark,
                5 => Self::Consonant,
                6 => Self::ConsonantDead,
                7 => Self::ConsonantFinal,
                8 => Self::ConsonantHeadLetter,
                9 => Self::ConsonantInitialPostfixed,
                10 => Self::ConsonantKiller,
                11 => Self::ConsonantMedial,
                12 => Self::ConsonantPlaceholder,
                13 => Self::ConsonantPrecedingRepha,
                14 => Self::ConsonantPrefixed,
                15 => Self::ConsonantSucceedingRepha,
                16 => Self::ConsonantSubjoined,
                17 => Self::ConsonantWithStacker,
                18 => Self::GeminationMark,
                19 => Self::InvisibleStacker,
                20 => Self::Joiner,
                21 => Self::ModifyingLetter,
                22 => Self::NonJoiner,
                23 => Self::Nukta,
                24 => Self::Number,
                25 => Self::NumberJoiner,
                26 => Self::PureKiller,
                27 => Self::RegisterShifter,
                28 => Self::SyllableModifier,
                29 => Self::ToneLetter,
                30 => Self::ToneMark,
                31 => Self::Virama,
                32 => Self::Visarga,
                33 => Self::Vowel,
                34 => Self::VowelDependent,
                35 => Self::VowelIndependent,
                36 => Self::ReorderingKiller,
                _ => return None,
            })
        }
    }

    #[diplomat::rust_link(icu::properties::props::JoiningType, Struct)]
    #[diplomat::enum_convert(icu_properties::props::JoiningType, needs_wildcard)]
    pub enum JoiningType {
        #[diplomat::rust_link(icu::properties::props::JoiningType::NonJoining, EnumVariant)]
        NonJoining = 0,
        #[diplomat::rust_link(icu::properties::props::JoiningType::JoinCausing, EnumVariant)]
        JoinCausing = 1,
        #[diplomat::rust_link(icu::properties::props::JoiningType::DualJoining, EnumVariant)]
        DualJoining = 2,
        #[diplomat::rust_link(icu::properties::props::JoiningType::LeftJoining, EnumVariant)]
        LeftJoining = 3,
        #[diplomat::rust_link(icu::properties::props::JoiningType::RightJoining, EnumVariant)]
        RightJoining = 4,
        #[diplomat::rust_link(icu::properties::props::JoiningType::Transparent, EnumVariant)]
        Transparent = 5,
    }

    impl JoiningType {
        #[diplomat::rust_link(icu::properties::props::EnumeratedProperty::for_char, FnInTrait)]
        #[cfg(feature = "compiled_data")]
        pub fn for_char(ch: DiplomatChar) -> Self {
            icu_properties::CodePointMapData::<props::JoiningType>::new()
                .get32(ch)
                .into()
        }

        #[diplomat::rust_link(icu::properties::PropertyNamesLongBorrowed::get, FnInStruct)]
        #[cfg(feature = "compiled_data")]
        /// Get the "long" name of this property value (returns empty if property value is unknown)
        pub fn long_name(self) -> Option<&'static str> {
            icu_properties::PropertyNamesLongBorrowed::<props::JoiningType>::new().get(self.into())
        }

        #[diplomat::rust_link(icu::properties::PropertyNamesShortBorrowed::get, FnInStruct)]
        #[cfg(feature = "compiled_data")]
        /// Get the "short" name of this property value (returns empty if property value is unknown)
        pub fn short_name(self) -> Option<&'static str> {
            icu_properties::PropertyNamesShortBorrowed::<props::JoiningType>::new().get(self.into())
        }
        #[diplomat::rust_link(icu::properties::props::JoiningType::to_icu4c_value, FnInStruct)]
        /// Convert to an integer value usable with ICU4C and CodePointMapData
        pub fn to_integer_value(self) -> u8 {
            self as u8
        }
        #[diplomat::rust_link(icu::properties::props::JoiningType::from_icu4c_value, FnInStruct)]
        /// Convert from an integer value from ICU4C or CodePointMapData
        pub fn from_integer_value(other: u8) -> Option<Self> {
            Some(match other {
                0 => Self::NonJoining,
                1 => Self::JoinCausing,
                2 => Self::DualJoining,
                3 => Self::LeftJoining,
                4 => Self::RightJoining,
                5 => Self::Transparent,
                _ => return None,
            })
        }
    }

    #[diplomat::rust_link(icu::properties::props::GeneralCategory, Struct)]
    #[diplomat::enum_convert(icu_properties::props::GeneralCategory)]
    pub enum GeneralCategory {
        #[diplomat::rust_link(icu::properties::props::GeneralCategory::Unassigned, EnumVariant)]
        Unassigned = 0,
        #[diplomat::rust_link(
            icu::properties::props::GeneralCategory::UppercaseLetter,
            EnumVariant
        )]
        UppercaseLetter = 1,
        #[diplomat::rust_link(
            icu::properties::props::GeneralCategory::LowercaseLetter,
            EnumVariant
        )]
        LowercaseLetter = 2,
        #[diplomat::rust_link(
            icu::properties::props::GeneralCategory::TitlecaseLetter,
            EnumVariant
        )]
        TitlecaseLetter = 3,
        #[diplomat::rust_link(
            icu::properties::props::GeneralCategory::ModifierLetter,
            EnumVariant
        )]
        ModifierLetter = 4,
        #[diplomat::rust_link(icu::properties::props::GeneralCategory::OtherLetter, EnumVariant)]
        OtherLetter = 5,
        #[diplomat::rust_link(
            icu::properties::props::GeneralCategory::NonspacingMark,
            EnumVariant
        )]
        NonspacingMark = 6,
        #[diplomat::rust_link(icu::properties::props::GeneralCategory::SpacingMark, EnumVariant)]
        SpacingMark = 8,
        #[diplomat::rust_link(icu::properties::props::GeneralCategory::EnclosingMark, EnumVariant)]
        EnclosingMark = 7,
        #[diplomat::rust_link(icu::properties::props::GeneralCategory::DecimalNumber, EnumVariant)]
        DecimalNumber = 9,
        #[diplomat::rust_link(icu::properties::props::GeneralCategory::LetterNumber, EnumVariant)]
        LetterNumber = 10,
        #[diplomat::rust_link(icu::properties::props::GeneralCategory::OtherNumber, EnumVariant)]
        OtherNumber = 11,
        #[diplomat::rust_link(
            icu::properties::props::GeneralCategory::SpaceSeparator,
            EnumVariant
        )]
        SpaceSeparator = 12,
        #[diplomat::rust_link(icu::properties::props::GeneralCategory::LineSeparator, EnumVariant)]
        LineSeparator = 13,
        #[diplomat::rust_link(
            icu::properties::props::GeneralCategory::ParagraphSeparator,
            EnumVariant
        )]
        ParagraphSeparator = 14,
        #[diplomat::rust_link(icu::properties::props::GeneralCategory::Control, EnumVariant)]
        Control = 15,
        #[diplomat::rust_link(icu::properties::props::GeneralCategory::Format, EnumVariant)]
        Format = 16,
        #[diplomat::rust_link(icu::properties::props::GeneralCategory::PrivateUse, EnumVariant)]
        PrivateUse = 17,
        #[diplomat::rust_link(icu::properties::props::GeneralCategory::Surrogate, EnumVariant)]
        Surrogate = 18,
        #[diplomat::rust_link(
            icu::properties::props::GeneralCategory::DashPunctuation,
            EnumVariant
        )]
        DashPunctuation = 19,
        #[diplomat::rust_link(
            icu::properties::props::GeneralCategory::OpenPunctuation,
            EnumVariant
        )]
        OpenPunctuation = 20,
        #[diplomat::rust_link(
            icu::properties::props::GeneralCategory::ClosePunctuation,
            EnumVariant
        )]
        ClosePunctuation = 21,
        #[diplomat::rust_link(
            icu::properties::props::GeneralCategory::ConnectorPunctuation,
            EnumVariant
        )]
        ConnectorPunctuation = 22,
        #[diplomat::rust_link(
            icu::properties::props::GeneralCategory::InitialPunctuation,
            EnumVariant
        )]
        InitialPunctuation = 28,
        #[diplomat::rust_link(
            icu::properties::props::GeneralCategory::FinalPunctuation,
            EnumVariant
        )]
        FinalPunctuation = 29,
        #[diplomat::rust_link(
            icu::properties::props::GeneralCategory::OtherPunctuation,
            EnumVariant
        )]
        OtherPunctuation = 23,
        #[diplomat::rust_link(icu::properties::props::GeneralCategory::MathSymbol, EnumVariant)]
        MathSymbol = 24,
        #[diplomat::rust_link(
            icu::properties::props::GeneralCategory::CurrencySymbol,
            EnumVariant
        )]
        CurrencySymbol = 25,
        #[diplomat::rust_link(
            icu::properties::props::GeneralCategory::ModifierSymbol,
            EnumVariant
        )]
        ModifierSymbol = 26,
        #[diplomat::rust_link(icu::properties::props::GeneralCategory::OtherSymbol, EnumVariant)]
        OtherSymbol = 27,
    }

    impl GeneralCategory {
        #[diplomat::rust_link(icu::properties::props::EnumeratedProperty::for_char, FnInTrait)]
        #[cfg(feature = "compiled_data")]
        pub fn for_char(ch: DiplomatChar) -> Self {
            icu_properties::CodePointMapData::<props::GeneralCategory>::new()
                .get32(ch)
                .into()
        }
        /// Convert to an integer using the ICU4C integer mappings for `General_Category`

        #[diplomat::rust_link(icu::properties::PropertyNamesLongBorrowed::get, FnInStruct)]
        #[cfg(feature = "compiled_data")]
        /// Get the "long" name of this property value (returns empty if property value is unknown)
        pub fn long_name(self) -> Option<&'static str> {
            icu_properties::PropertyNamesLongBorrowed::<props::GeneralCategory>::new()
                .get(self.into())
        }

        #[diplomat::rust_link(icu::properties::PropertyNamesShortBorrowed::get, FnInStruct)]
        #[cfg(feature = "compiled_data")]
        /// Get the "short" name of this property value (returns empty if property value is unknown)
        pub fn short_name(self) -> Option<&'static str> {
            icu_properties::PropertyNamesShortBorrowed::<props::GeneralCategory>::new()
                .get(self.into())
        }
        #[diplomat::rust_link(icu::properties::props::GeneralCategory::to_icu4c_value, FnInStruct)]
        /// Convert to an integer value usable with ICU4C and CodePointMapData
        pub fn to_integer_value(self) -> u8 {
            self as u8
        }

        /// Produces a GeneralCategoryGroup mask that can represent a group of general categories
        #[diplomat::rust_link(icu::properties::props::GeneralCategoryGroup, Struct)]
        pub fn to_group(self) -> GeneralCategoryGroup {
            GeneralCategoryGroup {
                mask: props::GeneralCategoryGroup::from(props::GeneralCategory::from(self)).into(),
            }
        }

        /// Convert from an integer using the ICU4C integer mappings for `General_Category`
        #[diplomat::rust_link(
            icu::properties::props::GeneralCategory::from_icu4c_value,
            FnInStruct
        )]
        #[diplomat::rust_link(
            icu::properties::props::GeneralCategoryOutOfBoundsError,
            Struct,
            hidden
        )]
        /// Convert from an integer value from ICU4C or CodePointMapData
        pub fn from_integer_value(other: u8) -> Option<Self> {
            Some(match other {
                0 => Self::Unassigned,
                1 => Self::UppercaseLetter,
                2 => Self::LowercaseLetter,
                3 => Self::TitlecaseLetter,
                4 => Self::ModifierLetter,
                5 => Self::OtherLetter,
                6 => Self::NonspacingMark,
                8 => Self::SpacingMark,
                7 => Self::EnclosingMark,
                9 => Self::DecimalNumber,
                10 => Self::LetterNumber,
                11 => Self::OtherNumber,
                12 => Self::SpaceSeparator,
                13 => Self::LineSeparator,
                14 => Self::ParagraphSeparator,
                15 => Self::Control,
                16 => Self::Format,
                17 => Self::PrivateUse,
                18 => Self::Surrogate,
                19 => Self::DashPunctuation,
                20 => Self::OpenPunctuation,
                21 => Self::ClosePunctuation,
                22 => Self::ConnectorPunctuation,
                28 => Self::InitialPunctuation,
                29 => Self::FinalPunctuation,
                23 => Self::OtherPunctuation,
                24 => Self::MathSymbol,
                25 => Self::CurrencySymbol,
                26 => Self::ModifierSymbol,
                27 => Self::OtherSymbol,
                _ => return None,
            })
        }
    }

    /// A mask that is capable of representing groups of `General_Category` values.
    #[diplomat::rust_link(icu::properties::props::GeneralCategoryGroup, Struct)]
    #[derive(Default)]
    pub struct GeneralCategoryGroup {
        pub mask: u32,
    }

    impl GeneralCategoryGroup {
        #[inline]
        pub(crate) fn into_props_group(self) -> props::GeneralCategoryGroup {
            self.mask.into()
        }

        #[diplomat::rust_link(icu::properties::props::GeneralCategoryGroup::contains, FnInStruct)]
        pub fn contains(self, val: GeneralCategory) -> bool {
            self.into_props_group().contains(val.into())
        }
        #[diplomat::rust_link(icu::properties::props::GeneralCategoryGroup::complement, FnInStruct)]
        pub fn complement(self) -> Self {
            self.into_props_group().complement().into()
        }

        #[diplomat::rust_link(icu::properties::props::GeneralCategoryGroup::all, FnInStruct)]
        pub fn all() -> Self {
            props::GeneralCategoryGroup::all().into()
        }
        #[diplomat::rust_link(icu::properties::props::GeneralCategoryGroup::empty, FnInStruct)]
        pub fn empty() -> Self {
            props::GeneralCategoryGroup::empty().into()
        }
        #[diplomat::rust_link(icu::properties::props::GeneralCategoryGroup::union, FnInStruct)]
        #[diplomat::attr(any(c, cpp), rename = "union_")]
        pub fn union(self, other: Self) -> Self {
            self.into_props_group()
                .union(other.into_props_group())
                .into()
        }
        #[diplomat::rust_link(
            icu::properties::props::GeneralCategoryGroup::intersection,
            FnInStruct
        )]
        pub fn intersection(self, other: Self) -> Self {
            self.into_props_group()
                .intersection(other.into_props_group())
                .into()
        }

        #[diplomat::rust_link(
            icu::properties::props::GeneralCategoryGroup::CasedLetter,
            AssociatedConstantInStruct
        )]
        pub fn cased_letter() -> Self {
            props::GeneralCategoryGroup::CasedLetter.into()
        }

        #[diplomat::rust_link(
            icu::properties::props::GeneralCategoryGroup::Letter,
            AssociatedConstantInStruct
        )]
        pub fn letter() -> Self {
            props::GeneralCategoryGroup::Letter.into()
        }

        #[diplomat::rust_link(
            icu::properties::props::GeneralCategoryGroup::Mark,
            AssociatedConstantInStruct
        )]
        pub fn mark() -> Self {
            props::GeneralCategoryGroup::Mark.into()
        }
        #[diplomat::rust_link(
            icu::properties::props::GeneralCategoryGroup::Number,
            AssociatedConstantInStruct
        )]
        pub fn number() -> Self {
            props::GeneralCategoryGroup::Number.into()
        }
        #[diplomat::rust_link(
            icu::properties::props::GeneralCategoryGroup::Other,
            AssociatedConstantInStruct
        )]
        pub fn separator() -> Self {
            props::GeneralCategoryGroup::Other.into()
        }
        #[diplomat::rust_link(
            icu::properties::props::GeneralCategoryGroup::Letter,
            AssociatedConstantInStruct
        )]
        pub fn other() -> Self {
            props::GeneralCategoryGroup::Letter.into()
        }
        #[diplomat::rust_link(
            icu::properties::props::GeneralCategoryGroup::Punctuation,
            AssociatedConstantInStruct
        )]
        pub fn punctuation() -> Self {
            props::GeneralCategoryGroup::Punctuation.into()
        }
        #[diplomat::rust_link(
            icu::properties::props::GeneralCategoryGroup::Symbol,
            AssociatedConstantInStruct
        )]
        pub fn symbol() -> Self {
            props::GeneralCategoryGroup::Symbol.into()
        }
    }
}

impl From<icu_properties::props::GeneralCategoryGroup> for ffi::GeneralCategoryGroup {
    #[inline]
    fn from(other: icu_properties::props::GeneralCategoryGroup) -> Self {
        Self { mask: other.into() }
    }
}
#[cfg(test)]
mod test {
    use super::ffi::*;
    use icu_properties::props;

    #[test]
    fn test_all_cases_covered() {
        for prop in props::BidiClass::ALL_VALUES {
            let ffi_prop = BidiClass::from_integer_value(prop.to_icu4c_value())
                .expect("Found BidiClass value not supported in ffi");
            assert_eq!(prop.to_icu4c_value(), ffi_prop.to_integer_value());
            assert_eq!(*prop, props::BidiClass::from(ffi_prop));
        }

        for prop in props::Script::ALL_VALUES {
            let ffi_prop = Script::from_integer_value(prop.to_icu4c_value())
                .expect("Found Script value not supported in ffi");
            assert_eq!(prop.to_icu4c_value(), ffi_prop.to_integer_value());
            assert_eq!(*prop, props::Script::from(ffi_prop));
        }

        for prop in props::HangulSyllableType::ALL_VALUES {
            let ffi_prop = HangulSyllableType::from_integer_value(prop.to_icu4c_value())
                .expect("Found HangulSyllableType value not supported in ffi");
            assert_eq!(prop.to_icu4c_value(), ffi_prop.to_integer_value());
            assert_eq!(*prop, props::HangulSyllableType::from(ffi_prop));
        }
        for prop in props::EastAsianWidth::ALL_VALUES {
            let ffi_prop = EastAsianWidth::from_integer_value(prop.to_icu4c_value())
                .expect("Found EastAsianWidth value not supported in ffi");
            assert_eq!(prop.to_icu4c_value(), ffi_prop.to_integer_value());
            assert_eq!(*prop, props::EastAsianWidth::from(ffi_prop));
        }
        for prop in props::LineBreak::ALL_VALUES {
            let ffi_prop = LineBreak::from_integer_value(prop.to_icu4c_value())
                .expect("Found LineBreak value not supported in ffi");
            assert_eq!(prop.to_icu4c_value(), ffi_prop.to_integer_value());
            assert_eq!(*prop, props::LineBreak::from(ffi_prop));
        }
        for prop in props::GraphemeClusterBreak::ALL_VALUES {
            let ffi_prop = GraphemeClusterBreak::from_integer_value(prop.to_icu4c_value())
                .expect("Found GraphemeClusterBreak value not supported in ffi");
            assert_eq!(prop.to_icu4c_value(), ffi_prop.to_integer_value());
            assert_eq!(*prop, props::GraphemeClusterBreak::from(ffi_prop));
        }
        for prop in props::WordBreak::ALL_VALUES {
            let ffi_prop = WordBreak::from_integer_value(prop.to_icu4c_value())
                .expect("Found WordBreak value not supported in ffi");
            assert_eq!(prop.to_icu4c_value(), ffi_prop.to_integer_value());
            assert_eq!(*prop, props::WordBreak::from(ffi_prop));
        }
        for prop in props::SentenceBreak::ALL_VALUES {
            let ffi_prop = SentenceBreak::from_integer_value(prop.to_icu4c_value())
                .expect("Found SentenceBreak value not supported in ffi");
            assert_eq!(prop.to_icu4c_value(), ffi_prop.to_integer_value());
            assert_eq!(*prop, props::SentenceBreak::from(ffi_prop));
        }
        for prop in props::CanonicalCombiningClass::ALL_VALUES {
            let ffi_prop = CanonicalCombiningClass::from_integer_value(prop.to_icu4c_value())
                .expect("Found CanonicalCombiningClass value not supported in ffi");
            assert_eq!(prop.to_icu4c_value(), ffi_prop.to_integer_value());
            assert_eq!(*prop, props::CanonicalCombiningClass::from(ffi_prop));
        }
        for prop in props::IndicSyllabicCategory::ALL_VALUES {
            let ffi_prop = IndicSyllabicCategory::from_integer_value(prop.to_icu4c_value())
                .expect("Found IndicSyllabicCategory value not supported in ffi");
            assert_eq!(prop.to_icu4c_value(), ffi_prop.to_integer_value());
            assert_eq!(*prop, props::IndicSyllabicCategory::from(ffi_prop));
        }
        for prop in props::JoiningType::ALL_VALUES {
            let ffi_prop = JoiningType::from_integer_value(prop.to_icu4c_value())
                .expect("Found JoiningType value not supported in ffi");
            assert_eq!(prop.to_icu4c_value(), ffi_prop.to_integer_value());
            assert_eq!(*prop, props::JoiningType::from(ffi_prop));
        }
        for prop in props::GeneralCategory::ALL_VALUES {
            let ffi_prop = GeneralCategory::from_integer_value(*prop as u8)
                .expect("Found GeneralCategory value not supported in ffi");
            assert_eq!(*prop as u8, ffi_prop.to_integer_value());
            assert_eq!(*prop, props::GeneralCategory::from(ffi_prop));
        }
    }
}

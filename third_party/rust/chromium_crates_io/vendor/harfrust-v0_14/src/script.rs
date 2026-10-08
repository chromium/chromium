use crate::{tag::TagExt, Tag};
use core::str::FromStr;

// In harfbuzz, despite having `Script`, script can actually have any tag.
// So we're doing the same.
// The only difference is that `Script` cannot be set to `HB_SCRIPT_INVALID`.
/// A text script identified by an ISO 15924 tag.
#[derive(Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Hash, Debug)]
pub struct Script(pub(crate) Tag);

impl Script {
    #[inline]
    pub(crate) const fn from_bytes(bytes: &[u8; 4]) -> Self {
        Script(Tag::new(bytes))
    }

    /// Converts an ISO 15924 script tag to a corresponding `Script`.
    pub const fn from_iso15924_tag(tag: Tag) -> Option<Script> {
        let tag = u32::from_be_bytes(tag.to_be_bytes());

        if tag == 0 {
            return None;
        }

        // Be lenient, adjust case (one capital letter followed by three small letters).
        let tag = (tag & 0xDFDF_DFDF) | 0x0020_2020;

        if tag & 0xE0E0_E0E0 != 0x4060_6060 {
            return Some(Script::UNKNOWN);
        }

        Some(match &tag.to_be_bytes() {
            // Script variants from https://unicode.org/iso15924/
            b"Aran" => Script::ARABIC,
            b"Cyrs" => Script::CYRILLIC,
            b"Geok" => Script::GEORGIAN,
            b"Hans" | b"Hant" => Script::HAN,
            b"Jamo" => Script::HANGUL,
            b"Latf" | b"Latg" => Script::LATIN,
            b"Syre" | b"Syrj" | b"Syrn" => Script::SYRIAC,
            &t => Script(Tag::from_be_bytes(t)),
        })
    }

    /// Returns script's tag.
    #[inline]
    pub fn tag(&self) -> Tag {
        self.0
    }
}

impl FromStr for Script {
    type Err = &'static str;

    fn from_str(s: &str) -> Result<Self, Self::Err> {
        let tag = Tag::from_bytes_lossy(s.as_bytes());
        Script::from_iso15924_tag(tag).ok_or("invalid script")
    }
}

#[allow(missing_docs)]
impl Script {
    // Since 1.1
    pub const COMMON: Self = Self::from_bytes(b"Zyyy");
    pub const INHERITED: Self = Self::from_bytes(b"Zinh");
    pub const ARABIC: Self = Self::from_bytes(b"Arab");
    pub const ARMENIAN: Self = Self::from_bytes(b"Armn");
    pub const BENGALI: Self = Self::from_bytes(b"Beng");
    pub const CYRILLIC: Self = Self::from_bytes(b"Cyrl");
    pub const DEVANAGARI: Self = Self::from_bytes(b"Deva");
    pub const GEORGIAN: Self = Self::from_bytes(b"Geor");
    pub const GREEK: Self = Self::from_bytes(b"Grek");
    pub const GUJARATI: Self = Self::from_bytes(b"Gujr");
    pub const GURMUKHI: Self = Self::from_bytes(b"Guru");
    pub const HANGUL: Self = Self::from_bytes(b"Hang");
    pub const HAN: Self = Self::from_bytes(b"Hani");
    pub const HEBREW: Self = Self::from_bytes(b"Hebr");
    pub const HIRAGANA: Self = Self::from_bytes(b"Hira");
    pub const KANNADA: Self = Self::from_bytes(b"Knda");
    pub const KATAKANA: Self = Self::from_bytes(b"Kana");
    pub const LAO: Self = Self::from_bytes(b"Laoo");
    pub const LATIN: Self = Self::from_bytes(b"Latn");
    pub const MALAYALAM: Self = Self::from_bytes(b"Mlym");
    pub const ORIYA: Self = Self::from_bytes(b"Orya");
    pub const TAMIL: Self = Self::from_bytes(b"Taml");
    pub const TELUGU: Self = Self::from_bytes(b"Telu");
    pub const THAI: Self = Self::from_bytes(b"Thai");
    // Since 2.0
    pub const TIBETAN: Self = Self::from_bytes(b"Tibt");
    // Since 3.0
    pub const BOPOMOFO: Self = Self::from_bytes(b"Bopo");
    pub const BRAILLE: Self = Self::from_bytes(b"Brai");
    pub const CANADIAN_SYLLABICS: Self = Self::from_bytes(b"Cans");
    pub const CHEROKEE: Self = Self::from_bytes(b"Cher");
    pub const ETHIOPIC: Self = Self::from_bytes(b"Ethi");
    pub const KHMER: Self = Self::from_bytes(b"Khmr");
    pub const MONGOLIAN: Self = Self::from_bytes(b"Mong");
    pub const MYANMAR: Self = Self::from_bytes(b"Mymr");
    pub const OGHAM: Self = Self::from_bytes(b"Ogam");
    pub const RUNIC: Self = Self::from_bytes(b"Runr");
    pub const SINHALA: Self = Self::from_bytes(b"Sinh");
    pub const SYRIAC: Self = Self::from_bytes(b"Syrc");
    pub const THAANA: Self = Self::from_bytes(b"Thaa");
    pub const YI: Self = Self::from_bytes(b"Yiii");
    // Since 3.1
    pub const DESERET: Self = Self::from_bytes(b"Dsrt");
    pub const GOTHIC: Self = Self::from_bytes(b"Goth");
    pub const OLD_ITALIC: Self = Self::from_bytes(b"Ital");
    // Since 3.2
    pub const BUHID: Self = Self::from_bytes(b"Buhd");
    pub const HANUNOO: Self = Self::from_bytes(b"Hano");
    pub const TAGALOG: Self = Self::from_bytes(b"Tglg");
    pub const TAGBANWA: Self = Self::from_bytes(b"Tagb");
    // Since 4.0
    pub const CYPRIOT: Self = Self::from_bytes(b"Cprt");
    pub const LIMBU: Self = Self::from_bytes(b"Limb");
    pub const LINEAR_B: Self = Self::from_bytes(b"Linb");
    pub const OSMANYA: Self = Self::from_bytes(b"Osma");
    pub const SHAVIAN: Self = Self::from_bytes(b"Shaw");
    pub const TAI_LE: Self = Self::from_bytes(b"Tale");
    pub const UGARITIC: Self = Self::from_bytes(b"Ugar");
    // Since 4.1
    pub const BUGINESE: Self = Self::from_bytes(b"Bugi");
    pub const COPTIC: Self = Self::from_bytes(b"Copt");
    pub const GLAGOLITIC: Self = Self::from_bytes(b"Glag");
    pub const KHAROSHTHI: Self = Self::from_bytes(b"Khar");
    pub const NEW_TAI_LUE: Self = Self::from_bytes(b"Talu");
    pub const OLD_PERSIAN: Self = Self::from_bytes(b"Xpeo");
    pub const SYLOTI_NAGRI: Self = Self::from_bytes(b"Sylo");
    pub const TIFINAGH: Self = Self::from_bytes(b"Tfng");
    // Since 5.0
    pub const UNKNOWN: Self = Self::from_bytes(b"Zzzz"); // Script can be Unknown, but not Invalid.
    pub const BALINESE: Self = Self::from_bytes(b"Bali");
    pub const CUNEIFORM: Self = Self::from_bytes(b"Xsux");
    pub const NKO: Self = Self::from_bytes(b"Nkoo");
    pub const PHAGS_PA: Self = Self::from_bytes(b"Phag");
    pub const PHOENICIAN: Self = Self::from_bytes(b"Phnx");
    // Since 5.1
    pub const CARIAN: Self = Self::from_bytes(b"Cari");
    pub const CHAM: Self = Self::from_bytes(b"Cham");
    pub const KAYAH_LI: Self = Self::from_bytes(b"Kali");
    pub const LEPCHA: Self = Self::from_bytes(b"Lepc");
    pub const LYCIAN: Self = Self::from_bytes(b"Lyci");
    pub const LYDIAN: Self = Self::from_bytes(b"Lydi");
    pub const OL_CHIKI: Self = Self::from_bytes(b"Olck");
    pub const REJANG: Self = Self::from_bytes(b"Rjng");
    pub const SAURASHTRA: Self = Self::from_bytes(b"Saur");
    pub const SUNDANESE: Self = Self::from_bytes(b"Sund");
    pub const VAI: Self = Self::from_bytes(b"Vaii");
    // Since 5.2
    pub const AVESTAN: Self = Self::from_bytes(b"Avst");
    pub const BAMUM: Self = Self::from_bytes(b"Bamu");
    pub const EGYPTIAN_HIEROGLYPHS: Self = Self::from_bytes(b"Egyp");
    pub const IMPERIAL_ARAMAIC: Self = Self::from_bytes(b"Armi");
    pub const INSCRIPTIONAL_PAHLAVI: Self = Self::from_bytes(b"Phli");
    pub const INSCRIPTIONAL_PARTHIAN: Self = Self::from_bytes(b"Prti");
    pub const JAVANESE: Self = Self::from_bytes(b"Java");
    pub const KAITHI: Self = Self::from_bytes(b"Kthi");
    pub const LISU: Self = Self::from_bytes(b"Lisu");
    pub const MEETEI_MAYEK: Self = Self::from_bytes(b"Mtei");
    pub const OLD_SOUTH_ARABIAN: Self = Self::from_bytes(b"Sarb");
    pub const OLD_TURKIC: Self = Self::from_bytes(b"Orkh");
    pub const SAMARITAN: Self = Self::from_bytes(b"Samr");
    pub const TAI_THAM: Self = Self::from_bytes(b"Lana");
    pub const TAI_VIET: Self = Self::from_bytes(b"Tavt");
    // Since 6.0
    pub const BATAK: Self = Self::from_bytes(b"Batk");
    pub const BRAHMI: Self = Self::from_bytes(b"Brah");
    pub const MANDAIC: Self = Self::from_bytes(b"Mand");
    // Since 6.1
    pub const CHAKMA: Self = Self::from_bytes(b"Cakm");
    pub const MEROITIC_CURSIVE: Self = Self::from_bytes(b"Merc");
    pub const MEROITIC_HIEROGLYPHS: Self = Self::from_bytes(b"Mero");
    pub const MIAO: Self = Self::from_bytes(b"Plrd");
    pub const SHARADA: Self = Self::from_bytes(b"Shrd");
    pub const SORA_SOMPENG: Self = Self::from_bytes(b"Sora");
    pub const TAKRI: Self = Self::from_bytes(b"Takr");
    // Since 7.0
    pub const BASSA_VAH: Self = Self::from_bytes(b"Bass");
    pub const CAUCASIAN_ALBANIAN: Self = Self::from_bytes(b"Aghb");
    pub const DUPLOYAN: Self = Self::from_bytes(b"Dupl");
    pub const ELBASAN: Self = Self::from_bytes(b"Elba");
    pub const GRANTHA: Self = Self::from_bytes(b"Gran");
    pub const KHOJKI: Self = Self::from_bytes(b"Khoj");
    pub const KHUDAWADI: Self = Self::from_bytes(b"Sind");
    pub const LINEAR_A: Self = Self::from_bytes(b"Lina");
    pub const MAHAJANI: Self = Self::from_bytes(b"Mahj");
    pub const MANICHAEAN: Self = Self::from_bytes(b"Mani");
    pub const MENDE_KIKAKUI: Self = Self::from_bytes(b"Mend");
    pub const MODI: Self = Self::from_bytes(b"Modi");
    pub const MRO: Self = Self::from_bytes(b"Mroo");
    pub const NABATAEAN: Self = Self::from_bytes(b"Nbat");
    pub const OLD_NORTH_ARABIAN: Self = Self::from_bytes(b"Narb");
    pub const OLD_PERMIC: Self = Self::from_bytes(b"Perm");
    pub const PAHAWH_HMONG: Self = Self::from_bytes(b"Hmng");
    pub const PALMYRENE: Self = Self::from_bytes(b"Palm");
    pub const PAU_CIN_HAU: Self = Self::from_bytes(b"Pauc");
    pub const PSALTER_PAHLAVI: Self = Self::from_bytes(b"Phlp");
    pub const SIDDHAM: Self = Self::from_bytes(b"Sidd");
    pub const TIRHUTA: Self = Self::from_bytes(b"Tirh");
    pub const WARANG_CITI: Self = Self::from_bytes(b"Wara");
    // Since 8.0
    pub const AHOM: Self = Self::from_bytes(b"Ahom");
    pub const ANATOLIAN_HIEROGLYPHS: Self = Self::from_bytes(b"Hluw");
    pub const HATRAN: Self = Self::from_bytes(b"Hatr");
    pub const MULTANI: Self = Self::from_bytes(b"Mult");
    pub const OLD_HUNGARIAN: Self = Self::from_bytes(b"Hung");
    pub const SIGNWRITING: Self = Self::from_bytes(b"Sgnw");
    // Since 9.0
    pub const ADLAM: Self = Self::from_bytes(b"Adlm");
    pub const BHAIKSUKI: Self = Self::from_bytes(b"Bhks");
    pub const MARCHEN: Self = Self::from_bytes(b"Marc");
    pub const OSAGE: Self = Self::from_bytes(b"Osge");
    pub const TANGUT: Self = Self::from_bytes(b"Tang");
    pub const NEWA: Self = Self::from_bytes(b"Newa");
    // Since 10.0
    pub const MASARAM_GONDI: Self = Self::from_bytes(b"Gonm");
    pub const NUSHU: Self = Self::from_bytes(b"Nshu");
    pub const SOYOMBO: Self = Self::from_bytes(b"Soyo");
    pub const ZANABAZAR_SQUARE: Self = Self::from_bytes(b"Zanb");
    // Since 11.0
    pub const DOGRA: Self = Self::from_bytes(b"Dogr");
    pub const GUNJALA_GONDI: Self = Self::from_bytes(b"Gong");
    pub const HANIFI_ROHINGYA: Self = Self::from_bytes(b"Rohg");
    pub const MAKASAR: Self = Self::from_bytes(b"Maka");
    pub const MEDEFAIDRIN: Self = Self::from_bytes(b"Medf");
    pub const OLD_SOGDIAN: Self = Self::from_bytes(b"Sogo");
    pub const SOGDIAN: Self = Self::from_bytes(b"Sogd");
    // Since 12.0
    pub const ELYMAIC: Self = Self::from_bytes(b"Elym");
    pub const NANDINAGARI: Self = Self::from_bytes(b"Nand");
    pub const NYIAKENG_PUACHUE_HMONG: Self = Self::from_bytes(b"Hmnp");
    pub const WANCHO: Self = Self::from_bytes(b"Wcho");
    // Since 13.0
    pub const CHORASMIAN: Self = Self::from_bytes(b"Chrs");
    pub const DIVES_AKURU: Self = Self::from_bytes(b"Diak");
    pub const KHITAN_SMALL_SCRIPT: Self = Self::from_bytes(b"Kits");
    pub const YEZIDI: Self = Self::from_bytes(b"Yezi");
    // Since 14.0
    pub const CYPRO_MINOAN: Self = Self::from_bytes(b"Cpmn");
    pub const OLD_UYGHUR: Self = Self::from_bytes(b"Ougr");
    pub const TANGSA: Self = Self::from_bytes(b"Tnsa");
    pub const TOTO: Self = Self::from_bytes(b"Toto");
    pub const VITHKUQI: Self = Self::from_bytes(b"Vith");
    // Since 15.0
    pub const KAWI: Self = Self::from_bytes(b"Kawi");
    pub const NAG_MUNDARI: Self = Self::from_bytes(b"Nagm");
    // Since 16.0
    pub const GARAY: Self = Self::from_bytes(b"Gara");
    pub const GURUNG_KHEMA: Self = Self::from_bytes(b"Gukh");
    pub const KIRAT_RAI: Self = Self::from_bytes(b"Krai");
    pub const OL_ONAL: Self = Self::from_bytes(b"Onao");
    pub const SUNUWAR: Self = Self::from_bytes(b"Sunu");
    pub const TODHRI: Self = Self::from_bytes(b"Todr");
    pub const TULU_TIGALARI: Self = Self::from_bytes(b"Tutg");
    // Since 17.0
    pub const BERIA_ERFE: Self = Self::from_bytes(b"Berf");
    pub const SIDETIC: Self = Self::from_bytes(b"Sidt");
    pub const TAI_YO: Self = Self::from_bytes(b"Tayo");
    pub const TOLONG_SIKI: Self = Self::from_bytes(b"Tols");
    // Since 18.0
    pub const JURCHEN: Self = Self::from_bytes(b"Jurc");
    pub const PROTO_CUNEIFORM: Self = Self::from_bytes(b"Pcun");
    pub const SEAL: Self = Self::from_bytes(b"Seal");

    pub const MATH: Self = Self::from_bytes(b"Zmth");

    // https://github.com/harfbuzz/harfbuzz/issues/1162
    pub const MYANMAR_ZAWGYI: Self = Self::from_bytes(b"Qaag");
}

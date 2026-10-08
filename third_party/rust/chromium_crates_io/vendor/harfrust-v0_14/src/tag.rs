use smallvec::SmallVec;

use super::{tag_table, Language, Script, Tag};

type ThreeTags = SmallVec<[Tag; 3]>;

trait SmallVecExt {
    fn left(&self) -> usize;
    fn is_full(&self) -> bool;
}

impl<A: smallvec::Array> SmallVecExt for SmallVec<A> {
    fn left(&self) -> usize {
        self.inline_size() - self.len()
    }

    fn is_full(&self) -> bool {
        self.len() == self.inline_size()
    }
}

/// Converts an `Script` and an `Language` to script and language tags.
pub fn tags_from_script_and_language(
    script: Option<Script>,
    language: Option<&Language>,
) -> (ThreeTags, ThreeTags) {
    let mut needs_script = true;
    let mut scripts = SmallVec::new();
    let mut languages = SmallVec::new();

    let mut private_use_subtag = None;
    let mut prefix = b"" as &[u8];
    if let Some(language) = language {
        let language = language.as_bytes();
        if language.starts_with(b"x-") {
            private_use_subtag = Some(language);
        } else {
            let mut i = 1;
            while i < language.len() {
                if language.get(i - 1) == Some(&b'-') && language.get(i + 1) == Some(&b'-') {
                    if language[i] == b'x' {
                        private_use_subtag = Some(&language[i..]);
                        if prefix.is_empty() {
                            prefix = &language[..i - 1];
                        }

                        break;
                    } else {
                        prefix = &language[..i - 1];
                    }
                }

                i += 1;
            }

            if prefix.is_empty() {
                prefix = &language[..i];
            }
        }

        needs_script = !parse_private_use_subtag(
            private_use_subtag,
            b"-hbsc",
            u8::to_ascii_lowercase,
            &mut scripts,
        );

        let needs_language = !parse_private_use_subtag(
            private_use_subtag,
            b"-hbot",
            u8::to_ascii_uppercase,
            &mut languages,
        );

        if needs_language {
            if let Some(language) = Language::new(prefix) {
                tags_from_language(&language, &mut languages);
            }
        }
    }

    if needs_script {
        all_tags_from_script(script, &mut scripts);
    }

    (scripts, languages)
}

fn parse_private_use_subtag(
    private_use_subtag: Option<&[u8]>,
    prefix: &[u8],
    normalize: fn(&u8) -> u8,
    tags: &mut ThreeTags,
) -> bool {
    let Some(private_use_subtag) = private_use_subtag else {
        return false;
    };

    let private_use_subtag = match private_use_subtag
        .windows(prefix.len())
        .position(|window| window == prefix)
    {
        Some(idx) => &private_use_subtag[idx + prefix.len()..],
        None => return false,
    };

    let mut tag = if let Some(hex) = private_use_subtag.strip_prefix(b"-") {
        // Hexadecimal tags encode four bytes without case normalization.
        let Some(hex) = hex.get(..8) else {
            return false;
        };
        let mut value = 0;
        for &c in hex {
            let Some(digit) = char::from(c).to_digit(16) else {
                return false;
            };
            value = (value << 4) | digit;
        }
        Tag::from_u32(value)
    } else {
        let mut tag = SmallVec::<[u8; 4]>::new();
        for c in private_use_subtag.iter().take(4) {
            if c.is_ascii_alphanumeric() {
                tag.push((normalize)(c));
            } else {
                break;
            }
        }

        if tag.is_empty() {
            return false;
        }

        Tag::from_bytes_lossy(tag.as_slice())
    };

    // Some bits magic from HarfBuzz...
    if tag.as_u32() & 0xDFDF_DFDF == Tag::default_script().as_u32() {
        tag = Tag::from_u32(tag.as_u32() ^ !0xDFDF_DFDF);
    }

    tags.push(tag);

    true
}

fn lang_cmp(s1: &[u8], s2: &[u8]) -> core::cmp::Ordering {
    let da = s1
        .iter()
        .position(|&c| c == b'-' || c == b'\0')
        .unwrap_or(s1.len());
    let db = s2
        .iter()
        .position(|&c| c == b'-' || c == b'\0')
        .unwrap_or(s2.len());
    let n = core::cmp::max(da, db);
    let ea = core::cmp::min(n, s1.len());
    let eb = core::cmp::min(n, s2.len());
    s1[..ea].cmp(&s2[..eb])
}

pub(super) fn subtag_matches(language: impl AsRef<[u8]>, subtag: impl AsRef<[u8]>) -> bool {
    let language = language.as_ref();
    let subtag = subtag.as_ref();

    for i in language
        .windows(subtag.len())
        .enumerate()
        .filter_map(|(i, window)| (window == subtag).then_some(i))
    {
        if let Some(c) = language.get(i + subtag.len()) {
            if !c.is_ascii_alphanumeric() {
                return true;
            }
        } else {
            return true;
        }
    }

    false
}

pub(super) fn lang_matches(language: impl AsRef<[u8]>, spec: impl AsRef<[u8]>) -> bool {
    let language = language.as_ref();
    let spec = spec.as_ref();

    if language.starts_with(spec) {
        return language.len() == spec.len() || language.get(spec.len()) == Some(&b'-');
    }

    false
}

pub(super) fn strncmp(s1: impl AsRef<[u8]>, s2: impl AsRef<[u8]>, n: usize) -> bool {
    let s1 = s1.as_ref();
    let s2 = s2.as_ref();

    let n1 = core::cmp::min(n, s1.len());
    let n2 = core::cmp::min(n, s2.len());
    s1[..n1] == s2[..n2]
}

fn tags_from_language(language: &Language, tags: &mut ThreeTags) {
    let language = language.as_bytes();

    // Check for matches of multiple subtags.
    if let Ok(language_str) = core::str::from_utf8(language) {
        if tag_table::tags_from_complex_language(language_str, tags) {
            return;
        }
    }

    let mut sublang = language;

    // Find a language matching in the first component.
    if let Some(i) = language.iter().position(|&c| c == b'-') {
        // If there is an extended language tag, use it.
        if language.len() >= 6 {
            let extlang = match language[i + 1..].iter().position(|&c| c == b'-') {
                Some(idx) => idx == 3,
                None => language.len() - i - 1 == 3,
            };

            if extlang && language[i + 1].is_ascii_alphabetic() {
                sublang = &language[i + 1..];
            }
        }
    }

    use tag_table::OPEN_TYPE_LANGUAGES as LANGUAGES;

    if let Ok(mut idx) = LANGUAGES.binary_search_by(|v| lang_cmp(&v.language, sublang)) {
        while idx != 0 && LANGUAGES[idx].language == LANGUAGES[idx - 1].language {
            idx -= 1;
        }

        let len = core::cmp::min(tags.left(), LANGUAGES.len() - idx - 1);
        for i in 0..len {
            if LANGUAGES[idx + i].language != LANGUAGES[idx].language {
                break;
            }

            if LANGUAGES[idx + i].tag.is_null() {
                break;
            }

            if tags.is_full() {
                break;
            }

            tags.push(LANGUAGES[idx + i].tag);
        }

        return;
    }

    if language.len() == 3 {
        tags.push(Tag::from_bytes_lossy(language).to_uppercase());
    }
}

fn all_tags_from_script(script: Option<Script>, tags: &mut ThreeTags) {
    if let Some(script) = script {
        if let Some(tag) = new_tag_from_script(script) {
            // Script::Myanmar maps to 'mym2', but there is no 'mym3'.
            if tag != Tag::new(b"mym2") {
                let mut tag3 = tag.to_be_bytes();
                tag3[3] = b'3';
                tags.push(Tag::new(&tag3));
            }

            if !tags.is_full() {
                tags.push(tag);
            }
        }

        if !tags.is_full() {
            tags.push(old_tag_from_script(script));
        }
    }
}

fn new_tag_from_script(script: Script) -> Option<Tag> {
    match script {
        Script::BENGALI => Some(Tag::new(b"bng2")),
        Script::DEVANAGARI => Some(Tag::new(b"dev2")),
        Script::GUJARATI => Some(Tag::new(b"gjr2")),
        Script::GURMUKHI => Some(Tag::new(b"gur2")),
        Script::KANNADA => Some(Tag::new(b"knd2")),
        Script::MALAYALAM => Some(Tag::new(b"mlm2")),
        Script::ORIYA => Some(Tag::new(b"ory2")),
        Script::TAMIL => Some(Tag::new(b"tml2")),
        Script::TELUGU => Some(Tag::new(b"tel2")),
        Script::MYANMAR => Some(Tag::new(b"mym2")),
        _ => None,
    }
}

fn old_tag_from_script(script: Script) -> Tag {
    const KATAKANA_OR_HIRAGANA: Script = Script::from_bytes(b"Hrkt");

    // This seems to be accurate as of end of 2012.
    match script {
        Script::MATH => Tag::new(b"math"),

        // Hiragana, Katakana, and their collective ISO 15924 script all map to 'kana'.
        Script::HIRAGANA | KATAKANA_OR_HIRAGANA => Tag::new(b"kana"),

        // Spaces at the end are preserved, unlike ISO 15924.
        Script::LAO => Tag::new(b"lao "),
        Script::YI => Tag::new(b"yi  "),
        // Unicode-5.0 additions.
        Script::NKO => Tag::new(b"nko "),
        // Unicode-5.1 additions.
        Script::VAI => Tag::new(b"vai "),

        // Else, just change first char to lowercase and return.
        _ => Tag::from_u32(script.tag().as_u32() | 0x2000_0000),
    }
}

pub trait TagExt {
    fn from_bytes_lossy(bytes: &[u8]) -> Self;
    fn as_u32(self) -> u32;
    fn is_null(self) -> bool;
    fn default_script() -> Self;
    fn default_language() -> Self;
    #[cfg(test)]
    fn to_lowercase(&self) -> Self;
    fn to_uppercase(&self) -> Self;
}

impl TagExt for Tag {
    fn from_bytes_lossy(bytes: &[u8]) -> Self {
        let mut array = [b' '; 4];
        for (src, dest) in bytes.iter().zip(&mut array) {
            *dest = *src;
        }
        Tag::new(&array)
    }

    fn as_u32(self) -> u32 {
        u32::from_be_bytes(self.to_be_bytes())
    }

    fn is_null(self) -> bool {
        self.to_be_bytes() == [0, 0, 0, 0]
    }

    #[inline]
    fn default_script() -> Self {
        Tag::new(b"DFLT")
    }

    #[inline]
    fn default_language() -> Self {
        Tag::new(b"dflt")
    }

    /// Converts tag to lowercase.
    #[cfg(test)]
    #[inline]
    fn to_lowercase(&self) -> Self {
        let b = self.to_be_bytes();
        Tag::new(&[
            b[0].to_ascii_lowercase(),
            b[1].to_ascii_lowercase(),
            b[2].to_ascii_lowercase(),
            b[3].to_ascii_lowercase(),
        ])
    }

    /// Converts tag to uppercase.
    #[inline]
    fn to_uppercase(&self) -> Self {
        let b = self.to_be_bytes();
        Tag::new(&[
            b[0].to_ascii_uppercase(),
            b[1].to_ascii_uppercase(),
            b[2].to_ascii_uppercase(),
            b[3].to_ascii_uppercase(),
        ])
    }
}

#[rustfmt::skip]
#[cfg(test)]
mod tests {
    #![allow(non_snake_case)]

    use super::*;

    #[test]
    fn language_matches() {
        assert!(lang_matches(b"pl", b"pl"));
        assert!(lang_matches(b"pl-pl", b"pl"));
        assert!(!lang_matches(b"pl", b"pl-pl"));
        assert!(!lang_matches(b"plx", b"pl"));
        assert!(!lang_matches(b"en", b"pl"));
    }
    use alloc::vec::Vec;

    fn new_tag_to_script(tag: Tag) -> Option<Script> {
        match &tag.to_be_bytes() {
            b"bng2" => Some(Script::BENGALI),
            b"dev2" => Some(Script::DEVANAGARI),
            b"gjr2" => Some(Script::GUJARATI),
            b"gur2" => Some(Script::GURMUKHI),
            b"knd2" => Some(Script::KANNADA),
            b"mlm2" => Some(Script::MALAYALAM),
            b"ory2" => Some(Script::ORIYA),
            b"tml2" => Some(Script::TAMIL),
            b"tel2" => Some(Script::TELUGU),
            b"mym2" => Some(Script::MYANMAR),
            _ => Some(Script::UNKNOWN),
        }
    }

    fn old_tag_to_script(tag: Tag) -> Option<Script> {
        if tag == Tag::default_script() {
            return None;
        }

        let mut bytes = tag.to_be_bytes();

        // This side of the conversion is fully algorithmic.

        // Any spaces at the end of the tag are replaced by repeating the last
        // letter.  Eg 'nko ' -> 'Nkoo'
        if bytes[2] == b' ' {
            bytes[2] = bytes[1];
        }
        if bytes[3] == b' ' {
            bytes[3] = bytes[2];
        }

        // Change first char to uppercase.
        bytes[0] = bytes[0].to_ascii_uppercase();

        Some(Script(Tag::new(&bytes)))
    }

    fn tag_to_script(tag: Tag) -> Option<Script> {
        let bytes = tag.to_be_bytes();
        if bytes[3] == b'2' || bytes[3] == b'3' {
            let mut tag2 = bytes;
            tag2[3] = b'2';
            return new_tag_to_script(Tag::new(&tag2));
        }

        old_tag_to_script(tag)
    }

    fn test_simple_tags(tag: &str, script: Script) {
        let tag = Tag::from_bytes_lossy(tag.as_bytes());

        let (scripts, _) = tags_from_script_and_language(Some(script), None);
        if !scripts.is_empty() {
            assert_eq!(tag, scripts[0]);
        } else {
            assert_eq!(tag, Tag::default_script());
        }

        assert_eq!(tag_to_script(tag), Some(script));
    }

    #[test]
    fn tag_to_uppercase() {
        assert_eq!(Tag::new(b"abcd").to_uppercase(), Tag::new(b"ABCD"));
        assert_eq!(Tag::new(b"abc ").to_uppercase(), Tag::new(b"ABC "));
        assert_eq!(Tag::new(b"ABCD").to_uppercase(), Tag::new(b"ABCD"));
    }

    #[test]
    fn tag_to_lowercase() {
        assert_eq!(Tag::new(b"abcd").to_lowercase(), Tag::new(b"abcd"));
        assert_eq!(Tag::new(b"abc ").to_lowercase(), Tag::new(b"abc "));
        assert_eq!(Tag::new(b"ABCD").to_lowercase(), Tag::new(b"abcd"));
    }

    #[test]
    fn script_degenerate() {
        assert_eq!(Tag::new(b"DFLT"), Tag::default_script());

        // Hiragana, Katakana, and their collective ISO 15924 script all map to 'kana'.
        test_simple_tags("kana", Script::KATAKANA);

        let (scripts, _) = tags_from_script_and_language(Some(Script::HIRAGANA), None);
        assert_eq!(scripts.as_slice(), &[Tag::new(b"kana")]);

        let katakana_or_hiragana =
            Script::from_iso15924_tag(Tag::new(b"Hrkt")).unwrap();
        let (scripts, _) = tags_from_script_and_language(Some(katakana_or_hiragana), None);
        assert_eq!(scripts.as_slice(), &[Tag::new(b"kana")]);

        // Spaces are replaced
        assert_eq!(tag_to_script(Tag::new(b"be  ")), Script::from_iso15924_tag(Tag::new(b"Beee")));
    }

    #[test]
    fn script_simple() {
        // Arbitrary non-existent script.
        test_simple_tags("wwyz", Script::from_iso15924_tag(Tag::new(b"wWyZ")).unwrap());

        // These we don't really care about.
        test_simple_tags("zyyy", Script::COMMON);
        test_simple_tags("zinh", Script::INHERITED);
        test_simple_tags("zzzz", Script::UNKNOWN);

        test_simple_tags("arab", Script::ARABIC);
        test_simple_tags("copt", Script::COPTIC);
        test_simple_tags("kana", Script::KATAKANA);
        test_simple_tags("latn", Script::LATIN);

        // These are trickier since their OT script tags have space.
        test_simple_tags("lao ", Script::LAO);
        test_simple_tags("yi  ", Script::YI);
        // Unicode-5.0 additions.
        test_simple_tags("nko ", Script::NKO);
        // Unicode-5.1 additions.
        test_simple_tags("vai ", Script::VAI);

        // https://docs.microsoft.com/en-us/typography/opentype/spec/scripttags

        // Unicode-5.2 additions.
        test_simple_tags("mtei", Script::MEETEI_MAYEK);
        // Unicode-6.0 additions.
        test_simple_tags("mand", Script::MANDAIC);
    }

    macro_rules! test_script_from_language {
        ($name:ident, $tag:expr, $lang:expr, $script:expr) => {
            #[test]
            fn $name() {
                let tag = Tag::from_bytes_lossy($tag.as_bytes());
                let (scripts, _) =
                    tags_from_script_and_language($script, Language::new($lang).as_ref());
                if !scripts.is_empty() {
                    assert_eq!(scripts.as_slice(), &[tag]);
                }
            }
        };
    }

    test_script_from_language!(script_from_language_01, "", "", None);
    test_script_from_language!(script_from_language_02, "", "en", None);
    test_script_from_language!(script_from_language_03, "copt", "en", Some(Script::COPTIC));
    test_script_from_language!(script_from_language_04, "", "x-hbsc", None);
    test_script_from_language!(script_from_language_05, "copt", "x-hbsc", Some(Script::COPTIC));
    test_script_from_language!(script_from_language_06, "abc ", "x-hbscabc", None);
    test_script_from_language!(script_from_language_07, "deva", "x-hbscdeva", None);
    test_script_from_language!(script_from_language_08, "dev2", "x-hbscdev2", None);
    test_script_from_language!(script_from_language_09, "dev3", "x-hbscdev3", None);
    test_script_from_language!(script_from_language_10, "copt", "x-hbotpap0-hbsccopt", None);
    test_script_from_language!(script_from_language_11, "", "en-x-hbsc", None);
    test_script_from_language!(script_from_language_12, "copt", "en-x-hbsc", Some(Script::COPTIC));
    test_script_from_language!(script_from_language_13, "abc ", "en-x-hbscabc", None);
    test_script_from_language!(script_from_language_14, "deva", "en-x-hbscdeva", None);
    test_script_from_language!(script_from_language_15, "dev2", "en-x-hbscdev2", None);
    test_script_from_language!(script_from_language_16, "dev3", "en-x-hbscdev3", None);
    test_script_from_language!(script_from_language_17, "copt", "en-x-hbotpap0-hbsccopt", None);

    #[test]
    fn script_indic() {
        fn check(tag1: &str, tag2: &str, tag3: &str, script: Script) {
            let tag1 = Tag::from_bytes_lossy(tag1.as_bytes());
            let tag2 = Tag::from_bytes_lossy(tag2.as_bytes());
            let tag3 = Tag::from_bytes_lossy(tag3.as_bytes());

            let (scripts, _) = tags_from_script_and_language(Some(script), None);
            assert_eq!(scripts.as_slice(), &[tag1, tag2, tag3]);
            assert_eq!(tag_to_script(tag1), Some(script));
            assert_eq!(tag_to_script(tag2), Some(script));
            assert_eq!(tag_to_script(tag3), Some(script));
        }

        check("bng3", "bng2", "beng", Script::BENGALI);
        check("dev3", "dev2", "deva", Script::DEVANAGARI);
        check("gjr3", "gjr2", "gujr", Script::GUJARATI);
        check("gur3", "gur2", "guru", Script::GURMUKHI);
        check("knd3", "knd2", "knda", Script::KANNADA);
        check("mlm3", "mlm2", "mlym", Script::MALAYALAM);
        check("ory3", "ory2", "orya", Script::ORIYA);
        check("tml3", "tml2", "taml", Script::TAMIL);
        check("tel3", "tel2", "telu", Script::TELUGU);
    }

    // TODO: swap tag and lang
    macro_rules! test_tag_from_language {
        ($name:ident, $tag:expr, $lang:expr) => {
            #[test]
            fn $name() {
                let tag = Tag::from_bytes_lossy($tag.as_bytes());
                let (_, languages) = tags_from_script_and_language(
                    None,
                    Language::new(&$lang.to_lowercase()).as_ref(),
                );
                if !languages.is_empty() {
                    assert_eq!(languages[0], tag);
                }
            }
        };
    }

    test_tag_from_language!(tag_from_language_dflt, "dflt", "");
    test_tag_from_language!(tag_from_language_ALT, "ALT", "alt");
    test_tag_from_language!(tag_from_language_ARA, "ARA", "ar");
    test_tag_from_language!(tag_from_language_AZE, "AZE", "az");
    test_tag_from_language!(tag_from_language_az_ir, "AZE", "az-ir");
    test_tag_from_language!(tag_from_language_az_az, "AZE", "az-az");
    test_tag_from_language!(tag_from_language_ENG, "ENG", "en");
    test_tag_from_language!(tag_from_language_en_US, "ENG", "en_US");
    test_tag_from_language!(tag_from_language_CJA, "CJA", "cja"); /* Western Cham */
    test_tag_from_language!(tag_from_language_CJM, "CJM", "cjm"); /* Eastern Cham */
    test_tag_from_language!(tag_from_language_ENV, "EVN", "eve");
    test_tag_from_language!(tag_from_language_HAL, "HAL", "cfm"); /* BCP47 and current ISO639-3 code for Halam/Falam Chin */
    test_tag_from_language!(tag_from_language_flm, "HAL", "flm"); /* Retired ISO639-3 code for Halam/Falam Chin */
    test_tag_from_language!(tag_from_language_hy, "HYE0", "hy");
    test_tag_from_language!(tag_from_language_hyw, "HYE", "hyw");
    test_tag_from_language!(tag_from_language_bgr, "QIN", "bgr"); /* Bawm Chin */
    test_tag_from_language!(tag_from_language_cbl, "QIN", "cbl"); /* Bualkhaw Chin */
    test_tag_from_language!(tag_from_language_cka, "QIN", "cka"); /* Khumi Awa Chin */
    test_tag_from_language!(tag_from_language_cmr, "QIN", "cmr"); /* Mro-Khimi Chin */
    test_tag_from_language!(tag_from_language_cnb, "QIN", "cnb"); /* Chinbon Chin */
    test_tag_from_language!(tag_from_language_cnh, "QIN", "cnh"); /* Hakha Chin */
    test_tag_from_language!(tag_from_language_cnk, "QIN", "cnk"); /* Khumi Chin */
    test_tag_from_language!(tag_from_language_cnw, "QIN", "cnw"); /* Ngawn Chin */
    test_tag_from_language!(tag_from_language_csh, "QIN", "csh"); /* Asho Chin */
    test_tag_from_language!(tag_from_language_csy, "QIN", "csy"); /* Siyin Chin */
    test_tag_from_language!(tag_from_language_ctd, "QIN", "ctd"); /* Tedim Chin */
    test_tag_from_language!(tag_from_language_czt, "QIN", "czt"); /* Zotung Chin */
    test_tag_from_language!(tag_from_language_dao, "QIN", "dao"); /* Daai Chin */
    test_tag_from_language!(tag_from_language_htl, "QIN", "hlt"); /* Matu Chin */
    test_tag_from_language!(tag_from_language_mrh, "QIN", "mrh"); /* Mara Chin */
    test_tag_from_language!(tag_from_language_pck, "QIN", "pck"); /* Paite Chin */
    test_tag_from_language!(tag_from_language_sez, "QIN", "sez"); /* Senthang Chin */
    test_tag_from_language!(tag_from_language_tcp, "QIN", "tcp"); /* Tawr Chin */
    test_tag_from_language!(tag_from_language_tcz, "QIN", "tcz"); /* Thado Chin */
    test_tag_from_language!(tag_from_language_yos, "QIN", "yos"); /* Yos, deprecated by IANA in favor of Zou [zom] */
    test_tag_from_language!(tag_from_language_zom, "QIN", "zom"); /* Zou */
    test_tag_from_language!(tag_from_language_FAR, "FAR", "fa");
    test_tag_from_language!(tag_from_language_fa_IR, "FAR", "fa_IR");
    test_tag_from_language!(tag_from_language_man, "MNK", "man");
    test_tag_from_language!(tag_from_language_SWA, "SWA", "aii"); /* Swadaya Aramaic */
    test_tag_from_language!(tag_from_language_SYR, "SYR", "syr"); /* Syriac [macrolanguage] */
    test_tag_from_language!(tag_from_language_amw, "SYR", "amw"); /* Western Neo-Aramaic */
    test_tag_from_language!(tag_from_language_cld, "SYR", "cld"); /* Chaldean Neo-Aramaic */
    test_tag_from_language!(tag_from_language_syc, "SYR", "syc"); /* Classical Syriac */
    test_tag_from_language!(tag_from_language_TUA, "TUA", "tru"); /* Turoyo Aramaic */
    test_tag_from_language!(tag_from_language_zh, "ZHS", "zh"); /* Chinese */
    test_tag_from_language!(tag_from_language_zh_cn, "ZHS", "zh-cn"); /* Chinese (China) */
    test_tag_from_language!(tag_from_language_zh_sg, "ZHS", "zh-sg"); /* Chinese (Singapore) */
    test_tag_from_language!(tag_from_language_zh_mo, "ZHTM", "zh-mo"); /* Chinese (Macao) */
    test_tag_from_language!(tag_from_language_zh_hant_mo, "ZHTM", "zh-hant-mo"); /* Chinese (Macao) */
    test_tag_from_language!(tag_from_language_zh_hans_mo, "ZHS", "zh-hans-mo"); /* Chinese (Simplified, Macao) */
    test_tag_from_language!(tag_from_language_ZHH, "ZHH", "zh-HK"); /* Chinese (Hong Kong) */
    test_tag_from_language!(tag_from_language_zh_HanT_hK, "ZHH", "zH-HanT-hK"); /* Chinese (Hong Kong) */
    test_tag_from_language!(tag_from_language_zh_HanS_hK, "ZHS", "zH-HanS-hK"); /* Chinese (Simplified, Hong Kong) */
    test_tag_from_language!(tag_from_language_zh_tw, "ZHT", "zh-tw"); /* Chinese (Taiwan) */
    test_tag_from_language!(tag_from_language_ZHS, "ZHS", "zh-Hans"); /* Chinese (Simplified) */
    test_tag_from_language!(tag_from_language_ZHT, "ZHT", "zh-Hant"); /* Chinese (Traditional) */
    test_tag_from_language!(tag_from_language_zh_xx, "ZHS", "zh-xx"); /* Chinese (Other) */
    test_tag_from_language!(tag_from_language_zh_Hans_TW, "ZHS", "zh-Hans-TW");
    test_tag_from_language!(tag_from_language_yue, "ZHH", "yue");
    test_tag_from_language!(tag_from_language_yue_Hant, "ZHH", "yue-Hant");
    test_tag_from_language!(tag_from_language_yue_Hans, "ZHS", "yue-Hans");
    test_tag_from_language!(tag_from_language_ABC, "ABC", "abc");
    test_tag_from_language!(tag_from_language_ABCD, "ABCD", "x-hbotabcd");
    test_tag_from_language!(tag_from_language_asdf_asdf_wer_x_hbotabc_zxc, "ABC", "asdf-asdf-wer-x-hbotabc-zxc");
    test_tag_from_language!(tag_from_language_asdf_asdf_wer_x_hbotabc, "ABC", "asdf-asdf-wer-x-hbotabc");
    test_tag_from_language!(tag_from_language_asdf_asdf_wer_x_hbotabcd, "ABCD", "asdf-asdf-wer-x-hbotabcd");
    test_tag_from_language!(tag_from_language_asdf_asdf_wer_x_hbot_zxc, "dflt", "asdf-asdf-wer-x-hbot-zxc");
    test_tag_from_language!(tag_from_language_xy, "dflt", "xy");
    test_tag_from_language!(tag_from_language_xyz, "XYZ", "xyz"); /* Unknown ISO 639-3 */
    test_tag_from_language!(tag_from_language_xyz_qw, "XYZ", "xyz-qw"); /* Unknown ISO 639-3 */

    /*
     * Invalid input. The precise answer does not matter, as long as it
     * does not crash or get into an infinite loop.
     */
    test_tag_from_language!(tag_from_language__fonipa, "IPPH", "-fonipa");

    /*
     * Tags that contain "-fonipa" as a substring but which do not contain
     * the subtag "fonipa".
     */
    test_tag_from_language!(tag_from_language_en_fonipax, "ENG", "en-fonipax");
    test_tag_from_language!(tag_from_language_en_x_fonipa, "ENG", "en-x-fonipa");
    test_tag_from_language!(tag_from_language_en_a_fonipa, "ENG", "en-a-fonipa");
    test_tag_from_language!(tag_from_language_en_a_qwe_b_fonipa, "ENG", "en-a-qwe-b-fonipa");

    /* International Phonetic Alphabet */
    test_tag_from_language!(tag_from_language_en_fonipa, "IPPH", "en-fonipa");
    test_tag_from_language!(tag_from_language_en_fonipax_fonipa, "IPPH", "en-fonipax-fonipa");
    test_tag_from_language!(tag_from_language_rm_ch_fonipa_sursilv_x_foobar, "IPPH", "rm-CH-fonipa-sursilv-x-foobar");
    test_tag_from_language!(tag_from_language_IPPH, "IPPH", "und-fonipa");
    test_tag_from_language!(tag_from_language_zh_fonipa, "IPPH", "zh-fonipa");

    /* North American Phonetic Alphabet (Americanist Phonetic Notation) */
    test_tag_from_language!(tag_from_language_en_fonnapa, "APPH", "en-fonnapa");
    test_tag_from_language!(tag_from_language_chr_fonnapa, "APPH", "chr-fonnapa");
    test_tag_from_language!(tag_from_language_APPH, "APPH", "und-fonnapa");

    /* Uralic Phonetic Alphabet */
    test_tag_from_language!(tag_from_language_fi_fonupa, "UPPH", "fi-fonupa");
    test_tag_from_language!(tag_from_language_UPPH, "UPPH", "und-fonupa");

    /* Khutsuri Georgian */
    test_tag_from_language!(tag_from_language_ka_geok, "KGE", "ka-Geok");
    test_tag_from_language!(tag_from_language_KGE, "KGE", "und-Geok");

    /* Irish Traditional */
    test_tag_from_language!(tag_from_language_IRT, "IRT", "ga-Latg");

    /* Moldavian */
    test_tag_from_language!(tag_from_language_MOL, "MOL", "ro-MD");

    /* Polytonic Greek */
    test_tag_from_language!(tag_from_language_PGR, "PGR", "el-polyton");
    test_tag_from_language!(tag_from_language_el_CY_polyton, "PGR", "el-CY-polyton");

    /* Estrangela Syriac */
    test_tag_from_language!(tag_from_language_aii_Syre, "SYRE", "aii-Syre");
    test_tag_from_language!(tag_from_language_de_Syre, "SYRE", "de-Syre");
    test_tag_from_language!(tag_from_language_syr_Syre, "SYRE", "syr-Syre");
    test_tag_from_language!(tag_from_language_und_Syre, "SYRE", "und-Syre");

    /* Western Syriac */
    test_tag_from_language!(tag_from_language_aii_Syrj, "SYRJ", "aii-Syrj");
    test_tag_from_language!(tag_from_language_de_Syrj, "SYRJ", "de-Syrj");
    test_tag_from_language!(tag_from_language_syr_Syrj, "SYRJ", "syr-Syrj");
    test_tag_from_language!(tag_from_language_SYRJ, "SYRJ", "und-Syrj");

    /* Eastern Syriac */
    test_tag_from_language!(tag_from_language_aii_Syrn, "SYRN", "aii-Syrn");
    test_tag_from_language!(tag_from_language_de_Syrn, "SYRN", "de-Syrn");
    test_tag_from_language!(tag_from_language_syr_Syrn, "SYRN", "syr-Syrn");
    test_tag_from_language!(tag_from_language_SYRN, "SYRN", "und-Syrn");

    /* Test that x-hbot overrides the base language */
    test_tag_from_language!(tag_from_language_fa_x_hbotabc_zxc, "ABC", "fa-x-hbotabc-zxc");
    test_tag_from_language!(tag_from_language_fa_ir_x_hbotabc_zxc, "ABC", "fa-ir-x-hbotabc-zxc");
    test_tag_from_language!(tag_from_language_zh_x_hbotabc_zxc, "ABC", "zh-x-hbotabc-zxc");
    test_tag_from_language!(tag_from_language_zh_cn_x_hbotabc_zxc, "ABC", "zh-cn-x-hbotabc-zxc");
    test_tag_from_language!(tag_from_language_zh_xy_x_hbotabc_zxc, "ABC", "zh-xy-x-hbotabc-zxc");
    test_tag_from_language!(tag_from_language_xyz_xy_x_hbotabc_zxc, "ABC", "xyz-xy-x-hbotabc-zxc");

    /* Unnormalized BCP 47 tags */
    test_tag_from_language!(tag_from_language_ar_aao, "ARA", "ar-aao");
    test_tag_from_language!(tag_from_language_art_lojban, "JBO", "art-lojban");
    test_tag_from_language!(tag_from_language_kok_gom, "KOK", "kok-gom");
    test_tag_from_language!(tag_from_language_i_lux, "LTZ", "i-lux");
    test_tag_from_language!(tag_from_language_drh, "MNG", "drh");
    test_tag_from_language!(tag_from_language_ar_ary1, "MOR", "ar-ary");
    test_tag_from_language!(tag_from_language_ar_ary_DZ, "MOR", "ar-ary-DZ");
    test_tag_from_language!(tag_from_language_no_bok, "NOR", "no-bok");
    test_tag_from_language!(tag_from_language_no_nyn, "NYN", "no-nyn");
    test_tag_from_language!(tag_from_language_i_hak, "ZHS", "i-hak");
    test_tag_from_language!(tag_from_language_zh_guoyu, "ZHS", "zh-guoyu");
    test_tag_from_language!(tag_from_language_zh_min, "ZHS", "zh-min");
    test_tag_from_language!(tag_from_language_zh_min_nan, "ZHS", "zh-min-nan");
    test_tag_from_language!(tag_from_language_zh_xiang, "ZHS", "zh-xiang");

    /* BCP 47 tags that look similar to unrelated language system tags */
    test_tag_from_language!(tag_from_language_als, "SQI", "als");
    test_tag_from_language!(tag_from_language_far, "dflt", "far");

    /* A UN M.49 region code, not an extended language subtag */
    test_tag_from_language!(tag_from_language_ar_001, "ARA", "ar-001");

    /* An invalid tag */
    test_tag_from_language!(tag_from_language_invalid, "TRK", "tr@foo=bar");

    macro_rules! test_tags {
        ($name:ident, $script:expr, $lang:expr, $scripts:expr, $langs:expr) => {
            #[test]
            fn $name() {
                let (scripts, languages) =
                    tags_from_script_and_language($script, Language::new($lang).as_ref());

                let exp_scripts: Vec<Tag> = $scripts.iter().map(|v| Tag::from_bytes_lossy(*v)).collect();
                let exp_langs: Vec<Tag> = $langs.iter().map(|v| Tag::from_bytes_lossy(*v)).collect();

                assert_eq!(exp_scripts, scripts.as_slice());
                assert_eq!(exp_langs, languages.as_slice());
            }
        };
    }

    test_tags!(tag_full_en, None, "en", &[], &[b"ENG"]);
    test_tags!(tag_full_en_x_hbscdflt, None, "en-x-hbscdflt", &[b"DFLT"], &[b"ENG"]);
    test_tags!(tag_full_en_latin, Some(Script::LATIN), "en", &[b"latn"], &[b"ENG"]);
    test_tags!(tag_full_und_fonnapa, None, "und-fonnapa", &[], &[b"APPH"]);
    test_tags!(tag_full_en_fonnapa, None, "en-fonnapa", &[], &[b"APPH"]);
    test_tags!(tag_full_und_fonupa, None, "und-fonupa", &[], &[b"UPPH"]);
    test_tags!(tag_full_fi_fonupa, None, "fi-fonupa", &[], &[b"UPPH"]);
    test_tags!(tag_full_x_hbot1234_hbsc5678, None, "x-hbot1234-hbsc5678", &[b"5678"], &[b"1234"]);
    test_tags!(tag_full_x_hbsc5678_hbot1234, None, "x-hbsc5678-hbot1234", &[b"5678"], &[b"1234"]);
    test_tags!(tag_hex_mont, Some(Script::MYANMAR), "x-hbot-4d4f4e54", &[b"mym2", b"mymr"], &[b"MONT"]);
    test_tags!(tag_hex_uppercase, None, "X-HBOT-4D4F4E54", &[], &[b"MONT"]);
    test_tags!(tag_hex_language_override, None, "my-x-hbot-4d4f4e54-zxc", &[], &[b"MONT"]);
    test_tags!(tag_hex_script_override, Some(Script::LATIN), "en-x-hbsc-64657633", &[b"dev3"], &[b"ENG"]);
    test_tags!(tag_hex_language_and_script, None, "x-hbot-4d4f4e54-hbsc-6d796d32", &[b"mym2"], &[b"MONT"]);
    test_tags!(tag_hex_script_and_language, None, "x-hbsc-6d796d32-hbot-4d4f4e54", &[b"mym2"], &[b"MONT"]);
    test_tags!(tag_hex_preserves_case_and_punctuation, None, "x-hbot-41686121-hbsc-41686121", &[b"Aha!"], &[b"Aha!"]);
    test_tags!(tag_hex_preserves_spaces, None, "x-hbot-41424320-hbsc-79692020", &[b"yi  "], &[b"ABC "]);
    test_tags!(tag_hex_preserves_arbitrary_bytes, None, "x-hbot-00ff80ab-hbsc-00ff80ab", &[b"\x00\xff\x80\xab"], &[b"\x00\xff\x80\xab"]);
    test_tags!(tag_hex_preserves_zero, None, "x-hbot-00000000-hbsc-00000000", &[b"\0\0\0\0"], &[b"\0\0\0\0"]);
    test_tags!(tag_hex_default_uppercase, None, "x-hbot-44464c54-hbsc-44464c54", &[b"dflt"], &[b"dflt"]);
    test_tags!(tag_hex_default_lowercase, None, "x-hbot-64666c74-hbsc-64666c74", &[b"DFLT"], &[b"DFLT"]);
    test_tags!(tag_hex_ignores_trailing_digits, None, "x-hbot-326c6f6e67-hbsc-326c6f6e67", &[b"2lon"], &[b"2lon"]);
    test_tags!(tag_hex_first_override, None, "fa-x-hbot-41686121-hbotabc-zxc", &[], &[b"Aha!"]);
    test_tags!(tag_text_first_override, None, "fa-x-hbotabc-hbot-41686121-zxc", &[], &[b"ABC"]);
    test_tags!(tag_text_mont, None, "x-hbotmont", &[], &[b"MONT"]);

    #[test]
    fn tag_hex_invalid() {
        for hex in [
            "", "1", "1a", "414243", "4142432", "1a2b3c4x", "4d4f-4e54",
            "+d4f4e54", "-d4f4e54", "4d4f4e5\u{e9}",
        ] {
            let language = alloc::format!("en-x-hbot-{hex}-hbsc-{hex}");
            let (scripts, languages) = tags_from_script_and_language(
                Some(Script::LATIN), Language::new(&language).as_ref(),
            );
            assert_eq!(scripts.as_slice(), &[Tag::new(b"latn")], "{language}");
            assert_eq!(languages.as_slice(), &[Tag::new(b"ENG ")], "{language}");

            let language = alloc::format!("x-hbot-{hex}-hbsc-{hex}");
            let (scripts, languages) =
                tags_from_script_and_language(None, Language::new(&language).as_ref());
            assert!(scripts.is_empty(), "{language}");
            assert!(languages.is_empty(), "{language}");
        }
    }

    test_tags!(tag_full_ml, Some(Script::MALAYALAM), "ml", &[b"mlm3", b"mlm2", b"mlym"], &[b"MAL", b"MLR"]);
    test_tags!(tag_full_xyz, None, "xyz", &[], &[b"XYZ"]);
    test_tags!(tag_full_xy, None, "xy", &[], &[]);
}

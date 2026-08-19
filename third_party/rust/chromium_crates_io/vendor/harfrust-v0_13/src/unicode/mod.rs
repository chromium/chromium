#![allow(
    non_camel_case_types,
    non_snake_case,
    non_upper_case_globals,
    clippy::inline_always,
    clippy::wrong_self_convention,
    clippy::enum_glob_use
)]

#[expect(clippy::identity_op)]
#[rustfmt::skip]
mod emoji_table;
#[cfg(not(feature = "icu"))]
#[rustfmt::skip]
mod ucd_table;

use crate::Script;

pub type Codepoint = u32;

// Space estimates based on:
// https://unicode.org/charts/PDF/U2000.pdf
// https://docs.microsoft.com/en-us/typography/develop/character-design-standards/whitespace
pub mod hb_unicode_funcs_t {
    pub type space_t = u8;
    pub const NOT_SPACE: u8 = 0;
    pub const SPACE_EM: u8 = 1;
    pub const SPACE_EM_2: u8 = 2;
    pub const SPACE_EM_3: u8 = 3;
    pub const SPACE_EM_4: u8 = 4;
    pub const SPACE_EM_5: u8 = 5;
    pub const SPACE_EM_6: u8 = 6;
    pub const SPACE_EM_16: u8 = 16;
    pub const SPACE_4_EM_18: u8 = 17; // 4/18th of an EM!
    pub const SPACE: u8 = 18;
    pub const SPACE_FIGURE: u8 = 19;
    pub const SPACE_PUNCTUATION: u8 = 20;
    pub const SPACE_NARROW: u8 = 21;
}

/// Data type for the "General_Category" (gc) property from the Unicode
/// Character Database.
#[derive(Copy, Clone, PartialEq, Eq, Debug)]
pub struct GeneralCategory(pub u8);

#[allow(unused)]
impl GeneralCategory {
    /// Control (`Cc`).
    pub const CONTROL: Self = Self(0);
    /// Format (`Cf`).
    pub const FORMAT: Self = Self(1);
    /// Unassigned (`Cn`).
    pub const UNASSIGNED: Self = Self(2);
    /// Private Use (`Co`).
    pub const PRIVATE_USE: Self = Self(3);
    /// Surrogate (`Cs`).
    pub const SURROGATE: Self = Self(4);
    /// Lowercase Letter (`Ll`).
    pub const LOWERCASE_LETTER: Self = Self(5);
    /// Modifier Letter (`Lm`).
    pub const MODIFIER_LETTER: Self = Self(6);
    /// Other Letter (`Lo`).
    pub const OTHER_LETTER: Self = Self(7);
    /// Titlecase Letter (`Lt`).
    pub const TITLECASE_LETTER: Self = Self(8);
    /// Uppercase Letter (`Lu`).
    pub const UPPERCASE_LETTER: Self = Self(9);
    /// Spacing Mark (`Mc`).
    pub const SPACING_MARK: Self = Self(10);
    /// Enclosing Mark (`Me`).
    pub const ENCLOSING_MARK: Self = Self(11);
    /// Nonspacing Mark (`Mn`).
    pub const NON_SPACING_MARK: Self = Self(12);
    /// Decimal Number (`Nd`).
    pub const DECIMAL_NUMBER: Self = Self(13);
    /// Letter Number (`Nl`).
    pub const LETTER_NUMBER: Self = Self(14);
    /// Other Number (`No`).
    pub const OTHER_NUMBER: Self = Self(15);
    /// Connector Punctuation (`Pc`).
    pub const CONNECT_PUNCTUATION: Self = Self(16);
    /// Dash Punctuation (`Pd`).
    pub const DASH_PUNCTUATION: Self = Self(17);
    /// Close Punctuation (`Pe`).
    pub const CLOSE_PUNCTUATION: Self = Self(18);
    /// Final Punctuation (`Pf`).
    pub const FINAL_PUNCTUATION: Self = Self(19);
    /// Initial Punctuation (`Pi`).
    pub const INITIAL_PUNCTUATION: Self = Self(20);
    /// Other Punctuation (`Po`).
    pub const OTHER_PUNCTUATION: Self = Self(21);
    /// Open Punctuation (`Ps`).
    pub const OPEN_PUNCTUATION: Self = Self(22);
    /// Currency Symbol (`Sc`).
    pub const CURRENCY_SYMBOL: Self = Self(23);
    /// Modifier Symbol (`Sk`).
    pub const MODIFIER_SYMBOL: Self = Self(24);
    /// Math Symbol (`Sm`).
    pub const MATH_SYMBOL: Self = Self(25);
    /// Other Symbol (`So`).
    pub const OTHER_SYMBOL: Self = Self(26);
    /// Line Separator (`Zl`).
    pub const LINE_SEPARATOR: Self = Self(27);
    /// Paragraph Separator (`Zp`).
    pub const PARAGRAPH_SEPARATOR: Self = Self(28);
    /// Space Separator (`Zs`).
    pub const SPACE_SEPARATOR: Self = Self(29);
}

impl GeneralCategory {
    pub fn is_mark(&self) -> bool {
        matches!(
            *self,
            Self::SPACING_MARK | Self::ENCLOSING_MARK | Self::NON_SPACING_MARK
        )
    }

    pub fn is_letter(&self) -> bool {
        matches!(
            *self,
            Self::LOWERCASE_LETTER
                | Self::MODIFIER_LETTER
                | Self::OTHER_LETTER
                | Self::TITLECASE_LETTER
                | Self::UPPERCASE_LETTER
        )
    }

    #[inline(always)]
    pub(crate) const fn flag(self) -> u32 {
        1 << self.0
    }

    #[inline(always)]
    pub(crate) const fn flag_unsafe(self) -> u32 {
        if self.0 < 32 {
            1 << self.0
        } else {
            0
        }
    }

    #[inline(always)]
    pub(crate) const fn in_range_inclusive(self, start: Self, end: Self) -> bool {
        debug_assert!(start.0 < end.0);
        let range_mask = (1 << (end.0 + 1)) - (1 << start.0);
        self.flag_unsafe() & range_mask != 0
    }
}

#[allow(dead_code)]
pub mod combining_class {
    pub const NotReordered: u8 = 0;
    pub const Overlay: u8 = 1;
    pub const Nukta: u8 = 7;
    pub const KanaVoicing: u8 = 8;
    pub const Virama: u8 = 9;

    /* Hebrew */
    pub const CCC10: u8 = 10;
    pub const CCC11: u8 = 11;
    pub const CCC12: u8 = 12;
    pub const CCC13: u8 = 13;
    pub const CCC14: u8 = 14;
    pub const CCC15: u8 = 15;
    pub const CCC16: u8 = 16;
    pub const CCC17: u8 = 17;
    pub const CCC18: u8 = 18;
    pub const CCC19: u8 = 19;
    pub const CCC20: u8 = 20;
    pub const CCC21: u8 = 21;
    pub const CCC22: u8 = 22;
    pub const CCC23: u8 = 23;
    pub const CCC24: u8 = 24;
    pub const CCC25: u8 = 25;
    pub const CCC26: u8 = 26;

    /* Arabic */
    pub const CCC27: u8 = 27;
    pub const CCC28: u8 = 28;
    pub const CCC29: u8 = 29;
    pub const CCC30: u8 = 30;
    pub const CCC31: u8 = 31;
    pub const CCC32: u8 = 32;
    pub const CCC33: u8 = 33;
    pub const CCC34: u8 = 34;
    pub const CCC35: u8 = 35;

    /* Syriac */
    pub const CCC36: u8 = 36;

    /* Telugu */
    pub const CCC84: u8 = 84;
    pub const CCC91: u8 = 91;

    /* Thai */
    pub const CCC103: u8 = 103;
    pub const CCC107: u8 = 107;

    /* Lao */
    pub const CCC118: u8 = 118;
    pub const CCC122: u8 = 122;

    /* Tibetan */
    pub const CCC129: u8 = 129;
    pub const CCC130: u8 = 130;
    pub const CCC132: u8 = 132;

    pub const AttachedBelowLeft: u8 = 200;
    pub const AttachedBelow: u8 = 202;
    pub const AttachedAbove: u8 = 214;
    pub const AttachedAboveRight: u8 = 216;
    pub const BelowLeft: u8 = 218;
    pub const Below: u8 = 220;
    pub const BelowRight: u8 = 222;
    pub const Left: u8 = 224;
    pub const Right: u8 = 226;
    pub const AboveLeft: u8 = 228;
    pub const Above: u8 = 230;
    pub const AboveRight: u8 = 232;
    pub const DoubleBelow: u8 = 233;
    pub const DoubleAbove: u8 = 234;

    pub const IotaSubscript: u8 = 240;

    pub const Invalid: u8 = 255;
}

#[allow(dead_code)]
pub mod modified_combining_class {
    // Hebrew
    //
    // We permute the "fixed-position" classes 10-26 into the order
    // described in the SBL Hebrew manual:
    //
    // https://www.sbl-site.org/Fonts/SBLHebrewUserManual1.5x.pdf
    //
    // (as recommended by:
    //  https://forum.fontlab.com/archive-old-microsoft-volt-group/vista-and-diacritic-ordering/msg22823/)
    //
    // More details here:
    // https://bugzilla.mozilla.org/show_bug.cgi?id=662055
    pub const CCC10: u8 = 22; // sheva
    pub const CCC11: u8 = 15; // hataf segol
    pub const CCC12: u8 = 16; // hataf patah
    pub const CCC13: u8 = 17; // hataf qamats
    pub const CCC14: u8 = 23; // hiriq
    pub const CCC15: u8 = 18; // tsere
    pub const CCC16: u8 = 19; // segol
    pub const CCC17: u8 = 20; // patah
    pub const CCC18: u8 = 21; // qamats & qamats qatan
    pub const CCC19: u8 = 14; // holam & holam haser for vav
    pub const CCC20: u8 = 24; // qubuts
    pub const CCC21: u8 = 12; // dagesh
    pub const CCC22: u8 = 25; // meteg
    pub const CCC23: u8 = 13; // rafe
    pub const CCC24: u8 = 10; // shin dot
    pub const CCC25: u8 = 11; // sin dot
    pub const CCC26: u8 = 26; // point varika

    // Arabic
    //
    // Modify to move Shadda (ccc=33) before other marks.  See:
    // https://unicode.org/faq/normalization.html#8
    // https://unicode.org/faq/normalization.html#9
    pub const CCC27: u8 = 28; // fathatan
    pub const CCC28: u8 = 29; // dammatan
    pub const CCC29: u8 = 30; // kasratan
    pub const CCC30: u8 = 31; // fatha
    pub const CCC31: u8 = 32; // damma
    pub const CCC32: u8 = 33; // kasra
    pub const CCC33: u8 = 27; // shadda
    pub const CCC34: u8 = 34; // sukun
    pub const CCC35: u8 = 35; // superscript alef

    // Syriac
    pub const CCC36: u8 = 36; // superscript alaph

    // Telugu
    //
    // Modify Telugu length marks (ccc=84, ccc=91).
    // These are the only matras in the main Indic scripts range that have
    // a non-zero ccc.  That makes them reorder with the Halant that is
    // ccc=9.  Just zero them, we don't need them in our Indic shaper.
    pub const CCC84: u8 = 0; // length mark
    pub const CCC91: u8 = 0; // ai length mark

    // Thai
    //
    // Modify U+0E38 and U+0E39 (ccc=103) to be reordered before U+0E3A (ccc=9).
    // Assign 3, which is unassigned otherwise.
    // Uniscribe does this reordering too.
    pub const CCC103: u8 = 3; // sara u / sara uu
    pub const CCC107: u8 = 107; // mai *

    // Lao
    pub const CCC118: u8 = 118; // sign u / sign uu
    pub const CCC122: u8 = 122; // mai *

    // Tibetan
    //
    // In case of multiple vowel-signs, use u first (but after achung)
    // this allows Dzongkha multi-vowel shortcuts to render correctly
    pub const CCC129: u8 = 129; // sign aa
    pub const CCC130: u8 = 132; // sign i
    pub const CCC132: u8 = 131; // sign u
}

#[rustfmt::skip]
static MODIFIED_COMBINING_CLASS: &[u8; 256] = &[
    combining_class::NotReordered,
    combining_class::Overlay,
    2, 3, 4, 5, 6,
    combining_class::Nukta,
    combining_class::KanaVoicing,
    combining_class::Virama,

    // Hebrew
    modified_combining_class::CCC10,
    modified_combining_class::CCC11,
    modified_combining_class::CCC12,
    modified_combining_class::CCC13,
    modified_combining_class::CCC14,
    modified_combining_class::CCC15,
    modified_combining_class::CCC16,
    modified_combining_class::CCC17,
    modified_combining_class::CCC18,
    modified_combining_class::CCC19,
    modified_combining_class::CCC20,
    modified_combining_class::CCC21,
    modified_combining_class::CCC22,
    modified_combining_class::CCC23,
    modified_combining_class::CCC24,
    modified_combining_class::CCC25,
    modified_combining_class::CCC26,

    // Arabic
    modified_combining_class::CCC27,
    modified_combining_class::CCC28,
    modified_combining_class::CCC29,
    modified_combining_class::CCC30,
    modified_combining_class::CCC31,
    modified_combining_class::CCC32,
    modified_combining_class::CCC33,
    modified_combining_class::CCC34,
    modified_combining_class::CCC35,

    // Syriac
    modified_combining_class::CCC36,

    37, 38, 39,
    40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59,
    60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79,
    80, 81, 82, 83,

    // Telugu
    modified_combining_class::CCC84,
    85, 86, 87, 88, 89, 90,
    modified_combining_class::CCC91,
    92, 93, 94, 95, 96, 97, 98, 99, 100, 101, 102,

    // Thai
    modified_combining_class::CCC103,
    104, 105, 106,
    modified_combining_class::CCC107,
    108, 109, 110, 111, 112, 113, 114, 115, 116, 117,

    // Lao
    modified_combining_class::CCC118,
    119, 120, 121,
    modified_combining_class::CCC122,
    123, 124, 125, 126, 127, 128,

    // Tibetan
    modified_combining_class::CCC129,
    modified_combining_class::CCC130,
    131,
    modified_combining_class::CCC132,
    133, 134, 135, 136, 137, 138, 139,


    140, 141, 142, 143, 144, 145, 146, 147, 148, 149,
    150, 151, 152, 153, 154, 155, 156, 157, 158, 159,
    160, 161, 162, 163, 164, 165, 166, 167, 168, 169,
    170, 171, 172, 173, 174, 175, 176, 177, 178, 179,
    180, 181, 182, 183, 184, 185, 186, 187, 188, 189,
    190, 191, 192, 193, 194, 195, 196, 197, 198, 199,

    combining_class::AttachedBelowLeft,
    201,
    combining_class::AttachedBelow,
    203, 204, 205, 206, 207, 208, 209, 210, 211, 212, 213,
    combining_class::AttachedAbove,
    215,
    combining_class::AttachedAboveRight,
    217,
    combining_class::BelowLeft,
    219,
    combining_class::Below,
    221,
    combining_class::BelowRight,
    223,
    combining_class::Left,
    225,
    combining_class::Right,
    227,
    combining_class::AboveLeft,
    229,
    combining_class::Above,
    231,
    combining_class::AboveRight,
    combining_class::DoubleBelow,
    combining_class::DoubleAbove,
    235, 236, 237, 238, 239,
    combining_class::IotaSubscript,
    241, 242, 243, 244, 245, 246, 247, 248, 249, 250, 251, 252, 253, 254,
    combining_class::Invalid,
];

pub trait CharExt {
    fn script(self) -> Script;
    fn general_category(self) -> GeneralCategory;
    fn space_fallback(self) -> hb_unicode_funcs_t::space_t;
    fn combining_class(self) -> u8;
    fn modified_combining_class(self) -> u8;
    fn mirroring(self) -> Option<Codepoint>;
    fn is_emoji_extended_pictographic(self) -> bool;
    fn is_default_ignorable(self) -> bool;
    fn is_variation_selector(self) -> bool;
    fn vertical(self) -> Option<Codepoint>;
}

impl CharExt for Codepoint {
    fn script(self) -> Script {
        script_for(self)
    }

    fn general_category(self) -> GeneralCategory {
        general_category_for(self)
    }

    fn combining_class(self) -> u8 {
        combining_class_for(self)
    }

    fn mirroring(self) -> Option<Codepoint> {
        mirroring_for(self)
    }

    fn space_fallback(self) -> hb_unicode_funcs_t::space_t {
        use hb_unicode_funcs_t::*;

        // All GC=Zs chars that can use a fallback.
        match self {
            0x0020 => SPACE,             // SPACE
            0x00A0 => SPACE,             // NO-BREAK SPACE
            0x2000 => SPACE_EM_2,        // EN QUAD
            0x2001 => SPACE_EM,          // EM QUAD
            0x2002 => SPACE_EM_2,        // EN SPACE
            0x2003 => SPACE_EM,          // EM SPACE
            0x2004 => SPACE_EM_3,        // THREE-PER-EM SPACE
            0x2005 => SPACE_EM_4,        // FOUR-PER-EM SPACE
            0x2006 => SPACE_EM_6,        // SIX-PER-EM SPACE
            0x2007 => SPACE_FIGURE,      // FIGURE SPACE
            0x2008 => SPACE_PUNCTUATION, // PUNCTUATION SPACE
            0x2009 => SPACE_EM_5,        // THIN SPACE
            0x200A => SPACE_EM_16,       // HAIR SPACE
            0x202F => SPACE_NARROW,      // NARROW NO-BREAK SPACE
            0x205F => SPACE_4_EM_18,     // MEDIUM MATHEMATICAL SPACE
            0x3000 => SPACE_EM,          // IDEOGRAPHIC SPACE
            _ => NOT_SPACE,              // OGHAM SPACE MARK
        }
    }

    fn modified_combining_class(self) -> u8 {
        let u = self;

        // Reorder SAKOT to ensure it comes after any tone marks.
        if u == 0x1A60 {
            return 254;
        }

        // Reorder PADMA to ensure it comes after any vowel marks.
        if u == 0x0FC6 {
            return 254;
        }

        // Reorder TSA -PHRU to reorder before U+0F74
        if u == 0x0F39 {
            return 127;
        }

        let k = u.combining_class();

        MODIFIED_COMBINING_CLASS[k as usize]
    }

    fn is_emoji_extended_pictographic(self) -> bool {
        emoji_table::is_Extended_Pictographic(self)
    }

    /// Default_Ignorable codepoints:
    ///
    /// Note: While U+115F, U+1160, U+3164 and U+FFA0 are Default_Ignorable,
    /// we do NOT want to hide them, as the way Uniscribe has implemented them
    /// is with regular spacing glyphs, and that's the way fonts are made to work.
    /// As such, we make exceptions for those four.
    /// Also ignoring U+1BCA0..1BCA3. https://github.com/harfbuzz/harfbuzz/issues/503
    ///
    /// Unicode 14.0:
    /// $ grep '; Default_Ignorable_Code_Point ' DerivedCoreProperties.txt | sed 's/;.*#/#/'
    /// 00AD          # Cf       SOFT HYPHEN
    /// 034F          # Mn       COMBINING GRAPHEME JOINER
    /// 061C          # Cf       ARABIC LETTER MARK
    /// 115F..1160    # Lo   [2] HANGUL CHOSEONG FILLER..HANGUL JUNGSEONG FILLER
    /// 17B4..17B5    # Mn   [2] KHMER VOWEL INHERENT AQ..KHMER VOWEL INHERENT AA
    /// 180B..180D    # Mn   [3] MONGOLIAN FREE VARIATION SELECTOR ONE..MONGOLIAN FREE VARIATION SELECTOR THREE
    /// 180E          # Cf       MONGOLIAN VOWEL SEPARATOR
    /// 180F          # Mn       MONGOLIAN FREE VARIATION SELECTOR FOUR
    /// 200B..200F    # Cf   [5] ZERO WIDTH SPACE..RIGHT-TO-LEFT MARK
    /// 202A..202E    # Cf   [5] LEFT-TO-RIGHT EMBEDDING..RIGHT-TO-LEFT OVERRIDE
    /// 2060..2064    # Cf   [5] WORD JOINER..INVISIBLE PLUS
    /// 2065          # Cn       <reserved-2065>
    /// 2066..206F    # Cf  [10] LEFT-TO-RIGHT ISOLATE..NOMINAL DIGIT SHAPES
    /// 3164          # Lo       HANGUL FILLER
    /// FE00..FE0F    # Mn  [16] VARIATION SELECTOR-1..VARIATION SELECTOR-16
    /// FEFF          # Cf       ZERO WIDTH NO-BREAK SPACE
    /// FFA0          # Lo       HALFWIDTH HANGUL FILLER
    /// FFF0..FFF8    # Cn   [9] <reserved-FFF0>..<reserved-FFF8>
    /// 1BCA0..1BCA3  # Cf   [4] SHORTHAND FORMAT LETTER OVERLAP..SHORTHAND FORMAT UP STEP
    /// 1D173..1D17A  # Cf   [8] MUSICAL SYMBOL BEGIN BEAM..MUSICAL SYMBOL END PHRASE
    /// E0000         # Cn       <reserved-E0000>
    /// E0001         # Cf       LANGUAGE TAG
    /// E0002..E001F  # Cn  [30] <reserved-E0002>..<reserved-E001F>
    /// E0020..E007F  # Cf  [96] TAG SPACE..CANCEL TAG
    /// E0080..E00FF  # Cn [128] <reserved-E0080>..<reserved-E00FF>
    /// E0100..E01EF  # Mn [240] VARIATION SELECTOR-17..VARIATION SELECTOR-256
    /// E01F0..E0FFF  # Cn [3600] <reserved-E01F0>..<reserved-E0FFF>
    fn is_default_ignorable(self) -> bool {
        let ch = self;
        let plane = ch >> 16;
        if plane == 0 {
            // BMP
            let page = ch >> 8;
            match page {
                0x00 => ch == 0x00AD,
                0x03 => ch == 0x034F,
                0x06 => ch == 0x061C,
                0x17 => (0x17B4..=0x17B5).contains(&ch),
                0x18 => (0x180B..=0x180E).contains(&ch),
                0x20 => {
                    (0x200B..=0x200F).contains(&ch)
                        || (0x202A..=0x202E).contains(&ch)
                        || (0x2060..=0x206F).contains(&ch)
                }
                0xFE => (0xFE00..=0xFE0F).contains(&ch) || ch == 0xFEFF,
                0xFF => (0xFFF0..=0xFFF8).contains(&ch),
                _ => false,
            }
        } else {
            // Other planes
            match plane {
                0x01 => (0x1D173..=0x1D17A).contains(&ch),
                0x0E => (0xE0000..=0xE0FFF).contains(&ch),
                _ => false,
            }
        }
    }

    fn is_variation_selector(self) -> bool {
        // U+180B..180D, U+180F MONGOLIAN FREE VARIATION SELECTORs are handled in the
        //Arabic shaper. No need to match them here.
        (0x0FE00..=0x0FE0F).contains(&self) || // VARIATION SELECTOR - 1..16
        (0xE0100..=0xE01EF).contains(&self) // VARIATION SELECTOR - 17..256
    }

    fn vertical(self) -> Option<Codepoint> {
        Some(match self >> 8 {
            0x20 => match self {
                0x2013 => 0xfe32, // EN DASH
                0x2014 => 0xfe31, // EM DASH
                0x2025 => 0xfe30, // TWO DOT LEADER
                0x2026 => 0xfe19, // HORIZONTAL ELLIPSIS
                _ => return None,
            },
            0x30 => match self {
                0x3001 => 0xfe11, // IDEOGRAPHIC COMMA
                0x3002 => 0xfe12, // IDEOGRAPHIC FULL STOP
                0x3008 => 0xfe3f, // LEFT ANGLE BRACKET
                0x3009 => 0xfe40, // RIGHT ANGLE BRACKET
                0x300a => 0xfe3d, // LEFT DOUBLE ANGLE BRACKET
                0x300b => 0xfe3e, // RIGHT DOUBLE ANGLE BRACKET
                0x300c => 0xfe41, // LEFT CORNER BRACKET
                0x300d => 0xfe42, // RIGHT CORNER BRACKET
                0x300e => 0xfe43, // LEFT WHITE CORNER BRACKET
                0x300f => 0xfe44, // RIGHT WHITE CORNER BRACKET
                0x3010 => 0xfe3b, // LEFT BLACK LENTICULAR BRACKET
                0x3011 => 0xfe3c, // RIGHT BLACK LENTICULAR BRACKET
                0x3014 => 0xfe39, // LEFT TORTOISE SHELL BRACKET
                0x3015 => 0xfe3a, // RIGHT TORTOISE SHELL BRACKET
                0x3016 => 0xfe17, // LEFT WHITE LENTICULAR BRACKET
                0x3017 => 0xfe18, // RIGHT WHITE LENTICULAR BRACKET
                _ => return None,
            },
            0xfe => match self {
                0xfe4f => 0xfe34, // WAVY LOW LINE
                _ => return None,
            },
            0xff => match self {
                0xff01 => 0xfe15, // FULLWIDTH EXCLAMATION MARK
                0xff08 => 0xfe35, // FULLWIDTH LEFT PARENTHESIS
                0xff09 => 0xfe36, // FULLWIDTH RIGHT PARENTHESIS
                0xff0c => 0xfe10, // FULLWIDTH COMMA
                0xff1a => 0xfe13, // FULLWIDTH COLON
                0xff1b => 0xfe14, // FULLWIDTH SEMICOLON
                0xff1f => 0xfe16, // FULLWIDTH QUESTION MARK
                0xff3b => 0xfe47, // FULLWIDTH LEFT SQUARE BRACKET
                0xff3d => 0xfe48, // FULLWIDTH RIGHT SQUARE BRACKET
                0xff3f => 0xfe33, // FULLWIDTH LOW LINE
                0xff5b => 0xfe37, // FULLWIDTH LEFT CURLY BRACKET
                0xff5d => 0xfe38, // FULLWIDTH RIGHT CURLY BRACKET
                _ => return None,
            },
            _ => return None,
        })
    }
}

#[cfg(feature = "icu")]
pub(crate) use icu::*;

#[cfg(not(feature = "icu"))]
pub(crate) use builtin::*;

#[cfg(feature = "icu")]
mod icu {
    use super::{Codepoint, GeneralCategory, Script};
    use icu_normalizer::properties::{
        CanonicalCompositionBorrowed, CanonicalDecompositionBorrowed, Decomposed,
    };
    use icu_properties::{
        props::{
            BidiMirroringGlyph, CanonicalCombiningClass as IcuCanonicalCombiningClass,
            GeneralCategory as IcuGeneralCategory, Script as IcuScript,
        },
        CodePointMapData,
    };

    pub(crate) fn script_for(c: u32) -> Script {
        let icu_script = CodePointMapData::<IcuScript>::new().get32(c);
        SCRIPT_MAP
            .get(icu_script.to_icu4c_value() as usize)
            .copied()
            .unwrap_or(crate::script::UNKNOWN)
    }

    pub(crate) fn general_category_for(c: u32) -> GeneralCategory {
        GeneralCategory::from_icu(CodePointMapData::<IcuGeneralCategory>::new().get32(c))
    }

    pub(crate) fn combining_class_for(c: u32) -> u8 {
        CodePointMapData::<IcuCanonicalCombiningClass>::new()
            .get32(c)
            .to_icu4c_value()
    }

    pub(crate) fn mirroring_for(c: u32) -> Option<Codepoint> {
        CodePointMapData::<BidiMirroringGlyph>::new()
            .get32(c)
            .mirroring_glyph
            .map(Codepoint::from)
    }

    pub(crate) fn compose(a: Codepoint, b: Codepoint) -> Option<Codepoint> {
        let a = char::from_u32(a)?;
        let b = char::from_u32(b)?;
        CanonicalCompositionBorrowed::new()
            .compose(a, b)
            .map(Codepoint::from)
    }

    pub(crate) fn decompose(ab: Codepoint) -> Option<(Codepoint, Codepoint)> {
        let ch = char::from_u32(ab)?;
        match CanonicalDecompositionBorrowed::new().decompose(ch) {
            Decomposed::Default => None,
            Decomposed::Singleton(a) => Some((Codepoint::from(a), 0)),
            Decomposed::Expansion(a, b) => Some((Codepoint::from(a), Codepoint::from(b))),
        }
    }

    impl GeneralCategory {
        fn from_icu(category: IcuGeneralCategory) -> Self {
            use IcuGeneralCategory::*;
            match category {
                Control => Self::CONTROL,
                Format => Self::FORMAT,
                Unassigned => Self::UNASSIGNED,
                PrivateUse => Self::PRIVATE_USE,
                Surrogate => Self::SURROGATE,
                LowercaseLetter => Self::LOWERCASE_LETTER,
                ModifierLetter => Self::MODIFIER_LETTER,
                OtherLetter => Self::OTHER_LETTER,
                TitlecaseLetter => Self::TITLECASE_LETTER,
                UppercaseLetter => Self::UPPERCASE_LETTER,
                SpacingMark => Self::SPACING_MARK,
                EnclosingMark => Self::ENCLOSING_MARK,
                NonspacingMark => Self::NON_SPACING_MARK,
                DecimalNumber => Self::DECIMAL_NUMBER,
                LetterNumber => Self::LETTER_NUMBER,
                OtherNumber => Self::OTHER_NUMBER,
                ConnectorPunctuation => Self::CONNECT_PUNCTUATION,
                DashPunctuation => Self::DASH_PUNCTUATION,
                ClosePunctuation => Self::CLOSE_PUNCTUATION,
                FinalPunctuation => Self::FINAL_PUNCTUATION,
                InitialPunctuation => Self::INITIAL_PUNCTUATION,
                OtherPunctuation => Self::OTHER_PUNCTUATION,
                OpenPunctuation => Self::OPEN_PUNCTUATION,
                CurrencySymbol => Self::CURRENCY_SYMBOL,
                ModifierSymbol => Self::MODIFIER_SYMBOL,
                MathSymbol => Self::MATH_SYMBOL,
                OtherSymbol => Self::OTHER_SYMBOL,
                LineSeparator => Self::LINE_SEPARATOR,
                ParagraphSeparator => Self::PARAGRAPH_SEPARATOR,
                SpaceSeparator => Self::SPACE_SEPARATOR,
            }
        }
    }

    pub(crate) static SCRIPT_MAP: [Script; 212] = [
        crate::script::COMMON,                 // 0
        crate::script::INHERITED,              // 1
        crate::script::ARABIC,                 // 2
        crate::script::ARMENIAN,               // 3
        crate::script::BENGALI,                // 4
        crate::script::BOPOMOFO,               // 5
        crate::script::CHEROKEE,               // 6
        crate::script::COPTIC,                 // 7
        crate::script::CYRILLIC,               // 8
        crate::script::DESERET,                // 9
        crate::script::DEVANAGARI,             // 10
        crate::script::ETHIOPIC,               // 11
        crate::script::GEORGIAN,               // 12
        crate::script::GOTHIC,                 // 13
        crate::script::GREEK,                  // 14
        crate::script::GUJARATI,               // 15
        crate::script::GURMUKHI,               // 16
        crate::script::HAN,                    // 17
        crate::script::HANGUL,                 // 18
        crate::script::HEBREW,                 // 19
        crate::script::HIRAGANA,               // 20
        crate::script::KANNADA,                // 21
        crate::script::KATAKANA,               // 22
        crate::script::KHMER,                  // 23
        crate::script::LAO,                    // 24
        crate::script::LATIN,                  // 25
        crate::script::MALAYALAM,              // 26
        crate::script::MONGOLIAN,              // 27
        crate::script::MYANMAR,                // 28
        crate::script::OGHAM,                  // 29
        crate::script::OLD_ITALIC,             // 30
        crate::script::ORIYA,                  // 31
        crate::script::RUNIC,                  // 32
        crate::script::SINHALA,                // 33
        crate::script::SYRIAC,                 // 34
        crate::script::TAMIL,                  // 35
        crate::script::TELUGU,                 // 36
        crate::script::THAANA,                 // 37
        crate::script::THAI,                   // 38
        crate::script::TIBETAN,                // 39
        crate::script::CANADIAN_SYLLABICS,     // 40
        crate::script::YI,                     // 41
        crate::script::TAGALOG,                // 42
        crate::script::HANUNOO,                // 43
        crate::script::BUHID,                  // 44
        crate::script::TAGBANWA,               // 45
        crate::script::BRAILLE,                // 46
        crate::script::CYPRIOT,                // 47
        crate::script::LIMBU,                  // 48
        crate::script::LINEAR_B,               // 49
        crate::script::OSMANYA,                // 50
        crate::script::SHAVIAN,                // 51
        crate::script::TAI_LE,                 // 52
        crate::script::UGARITIC,               // 53
        crate::script::UNKNOWN,                // 54
        crate::script::BUGINESE,               // 55
        crate::script::GLAGOLITIC,             // 56
        crate::script::KHAROSHTHI,             // 57
        crate::script::SYLOTI_NAGRI,           // 58
        crate::script::NEW_TAI_LUE,            // 59
        crate::script::TIFINAGH,               // 60
        crate::script::OLD_PERSIAN,            // 61
        crate::script::BALINESE,               // 62
        crate::script::BATAK,                  // 63
        crate::script::UNKNOWN,                // 64
        crate::script::BRAHMI,                 // 65
        crate::script::CHAM,                   // 66
        crate::script::UNKNOWN,                // 67
        crate::script::CYRILLIC,               // 68 (Cyrs)
        crate::script::UNKNOWN,                // 69
        crate::script::UNKNOWN,                // 70
        crate::script::EGYPTIAN_HIEROGLYPHS,   // 71
        crate::script::GEORGIAN,               // 72 (Geok)
        crate::script::HAN,                    // 73 (Hans)
        crate::script::HAN,                    // 74 (Hant)
        crate::script::PAHAWH_HMONG,           // 75
        crate::script::OLD_HUNGARIAN,          // 76
        crate::script::UNKNOWN,                // 77
        crate::script::JAVANESE,               // 78
        crate::script::KAYAH_LI,               // 79
        crate::script::LATIN,                  // 80 (Latf)
        crate::script::LATIN,                  // 81 (Latg)
        crate::script::LEPCHA,                 // 82
        crate::script::LINEAR_A,               // 83
        crate::script::MANDAIC,                // 84
        crate::script::UNKNOWN,                // 85
        crate::script::MEROITIC_HIEROGLYPHS,   // 86
        crate::script::NKO,                    // 87
        crate::script::OLD_TURKIC,             // 88
        crate::script::OLD_PERMIC,             // 89
        crate::script::PHAGS_PA,               // 90
        crate::script::PHOENICIAN,             // 91
        crate::script::MIAO,                   // 92
        crate::script::UNKNOWN,                // 93
        crate::script::UNKNOWN,                // 94
        crate::script::SYRIAC,                 // 95 (Syre)
        crate::script::SYRIAC,                 // 96 (Syrj)
        crate::script::SYRIAC,                 // 97 (Syrn)
        crate::script::UNKNOWN,                // 98
        crate::script::VAI,                    // 99
        crate::script::UNKNOWN,                // 100
        crate::script::CUNEIFORM,              // 101
        crate::script::UNKNOWN,                // 102
        crate::script::UNKNOWN,                // 103
        crate::script::CARIAN,                 // 104
        crate::script::UNKNOWN,                // 105
        crate::script::TAI_THAM,               // 106
        crate::script::LYCIAN,                 // 107
        crate::script::LYDIAN,                 // 108
        crate::script::OL_CHIKI,               // 109
        crate::script::REJANG,                 // 110
        crate::script::SAURASHTRA,             // 111
        crate::script::SIGNWRITING,            // 112
        crate::script::SUNDANESE,              // 113
        crate::script::UNKNOWN,                // 114
        crate::script::MEETEI_MAYEK,           // 115
        crate::script::IMPERIAL_ARAMAIC,       // 116
        crate::script::AVESTAN,                // 117
        crate::script::CHAKMA,                 // 118
        crate::script::UNKNOWN,                // 119
        crate::script::KAITHI,                 // 120
        crate::script::MANICHAEAN,             // 121
        crate::script::INSCRIPTIONAL_PAHLAVI,  // 122
        crate::script::PSALTER_PAHLAVI,        // 123
        crate::script::UNKNOWN,                // 124
        crate::script::INSCRIPTIONAL_PARTHIAN, // 125
        crate::script::SAMARITAN,              // 126
        crate::script::TAI_VIET,               // 127
        crate::script::UNKNOWN,                // 128
        crate::script::UNKNOWN,                // 129
        crate::script::BAMUM,                  // 130
        crate::script::LISU,                   // 131
        crate::script::UNKNOWN,                // 132
        crate::script::OLD_SOUTH_ARABIAN,      // 133
        crate::script::BASSA_VAH,              // 134
        crate::script::DUPLOYAN,               // 135
        crate::script::ELBASAN,                // 136
        crate::script::GRANTHA,                // 137
        crate::script::UNKNOWN,                // 138
        crate::script::UNKNOWN,                // 139
        crate::script::MENDE_KIKAKUI,          // 140
        crate::script::MEROITIC_CURSIVE,       // 141
        crate::script::OLD_NORTH_ARABIAN,      // 142
        crate::script::NABATAEAN,              // 143
        crate::script::PALMYRENE,              // 144
        crate::script::KHUDAWADI,              // 145
        crate::script::WARANG_CITI,            // 146
        crate::script::UNKNOWN,                // 147
        crate::script::UNKNOWN,                // 148
        crate::script::MRO,                    // 149
        crate::script::NUSHU,                  // 150
        crate::script::SHARADA,                // 151
        crate::script::SORA_SOMPENG,           // 152
        crate::script::TAKRI,                  // 153
        crate::script::TANGUT,                 // 154
        crate::script::UNKNOWN,                // 155
        crate::script::ANATOLIAN_HIEROGLYPHS,  // 156
        crate::script::KHOJKI,                 // 157
        crate::script::TIRHUTA,                // 158
        crate::script::CAUCASIAN_ALBANIAN,     // 159
        crate::script::MAHAJANI,               // 160
        crate::script::AHOM,                   // 161
        crate::script::HATRAN,                 // 162
        crate::script::MODI,                   // 163
        crate::script::MULTANI,                // 164
        crate::script::PAU_CIN_HAU,            // 165
        crate::script::SIDDHAM,                // 166
        crate::script::ADLAM,                  // 167
        crate::script::BHAIKSUKI,              // 168
        crate::script::MARCHEN,                // 169
        crate::script::NEWA,                   // 170
        crate::script::OSAGE,                  // 171
        crate::script::UNKNOWN,                // 172
        crate::script::HANGUL,                 // 173 (Jamo)
        crate::script::UNKNOWN,                // 174
        crate::script::MASARAM_GONDI,          // 175
        crate::script::SOYOMBO,                // 176
        crate::script::ZANABAZAR_SQUARE,       // 177
        crate::script::DOGRA,                  // 178
        crate::script::GUNJALA_GONDI,          // 179
        crate::script::MAKASAR,                // 180
        crate::script::MEDEFAIDRIN,            // 181
        crate::script::HANIFI_ROHINGYA,        // 182
        crate::script::SOGDIAN,                // 183
        crate::script::OLD_SOGDIAN,            // 184
        crate::script::ELYMAIC,                // 185
        crate::script::NYIAKENG_PUACHUE_HMONG, // 186
        crate::script::NANDINAGARI,            // 187
        crate::script::WANCHO,                 // 188
        crate::script::CHORASMIAN,             // 189
        crate::script::DIVES_AKURU,            // 190
        crate::script::KHITAN_SMALL_SCRIPT,    // 191
        crate::script::YEZIDI,                 // 192
        crate::script::CYPRO_MINOAN,           // 193
        crate::script::OLD_UYGHUR,             // 194
        crate::script::TANGSA,                 // 195
        crate::script::TOTO,                   // 196
        crate::script::VITHKUQI,               // 197
        crate::script::KAWI,                   // 198
        crate::script::NAG_MUNDARI,            // 199
        crate::script::ARABIC,                 // 200
        crate::script::GARAY,                  // 201
        crate::script::GURUNG_KHEMA,           // 202
        crate::script::KIRAT_RAI,              // 203
        crate::script::OL_ONAL,                // 204
        crate::script::SUNUWAR,                // 205
        crate::script::TODHRI,                 // 206
        crate::script::TULU_TIGALARI,          // 207
        crate::script::BERIA_ERFE,             // 208
        crate::script::SIDETIC,                // 209
        crate::script::TAI_YO,                 // 210
        crate::script::TOLONG_SIKI,            // 211
    ];
}

#[cfg(not(feature = "icu"))]
#[expect(clippy::manual_range_contains)]
mod builtin {
    use super::{super::algs::*, ucd_table::ucd::*, Codepoint, GeneralCategory, Script};

    pub(crate) fn script_for(c: u32) -> Script {
        _hb_ucd_sc_map[_hb_ucd_sc(c as usize) as usize]
    }

    pub(crate) fn general_category_for(c: u32) -> GeneralCategory {
        GeneralCategory(_hb_ucd_gc(c as usize))
    }

    pub(crate) fn combining_class_for(c: u32) -> u8 {
        _hb_ucd_ccc(c as usize)
    }

    pub(crate) fn mirroring_for(c: u32) -> Option<Codepoint> {
        let delta = _hb_ucd_bmg(c as usize);
        if delta == 0 {
            None
        } else {
            Some(((c as i32).wrapping_add(delta as i32)) as u32)
        }
    }

    pub(crate) fn compose(a: Codepoint, b: Codepoint) -> Option<Codepoint> {
        // Hangul is handled algorithmically.
        if let Some(ab) = compose_hangul(a, b) {
            return Some(ab);
        }

        let u: u32;

        if (a & 0xFFFF_F800) == 0x0000 && (b & 0xFFFF_FF80) == 0x0300 {
            /* If "a" is small enough and "b" is in the U+0300 range,
             * the composition data is encoded in a 32bit array sorted
             * by "a,b" pair. */
            let k = HB_CODEPOINT_ENCODE3_11_7_14(a, b, 0);
            let v = _hb_ucd_dm2_u32_map
                .binary_search_by(|probe| {
                    let key = probe & HB_CODEPOINT_ENCODE3_11_7_14(0x001F_FFFF, 0x001F_FFFF, 0);
                    key.cmp(&k)
                })
                .ok()
                .map(|index| _hb_ucd_dm2_u32_map[index]);

            if let Some(value) = v {
                u = HB_CODEPOINT_DECODE3_11_7_14_3(value);
            } else {
                return None;
            }
        } else {
            /* Otherwise it is stored in a 64bit array sorted by
             * "a,b" pair. */
            let k = HB_CODEPOINT_ENCODE3(a, b, 0);
            let v = _hb_ucd_dm2_u64_map
                .binary_search_by(|probe| {
                    let key = probe & HB_CODEPOINT_ENCODE3(0x001F_FFFF, 0x001F_FFFF, 0);
                    key.cmp(&k)
                })
                .ok()
                .map(|index| _hb_ucd_dm2_u64_map[index]);

            if let Some(value) = v {
                u = HB_CODEPOINT_DECODE3_3(value);
            } else {
                return None;
            }
        }

        if u == 0 {
            None
        } else {
            Some(u)
        }
    }

    pub(crate) fn decompose(ab: Codepoint) -> Option<(Codepoint, Codepoint)> {
        if let Some((a, b)) = decompose_hangul(ab) {
            return Some((a, b));
        }

        let mut i = _hb_ucd_dm(ab as usize) as usize;

        // If no data, there's no decomposition.
        if i == 0 {
            return None;
        }
        i -= 1;

        if i < _hb_ucd_dm1_p0_map.len() + _hb_ucd_dm1_p2_map.len() {
            let a = if i < _hb_ucd_dm1_p0_map.len() {
                _hb_ucd_dm1_p0_map[i] as u32
            } else {
                let j = i - _hb_ucd_dm1_p0_map.len();
                0x20000 | _hb_ucd_dm1_p2_map[j] as u32
            };
            return Some((a, 0));
        }

        i -= _hb_ucd_dm1_p0_map.len() + _hb_ucd_dm1_p2_map.len();

        if i < _hb_ucd_dm2_u32_map.len() {
            let v = _hb_ucd_dm2_u32_map[i];
            let a = HB_CODEPOINT_DECODE3_11_7_14_1(v);
            let b = HB_CODEPOINT_DECODE3_11_7_14_2(v);
            return Some((a, b));
        }

        i -= _hb_ucd_dm2_u32_map.len();

        let v = _hb_ucd_dm2_u64_map[i];
        let a = HB_CODEPOINT_DECODE3_1(v);
        let b = HB_CODEPOINT_DECODE3_2(v);
        Some((a, b))
    }

    const S_BASE: u32 = 0xAC00;
    const L_BASE: u32 = 0x1100;
    const V_BASE: u32 = 0x1161;
    const T_BASE: u32 = 0x11A7;
    const L_COUNT: u32 = 19;
    const V_COUNT: u32 = 21;
    const T_COUNT: u32 = 28;
    const N_COUNT: u32 = V_COUNT * T_COUNT;
    const S_COUNT: u32 = L_COUNT * N_COUNT;

    fn compose_hangul(a: Codepoint, b: Codepoint) -> Option<Codepoint> {
        let l = a;
        let v = b;
        if L_BASE <= l && l < (L_BASE + L_COUNT) && V_BASE <= v && v < (V_BASE + V_COUNT) {
            let r = S_BASE + (l - L_BASE) * N_COUNT + (v - V_BASE) * T_COUNT;
            Some(r)
        } else if S_BASE <= l
            && l <= (S_BASE + S_COUNT - T_COUNT)
            && T_BASE <= v
            && v < (T_BASE + T_COUNT)
            && (l - S_BASE) % T_COUNT == 0
        {
            let r = l + (v - T_BASE);
            Some(r)
        } else {
            None
        }
    }

    fn decompose_hangul(ab: Codepoint) -> Option<(Codepoint, Codepoint)> {
        let si = ab.wrapping_sub(S_BASE);
        if si >= S_COUNT {
            return None;
        }
        let (a, b) = if si % T_COUNT != 0 {
            // LV,T
            (S_BASE + (si / T_COUNT) * T_COUNT, T_BASE + (si % T_COUNT))
        } else {
            // L,V
            (L_BASE + (si / N_COUNT), V_BASE + (si % N_COUNT) / T_COUNT)
        };
        Some((a, b))
    }
}

#[cfg(test)]
mod tests {
    #[test]
    fn general_category_ranges() {
        use super::GeneralCategory as Gc;
        assert!(!Gc::CONTROL.in_range_inclusive(Gc::FORMAT, Gc::NON_SPACING_MARK));
        assert!(Gc::LOWERCASE_LETTER.in_range_inclusive(Gc::LOWERCASE_LETTER, Gc::UPPERCASE_LETTER));
        assert!(Gc::UPPERCASE_LETTER.in_range_inclusive(Gc::LOWERCASE_LETTER, Gc::UPPERCASE_LETTER));
        assert!(!Gc::SPACING_MARK.in_range_inclusive(Gc::LOWERCASE_LETTER, Gc::UPPERCASE_LETTER));
    }

    #[cfg(feature = "icu")]
    #[test]
    fn script_mapping() {
        use icu_properties::props::Script as IcuScript;
        use icu_properties::PropertyNamesShort;
        // These ICU script codes are aliases, placeholders, or meta scripts.
        // No Unicode code points map directly to them via the Script property,
        // so our direct-script table intentionally keeps them as UNKNOWN.
        const ALLOWED_UNKNOWN_SCRIPT_VALUES: &[usize] = &[
            54, 64, 67, 69, 70, 77, 85, 93, 94, 98, 100, 102, 105, 114, 119, 124, 128, 129, 132,
            138, 139, 147, 148, 155, 172, 174,
        ];
        let short_names = PropertyNamesShort::<IcuScript>::new();
        let mut mismatches = Vec::new();
        for (icu4c_value, script) in super::icu::SCRIPT_MAP.iter().enumerate() {
            let icu_script = IcuScript::from_icu4c_value(icu4c_value as u16);
            match short_names.get(icu_script) {
                Some(short_name) => {
                    let expected = if short_name.len() == 4 {
                        let bytes = short_name.as_bytes();
                        let tag =
                            crate::Tag::from_be_bytes([bytes[0], bytes[1], bytes[2], bytes[3]]);
                        crate::Script::from_iso15924_tag(tag).unwrap_or(crate::script::UNKNOWN)
                    } else {
                        crate::script::UNKNOWN
                    };
                    if *script == crate::script::UNKNOWN
                        && ALLOWED_UNKNOWN_SCRIPT_VALUES.contains(&icu4c_value)
                    {
                        continue;
                    }
                    if expected != *script {
                        let expected_tag = expected.tag().to_be_bytes();
                        let actual_tag = script.tag().to_be_bytes();
                        mismatches.push(format!(
                            "value {icu4c_value}: Script::from_iso15924_tag({short_name}) -> {}, mapped tag {}",
                            core::str::from_utf8(&expected_tag).unwrap_or("????"),
                            core::str::from_utf8(&actual_tag).unwrap_or("????")
                        ));
                    }
                }
                None => {
                    if *script != crate::script::UNKNOWN {
                        mismatches.push(format!(
                            "value {icu4c_value}: no ICU short name but mapped tag {}",
                            core::str::from_utf8(&script.tag().to_be_bytes()).unwrap_or("????")
                        ));
                    }
                }
            }
        }
        assert!(
            mismatches.is_empty(),
            "script mapping mismatches:\n{}",
            mismatches.join("\n")
        );
    }
}

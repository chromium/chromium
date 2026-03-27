//! PostScript encodings.
//!
//! This maps font specific character codes to string ids.
//!
//! See "Glyph Organization" at <https://adobe-type-tools.github.io/font-tech-notes/pdfs/5176.CFF.pdf#page=18>
//! for an explanation of how charsets, encodings and glyphs are related.

/// See "Standard" encoding at <https://adobe-type-tools.github.io/font-tech-notes/pdfs/5176.CFF.pdf#page=37>
/// for this particular mapping.
#[rustfmt::skip]
pub(super) static STANDARD_ENCODING: [u8; 256] = [
      0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
      0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
      1,   2,   3,   4,   5,   6,   7,   8,   9,  10,  11,  12,  13,  14,  15,  16,
     17,  18,  19,  20,  21,  22,  23,  24,  25,  26,  27,  28,  29,  30,  31,  32,
     33,  34,  35,  36,  37,  38,  39,  40,  41,  42,  43,  44,  45,  46,  47,  48,
     49,  50,  51,  52,  53,  54,  55,  56,  57,  58,  59,  60,  61,  62,  63,  64,
     65,  66,  67,  68,  69,  70,  71,  72,  73,  74,  75,  76,  77,  78,  79,  80,
     81,  82,  83,  84,  85,  86,  87,  88,  89,  90,  91,  92,  93,  94,  95,   0,
      0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
      0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
      0,  96,  97,  98,  99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110,
      0, 111, 112, 113, 114,   0, 115, 116, 117, 118, 119, 120, 121, 122,   0, 123,
      0, 124, 125, 126, 127, 128, 129, 130, 131,   0, 132, 133,   0, 134, 135, 136,
    137,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
      0, 138,   0, 139,   0,   0,   0,   0, 140, 141, 142, 143,   0,   0,   0,   0,
      0, 144,   0,   0,   0, 145,   0,   0, 146, 147, 148, 149,   0,   0,   0,   0,
];

/// See "Expert" encoding at <https://adobe-type-tools.github.io/font-tech-notes/pdfs/5176.CFF.pdf#page=40>
/// for this particular mapping.
#[rustfmt::skip]
pub(super) static EXPERT_ENCODING: [u16; 256] = [
      0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, 
      1, 229, 230,   0, 231, 232, 233, 234, 235, 236, 237, 238,  13,  14,  15,  99, 
    239, 240, 241, 242, 243, 244, 245, 246, 247, 248,  27,  28, 249, 250, 251, 252, 
      0, 253, 254, 255, 256, 257,   0,   0,   0, 258,   0,   0, 259, 260, 261, 262, 
      0,   0, 263, 264, 265,   0, 266, 109, 110, 267, 268, 269,   0, 270, 271, 272, 
    273, 274, 275, 276, 277, 278, 279, 280, 281, 282, 283, 284, 285, 286, 287, 288, 
    289, 290, 291, 292, 293, 294, 295, 296, 297, 298, 299, 300, 301, 302, 303,   0, 
      0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, 
      0, 304, 305, 306,   0,   0, 307, 308, 309, 310, 311,   0, 312,   0,   0, 313, 
      0,   0, 314, 315,   0,   0, 316, 317, 318,   0,   0,   0, 158, 155, 163, 319, 
    320, 321, 322, 323, 324, 325,   0,   0, 326, 150, 164, 169, 327, 328, 329, 330, 
    331, 332, 333, 334, 335, 336, 337, 338, 339, 340, 341, 342, 343, 344, 345, 346, 
    347, 348, 349, 350, 351, 352, 353, 354, 355, 356, 357, 358, 359, 360, 361, 362, 
    363, 364, 365, 366, 367, 368, 369, 370, 371, 372, 373, 374, 375, 376, 377, 378, 
];

/// Maps ISO Latin-1 byte codes (0-255) to Adobe CFF Standard String IDs (SIDs).
/// SIDs are based on the Adobe CFF Specification, Appendix A.
/// 
/// Note that U+00A0 (non breaking space) and U+00AD (soft hyphen) do not have
/// corresponding SIDs. These are mapped here to space and hyphen, respectively,
/// but are special cased to return correct values in the public API below.
/// 
/// See <https://adobe-type-tools.github.io/font-tech-notes/pdfs/5176.CFF.pdf>
#[rustfmt::skip]
pub(super) static ISO_LATIN1_ENCODING: [u16; 256] = [
      0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
      0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
      1,   2,   3,   4,   5,   6,   7, 104,   9,  10,  11,  12,  13,  14,  15,  16,
     17,  18,  19,  20,  21,  22,  23,  24,  25,  26,  27,  28,  29,  30,  31,  32,
     33,  34,  35,  36,  37,  38,  39,  40,  41,  42,  43,  44,  45,  46,  47,  48,
     49,  50,  51,  52,  53,  54,  55,  56,  57,  58,  59,  60,  61,  62,  63,  64,
     65,  66,  67,  68,  69,  70,  71,  72,  73,  74,  75,  76,  77,  78,  79,  80,
     81,  82,  83,  84,  85,  86,  87,  88,  89,  90,  91,  92,  93,  94,  95,   0,
      0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
      0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
      1, 161, 162, 163, 164, 165, 166, 167, 168, 169, 170, 171, 172,  14, 173, 174,
    175, 176, 177, 178, 179, 180, 181, 182, 183, 184, 185, 186, 187, 188, 189, 190,
    191, 192, 193, 194, 195, 196, 197, 198, 199, 200, 201, 202, 203, 204, 205, 206,
    207, 208, 209, 210, 211, 212, 213, 214, 215, 216, 217, 218, 219, 220, 221, 222,
    223, 224, 225, 226, 227, 228, 229, 230, 231, 232, 233, 234, 235, 236, 237, 238,
    239, 240, 241, 242, 243, 244, 245, 246, 247, 248, 249, 250, 251, 252, 253, 254,
];

/// Predefined encodings for Adobe CFF and Type1 fonts.
///
/// Encodings map character codes to glyph names.
///
/// See <https://adobe-type-tools.github.io/font-tech-notes/pdfs/5176.CFF.pdf#page=37>.
#[derive(Copy, Clone, PartialEq, Eq, Default, Debug)]
pub enum PredefinedEncoding {
    #[default]
    Standard,
    Expert,
    IsoLatin1,
}

impl PredefinedEncoding {
    /// Converts a character code to the associated glyph name according to
    /// the selected encoding.
    pub fn code_to_glyph_name(&self, code: u8) -> &'static str {
        let code = code as usize;
        // All arrays have 256 entries so code is guaranteed to be in bounds
        let sid = match self {
            Self::Standard => STANDARD_ENCODING[code] as u16,
            Self::Expert => EXPERT_ENCODING[code],
            Self::IsoLatin1 => {
                // The standard string set is missing names for non breaking
                // space and soft hyphen so catch these here and return names
                // from the Adobe Glyph List.
                //
                // nonbreakingspace;00A0
                // softhyphen;00AD
                //
                // See <https://github.com/adobe-type-tools/agl-aglfn/blob/4036a9ca80a62f64f9de4f7321a9a045ad0ecfd6/glyphlist.txt>
                match code {
                    0x00A0 => return "nonbreakingspace",
                    0x00AD => return "softhyphen",
                    _ => ISO_LATIN1_ENCODING[code],
                }
            }
        };
        super::STANDARD_STRINGS
            .get(sid as usize)
            .copied()
            .unwrap_or_default()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn standard_encoding_names() {
        let pairs = [
            (0, ".notdef"),
            (32, "space"),
            (33, "exclam"),
            (34, "quotedbl"),
            (35, "numbersign"),
            (42, "asterisk"),
            (43, "plus"),
            (44, "comma"),
            (45, "hyphen"),
            (46, "period"),
            (47, "slash"),
            (48, "zero"),
            (49, "one"),
            (57, "nine"),
            (58, "colon"),
            (61, "equal"),
            (62, "greater"),
            (65, "A"),
            (77, "M"),
            (90, "Z"),
            (95, "underscore"),
            (96, "quoteleft"),
            (97, "a"),
            (109, "m"),
            (122, "z"),
            (164, "fraction"),
            (165, "yen"),
            (166, "florin"),
            (174, "fi"),
            (175, "fl"),
            (188, "ellipsis"),
            (207, "caron"),
            (208, "emdash"),
            (225, "AE"),
            (255, ".notdef"),
        ];
        check_names(&pairs, PredefinedEncoding::Standard);
    }

    #[test]
    fn expert_encoding_names() {
        let pairs = [
            (0, ".notdef"),
            (32, "space"),
            (44, "comma"),
            (45, "hyphen"),
            (46, "period"),
            (47, "fraction"),
            (48, "zerooldstyle"),
            (57, "nineoldstyle"),
            (58, "colon"),
            (59, "semicolon"),
            (60, "commasuperior"),
            (61, "threequartersemdash"),
            (62, "periodsuperior"),
            (63, "questionsmall"),
            (65, "asuperior"),
            (84, "tsuperior"),
            (86, "ff"),
            (87, "fi"),
            (88, "fl"),
            (89, "ffi"),
            (90, "ffl"),
            (91, "parenleftinferior"),
            (96, "Gravesmall"),
            (97, "Asmall"),
            (109, "Msmall"),
            (122, "Zsmall"),
            (123, "colonmonetary"),
            (124, "onefitted"),
            (125, "rupiah"),
            (126, "Tildesmall"),
            (188, "onequarter"),
            (200, "zerosuperior"),
            (201, "onesuperior"),
            (219, "nineinferior"),
            (220, "centinferior"),
            (221, "dollarinferior"),
            (222, "periodinferior"),
            (223, "commainferior"),
            (224, "Agravesmall"),
            (225, "Aacutesmall"),
            (226, "Acircumflexsmall"),
            (227, "Atildesmall"),
            (255, "Ydieresissmall"),
        ];
        check_names(&pairs, PredefinedEncoding::Expert);
    }

    #[test]
    fn iso_latin1_encoding_names() {
        let pairs = [
            (0, ".notdef"),
            (32, "space"),
            (42, "asterisk"),
            (43, "plus"),
            (44, "comma"),
            (46, "period"),
            (48, "zero"),
            (49, "one"),
            (57, "nine"),
            (58, "colon"),
            (62, "greater"),
            (63, "question"),
            (64, "at"),
            (65, "A"),
            (77, "M"),
            (90, "Z"),
            (95, "underscore"),
            (97, "a"),
            (109, "m"),
            (122, "z"),
            (123, "braceleft"),
            (124, "bar"),
            (125, "braceright"),
            (126, "asciitilde"),
            (160, "nonbreakingspace"),
            (166, "minus"),
            (173, "softhyphen"),
            (187, "Ntilde"),
            (205, "aring"),
            (226, "ugrave"),
            (238, "twodotenleader"),
            (239, "onedotenleader"),
            (240, "zerooldstyle"),
            (249, "nineoldstyle"),
            (255, "bsuperior"),
        ];
        check_names(&pairs, PredefinedEncoding::IsoLatin1);
    }

    #[track_caller]
    fn check_names(pairs: &[(u8, &str)], encoding: PredefinedEncoding) {
        for (code, expected_name) in pairs.iter().copied() {
            let name = encoding.code_to_glyph_name(code);
            assert_eq!(
                name, expected_name,
                "expected {expected_name}, got {name} for {code}"
            );
        }
    }
}

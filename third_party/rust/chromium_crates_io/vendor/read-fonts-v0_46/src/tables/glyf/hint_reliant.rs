//! Detection of fonts that need their TrueType instructions to draw correct
//! contours.
//!
//! These are often called "tricky" fonts. They are recognized by family
//! name or by the checksums of the `cvt`, `fpgm` and `prep` tables. The
//! lists of names and checksums come from FreeType.

use crate::{
    tables::{name::Name, os2::SelectionFlags},
    types::{BigEndian, NameId},
    TableProvider,
};

/// Returns `true` if the font needs its instructions to draw correct
/// contours.
///
/// Only fonts with TrueType outlines qualify, so a font with neither a
/// `glyf` nor a `GLYF` table returns `false`.
///
/// This reads the family name and the hinting tables. A caller that checks
/// more than once should cache the result, as
/// [`Font::is_hint_reliant`](crate::model::Font::is_hint_reliant) does.
pub fn is_hint_reliant<'a>(font: &impl TableProvider<'a>) -> bool {
    if font.glyf().is_err() && font.glyf_extended().is_err() {
        return false;
    }
    is_hint_reliant_name(font) || matches_hint_reliant_id_list(FontId::from_font(font))
}

/// Returns `true` if the family name that FreeType reports for the font is
/// on the list.
fn is_hint_reliant_name<'a>(font: &impl TableProvider<'a>) -> bool {
    let Ok(name) = font.name() else {
        return false;
    };
    // With the OS/2 WWS bit set the typographic family comes first, and
    // without it the WWS family does.
    //
    // <https://gitlab.freedesktop.org/freetype/freetype/-/blob/80a507a6b8e3d2906ad2c8ba69329bd2fb2a85ef/src/sfnt/sfobjs.c#L1045>
    let wws = font
        .os2()
        .is_ok_and(|os2| os2.fs_selection().contains(SelectionFlags::WWS));
    let ids: &[NameId] = if wws {
        &[NameId::TYPOGRAPHIC_FAMILY_NAME, NameId::FAMILY_NAME]
    } else {
        &[
            NameId::WWS_FAMILY_NAME,
            NameId::TYPOGRAPHIC_FAMILY_NAME,
            NameId::FAMILY_NAME,
        ]
    };
    ids.iter()
        .find_map(|id| AsciiName::read(&name, *id))
        .is_some_and(|family| matches_hint_reliant_name_list(family.bytes()))
}

/// A name decoded to ASCII the same way as FreeType.
///
/// <https://gitlab.freedesktop.org/freetype/freetype/-/blob/80a507a6b8e3d2906ad2c8ba69329bd2fb2a85ef/src/sfnt/sfobjs.c#L136>
struct AsciiName<'a> {
    data: &'a [u8],
    utf16: bool,
}

impl<'a> AsciiName<'a> {
    /// Reads the record that FreeType chooses for `id`.
    ///
    /// A Windows record is used if it is English or if there is no Macintosh
    /// record. Otherwise, a Macintosh English or Roman record is used. A
    /// Unicode record is used if there is neither.
    fn read(name: &Name<'a>, id: NameId) -> Option<Self> {
        let (mut apple_english, mut apple_roman, mut windows, mut unicode) =
            (None, None, None, None);
        let mut windows_is_english = false;
        let records = name.name_record().iter();
        for record in records.filter(|record| record.name_id() == id && record.length() > 0) {
            match record.platform_id() {
                0 | 2 => unicode = Some(record),
                1 if record.language_id() == 0 => apple_english = Some(record),
                1 if record.encoding_id() == 0 => apple_roman = Some(record),
                3 => {
                    let english = record.language_id() & 0x3ff == 0x009;
                    if (windows.is_none() || english) && matches!(record.encoding_id(), 0 | 1 | 10)
                    {
                        windows = Some(record);
                        windows_is_english = english;
                    }
                }
                _ => {}
            }
        }
        let apple = apple_english.or(apple_roman);
        let (record, utf16) = match (windows, apple) {
            (Some(windows), _) if apple.is_none() || windows_is_english => (windows, true),
            (_, Some(apple)) => (apple, false),
            _ => (unicode?, true),
        };
        let start = record.string_offset().to_u32() as usize;
        let data = name
            .string_data()
            .as_bytes()
            .get(start..start + record.length() as usize)?;
        Some(Self { data, utf16 })
    }

    /// Returns the name's characters up to the first NUL, with `?` in place
    /// of each character outside the range 32 to 127.
    fn bytes(&self) -> impl Iterator<Item = u8> + '_ {
        let step = if self.utf16 { 2 } else { 1 };
        self.data
            .chunks_exact(step)
            .map(|unit| {
                unit.iter()
                    .fold(0u32, |code, byte| code << 8 | *byte as u32)
            })
            .take_while(|code| *code != 0)
            .map(|code| {
                if (32..=127).contains(&code) {
                    code as u8
                } else {
                    b'?'
                }
            })
    }
}

/// Returns `true` if `name` contains a family name that FreeType knows to
/// rely on hinting.
///
/// The random tag of a font embedded in a PDF is skipped. Matching follows
/// FreeType's `strstr` search.
///
/// <https://gitlab.freedesktop.org/freetype/freetype/-/blob/80a507a6b8e3d2906ad2c8ba69329bd2fb2a85ef/src/truetype/ttobjs.c#L153>
fn matches_hint_reliant_name_list(mut name: impl Iterator<Item = u8>) -> bool {
    let mut head = [0u8; 8];
    let mut head_len = 0;
    for byte in name.by_ref().take(head.len()) {
        head[head_len] = byte;
        head_len += 1;
    }
    // The tag is six capitals and a plus, in a name of at least eight.
    let tagged =
        head_len == head.len() && head[6] == b'+' && head[..6].iter().all(u8::is_ascii_uppercase);
    let skip = if tagged { 7 } else { 0 };
    // The trailing bytes, enough to hold the longest name on the list.
    let mut window = [0u8; MAX_HINT_RELIANT_NAME_LEN];
    let mut seen = 0;
    head[skip..head_len]
        .iter()
        .copied()
        .chain(name)
        .any(|byte| {
            window.copy_within(1.., 0);
            window[MAX_HINT_RELIANT_NAME_LEN - 1] = byte;
            seen += 1;
            HINT_RELIANT_NAMES
                .iter()
                .any(|tricky| tricky.len() <= seen && window.ends_with(tricky.as_bytes()))
        })
}

/// <https://gitlab.freedesktop.org/freetype/freetype/-/blob/80a507a6b8e3d2906ad2c8ba69329bd2fb2a85ef/src/truetype/ttobjs.c#L180>
#[rustfmt::skip]
const HINT_RELIANT_NAMES: &[&str] = &[
    "cpop",               /* dftt-p7.ttf; version 1.00, 1992 [DLJGyShoMedium] */
    "DFGirl-W6-WIN-BF",   /* dftt-h6.ttf; version 1.00, 1993 */
    "DFGothic-EB",        /* DynaLab Inc. 1992-1995 */
    "DFGyoSho-Lt",        /* DynaLab Inc. 1992-1995 */
    "DFHei",              /* DynaLab Inc. 1992-1995 [DFHei-Bd-WIN-HK-BF] */
                          /* covers "DFHei-Md-HK-BF", maybe DynaLab Inc. */

    "DFHSGothic-W5",      /* DynaLab Inc. 1992-1995 */
    "DFHSMincho-W3",      /* DynaLab Inc. 1992-1995 */
    "DFHSMincho-W7",      /* DynaLab Inc. 1992-1995 */
    "DFKaiSho-SB",        /* dfkaisb.ttf */
    "DFKaiShu",           /* covers "DFKaiShu-Md-HK-BF", maybe DynaLab Inc. */
    "DFKai-SB",           /* kaiu.ttf; version 3.00, 1998 [DFKaiShu-SB-Estd-BF] */

    "DFMing",             /* DynaLab Inc. 1992-1995 [DFMing-Md-WIN-HK-BF] */
                          /* covers "DFMing-Bd-HK-BF", maybe DynaLab Inc. */

    "DLC",                /* dftt-m7.ttf; version 1.00, 1993 [DLCMingBold] */
                          /* dftt-f5.ttf; version 1.00, 1993 [DLCFongSung] */
                          /* covers following */
                          /* "DLCHayMedium", dftt-b5.ttf; version 1.00, 1993 */
                          /* "DLCHayBold",   dftt-b7.ttf; version 1.00, 1993 */
                          /* "DLCKaiMedium", dftt-k5.ttf; version 1.00, 1992 */
                          /* "DLCLiShu",     dftt-l5.ttf; version 1.00, 1992 */
                          /* "DLCRoundBold", dftt-r7.ttf; version 1.00, 1993 */

    "HuaTianKaiTi?",      /* htkt2.ttf */
    "HuaTianSongTi?",     /* htst3.ttf */
    "Ming(for ISO10646)", /* hkscsiic.ttf; version 0.12, 2007 [Ming] */
                          /* iicore.ttf; version 0.07, 2007 [Ming] */
    "MingLiU",            /* mingliu.ttf */
                          /* mingliu.ttc; version 3.21, 2001 */
    "MingMedium",         /* dftt-m5.ttf; version 1.00, 1993 [DLCMingMedium] */
    "PMingLiU",           /* mingliu.ttc; version 3.21, 2001 */
    "MingLi43",           /* mingli.ttf; version 1.00, 1992 */
];

const MAX_HINT_RELIANT_NAME_LEN: usize = 18;

#[derive(Copy, Clone, PartialEq, Default, Debug)]
struct TableId {
    checksum: u32,
    len: u32,
}

impl TableId {
    fn from_data(data: Option<&[u8]>) -> Self {
        let Some(data) = data else {
            return Self::default();
        };
        Self {
            // Note: FreeType always just computes the checksum
            // <https://gitlab.freedesktop.org/freetype/freetype/-/blob/80a507a6b8e3d2906ad2c8ba69329bd2fb2a85ef/src/truetype/ttobjs.c#L281>
            checksum: crate::tables::compute_checksum(data),
            len: data.len() as u32,
        }
    }

    /// Creates the identity of a control value table.
    ///
    /// The checksum sums the entries the same way that
    /// [`compute_checksum`](crate::tables::compute_checksum) sums bytes. Two
    /// big-endian entries form a word, and a trailing entry is padded.
    fn from_cvt(cvt: Option<&[BigEndian<i16>]>) -> Self {
        let Some(cvt) = cvt else {
            return Self::default();
        };
        let mut checksum = 0u32;
        for pair in cvt.chunks(2) {
            let high = pair[0].get() as u16 as u32;
            let low = pair.get(1).map_or(0, |value| value.get() as u16 as u32);
            checksum = checksum.wrapping_add((high << 16) | low);
        }
        Self {
            checksum,
            len: (cvt.len() * 2) as u32,
        }
    }
}

#[derive(Copy, Clone, PartialEq, Default, Debug)]
struct FontId {
    cvt: TableId,
    fpgm: TableId,
    prep: TableId,
}

impl FontId {
    fn from_font<'a>(font: &impl TableProvider<'a>) -> Self {
        Self {
            cvt: TableId::from_cvt(font.cvt().ok()),
            fpgm: TableId::from_data(font.fpgm().ok()),
            prep: TableId::from_data(font.prep().ok()),
        }
    }
}

/// Checks for fonts that require hinting based on the length and checksum of
/// the cvt, fpgm and prep tables.
///
/// Roughly equivalent to <https://gitlab.freedesktop.org/freetype/freetype/-/blob/80a507a6b8e3d2906ad2c8ba69329bd2fb2a85ef/src/truetype/ttobjs.c#L309>
fn matches_hint_reliant_id_list(font_id: FontId) -> bool {
    HINT_RELIANT_IDS.contains(&font_id)
}

/// <https://gitlab.freedesktop.org/freetype/freetype/-/blob/80a507a6b8e3d2906ad2c8ba69329bd2fb2a85ef/src/truetype/ttobjs.c#L314>
#[rustfmt::skip]
const HINT_RELIANT_IDS: &[FontId] = &[
    // MingLiU 1995
    FontId {
        cvt: TableId { checksum: 0x05BCF058, len: 0x000002E4 },
        fpgm: TableId { checksum: 0x28233BF1, len: 0x000087C4 },
        prep: TableId { checksum: 0xA344A1EA, len: 0x000001E1 },
    },
    // MingLiU 1996-
    FontId {
        cvt: TableId { checksum: 0x05BCF058, len: 0x000002E4 },
        fpgm: TableId { checksum: 0x28233BF1, len: 0x000087C4 },
        prep: TableId { checksum: 0xA344A1EB, len: 0x000001E1 },
    },
    // DFGothic-EB
    FontId {
        cvt: TableId { checksum: 0x12C3EBB2, len: 0x00000350 },
        fpgm: TableId { checksum: 0xB680EE64, len: 0x000087A7 },
        prep: TableId { checksum: 0xCE939563, len: 0x00000758 },
    },
    // DFGyoSho-Lt
    FontId {
        cvt: TableId { checksum: 0x11E5EAD4, len: 0x00000350 },
        fpgm: TableId { checksum: 0xCE5956E9, len: 0x0000BC85 },
        prep: TableId { checksum: 0x8272F416, len: 0x00000045 },
    },
    // DFHei-Md-HK-BF
    FontId {
        cvt: TableId { checksum: 0x1257EB46, len: 0x00000350 },
        fpgm: TableId { checksum: 0xF699D160, len: 0x0000715F },
        prep: TableId { checksum: 0xD222F568, len: 0x000003BC },
    },
    // DFHSGothic-W5
    FontId {
        cvt: TableId { checksum: 0x1262EB4E, len: 0x00000350 },
        fpgm: TableId { checksum: 0xE86A5D64, len: 0x00007940 },
        prep: TableId { checksum: 0x7850F729, len: 0x000005FF },
    },
    // DFHSMincho-W3
    FontId {
        cvt: TableId { checksum: 0x122DEB0A, len: 0x00000350 },
        fpgm: TableId { checksum: 0x3D16328A, len: 0x0000859B },
        prep: TableId { checksum: 0xA93FC33B, len: 0x000002CB },
    },
    // DFHSMincho-W7
    FontId {
        cvt: TableId { checksum: 0x125FEB26, len: 0x00000350 },
        fpgm: TableId { checksum: 0xA5ACC982, len: 0x00007EE1 },
        prep: TableId { checksum: 0x90999196, len: 0x0000041F },
    },
    // DFKaiShu
    FontId {
        cvt: TableId { checksum: 0x11E5EAD4, len: 0x00000350 },
        fpgm: TableId { checksum: 0x5A30CA3B, len: 0x00009063 },
        prep: TableId { checksum: 0x13A42602, len: 0x0000007E },
    },
    // DFKaiShu, variant
    FontId {
        cvt: TableId { checksum: 0x11E5EAD4, len: 0x00000350 },
        fpgm: TableId { checksum: 0xA6E78C01, len: 0x00008998 },
        prep: TableId { checksum: 0x13A42602, len: 0x0000007E },
    },
    // DFKaiShu-Md-HK-BF
    FontId {
        cvt: TableId { checksum: 0x11E5EAD4, len: 0x00000360 },
        fpgm: TableId { checksum: 0x9DB282B2, len: 0x0000C06E },
        prep: TableId { checksum: 0x53E6D7CA, len: 0x00000082 },
    },
    // DFMing-Bd-HK-BF
    FontId {
        cvt: TableId { checksum: 0x1243EB18, len: 0x00000350 },
        fpgm: TableId { checksum: 0xBA0A8C30, len: 0x000074AD },
        prep: TableId { checksum: 0xF3D83409, len: 0x0000037B },
    },
    // DLCLiShu
    FontId {
        cvt: TableId { checksum: 0x07DCF546, len: 0x00000308 },
        fpgm: TableId { checksum: 0x40FE7C90, len: 0x00008E2A },
        prep: TableId { checksum: 0x608174B5, len: 0x0000007A },
    },
    // DLCHayBold
    FontId {
        cvt: TableId { checksum: 0xEB891238, len: 0x00000308 },
        fpgm: TableId { checksum: 0xD2E4DCD4, len: 0x0000676F },
        prep: TableId { checksum: 0x8EA5F293, len: 0x000003B8 },
    },
    // HuaTianKaiTi
    FontId {
        cvt: TableId { checksum: 0xFFFBFFFC, len: 0x00000008 },
        fpgm: TableId { checksum: 0x9C9E48B8, len: 0x0000BEA2 },
        prep: TableId { checksum: 0x70020112, len: 0x00000008 },
    },
    // HuaTianSongTi
    FontId {
        cvt: TableId { checksum: 0xFFFBFFFC, len: 0x00000008 },
        fpgm: TableId { checksum: 0x0A5A0483, len: 0x00017C39 },
        prep: TableId { checksum: 0x70020112, len: 0x00000008 },
    },
    // NEC fadpop7.ttf
    FontId {
        cvt: TableId { checksum: 0x00000000, len: 0x00000000 },
        fpgm: TableId { checksum: 0x40C92555, len: 0x000000E5 },
        prep: TableId { checksum: 0xA39B58E3, len: 0x0000117C },
    },
    // NEC fadrei5.ttf
    FontId {
        cvt: TableId { checksum: 0x00000000, len: 0x00000000 },
        fpgm: TableId { checksum: 0x33C41652, len: 0x000000E5 },
        prep: TableId { checksum: 0x26D6C52A, len: 0x00000F6A },
    },
    // NEC fangot7.ttf
    FontId {
        cvt: TableId { checksum: 0x00000000, len: 0x00000000 },
        fpgm: TableId { checksum: 0x6DB1651D, len: 0x0000019D },
        prep: TableId { checksum: 0x6C6E4B03, len: 0x00002492 },
    },
    // NEC fangyo5.ttf
    FontId {
        cvt: TableId { checksum: 0x00000000, len: 0x00000000 },
        fpgm: TableId { checksum: 0x40C92555, len: 0x000000E5 },
        prep: TableId { checksum: 0xDE51FAD0, len: 0x0000117C },
    },
    // NEC fankyo5.ttf
    FontId {
        cvt: TableId { checksum: 0x00000000, len: 0x00000000 },
        fpgm: TableId { checksum: 0x85E47664, len: 0x000000E5 },
        prep: TableId { checksum: 0xA6C62831, len: 0x00001CAA },
    },
    // NEC fanrgo5.ttf
    FontId {
        cvt: TableId { checksum: 0x00000000, len: 0x00000000 },
        fpgm: TableId { checksum: 0x2D891CFD, len: 0x0000019D },
        prep: TableId { checksum: 0xA0604633, len: 0x00001DE8 },
    },
    // NEC fangot5.ttc
    FontId {
        cvt: TableId { checksum: 0x00000000, len: 0x00000000 },
        fpgm: TableId { checksum: 0x40AA774C, len: 0x000001CB },
        prep: TableId { checksum: 0x9B5CAA96, len: 0x00001F9A },
    },
    // NEC fanmin3.ttc
    FontId {
        cvt: TableId { checksum: 0x00000000, len: 0x00000000 },
        fpgm: TableId { checksum: 0x0D3DE9CB, len: 0x00000141 },
        prep: TableId { checksum: 0xD4127766, len: 0x00002280 },
    },
    // NEC FA-Gothic, 1996
    FontId {
        cvt: TableId { checksum: 0x00000000, len: 0x00000000 },
        fpgm: TableId { checksum: 0x4A692698, len: 0x000001F0 },
        prep: TableId { checksum: 0x340D4346, len: 0x00001FCA },
    },
    // NEC FA-Minchou, 1996
    FontId {
        cvt: TableId { checksum: 0x00000000, len: 0x00000000 },
        fpgm: TableId { checksum: 0xCD34C604, len: 0x00000166 },
        prep: TableId { checksum: 0x6CF31046, len: 0x000022B0 },
    },
    // NEC FA-RoundGothicB, 1996
    FontId {
        cvt: TableId { checksum: 0x00000000, len: 0x00000000 },
        fpgm: TableId { checksum: 0x5DA75315, len: 0x0000019D },
        prep: TableId { checksum: 0x40745A5F, len: 0x000022E0 },
    },
    // NEC FA-RoundGothicM, 1996
    FontId {
        cvt: TableId { checksum: 0x00000000, len: 0x00000000 },
        fpgm: TableId { checksum: 0xF055FC48, len: 0x000001C2 },
        prep: TableId { checksum: 0x3900DED3, len: 0x00001E18 },
    },
    // MINGLI.TTF, 1992
    FontId {
        cvt: TableId { checksum: 0x00170003, len: 0x00000060 },
        fpgm: TableId { checksum: 0xDBB4306E, len: 0x000058AA },
        prep: TableId { checksum: 0xD643482A, len: 0x00000035 },
    },
    // DFHei-Bd-WIN-HK-BF, issue #1087
    FontId {
        cvt: TableId { checksum: 0x1269EB58, len: 0x00000350 },
        fpgm: TableId { checksum: 0x5CD5957A, len: 0x00006A4E },
        prep: TableId { checksum: 0xF758323A, len: 0x00000380 },
    },
    // DFMing-Md-WIN-HK-BF, issue #1087
    FontId {
        cvt: TableId { checksum: 0x122FEB0B, len: 0x00000350 },
        fpgm: TableId { checksum: 0x7F10919A, len: 0x000070A9 },
        prep: TableId { checksum: 0x7CD7E7B7, len: 0x0000025C },
    },
];

#[cfg(test)]
mod tests {
    use super::*;
    use crate::{FontRef, TableProvider};

    // The identity of a cvt is summed from its entries, which has to agree
    // with the byte checksum FreeType takes.
    #[test]
    fn cvt_identity_matches_the_byte_checksum() {
        for data in [font_test_data::TINOS_SUBSET, font_test_data::CVAR] {
            let font = FontRef::new(data).unwrap();
            let cvt = font.cvt().unwrap();
            let bytes = font.data_for_tag(crate::types::Tag::new(b"cvt ")).unwrap();
            assert_eq!(
                TableId::from_cvt(Some(cvt)),
                TableId::from_data(Some(bytes.as_bytes()))
            );
        }
        // An odd entry count pads the last word.
        let cvt = [0x0102i16, 0x0304, 0x0506].map(BigEndian::from);
        assert_eq!(
            TableId::from_cvt(Some(&cvt)),
            TableId::from_data(Some(&[1, 2, 3, 4, 5, 6]))
        );
    }

    // The font model answers the same as a FontRef, through its own tables.
    #[test]
    fn font_model_agrees_with_font_ref() {
        for data in [font_test_data::TINOS_SUBSET, font_test_data::VAZIRMATN_VAR] {
            let font = FontRef::new(data).unwrap();
            let model = crate::model::Font::new(data.to_vec(), 0).unwrap();
            assert_eq!(model.is_hint_reliant(), is_hint_reliant(&font));
            assert_eq!(is_hint_reliant(&model.tables()), is_hint_reliant(&font));
        }
    }

    // A font with its outlines in GLYF and LOCA qualifies.
    #[test]
    fn uppercase_glyf_fonts_qualify() {
        let data = uppercase_glyf(font_test_data::TINOS_SUBSET);
        assert!(FontRef::new(&data).unwrap().glyf().is_err());
        assert!(with_names(&data, &[(3, 0x409, 1, "MingLiU")]));
    }

    /// `data` with its `glyf` and `loca` tables renamed `GLYF` and `LOCA`.
    fn uppercase_glyf(data: &[u8]) -> Vec<u8> {
        let count = u16::from_be_bytes([data[4], data[5]]) as usize;
        let mut records: Vec<[u8; 16]> = data[12..12 + 16 * count]
            .chunks_exact(16)
            .map(|record| record.try_into().unwrap())
            .collect();
        for record in &mut records {
            match &record[..4] {
                b"glyf" => record[..4].copy_from_slice(b"GLYF"),
                b"loca" => record[..4].copy_from_slice(b"LOCA"),
                _ => {}
            }
        }
        // The directory stays sorted by tag.
        records.sort();
        let mut font = data.to_vec();
        font[12..12 + 16 * count].copy_from_slice(&records.concat());
        font
    }

    /// Returns whether a font with Tinos's tables and a `name` table
    /// containing `records` is hint reliant.
    ///
    /// Each record is a platform, language, name id and string.
    fn hint_reliant_with_names(records: &[(u16, u16, u16, &str)]) -> bool {
        with_names(font_test_data::TINOS_SUBSET, records)
    }

    /// Whether `data`, with a name table holding `records`, is hint reliant.
    fn with_names(data: &[u8], records: &[(u16, u16, u16, &str)]) -> bool {
        let strings: Vec<Vec<u8>> = records
            .iter()
            .map(|(platform, _, _, value)| match platform {
                1 => value.bytes().collect(),
                _ => value.encode_utf16().flat_map(u16::to_be_bytes).collect(),
            })
            .collect();
        let count = records.len() as u16;
        let mut name = alloc::vec![0, 0];
        name.extend_from_slice(&count.to_be_bytes());
        name.extend_from_slice(&(6 + 12 * count).to_be_bytes());
        let mut offset = 0u16;
        for ((platform, language, id, _), string) in records.iter().zip(&strings) {
            let encoding = if *platform == 1 { 0u16 } else { 1 };
            for field in [
                *platform,
                encoding,
                *language,
                *id,
                string.len() as u16,
                offset,
            ] {
                name.extend_from_slice(&field.to_be_bytes());
            }
            offset += string.len() as u16;
        }
        strings
            .iter()
            .for_each(|string| name.extend_from_slice(string));
        is_hint_reliant(&WithName(FontRef::new(data).unwrap(), &name))
    }

    /// A font's tables with a replacement `name` table.
    struct WithName<'a>(FontRef<'a>, &'a [u8]);

    impl<'a> TableProvider<'a> for WithName<'a> {
        fn data_for_tag(&self, tag: crate::types::Tag) -> Option<crate::FontData<'a>> {
            if tag == crate::types::Tag::new(b"name") {
                return Some(crate::FontData::new(self.1));
            }
            self.0.data_for_tag(tag)
        }
    }

    // FreeType's family name: the whole string, its non-ASCII characters
    // read as `?`, from the record and name id FreeType picks.
    #[test]
    fn family_names_are_read_as_freetype_reads_them() {
        const FAMILY: u16 = 1;
        const TYPOGRAPHIC: u16 = 16;
        assert!(!hint_reliant_with_names(&[(3, 0x409, FAMILY, "Arial")]));
        assert!(hint_reliant_with_names(&[(3, 0x409, FAMILY, "MingLiU")]));
        // Longer than any name on the list.
        assert!(hint_reliant_with_names(&[(
            3,
            0x409,
            FAMILY,
            "ABCDEF+DFKaiShu-SB-Estd-BF"
        )]));
        // The list's "HuaTianKaiTi?" ends in a character FreeType could not
        // read as ASCII.
        assert!(hint_reliant_with_names(&[(
            3,
            0x409,
            FAMILY,
            "HuaTianKaiTi体"
        )]));
        assert!(!hint_reliant_with_names(&[(
            3,
            0x409,
            FAMILY,
            "HuaTianKaiTi"
        )]));
        // The typographic family wins over the family.
        assert!(!hint_reliant_with_names(&[
            (3, 0x409, FAMILY, "MingLiU"),
            (3, 0x409, TYPOGRAPHIC, "Arial"),
        ]));
        // A Macintosh English record wins over a non-English Windows one,
        assert!(!hint_reliant_with_names(&[
            (3, 0x404, FAMILY, "MingLiU"),
            (1, 0, FAMILY, "Arial"),
        ]));
        // and loses to an English one.
        assert!(hint_reliant_with_names(&[
            (3, 0x409, FAMILY, "MingLiU"),
            (1, 0, FAMILY, "Arial"),
        ]));
    }

    // FreeType checks only fonts with TrueType outlines.
    #[test]
    fn only_truetype_fonts_are_hint_reliant() {
        const FAMILY: u16 = 1;
        let mingliu = [(3, 0x409, FAMILY, "MingLiU")];
        assert!(with_names(font_test_data::TINOS_SUBSET, &mingliu));
        assert!(!with_names(
            font_test_data::NOTO_SERIF_DISPLAY_TRIMMED,
            &mingliu
        ));
    }

    #[test]
    fn ensure_max_name_len() {
        let max_len = HINT_RELIANT_NAMES
            .iter()
            .fold(0, |acc, name| acc.max(name.len()));
        assert_eq!(max_len, MAX_HINT_RELIANT_NAME_LEN);
    }

    #[test]
    fn skip_pdf_tags() {
        let matches = |name: &str| matches_hint_reliant_name_list(name.bytes());
        // A tag is skipped, so a listed name inside it does not count.
        assert!(!matches("DLCABC+Arial"));
        assert!(matches("ABCDEF+MingLiU"));
        // Not a tag: lowercase, digits, a misplaced plus.
        assert!(matches("DLCabc+Arial"));
        assert!(matches("DLC12A+Arial"));
        assert!(matches("DLCABCD+Arial"));
        // A name shorter than eight is never tagged.
        assert!(matches("DLCAB+X"));
        // The name after a tag can be longer than any on the list.
        assert!(matches("ABCDEF+DFKaiShu-SB-Estd-BF"));
    }

    #[test]
    fn all_hint_reliant_names() {
        for name in HINT_RELIANT_NAMES {
            assert!(matches_hint_reliant_name_list(name.bytes()));
        }
    }

    #[test]
    fn non_hint_reliant_names() {
        for not_tricky in ["Roboto", "Arial", "Helvetica", "Blah", ""] {
            assert!(!matches_hint_reliant_name_list(not_tricky.bytes()));
        }
    }
}

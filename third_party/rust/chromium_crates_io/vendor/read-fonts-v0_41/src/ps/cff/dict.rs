//! Parsing for PostScript DICTs.

use super::{
    blend::BlendState,
    stack::{Number, Stack},
};
use crate::{
    ps::{
        error::Error,
        hinting::{Blues, StemSnaps},
        num::{self, BcdComponents},
        string::Sid,
        transform::ScaledFontMatrix,
    },
    types::Fixed,
    Cursor, ReadError,
};
use std::ops::Range;

/// PostScript DICT operator.
///
/// See "Table 9 Top DICT Operator Entries" and "Table 23 Private DICT
/// Operators" at <https://adobe-type-tools.github.io/font-tech-notes/pdfs/5176.CFF.pdf>
#[derive(Copy, Clone, PartialEq, Eq, Hash, Debug)]
pub enum Operator {
    Version,
    Notice,
    FullName,
    FamilyName,
    Weight,
    FontBbox,
    CharstringsOffset,
    PrivateDictRange,
    VariationStoreOffset,
    Copyright,
    IsFixedPitch,
    ItalicAngle,
    UnderlinePosition,
    UnderlineThickness,
    PaintType,
    CharstringType,
    FontMatrix,
    StrokeWidth,
    FdArrayOffset,
    FdSelectOffset,
    BlueValues,
    OtherBlues,
    FamilyBlues,
    FamilyOtherBlues,
    SubrsOffset,
    VariationStoreIndex,
    BlueScale,
    BlueShift,
    BlueFuzz,
    LanguageGroup,
    ExpansionFactor,
    Encoding,
    Charset,
    UniqueId,
    Xuid,
    SyntheticBase,
    PostScript,
    BaseFontName,
    BaseFontBlend,
    Ros,
    CidFontVersion,
    CidFontRevision,
    CidFontType,
    CidCount,
    UidBase,
    FontName,
    StdHw,
    StdVw,
    DefaultWidthX,
    NominalWidthX,
    Blend,
    StemSnapH,
    StemSnapV,
    ForceBold,
    InitialRandomSeed,
}

impl Operator {
    fn from_opcode(opcode: u8) -> Option<Self> {
        use Operator::*;
        Some(match opcode {
            // Top DICT operators
            0 => Version,
            1 => Notice,
            2 => FullName,
            3 => FamilyName,
            4 => Weight,
            5 => FontBbox,
            13 => UniqueId,
            14 => Xuid,
            15 => Charset,
            16 => Encoding,
            17 => CharstringsOffset,
            18 => PrivateDictRange,
            24 => VariationStoreOffset,
            // Private DICT operators
            6 => BlueValues,
            7 => OtherBlues,
            8 => FamilyBlues,
            9 => FamilyOtherBlues,
            10 => StdHw,
            11 => StdVw,
            19 => SubrsOffset,
            20 => DefaultWidthX,
            21 => NominalWidthX,
            22 => VariationStoreIndex,
            23 => Blend,
            // Font DICT only uses PrivateDictRange
            _ => return None,
        })
    }

    fn from_extended_opcode(opcode: u8) -> Option<Self> {
        use Operator::*;
        Some(match opcode {
            // Top DICT operators
            0 => Copyright,
            1 => IsFixedPitch,
            2 => ItalicAngle,
            3 => UnderlinePosition,
            4 => UnderlineThickness,
            5 => PaintType,
            6 => CharstringType,
            7 => FontMatrix,
            8 => StrokeWidth,
            20 => SyntheticBase,
            21 => PostScript,
            22 => BaseFontName,
            23 => BaseFontBlend,
            30 => Ros,
            31 => CidFontVersion,
            32 => CidFontRevision,
            33 => CidFontType,
            34 => CidCount,
            35 => UidBase,
            36 => FdArrayOffset,
            37 => FdSelectOffset,
            38 => FontName,
            // Private DICT operators
            9 => BlueScale,
            10 => BlueShift,
            11 => BlueFuzz,
            12 => StemSnapH,
            13 => StemSnapV,
            14 => ForceBold,
            17 => LanguageGroup,
            18 => ExpansionFactor,
            19 => InitialRandomSeed,
            _ => return None,
        })
    }
}

/// Either a PostScript DICT operator or a (numeric) operand.
#[derive(Copy, Clone, PartialEq, Eq, Debug)]
pub enum Token {
    /// An operator parsed from a DICT.
    Operator(Operator),
    /// A number parsed from a DICT. If the source was in
    /// binary coded decimal format, then the second field
    /// contains the parsed components.
    Operand(Number, Option<BcdComponents>),
}

impl From<Operator> for Token {
    fn from(value: Operator) -> Self {
        Self::Operator(value)
    }
}

impl<T> From<T> for Token
where
    T: Into<Number>,
{
    fn from(value: T) -> Self {
        Self::Operand(value.into(), None)
    }
}

/// Given a byte slice containing DICT data, returns an iterator yielding
/// raw operands and operators.
///
/// This does not perform any additional processing such as type conversion,
/// delta decoding or blending.
pub fn tokens(dict_data: &[u8]) -> impl Iterator<Item = Result<Token, Error>> + '_ + Clone {
    let mut cursor = crate::FontData::new(dict_data).cursor();
    std::iter::from_fn(move || {
        if cursor.remaining_bytes() == 0 {
            None
        } else {
            Some(parse_token(&mut cursor))
        }
    })
}

fn parse_token(cursor: &mut Cursor) -> Result<Token, Error> {
    // Escape opcode for accessing extensions.
    const ESCAPE: u8 = 12;
    let b0 = cursor.read::<u8>()?;
    Ok(if b0 == ESCAPE {
        let b1 = cursor.read::<u8>()?;
        Token::Operator(Operator::from_extended_opcode(b1).ok_or(Error::InvalidDictOperator(b1))?)
    } else {
        // See <https://learn.microsoft.com/en-us/typography/opentype/spec/cff2#table-3-operand-encoding>
        match b0 {
            28 | 29 | 32..=254 => Token::Operand(num::parse_int(cursor, b0)?.into(), None),
            30 => {
                let components = BcdComponents::parse(cursor)?;
                Token::Operand(components.value(false).into(), Some(components))
            }
            _ => Token::Operator(Operator::from_opcode(b0).ok_or(Error::InvalidDictOperator(b0))?),
        }
    })
}

/// PostScript DICT Operator with its associated operands.
#[derive(Clone, PartialEq, Eq, Debug)]
pub enum Entry {
    Version(Sid),
    Notice(Sid),
    FullName(Sid),
    FamilyName(Sid),
    Weight(Sid),
    FontBbox([Fixed; 4]),
    CharstringsOffset(usize),
    PrivateDictRange(Range<usize>),
    VariationStoreOffset(usize),
    Copyright(Sid),
    IsFixedPitch(bool),
    ItalicAngle(Fixed),
    UnderlinePosition(Fixed),
    UnderlineThickness(Fixed),
    PaintType(i32),
    CharstringType(i32),
    FontMatrix(ScaledFontMatrix),
    StrokeWidth(Fixed),
    FdArrayOffset(usize),
    FdSelectOffset(usize),
    BlueValues(Blues),
    OtherBlues(Blues),
    FamilyBlues(Blues),
    FamilyOtherBlues(Blues),
    SubrsOffset(usize),
    VariationStoreIndex(u16),
    BlueScale(Fixed),
    BlueShift(Fixed),
    BlueFuzz(Fixed),
    LanguageGroup(i32),
    ExpansionFactor(Fixed),
    Encoding(usize),
    Charset(usize),
    UniqueId(i32),
    Xuid,
    SyntheticBase(i32),
    PostScript(Sid),
    BaseFontName(Sid),
    BaseFontBlend,
    Ros {
        registry: Sid,
        ordering: Sid,
        supplement: Fixed,
    },
    CidFontVersion(Fixed),
    CidFontRevision(Fixed),
    CidFontType(i32),
    CidCount(u32),
    UidBase(i32),
    FontName(Sid),
    StdHw(Fixed),
    StdVw(Fixed),
    DefaultWidthX(Fixed),
    NominalWidthX(Fixed),
    StemSnapH(StemSnaps),
    StemSnapV(StemSnaps),
    ForceBold(bool),
    InitialRandomSeed(i32),
}

/// Given a byte slice containing DICT data, returns an iterator yielding
/// each operator with its associated operands.
///
/// This performs appropriate type conversions, decodes deltas and applies
/// blending.
///
/// If processing a Private DICT from a CFF2 table and an item variation
/// store is present, then `blend_state` must be provided.
pub fn entries<'a>(
    dict_data: &'a [u8],
    mut blend_state: Option<BlendState<'a>>,
) -> impl Iterator<Item = Result<Entry, Error>> + 'a {
    let mut stack = Stack::new();
    let mut last_bcd_components = None;
    let mut cursor = crate::FontData::new(dict_data).cursor();
    let mut cursor_pos = 0;
    std::iter::from_fn(move || loop {
        if cursor.remaining_bytes() == 0 {
            return None;
        }
        let token = match parse_token(&mut cursor) {
            Ok(token) => token,
            Err(Error::InvalidDictOperator(_)) => {
                // Some buggy fonts have invalid dict operators. Clear
                // the stack and attempt to continue.
                // FreeType only processes known fields:
                // <https://gitlab.freedesktop.org/freetype/freetype/-/blob/80a507a6b8e3d2906ad2c8ba69329bd2fb2a85ef/src/cff/cffparse.c#L1328>
                // And then clears the stack regardless:
                // <https://gitlab.freedesktop.org/freetype/freetype/-/blob/80a507a6b8e3d2906ad2c8ba69329bd2fb2a85ef/src/cff/cffparse.c#L1469>
                stack.clear();
                continue;
            }
            Err(e) => return Some(Err(e)),
        };
        match token {
            Token::Operand(number, bcd_components) => {
                last_bcd_components = bcd_components;
                match stack.push(number) {
                    Ok(_) => continue,
                    Err(e) => return Some(Err(e)),
                }
            }
            Token::Operator(op) => {
                if op == Operator::Blend || op == Operator::VariationStoreIndex {
                    let state = match blend_state.as_mut() {
                        Some(state) => state,
                        None => return Some(Err(Error::MissingBlendState)),
                    };
                    if op == Operator::VariationStoreIndex {
                        match stack
                            .get_i32(0)
                            .and_then(|ix| state.set_store_index(ix as u16))
                        {
                            Ok(_) => {}
                            Err(e) => return Some(Err(e)),
                        }
                    }
                    if op == Operator::Blend {
                        match stack.apply_blend(state) {
                            Ok(_) => continue,
                            Err(e) => return Some(Err(e)),
                        }
                    }
                }
                if op == Operator::BlueScale {
                    // FreeType parses BlueScale using a scaling factor of
                    // 1000, presumably to capture more precision in the
                    // fractional part. We do the same.
                    // See <https://gitlab.freedesktop.org/freetype/freetype/-/blob/master/src/cff/cfftoken.h?ref_type=heads#L87>
                    if let Some(bcd_components) = last_bcd_components.take() {
                        // If the most recent numeric value was parsed as a
                        // binary coded decimal then recompute the value using
                        // the desired scaling and replace it on the stack
                        stack.pop_fixed().ok()?;
                        stack.push(bcd_components.value(true)).ok()?;
                    }
                }
                if op == Operator::FontMatrix {
                    // FontMatrix is also parsed specially... *sigh*
                    // Redo the entire thing with special scaling factors
                    // See <https://gitlab.freedesktop.org/freetype/freetype/-/blob/f1cd6dbfa0c98f352b698448f40ac27e8fb3832e/src/cff/cffparse.c#L623>
                    // Dump the current values
                    stack.clear();
                    last_bcd_components = None;
                    // Now reparse with dynamic scaling
                    let mut cursor = crate::FontData::new(dict_data).cursor();
                    cursor.advance_by(cursor_pos);
                    if let Some(matrix) = ScaledFontMatrix::parse(&mut cursor) {
                        return Some(Ok(Entry::FontMatrix(matrix)));
                    }
                    continue;
                }
                last_bcd_components = None;
                let entry = parse_entry(op, &mut stack);
                stack.clear();
                cursor_pos = cursor.position().unwrap_or_default();
                return Some(entry);
            }
        }
    })
}

fn parse_entry(op: Operator, stack: &mut Stack) -> Result<Entry, Error> {
    use Operator::*;
    Ok(match op {
        Version => Entry::Version(stack.pop_i32()?.into()),
        Notice => Entry::Notice(stack.pop_i32()?.into()),
        FullName => Entry::FullName(stack.pop_i32()?.into()),
        FamilyName => Entry::FamilyName(stack.pop_i32()?.into()),
        Weight => Entry::Weight(stack.pop_i32()?.into()),
        FontBbox => Entry::FontBbox([
            stack.get_fixed(0)?,
            stack.get_fixed(1)?,
            stack.get_fixed(2)?,
            stack.get_fixed(3)?,
        ]),
        CharstringsOffset => Entry::CharstringsOffset(stack.pop_i32()? as usize),
        PrivateDictRange => {
            let len = stack.get_i32(0)? as usize;
            let start = stack.get_i32(1)? as usize;
            let end = start.checked_add(len).ok_or(ReadError::OutOfBounds)?;
            Entry::PrivateDictRange(start..end)
        }
        VariationStoreOffset => Entry::VariationStoreOffset(stack.pop_i32()? as usize),
        Copyright => Entry::Copyright(stack.pop_i32()?.into()),
        IsFixedPitch => Entry::IsFixedPitch(stack.pop_i32()? != 0),
        ItalicAngle => Entry::ItalicAngle(stack.pop_fixed()?),
        UnderlinePosition => Entry::UnderlinePosition(stack.pop_fixed()?),
        UnderlineThickness => Entry::UnderlineThickness(stack.pop_fixed()?),
        PaintType => Entry::PaintType(stack.pop_i32()?),
        CharstringType => Entry::CharstringType(stack.pop_i32()?),
        FontMatrix => unreachable!(),
        StrokeWidth => Entry::StrokeWidth(stack.pop_fixed()?),
        FdArrayOffset => Entry::FdArrayOffset(stack.pop_i32()? as usize),
        FdSelectOffset => Entry::FdSelectOffset(stack.pop_i32()? as usize),
        BlueValues => {
            stack.apply_delta_prefix_sum();
            Entry::BlueValues(Blues::new(stack.fixed_values()))
        }
        OtherBlues => {
            stack.apply_delta_prefix_sum();
            Entry::OtherBlues(Blues::new(stack.fixed_values()))
        }
        FamilyBlues => {
            stack.apply_delta_prefix_sum();
            Entry::FamilyBlues(Blues::new(stack.fixed_values()))
        }
        FamilyOtherBlues => {
            stack.apply_delta_prefix_sum();
            Entry::FamilyOtherBlues(Blues::new(stack.fixed_values()))
        }
        SubrsOffset => Entry::SubrsOffset(stack.pop_i32()? as usize),
        VariationStoreIndex => Entry::VariationStoreIndex(stack.pop_i32()? as u16),
        BlueScale => Entry::BlueScale(stack.pop_fixed()?),
        BlueShift => Entry::BlueShift(stack.pop_fixed()?),
        BlueFuzz => Entry::BlueFuzz(stack.pop_fixed()?),
        LanguageGroup => Entry::LanguageGroup(stack.pop_i32()?),
        ExpansionFactor => Entry::ExpansionFactor(stack.pop_fixed()?),
        Encoding => Entry::Encoding(stack.pop_i32()? as usize),
        Charset => Entry::Charset(stack.pop_i32()? as usize),
        UniqueId => Entry::UniqueId(stack.pop_i32()?),
        Xuid => Entry::Xuid,
        SyntheticBase => Entry::SyntheticBase(stack.pop_i32()?),
        PostScript => Entry::PostScript(stack.pop_i32()?.into()),
        BaseFontName => Entry::BaseFontName(stack.pop_i32()?.into()),
        BaseFontBlend => Entry::BaseFontBlend,
        Ros => Entry::Ros {
            registry: stack.get_i32(0)?.into(),
            ordering: stack.get_i32(1)?.into(),
            supplement: stack.get_fixed(2)?,
        },
        CidFontVersion => Entry::CidFontVersion(stack.pop_fixed()?),
        CidFontRevision => Entry::CidFontRevision(stack.pop_fixed()?),
        CidFontType => Entry::CidFontType(stack.pop_i32()?),
        CidCount => Entry::CidCount(stack.pop_i32()? as u32),
        UidBase => Entry::UidBase(stack.pop_i32()?),
        FontName => Entry::FontName(stack.pop_i32()?.into()),
        StdHw => Entry::StdHw(stack.pop_fixed()?),
        StdVw => Entry::StdVw(stack.pop_fixed()?),
        DefaultWidthX => Entry::DefaultWidthX(stack.pop_fixed()?),
        NominalWidthX => Entry::NominalWidthX(stack.pop_fixed()?),
        StemSnapH => {
            stack.apply_delta_prefix_sum();
            Entry::StemSnapH(StemSnaps::new(stack.fixed_values()))
        }
        StemSnapV => {
            stack.apply_delta_prefix_sum();
            Entry::StemSnapV(StemSnaps::new(stack.fixed_values()))
        }
        ForceBold => Entry::ForceBold(stack.pop_i32()? != 0),
        InitialRandomSeed => Entry::InitialRandomSeed(stack.pop_i32()?),
        // Blend is handled at the layer above
        Blend => unreachable!(),
    })
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::{
        tables::variations::ItemVariationStore, types::F2Dot14, FontData, FontRead, FontRef,
        TableProvider,
    };
    use font_test_data::bebuffer::BeBuffer;

    #[test]
    fn example_top_dict_tokens() {
        use Operator::*;
        let top_dict_data = &font_test_data::cff2::EXAMPLE[5..12];
        let tokens: Vec<_> = tokens(top_dict_data).map(|entry| entry.unwrap()).collect();
        let expected: &[Token] = &[
            68.into(),
            FdArrayOffset.into(),
            56.into(),
            CharstringsOffset.into(),
            16.into(),
            VariationStoreOffset.into(),
        ];
        assert_eq!(&tokens, expected);
    }

    #[test]
    fn example_top_dict_entries() {
        use Entry::*;
        let top_dict_data = &font_test_data::cff2::EXAMPLE[0x5..=0xB];
        let entries: Vec<_> = entries(top_dict_data, None)
            .map(|entry| entry.unwrap())
            .collect();
        let expected: &[Entry] = &[
            FdArrayOffset(68),
            CharstringsOffset(56),
            VariationStoreOffset(16),
        ];
        assert_eq!(&entries, expected);
    }

    #[test]
    fn example_private_dict_entries() {
        use Entry::*;
        let private_dict_data = &font_test_data::cff2::EXAMPLE[0x4f..=0xc0];
        let store =
            ItemVariationStore::read(FontData::new(&font_test_data::cff2::EXAMPLE[18..])).unwrap();
        let coords = &[F2Dot14::from_f32(0.0)];
        let blend_state = BlendState::new(store, coords, 0).unwrap();
        let entries: Vec<_> = entries(private_dict_data, Some(blend_state))
            .map(|entry| entry.unwrap())
            .collect();
        fn make_blues(values: &[f64]) -> Blues {
            Blues::new(values.iter().copied().map(Fixed::from_f64))
        }
        fn make_stem_snaps(values: &[f64]) -> StemSnaps {
            StemSnaps::new(values.iter().copied().map(Fixed::from_f64))
        }
        let expected: &[Entry] = &[
            BlueValues(make_blues(&[
                -20.0, 0.0, 472.0, 490.0, 525.0, 540.0, 645.0, 660.0, 670.0, 690.0, 730.0, 750.0,
            ])),
            OtherBlues(make_blues(&[-250.0, -240.0])),
            FamilyBlues(make_blues(&[
                -20.0, 0.0, 473.0, 491.0, 525.0, 540.0, 644.0, 659.0, 669.0, 689.0, 729.0, 749.0,
            ])),
            FamilyOtherBlues(make_blues(&[-249.0, -239.0])),
            BlueScale(Fixed::from_f64(0.037506103515625)),
            BlueFuzz(Fixed::ZERO),
            StdHw(Fixed::from_f64(55.0)),
            StdVw(Fixed::from_f64(80.0)),
            StemSnapH(make_stem_snaps(&[40.0, 55.0])),
            StemSnapV(make_stem_snaps(&[80.0, 90.0])),
            SubrsOffset(114),
        ];
        assert_eq!(&entries, expected);
    }

    #[test]
    fn noto_serif_display_top_dict_entries() {
        use Entry::*;
        let top_dict_data = FontRef::new(font_test_data::NOTO_SERIF_DISPLAY_TRIMMED)
            .unwrap()
            .cff()
            .unwrap()
            .top_dicts()
            .get(0)
            .unwrap();
        let entries: Vec<_> = entries(top_dict_data, None)
            .map(|entry| entry.unwrap())
            .collect();
        let expected = &[
            Version(Sid::new(391)),
            Notice(Sid::new(392)),
            Copyright(Sid::new(393)),
            FullName(Sid::new(394)),
            FamilyName(Sid::new(395)),
            FontBbox([-693.0, -470.0, 2797.0, 1048.0].map(Fixed::from_f64)),
            Charset(517),
            PrivateDictRange(549..587),
            CharstringsOffset(521),
        ];
        assert_eq!(&entries, expected);
    }

    // Fuzzer caught add with overflow when constructing private DICT
    // range.
    // See <https://bugs.chromium.org/p/oss-fuzz/issues/detail?id=71746>
    // and <https://oss-fuzz.com/testcase?key=4591358306746368>
    #[test]
    fn private_dict_range_avoid_overflow() {
        // A Private DICT that tries to construct a range from -1..(-1 + -1)
        // which overflows when converted to usize
        let private_dict = BeBuffer::new()
            .push(29u8) // integer operator
            .push(-1i32) // integer value
            .push(29u8) // integer operator
            .push(-1i32) // integer value
            .push(18u8) // PrivateDICT operator
            .to_vec();
        // Just don't panic
        let _ = entries(&private_dict, None).count();
    }

    #[test]
    fn read_font_matrix() {
        let dict_data = [
            30u8, 10, 0, 31, 139, 30, 10, 0, 1, 103, 255, 30, 10, 0, 31, 139, 139, 12, 7,
        ];
        let Entry::FontMatrix(matrix) = entries(&dict_data, None).next().unwrap().unwrap() else {
            panic!("This was totally a font matrix");
        };
        // From ttx: <FontMatrix value="0.001 0 0.000167 0.001 0 0"/>
        // But scaled by 1000 because that's how FreeType does it
        assert_eq!(
            matrix.matrix.elements(),
            [
                Fixed::ONE,
                Fixed::ZERO,
                Fixed::from_f64(0.167007446289062),
                Fixed::ONE,
                Fixed::ZERO,
                Fixed::ZERO,
            ]
        );
    }

    #[test]
    fn parse_degenerate_font_matrix() {
        let dict_data = [
            30u8, 0x0F, 30, 0x0F, 30, 0x0F, 30, 0x0F, 30, 0x0F, 30, 0x0F, 12, 7,
        ];
        // Don't return a degenerate matrix at all
        assert!(entries(&dict_data, None).next().is_none());
    }
}

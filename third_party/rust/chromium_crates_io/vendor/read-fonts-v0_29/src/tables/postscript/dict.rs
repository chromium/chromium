//! Parsing for PostScript DICTs.

use std::ops::Range;

use super::{BlendState, Error, Number, Stack, StringId};
use crate::{types::Fixed, Cursor, ReadError};

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
    Operator(Operator),
    Operand(Number),
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
        Self::Operand(value.into())
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
            28 | 29 | 32..=254 => Token::Operand(parse_int(cursor, b0)?.into()),
            30 => Token::Operand(parse_bcd(cursor)?.into()),
            _ => Token::Operator(Operator::from_opcode(b0).ok_or(Error::InvalidDictOperator(b0))?),
        }
    })
}

/// PostScript DICT Operator with its associated operands.
#[derive(Clone, PartialEq, Eq, Debug)]
pub enum Entry {
    Version(StringId),
    Notice(StringId),
    FullName(StringId),
    FamilyName(StringId),
    Weight(StringId),
    FontBbox([Fixed; 4]),
    CharstringsOffset(usize),
    PrivateDictRange(Range<usize>),
    VariationStoreOffset(usize),
    Copyright(StringId),
    IsFixedPitch(bool),
    ItalicAngle(Fixed),
    UnderlinePosition(Fixed),
    UnderlineThickness(Fixed),
    PaintType(i32),
    CharstringType(i32),
    FontMatrix([Fixed; 6]),
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
    PostScript(StringId),
    BaseFontName(StringId),
    BaseFontBlend,
    Ros {
        registry: StringId,
        ordering: StringId,
        supplement: Fixed,
    },
    CidFontVersion(Fixed),
    CidFontRevision(Fixed),
    CidFontType(i32),
    CidCount(u32),
    UidBase(i32),
    FontName(StringId),
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
    let mut token_iter = tokens(dict_data);
    std::iter::from_fn(move || loop {
        let token = match token_iter.next()? {
            Ok(token) => token,
            Err(e) => return Some(Err(e)),
        };
        match token {
            Token::Operand(number) => match stack.push(number) {
                Ok(_) => continue,
                Err(e) => return Some(Err(e)),
            },
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
                let entry = parse_entry(op, &mut stack);
                stack.clear();
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
        FontMatrix => Entry::FontMatrix([
            stack.get_fixed(0)?,
            stack.get_fixed(1)?,
            stack.get_fixed(2)?,
            stack.get_fixed(3)?,
            stack.get_fixed(4)?,
            stack.get_fixed(5)?,
        ]),
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

/// <https://gitlab.freedesktop.org/freetype/freetype/-/blob/80a507a6b8e3d2906ad2c8ba69329bd2fb2a85ef/src/psaux/psblues.h#L141>
const MAX_BLUE_VALUES: usize = 7;

/// Operand for the `BlueValues`, `OtherBlues`, `FamilyBlues` and
/// `FamilyOtherBlues` operators.
///
/// These are used to generate zones when applying hints.
#[derive(Copy, Clone, PartialEq, Eq, Default, Debug)]
pub struct Blues {
    values: [(Fixed, Fixed); MAX_BLUE_VALUES],
    len: u32,
}

impl Blues {
    pub fn new(values: impl Iterator<Item = Fixed>) -> Self {
        let mut blues = Self::default();
        let mut stash = Fixed::ZERO;
        for (i, value) in values.take(MAX_BLUE_VALUES * 2).enumerate() {
            if (i & 1) == 0 {
                stash = value;
            } else {
                blues.values[i / 2] = (stash, value);
                blues.len += 1;
            }
        }
        blues
    }

    pub fn values(&self) -> &[(Fixed, Fixed)] {
        &self.values[..self.len as usize]
    }
}

/// Summary: older PostScript interpreters accept two values, but newer ones
/// accept 12. We'll assume that as maximum.
/// <https://adobe-type-tools.github.io/font-tech-notes/pdfs/5049.StemSnap.pdf>
const MAX_STEM_SNAPS: usize = 12;

/// Operand for the `StemSnapH` and `StemSnapV` operators.
///
/// These are used for stem darkening when applying hints.
#[derive(Copy, Clone, PartialEq, Eq, Default, Debug)]
pub struct StemSnaps {
    values: [Fixed; MAX_STEM_SNAPS],
    len: u32,
}

impl StemSnaps {
    fn new(values: impl Iterator<Item = Fixed>) -> Self {
        let mut snaps = Self::default();
        for (value, target_value) in values.take(MAX_STEM_SNAPS).zip(&mut snaps.values) {
            *target_value = value;
            snaps.len += 1;
        }
        snaps
    }

    pub fn values(&self) -> &[Fixed] {
        &self.values[..self.len as usize]
    }
}

pub(crate) fn parse_int(cursor: &mut Cursor, b0: u8) -> Result<i32, Error> {
    // Size   b0 range     Value range              Value calculation
    //--------------------------------------------------------------------------------
    // 1      32 to 246    -107 to +107             b0 - 139
    // 2      247 to 250   +108 to +1131            (b0 - 247) * 256 + b1 + 108
    // 2      251 to 254   -1131 to -108            -(b0 - 251) * 256 - b1 - 108
    // 3      28           -32768 to +32767         b1 << 8 | b2
    // 5      29           -(2^31) to +(2^31 - 1)   b1 << 24 | b2 << 16 | b3 << 8 | b4
    // <https://learn.microsoft.com/en-us/typography/opentype/spec/cff2#table-3-operand-encoding>
    Ok(match b0 {
        32..=246 => b0 as i32 - 139,
        247..=250 => (b0 as i32 - 247) * 256 + cursor.read::<u8>()? as i32 + 108,
        251..=254 => -(b0 as i32 - 251) * 256 - cursor.read::<u8>()? as i32 - 108,
        28 => cursor.read::<i16>()? as i32,
        29 => cursor.read::<i32>()?,
        _ => {
            return Err(Error::InvalidNumber);
        }
    })
}

/// Parse a binary coded decimal number.
/// See <https://gitlab.freedesktop.org/freetype/freetype/-/blob/82090e67c24259c343c83fd9cefe6ff0be7a7eca/src/cff/cffparse.c#L183>
fn parse_bcd(cursor: &mut Cursor) -> Result<Fixed, Error> {
    // Value returned on overflow
    const OVERFLOW: Fixed = Fixed::from_bits(0x7FFFFFFF);
    // Value returned on underflow
    const UNDERFLOW: Fixed = Fixed::ZERO;
    // Limit at which we stop accumulating `number` and increase
    // the exponent instead
    const NUMBER_LIMIT: i32 = 0xCCCCCCC;
    // Limit for the integral part of the result
    const INTEGER_LIMIT: i32 = 0x7FFF;
    // <https://gitlab.freedesktop.org/freetype/freetype/-/blob/82090e67c24259c343c83fd9cefe6ff0be7a7eca/src/cff/cffparse.c#L150>
    const POWER_TENS: [i32; 10] = [
        1, 10, 100, 1000, 10000, 100000, 1000000, 10000000, 100000000, 1000000000,
    ];
    enum Phase {
        Integer,
        Fraction,
        Exponent,
    }
    let mut phase = Phase::Integer;
    let mut sign = 1i32;
    let mut exponent_sign = 1i32;
    let mut number = 0i32;
    let mut exponent = 0i32;
    let mut exponent_add = 0i32;
    let mut integer_len = 0;
    let mut fraction_len = 0;
    // Nibble value    Represents
    //----------------------------------
    // 0 to 9          0 to 9
    // a               . (decimal point)
    // b               E
    // c               E-
    // d               <reserved>
    // e               - (minus)
    // f               end of number
    // <https://learn.microsoft.com/en-us/typography/opentype/spec/cff2#table-5-nibble-definitions>
    'outer: loop {
        let b = cursor.read::<u8>()?;
        for nibble in [(b >> 4) & 0xF, b & 0xF] {
            match phase {
                Phase::Integer => match nibble {
                    0x0..=0x9 => {
                        if number >= NUMBER_LIMIT {
                            exponent_add += 1;
                        } else if nibble != 0 || number != 0 {
                            number = number * 10 + nibble as i32;
                            integer_len += 1;
                        }
                    }
                    0xE => sign = -1,
                    0xA => {
                        phase = Phase::Fraction;
                    }
                    0xB => {
                        phase = Phase::Exponent;
                    }
                    0xC => {
                        phase = Phase::Exponent;
                        exponent_sign = -1;
                    }
                    _ => break 'outer,
                },
                Phase::Fraction => match nibble {
                    0x0..=0x9 => {
                        if nibble == 0 && number == 0 {
                            exponent_add -= 1;
                        } else if number < NUMBER_LIMIT && fraction_len < 9 {
                            number = number * 10 + nibble as i32;
                            fraction_len += 1;
                        }
                    }
                    0xB => {
                        phase = Phase::Exponent;
                    }
                    0xC => {
                        phase = Phase::Exponent;
                        exponent_sign = -1;
                    }
                    _ => break 'outer,
                },
                Phase::Exponent => {
                    match nibble {
                        0x0..=0x9 => {
                            // Arbitrarily limit exponent
                            if exponent > 1000 {
                                return if exponent_sign == -1 {
                                    Ok(UNDERFLOW)
                                } else {
                                    Ok(OVERFLOW)
                                };
                            } else {
                                exponent = exponent * 10 + nibble as i32;
                            }
                        }
                        _ => break 'outer,
                    }
                }
            }
        }
    }
    if number == 0 {
        return Ok(Fixed::ZERO);
    }
    exponent *= exponent_sign;
    exponent += exponent_add;
    integer_len += exponent;
    fraction_len -= exponent;
    if integer_len > 5 {
        return Ok(OVERFLOW);
    }
    if integer_len < -5 {
        return Ok(UNDERFLOW);
    }
    // Remove non-significant digits
    if integer_len < 0 {
        number /= POWER_TENS[(-integer_len) as usize];
        fraction_len += integer_len;
    }
    // Can only happen if exponent was non-zero
    if fraction_len == 10 {
        number /= 10;
        fraction_len -= 1;
    }
    // Convert to fixed
    let result = if fraction_len > 0 {
        let b = POWER_TENS[fraction_len as usize];
        if number / b > INTEGER_LIMIT {
            0
        } else {
            (Fixed::from_bits(number) / Fixed::from_bits(b)).to_bits()
        }
    } else {
        number = number.wrapping_mul(-fraction_len);
        if number > INTEGER_LIMIT {
            return Ok(OVERFLOW);
        } else {
            number << 16
        }
    };
    Ok(Fixed::from_bits(result * sign))
}

#[cfg(test)]
mod tests {
    use font_test_data::bebuffer::BeBuffer;

    use super::*;
    use crate::{
        tables::variations::ItemVariationStore, types::F2Dot14, FontData, FontRead, FontRef,
        TableProvider,
    };

    #[test]
    fn int_operands() {
        // Test the boundary conditions of the ranged int operators
        let empty = FontData::new(&[]);
        let min_byte = FontData::new(&[0]);
        let max_byte = FontData::new(&[255]);
        // 32..=246 => -107..=107
        assert_eq!(parse_int(&mut empty.cursor(), 32).unwrap(), -107);
        assert_eq!(parse_int(&mut empty.cursor(), 246).unwrap(), 107);
        // 247..=250 => +108 to +1131
        assert_eq!(parse_int(&mut min_byte.cursor(), 247).unwrap(), 108);
        assert_eq!(parse_int(&mut max_byte.cursor(), 250).unwrap(), 1131);
        // 251..=254 => -1131 to -108
        assert_eq!(parse_int(&mut min_byte.cursor(), 251).unwrap(), -108);
        assert_eq!(parse_int(&mut max_byte.cursor(), 254).unwrap(), -1131);
    }

    #[test]
    fn binary_coded_decimal_operands() {
        // From <https://learn.microsoft.com/en-us/typography/opentype/spec/cff2#table-5-nibble-definitions>:
        //
        // "A real number is terminated by one (or two) 0xf nibbles so that it is always padded
        // to a full byte. Thus, the value -2.25 is encoded by the byte sequence (1e e2 a2 5f)
        // and the value 0.140541E-3 by the sequence (1e 0a 14 05 41 c3 ff)."
        //
        // The initial 1e byte in the examples above is the dictionary operator to trigger
        // parsing of BCD so it is dropped in the tests here.
        let bytes = FontData::new(&[0xe2, 0xa2, 0x5f]);
        assert_eq!(
            parse_bcd(&mut bytes.cursor()).unwrap(),
            Fixed::from_f64(-2.25)
        );
        let bytes = FontData::new(&[0x0a, 0x14, 0x05, 0x41, 0xc3, 0xff]);
        assert_eq!(
            parse_bcd(&mut bytes.cursor()).unwrap(),
            Fixed::from_f64(0.140541E-3)
        );
        // Check that we match FreeType for 375e-4.
        // Note: we used to parse 0.0375... but the new FT matching code
        // has less precision
        let bytes = FontData::new(&[0x37, 0x5c, 0x4f]);
        assert_eq!(
            parse_bcd(&mut bytes.cursor()).unwrap(),
            Fixed::from_f64(0.0370025634765625)
        );
    }

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
            BlueScale(Fixed::from_f64(0.0370025634765625)),
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
            Version(StringId::new(391)),
            Notice(StringId::new(392)),
            Copyright(StringId::new(393)),
            FullName(StringId::new(394)),
            FamilyName(StringId::new(395)),
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
}

//! Type1 font parsing.

use super::super::{
    charstring::{self, CharstringContext, CharstringKind, CommandSink},
    dict::{FontMatrix, ScaledFontMatrix},
    Error, PredefinedEncoding,
};
use crate::{
    types::{Fixed, GlyphId},
    ReadError,
};
use core::ops::Range;

/// A Type1 font.
pub struct Type1Font {
    matrix: ScaledFontMatrix,
    charstrings: Charstrings,
    subrs: Subrs,
    encoding: Encoding,
    weight_vector: Vec<Fixed>,
}

impl Type1Font {
    /// Creates a new Type1 font from the given data.
    pub fn new(data: &[u8]) -> Result<Self, Error> {
        // Any failure to parse is simply represented by an invalid font format
        // error
        Self::new_impl(data).ok_or(Error::InvalidFontFormat)
    }

    fn new_impl(data: &[u8]) -> Option<Self> {
        let raw_dicts = RawDicts::new(data)?;
        Self::from_dicts(raw_dicts.base, &raw_dicts.private)
    }

    fn from_dicts(base: &[u8], private: &[u8]) -> Option<Self> {
        let mut font = Type1Font {
            matrix: ScaledFontMatrix {
                matrix: FontMatrix::IDENTITY,
                scale: 1000,
            },
            charstrings: Charstrings::default(),
            subrs: Subrs::default(),
            encoding: Encoding::Predefined(PredefinedEncoding::Standard),
            weight_vector: Vec::new(),
        };
        // Read base dict entries
        let mut encoding_offset = None;
        let mut parser = Parser::new(base);
        while let Some(token) = parser.next() {
            match token {
                Token::Name(b"FontMatrix") => font.matrix = parser.read_font_matrix()?,
                // Simply save the encoding offset. We'll parse it after
                // we have read charstrings so we have an accurate mapping
                // if we've synthesized or remapped a notdef glyph
                Token::Name(b"Encoding") => encoding_offset = Some(parser.pos),
                Token::Name(b"WeightVector") => {
                    // Gated on a successful read because some fonts reference
                    // the /WeightVector name outside of a proc, leading to
                    // spurious field reads
                    if let Some(weights) = parser.read_weight_vector() {
                        font.weight_vector = weights;
                    }
                }
                _ => {}
            }
        }
        // Read private dict entries
        let mut parser = Parser::new(private);
        // Default value if not present
        let mut len_iv = 4;
        while let Some(token) = parser.next() {
            match token {
                Token::Name(b"lenIV") => len_iv = parser.read_int()?,
                Token::Name(b"Subrs") => {
                    // With synthetic fonts, it's possible to read subroutines
                    // twice. FreeType ignores the second copy.
                    // <https://gitlab.freedesktop.org/freetype/freetype/-/blob/80a507a6b8e3d2906ad2c8ba69329bd2fb2a85ef/src/type1/t1load.c#L1855>
                    if font.subrs.index.is_empty() {
                        font.subrs = parser.read_subrs(len_iv)?;
                    }
                }
                Token::Name(b"CharStrings") => {
                    // Some non-standard fonts provide multiple copies of
                    // outlines for different resolutions and FreeType only
                    // retains the first copy, so skip parsing if we've
                    // already read some charstrings.
                    // <https://gitlab.freedesktop.org/freetype/freetype/-/blob/80a507a6b8e3d2906ad2c8ba69329bd2fb2a85ef/src/type1/t1load.c#L2058>
                    if font.charstrings.index.is_empty() {
                        font.charstrings = parser.read_charstrings(len_iv)?;
                    }
                }
                _ => {}
            }
        }
        if let Some(encoding_offset) = encoding_offset {
            let mut parser = Parser::new(base.get(encoding_offset..)?);
            font.encoding = parser.read_encoding(&font.charstrings)?;
        }
        Some(font)
    }

    /// Returns the number of glyphs in the Type1 font.
    pub fn num_glyphs(&self) -> u32 {
        self.charstrings.num_glyphs()
    }

    /// Returns the top level font matrix.
    pub fn matrix(&self) -> FontMatrix {
        self.matrix.matrix
    }

    /// Returns the units per em.
    pub fn upem(&self) -> i32 {
        self.matrix.scale
    }

    /// Returns the fixed point scale factor for the given font size in
    /// pixels per em.
    pub fn scale_for_ppem(&self, ppem: f32) -> Fixed {
        Fixed::from_bits((ppem * 64.) as i32) / Fixed::from_bits(self.upem().max(1))
    }

    /// Applies font matrix and scale to a horizontal metric (such as advance
    /// width).
    pub fn transform_h_metric(&self, scale: Option<Fixed>, mut metric: Fixed) -> Fixed {
        metric = Fixed::from_bits(metric.to_i32());
        let matrix = &self.matrix.matrix.0;
        if matrix[0] != Fixed::ONE {
            // x scale
            metric *= matrix[0];
        }
        // x translation
        metric += matrix[4];
        if let Some(scale) = scale {
            // Multiplying by scale converts to 26.6 but we want to keep the
            // result in 16.16
            Fixed::from_bits((metric * scale).to_bits() << 10)
        } else {
            // Metric is currently in font units. Convert back to 16.16
            Fixed::from_bits(metric.to_bits() << 16)
        }
    }

    /// Returns an iterator over the pairs of glyph ids and associated names in
    /// the Type1 font.
    pub fn glyph_names(&self) -> impl Iterator<Item = (GlyphId, &str)> {
        (0..self.num_glyphs())
            .filter_map(|idx| Some((GlyphId::new(idx), self.charstrings.name(idx)?)))
    }

    /// Evaluates the charstring for the requested glyph and sends the results
    /// to the given sink.
    pub fn evaluate_charstring(
        &self,
        gid: GlyphId,
        sink: &mut impl CommandSink,
    ) -> Result<Option<Fixed>, Error> {
        let charstring_data = self
            .charstrings
            .get(gid.to_u32())
            .ok_or(ReadError::OutOfBounds)?;
        charstring::evaluate(self, None, charstring_data, sink)
    }
}

impl CharstringContext for Type1Font {
    fn kind(&self) -> CharstringKind {
        CharstringKind::Type1
    }

    fn seac_components(&self, base_code: i32, accent_code: i32) -> Result<[&[u8]; 2], Error> {
        let decode = |code: i32| {
            let name = PredefinedEncoding::Standard
                .code_to_glyph_name(code.try_into().map_err(|_| Error::InvalidSeacCode(code))?);
            self.charstrings
                .index_for_name(name)
                .and_then(|idx| self.charstrings.get(idx))
                .ok_or(Error::InvalidSeacCode(code))
        };
        let base = decode(base_code)?;
        let accent = decode(accent_code)?;
        Ok([base, accent])
    }

    fn subr(&self, index: i32) -> Result<&[u8], Error> {
        Ok(self.subrs.get(index as u32).ok_or(ReadError::OutOfBounds)?)
    }

    fn global_subr(&self, _index: i32) -> Result<&[u8], Error> {
        // Type1 fonts don't have global subroutines
        Err(Error::MissingSubroutines)
    }

    fn weight_vector(&self) -> &[Fixed] {
        &self.weight_vector
    }
}

/// Raw dictionary data for a Type1 font.
struct RawDicts<'a> {
    /// Data containing the base dicitionary.
    base: &'a [u8],
    /// Data containing the decrypted private dictionary.
    private: Vec<u8>,
}

impl<'a> RawDicts<'a> {
    fn new(data: &'a [u8]) -> Option<Self> {
        if let Some((PFB_TEXT_SEGMENT_TAG, base_size)) = decode_pfb_tag(data, 0) {
            // We have a PFB; skip the tag
            let data = data.get(6..)?;
            verify_header(data)?;
            let (base_dict, raw_private_dict) = data.split_at_checked(base_size as usize)?;
            // Decrypt private dict segments
            let private_dict = decrypt(
                decode_pfb_binary_segments(raw_private_dict)
                    .flat_map(|segment| segment.iter().copied()),
                EEXEC_SEED,
            )
            // First four bytes are random garbage
            .skip(4)
            .collect::<Vec<_>>();
            Some(Self {
                base: base_dict,
                private: private_dict,
            })
        } else {
            // We have a PFA
            verify_header(data)?;
            // Now find the start of the private dictionary
            let start = find_eexec_data(data)?;
            let (base_dict, raw_private_dict) = data.split_at_checked(start)?;
            let private_dict = if raw_private_dict.len() > 3
                && raw_private_dict[..4].iter().all(|b| b.is_ascii_hexdigit())
            {
                // Hex decode and then decrypt
                decrypt(decode_hex(raw_private_dict.iter().copied()), EEXEC_SEED)
                    .skip(4)
                    .collect::<Vec<_>>()
            } else {
                // Just decrypt
                decrypt(raw_private_dict.iter().copied(), EEXEC_SEED)
                    .skip(4)
                    .collect::<Vec<_>>()
            };
            Some(Self {
                base: base_dict,
                private: private_dict,
            })
        }
    }
}

fn verify_header(data: &[u8]) -> Option<()> {
    (data.starts_with(b"%!PS-AdobeFont") || data.starts_with(b"%!FontType")).then_some(())
}

const PFB_TEXT_SEGMENT_TAG: u16 = 0x8001;
const PFB_BINARY_SEGMENT_TAG: u16 = 0x8002;

/// Returns the PFB tag and segment size.
///
/// See <https://gitlab.freedesktop.org/freetype/freetype/-/blob/80a507a6b8e3d2906ad2c8ba69329bd2fb2a85ef/src/type1/t1parse.c#L69>
fn decode_pfb_tag(data: &[u8], start: usize) -> Option<(u16, u32)> {
    let header: [u8; 6] = data.get(start..start + 6)?.try_into().ok()?;
    let tag = ((header[0] as u16) << 8) | header[1] as u16;
    if matches!(tag, PFB_BINARY_SEGMENT_TAG | PFB_TEXT_SEGMENT_TAG) {
        let size = u32::from_le_bytes(header[2..].try_into().unwrap());
        Some((tag, size))
    } else {
        None
    }
}

/// Returns an iterator over the sequence of PFB binary segments.
fn decode_pfb_binary_segments(data: &[u8]) -> impl Iterator<Item = &[u8]> + '_ {
    let mut pos = 0usize;
    core::iter::from_fn(move || {
        let (tag, len) = decode_pfb_tag(data, pos)?;
        // FT only decodes the sequence of binary segments here
        // <https://gitlab.freedesktop.org/freetype/freetype/-/blob/80a507a6b8e3d2906ad2c8ba69329bd2fb2a85ef/src/type1/t1parse.c#L286>
        if tag != PFB_BINARY_SEGMENT_TAG {
            return None;
        }
        // Skip tag and size bytes
        let start = pos + 6;
        let end = start + len as usize;
        let segment = data.get(start..end)?;
        pos = end;
        Some(segment)
    })
}

/// Helper to find the position of the data following the 'eexec' token.
///
/// Unsurprisingly, more complicated than it should be.
fn find_eexec_data(data: &[u8]) -> Option<usize> {
    // Use a parser to avoid catching "eexec" in a comment or string
    // which apparently occurs in some fonts.
    let mut parser = Parser::new(data);
    while let Some(token) = parser.next() {
        if token != Token::Raw(b"eexec") {
            continue;
        }
        let mut start = parser.pos;
        // FreeType has some unfun logic for skipping whitespace
        // after the eexec token
        // <https://gitlab.freedesktop.org/freetype/freetype/-/blob/80a507a6b8e3d2906ad2c8ba69329bd2fb2a85ef/src/type1/t1parse.c#L382>
        while start < data.len() {
            match data[start] {
                b' ' | b'\t' | b'\n' => {}
                b'\r' => {
                    // Only stop at \r if it is not followed by \n
                    if data.get(start + 1) != Some(&b'\n') {
                        start += 1;
                        break;
                    }
                }
                _ => break,
            }
            start += 1;
        }
        if start == data.len() {
            // eexec not properly terminated
            return None;
        }
        return Some(start);
    }
    None
}

/// Converts hex formatted data to associated bytes.
///
/// See <https://gitlab.freedesktop.org/freetype/freetype/-/blob/80a507a6b8e3d2906ad2c8ba69329bd2fb2a85ef/src/psaux/psconv.c#L464>
fn decode_hex(mut bytes: impl Iterator<Item = u8>) -> impl Iterator<Item = u8> {
    /// Converts digits (as ASCII characters) into integer values.
    const DIGIT_TO_NUM: [i8; 128] = [
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, -1, -1, -1, -1, -1, -1, -1, 10, 11, 12, 13, 14, 15,
        16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, -1, -1, -1,
        -1, -1, -1, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29,
        30, 31, 32, 33, 34, 35, -1, -1, -1, -1, -1,
    ];
    let mut pad = 0x1_u32;
    core::iter::from_fn(move || {
        loop {
            let Some(c) = bytes.next() else {
                break;
            };
            if is_whitespace(c) {
                continue;
            }
            if c >= 0x80 {
                break;
            }
            let c = DIGIT_TO_NUM[(c & 0x7F) as usize] as u32;
            if c >= 16 {
                break;
            }
            pad = (pad << 4) | c;
            if pad & 0x100 != 0 {
                let res = pad as u8;
                pad = 0x1;
                return Some(res);
            } else {
                continue;
            }
        }
        if pad != 0x1 {
            let res = (pad << 4) as u8;
            pad = 0x1;
            return Some(res);
        }
        None
    })
}

/// Decryption seed for eexec segment.
const EEXEC_SEED: u32 = 55665;

/// Decryption seed for charstring (and subroutine) data.
const CHARSTRING_SEED: u32 = 4330;

/// Returns an iterator yielding the decrypted bytes.
///
/// See <https://gitlab.freedesktop.org/freetype/freetype/-/blob/80a507a6b8e3d2906ad2c8ba69329bd2fb2a85ef/src/psaux/psconv.c#L557>
fn decrypt(bytes: impl Iterator<Item = u8>, mut seed: u32) -> impl Iterator<Item = u8> {
    bytes.map(move |b| {
        let b = b as u32;
        let plain = b ^ (seed >> 8);
        seed = b.wrapping_add(seed).wrapping_mul(52845).wrapping_add(22719) & 0xFFFF;
        plain as u8
    })
}

fn is_whitespace(c: u8) -> bool {
    if c <= 32 {
        return matches!(c, b' ' | b'\n' | b'\r' | b'\t' | b'\0' | 0x0C);
    }
    false
}

/// Characters that always delimit tokens.
///
/// See <https://gitlab.freedesktop.org/freetype/freetype/-/blob/80a507a6b8e3d2906ad2c8ba69329bd2fb2a85ef/include/freetype/internal/psaux.h#L1398>
fn is_special(c: u8) -> bool {
    matches!(
        c,
        b'(' | b')' | b'<' | b'>' | b'[' | b']' | b'{' | b'}' | b'/' | b'%'
    )
}

fn is_special_or_whitespace(c: u8) -> bool {
    is_special(c) || is_whitespace(c)
}

#[derive(Copy, Clone, PartialEq, Eq, Debug)]
enum Token<'a> {
    /// Integers
    Int(i64),
    /// Literal strings, delimited by ()
    LitString(&'a [u8]),
    /// Hex strings, delimited by <>
    HexString(&'a [u8]),
    /// Procedures, delimited by {}
    Proc(&'a [u8]),
    /// Binary blobs
    Binary(&'a [u8]),
    /// Names, preceded by /
    Name(&'a [u8]),
    /// All other raw tokens (identifiers and self-delimiting punctuation)
    Raw(&'a [u8]),
}

/// Collection of subroutines.
#[derive(Default)]
struct Subrs {
    /// Packed data for all subroutines.
    data: Vec<u8>,
    /// Index mapping subroutine number to range in the packed data. Sorted
    /// by subroutine number.
    index: Vec<(u32, Range<usize>)>,
    /// If true, subroutine number == index so we don't need to
    /// bsearch.
    is_dense: bool,
}

impl Subrs {
    fn get(&self, index: u32) -> Option<&[u8]> {
        let entry_idx = if self.is_dense {
            index as usize
        } else {
            self.index.binary_search_by_key(&index, |e| e.0).ok()?
        };
        self.data.get(self.index.get(entry_idx)?.1.clone())
    }
}

struct CharstringEntry {
    name: Range<usize>,
    data: Range<usize>,
}

/// Collection of charstrings.
#[derive(Default)]
struct Charstrings {
    /// Packed data for all charstrings.
    data: Vec<u8>,
    /// Packed data for all glyph names.
    names: Vec<u8>,
    /// Index containing all charstrings.
    index: Vec<CharstringEntry>,
    /// If notdef was remapped, holds the original index of the notdef
    /// charstring.
    orig_notdef_index: Option<usize>,
}

impl Charstrings {
    fn num_glyphs(&self) -> u32 {
        self.index.len() as u32
    }

    fn get(&self, index: u32) -> Option<&[u8]> {
        self.data.get(self.index.get(index as usize)?.data.clone())
    }

    fn name(&self, index: u32) -> Option<&str> {
        core::str::from_utf8(
            self.names
                .get(self.index.get(index as usize)?.name.clone())?,
        )
        .ok()
    }

    fn index_for_name(&self, name: &str) -> Option<u32> {
        let name = name.as_bytes();
        for (idx, entry) in self.index.iter().enumerate() {
            if self.names.get(entry.name.clone()) == Some(name) {
                return Some(idx as u32);
            }
        }
        None
    }

    fn push(&mut self, name: &[u8], data: &[u8], len_iv: i64) {
        let start = self.data.len();
        if len_iv >= 0 {
            // use decryption; skip first len_iv bytes
            self.data
                .extend(decrypt(data.iter().copied(), CHARSTRING_SEED).skip(len_iv as usize));
        } else {
            // just add the data
            self.data.extend_from_slice(data);
        }
        let end = self.data.len();
        let name_start = self.names.len();
        self.names.extend_from_slice(name);
        let name_end = self.names.len();
        self.index.push(CharstringEntry {
            name: name_start..name_end,
            data: start..end,
        });
    }
}

/// Encoding that maps characters to glyph identifiers.
#[derive(PartialEq, Debug)]
enum Encoding {
    Predefined(PredefinedEncoding),
    Custom(Vec<GlyphId>),
}

/// Simulated .notdef glyph, same as FreeType:
///
/// 0 333 hsbw endchar
///
/// See <https://gitlab.freedesktop.org/freetype/freetype/-/blob/80a507a6b8e3d2906ad2c8ba69329bd2fb2a85ef/src/type1/t1load.c#L2192>
const NOTDEF_GLYPH: &[u8] = &[0x8B, 0xF7, 0xE1, 0x0D, 0x0E];

#[derive(Clone)]
struct Parser<'a> {
    data: &'a [u8],
    pos: usize,
}

impl<'a> Parser<'a> {
    fn new(data: &'a [u8]) -> Self {
        Self { data, pos: 0 }
    }

    fn next(&mut self) -> Option<Token<'a>> {
        // Roughly follows the logic of ps_parser_skip_PS_token
        // <https://gitlab.freedesktop.org/freetype/freetype/-/blob/80a507a6b8e3d2906ad2c8ba69329bd2fb2a85ef/src/psaux/psobjs.c#L482>
        loop {
            self.skip_whitespace()?;
            let start = self.pos;
            let c = self.next_byte()?;
            match c {
                // Line comment
                b'%' => self.skip_line(),
                // Procedures
                b'{' => return self.read_proc(start),
                // Literal strings
                b'(' => return self.read_lit_string(start),
                b'<' => {
                    if self.peek_byte() == Some(b'<') {
                        // Just ignore these
                        self.pos += 1;
                        continue;
                    }
                    // Hex string: hex digits and whitespace
                    return self.read_hex_string(start);
                }
                b'>' => {
                    // We consume single '>' when parsing hex strings so a
                    // double >> is expected here
                    if self.next_byte()? != b'>' {
                        return None;
                    }
                }
                // Name
                b'/' => {
                    if let Some(c) = self.peek_byte() {
                        if is_whitespace(c) || is_special(c) {
                            if !is_special(c) {
                                self.pos += 1;
                            }
                            return Some(Token::Name(&[]));
                        } else {
                            let count = self.skip_until(|c| is_whitespace(c) || is_special(c));
                            return self.data.get(start + 1..start + count).map(Token::Name);
                        }
                    }
                }
                // Brackets
                b'[' | b']' => {
                    let data = self.data.get(start..start + 1)?;
                    return Some(Token::Raw(data));
                }
                _ => {
                    let count = self.skip_until(is_special_or_whitespace);
                    let content = self.data.get(start..start + count)?;
                    // Look for numbers but don't try to parse fractional
                    // values since we want to handle those with special
                    // precision
                    if (c.is_ascii_digit() || c == b'-') && !content.contains(&b'.') {
                        if let Some(int) = decode_int(content) {
                            // HACK: if we have an int followed by RD or -|
                            // then is a binary blob in Type1. Hack because
                            // this is not actually how PostScript works
                            // but Type1 fonts define /RD procs and this
                            // pattern is used by FreeType.
                            // <https://gitlab.freedesktop.org/freetype/freetype/-/blob/80a507a6b8e3d2906ad2c8ba69329bd2fb2a85ef/src/type1/t1load.c#L1351>
                            if matches!(
                                self.peek(),
                                Some(Token::Raw(b"RD")) | Some(Token::Raw(b"-|"))
                            ) {
                                // skip the token
                                self.next();
                                // and a single space
                                self.pos += 1;
                                // read the internal data
                                let data = self.read_bytes(int as usize)?;
                                // and skip the terminator (usually ND, NP or |-)
                                self.next();
                                return Some(Token::Binary(data));
                            }
                            return Some(Token::Int(int));
                        }
                    }
                    return Some(Token::Raw(content));
                }
            }
        }
    }

    fn peek(&self) -> Option<Token<'a>> {
        self.clone().next()
    }

    fn accept(&mut self, token: Token) -> bool {
        if self.peek() == Some(token) {
            self.next();
            true
        } else {
            false
        }
    }

    fn expect(&mut self, token: Token) -> Option<()> {
        (self.next()? == token).then_some(())
    }

    fn next_byte(&mut self) -> Option<u8> {
        let byte = self.peek_byte()?;
        self.pos += 1;
        Some(byte)
    }

    fn peek_byte(&self) -> Option<u8> {
        self.data.get(self.pos).copied()
    }

    fn read_bytes(&mut self, len: usize) -> Option<&'a [u8]> {
        let end = self.pos.checked_add(len)?;
        let content = self.data.get(self.pos..end)?;
        self.pos = end;
        Some(content)
    }

    fn skip_whitespace(&mut self) -> Option<()> {
        while is_whitespace(*self.data.get(self.pos)?) {
            self.pos += 1;
        }
        Some(())
    }

    fn skip_line(&mut self) {
        while let Some(c) = self.next_byte() {
            if c == b'\n' || c == b'\r' {
                break;
            }
        }
    }

    fn skip_until(&mut self, f: impl Fn(u8) -> bool) -> usize {
        let mut count = 0;
        while let Some(byte) = self.peek_byte() {
            if f(byte) {
                break;
            }
            self.pos += 1;
            count += 1;
        }
        count + 1
    }

    fn read_proc(&mut self, start: usize) -> Option<Token<'a>> {
        while self.next_byte()? != b'}' {
            // This handles nested procedures
            self.next()?;
            self.skip_whitespace();
        }
        let end = self.pos;
        if self.data.get(end - 1) != Some(&b'}') {
            // unterminated procedure
            return None;
        }
        Some(Token::Proc(self.data.get(start + 1..end - 1)?))
    }

    fn read_lit_string(&mut self, start: usize) -> Option<Token<'a>> {
        let mut nest_depth = 1;
        while let Some(c) = self.next_byte() {
            match c {
                b'(' => nest_depth += 1,
                b')' => {
                    nest_depth -= 1;
                    if nest_depth == 0 {
                        break;
                    }
                }
                // Escape sequence
                b'\\' => {
                    // Just eat the next byte. We only care
                    // about avoiding \( and \) anyway.
                    self.next_byte()?;
                }
                _ => {}
            }
        }
        if nest_depth != 0 {
            // unterminated string
            return None;
        }
        let end = self.pos;
        self.pos += 1;
        Some(Token::LitString(self.data.get(start + 1..end - 1)?))
    }

    fn read_hex_string(&mut self, start: usize) -> Option<Token<'a>> {
        while let Some(c) = self.next_byte() {
            if !is_whitespace(c) && !c.is_ascii_hexdigit() {
                break;
            }
        }
        let end = self.pos;
        if self.data.get(end - 1) != Some(&b'>') {
            // unterminated hex string
            return None;
        }
        Some(Token::HexString(self.data.get(start + 1..end - 1)?))
    }

    fn read_int(&mut self) -> Option<i64> {
        self.next().and_then(|t| match t {
            Token::Int(n) => Some(n),
            _ => None,
        })
    }
}

impl Parser<'_> {
    /// Parse a font matrix.
    ///
    /// Like FreeType, this is designed assuming a upem of 1000 and produces
    /// an identity matrix in that case. This is, the result is scaled such
    /// that 0.001 yields a value of 1.0.
    ///
    /// See <https://gitlab.freedesktop.org/freetype/freetype/-/blob/80a507a6b8e3d2906ad2c8ba69329bd2fb2a85ef/src/type1/t1load.c#L1403>
    fn read_font_matrix(&mut self) -> Option<ScaledFontMatrix> {
        let mut components = [Fixed::ZERO; 6];
        // accept [ or { to match FreeType
        if !self.accept(Token::Raw(b"[")) {
            self.expect(Token::Raw(b"{"))?;
        }
        // read all components
        for component in &mut components {
            *component = match self.next()? {
                Token::Int(int) => Fixed::from_i32((int as i32).checked_mul(1000)?),
                Token::Raw(bytes) => decode_fixed(bytes, 3)?,
                _ => return None,
            }
        }
        // FreeType doesn't validate the closing delimiter, so just skip
        self.next()?;
        let temp_scale = components[3].abs();
        if temp_scale == Fixed::ZERO {
            return None;
        }
        let mut upem = 1000;
        if temp_scale != Fixed::ONE {
            upem = (Fixed::from_bits(1000) / temp_scale).to_bits();
            components[0] /= temp_scale;
            components[1] /= temp_scale;
            components[2] /= temp_scale;
            // don't scale components[3]
            components[4] /= temp_scale;
            components[5] /= temp_scale;
            if components[3] < Fixed::ZERO {
                components[3] = -Fixed::ONE;
            } else {
                components[3] = Fixed::ONE;
            }
        }
        // offsets must be expressed in integer font units
        for offset in components.iter_mut().skip(4) {
            *offset = Fixed::from_bits(offset.to_bits() >> 16);
        }
        let matrix = FontMatrix(components);
        if matrix.is_degenerate() {
            return None;
        }
        Some(ScaledFontMatrix {
            matrix,
            scale: upem,
        })
    }

    /// Parse the set of subroutines.
    ///
    /// The `len_iv` parameter defines the number of prefix padding bytes for
    /// encrypted data. If < 0, then the data is not encrypted.
    ///
    /// See <https://gitlab.freedesktop.org/freetype/freetype/-/blob/80a507a6b8e3d2906ad2c8ba69329bd2fb2a85ef/src/type1/t1load.c#L1720>
    fn read_subrs(&mut self, len_iv: i64) -> Option<Subrs> {
        let mut subrs = Subrs::default();
        let _: usize = match self.next()? {
            Token::Raw(b"[") => {
                // Just an empty array
                self.expect(Token::Raw(b"]"))?;
                return Some(subrs);
            }
            Token::Int(n) => n.try_into().ok()?,
            _ => return None,
        };
        self.expect(Token::Raw(b"array"))?;
        let mut is_dense = true;
        // The pattern for each subroutine is `dup <subr_num> <data>`
        while self.accept(Token::Raw(b"dup")) {
            let (Token::Int(n), Token::Binary(data)) = (self.next()?, self.next()?) else {
                return None;
            };
            // There might be an additional put token following the binary data
            self.accept(Token::Raw(b"put"));
            let subr_num: u32 = n.try_into().ok()?;
            if subr_num as usize != subrs.index.len() {
                is_dense = false;
            }
            let start = subrs.data.len();
            if len_iv >= 0 {
                // use decryption; skip first len_iv bytes
                subrs
                    .data
                    .extend(decrypt(data.iter().copied(), CHARSTRING_SEED).skip(len_iv as usize));
            } else {
                // just add the data
                subrs.data.extend_from_slice(data);
            }
            let end = subrs.data.len();
            subrs.index.push((subr_num, start..end));
        }
        // If we don't have a dense set, sort the index by number
        if !is_dense {
            subrs.index.sort_unstable_by_key(|(n, ..)| *n);
        }
        subrs.is_dense = is_dense;
        subrs.data.shrink_to_fit();
        subrs.index.shrink_to_fit();
        Some(subrs)
    }

    /// Parse the set of charstrings.
    ///
    /// The `len_iv` parameter defines the number of prefix padding bytes for
    /// encrypted data. If < 0, then the data is not encrypted.
    ///
    /// See <https://gitlab.freedesktop.org/freetype/freetype/-/blob/80a507a6b8e3d2906ad2c8ba69329bd2fb2a85ef/src/type1/t1load.c#L1919>
    fn read_charstrings(&mut self, len_iv: i64) -> Option<Charstrings> {
        let mut charstrings = Charstrings::default();
        let _: usize = match self.next()? {
            Token::Int(n) => n.try_into().ok()?,
            _ => return None,
        };
        let mut notdef_idx = None;
        while let Some(token) = self.next() {
            let name = match token {
                // Stop when we find a `def` or `end` keyword.
                // The ugliness matches the FT logic to handle some malformed
                // fonts:
                // <https://gitlab.freedesktop.org/freetype/freetype/-/blob/80a507a6b8e3d2906ad2c8ba69329bd2fb2a85ef/src/type1/t1load.c#L2006>
                Token::Raw(b"end") => {
                    if self
                        .peek_byte()
                        .map(is_special_or_whitespace)
                        .unwrap_or_default()
                    {
                        break;
                    } else {
                        continue;
                    }
                }
                Token::Raw(b"def") => {
                    // but ignore `def` if no charstring has been seen
                    if self
                        .peek_byte()
                        .map(is_special_or_whitespace)
                        .unwrap_or_default()
                        && !charstrings.index.is_empty()
                    {
                        break;
                    } else {
                        continue;
                    }
                }
                Token::Name(name) => name,
                _ => continue,
            };
            if name == b".notdef" {
                notdef_idx = Some(charstrings.index.len());
            }
            let Token::Binary(data) = self.next()? else {
                return None;
            };
            charstrings.push(name, data, len_iv);
        }
        match notdef_idx {
            Some(0) => {
                // .notdef found and at correct location
            }
            Some(idx) => {
                // .notdef found but at incorrect location. Swap with the
                // glyph at 0
                charstrings.index.swap(0, idx);
                charstrings.orig_notdef_index = Some(idx);
            }
            None => {
                // .notdef not found. Add it to the end and then swap with
                // the glyph at 0
                let idx = charstrings.index.len();
                charstrings.push(b".notdef", NOTDEF_GLYPH, -1);
                charstrings.index.swap(0, idx);
                charstrings.orig_notdef_index = Some(idx);
            }
        }
        charstrings.data.shrink_to_fit();
        charstrings.names.shrink_to_fit();
        charstrings.index.shrink_to_fit();
        Some(charstrings)
    }

    fn read_weight_vector(&mut self) -> Option<Vec<Fixed>> {
        self.accept(Token::Raw(b"["));
        let mut weights = Vec::new();
        while let Some(token) = self.next() {
            match token {
                Token::Raw(b"]") => break,
                Token::Int(val) => weights.push(Fixed::from_i32(val as _)),
                Token::Raw(raw) => weights.push(decode_fixed(raw, 0)?),
                _ => return None,
            }
        }
        Some(weights)
    }

    /// Parse the encoding.
    ///
    /// See <https://gitlab.freedesktop.org/freetype/freetype/-/blob/80a507a6b8e3d2906ad2c8ba69329bd2fb2a85ef/src/type1/t1load.c#L1474>
    fn read_encoding(&mut self, charstrings: &Charstrings) -> Option<Encoding> {
        match self.next()? {
            // Array of names where index == character code
            Token::Raw(b"[") => {
                let mut map = Vec::new();
                // Should always be 256 entries but preset values to notdef
                map.resize(256, GlyphId::NOTDEF);
                self.read_dense_encoding(|idx, name| {
                    if let Some((slot, gid)) = map
                        .get_mut(idx as usize)
                        .zip(charstrings.index_for_name(name))
                    {
                        *slot = gid.into();
                    }
                });
                Some(Encoding::Custom(map))
            }
            // Map of index to glyph name
            Token::Int(count) => {
                // We're limited to 256 character codes
                // <https://gitlab.freedesktop.org/freetype/freetype/-/blob/80a507a6b8e3d2906ad2c8ba69329bd2fb2a85ef/src/type1/t1load.c#L1518>
                let count: usize = count.clamp(0, 256) as usize;
                let mut map = Vec::new();
                // Start with all glyphs mapped to notdef
                map.resize(count, GlyphId::NOTDEF);
                self.read_sparse_encoding(|idx, name| {
                    if let Some((slot, gid)) = map
                        .get_mut(idx as usize)
                        .zip(charstrings.index_for_name(name))
                    {
                        *slot = gid.into();
                    }
                });
                Some(Encoding::Custom(map))
            }
            Token::Raw(b"StandardEncoding") => {
                Some(Encoding::Predefined(PredefinedEncoding::Standard))
            }
            Token::Raw(b"ExpertEncoding") => Some(Encoding::Predefined(PredefinedEncoding::Expert)),
            Token::Raw(b"ISOLatin1Encoding") => {
                Some(Encoding::Predefined(PredefinedEncoding::IsoLatin1))
            }
            _ => None,
        }
    }

    /// Returns a custom encoding defined by an array of `/<name>`, where
    /// the index represents the character code, and invokes the given
    /// callback for each.
    fn read_dense_encoding(&mut self, mut f: impl FnMut(i64, &str)) -> Option<()> {
        // Eat the opening brace if present
        self.accept(Token::Raw(b"["));
        // Always expect 256 entries
        // <https://gitlab.freedesktop.org/freetype/freetype/-/blob/80a507a6b8e3d2906ad2c8ba69329bd2fb2a85ef/src/type1/t1load.c#L1510>
        let mut idx = 0;
        while let Some(token) = self.next() {
            match token {
                Token::Raw(b"]") => break,
                Token::Name(name) => {
                    let code = idx;
                    idx += 1;
                    let Ok(name) = core::str::from_utf8(name) else {
                        continue;
                    };
                    f(code, name);
                }
                _ => {
                    // FreeType fails if missing a literal name here
                    return None;
                }
            }
        }
        Some(())
    }

    /// Reads a custom encoding defined by a map of `<charcode> /<name>`
    /// and invokes the given callback for each.
    fn read_sparse_encoding(&mut self, mut f: impl FnMut(i64, &str)) -> Option<()> {
        while let Some(token) = self.next() {
            match token {
                // The 'def' keyword ends the mapping
                Token::Raw(b"def") => break,
                Token::Int(code) => {
                    // read the name
                    let Some(Token::Name(name)) = self.next() else {
                        continue;
                    };
                    let Ok(name) = core::str::from_utf8(name) else {
                        continue;
                    };
                    f(code, name);
                }
                _ => {}
            }
        }
        Some(())
    }
}

/// Decode an integer, optionally with a base.
///
/// See <https://gitlab.freedesktop.org/freetype/freetype/-/blob/80a507a6b8e3d2906ad2c8ba69329bd2fb2a85ef/src/psaux/psconv.c#L161>
fn decode_int(bytes: &[u8]) -> Option<i64> {
    let s = std::str::from_utf8(bytes).ok()?;
    if let Some(hash_idx) = s.find('#') {
        if hash_idx == 1 || hash_idx == 2 {
            // It's a radix number, like 8#40.
            let radix_str = s.get(0..hash_idx)?;
            let number_str = s.get(hash_idx + 1..)?;
            let radix = radix_str
                .parse::<u32>()
                .ok()
                .filter(|n| (2..=36).contains(n))?;
            i64::from_str_radix(number_str, radix).ok()
        } else {
            s.parse::<i64>().ok()
        }
    } else {
        s.parse::<i64>().ok()
    }
}

/// Decode an integer at the given position, returning the value and the
/// index of the position following the decoded integer.
fn decode_int_prefix(bytes: &[u8], start: usize) -> Option<(i64, usize)> {
    let tail = bytes.get(start..)?;
    let end = tail
        .iter()
        .position(|c| *c != b'-' && !c.is_ascii_digit())
        .unwrap_or(tail.len());
    let int = decode_int(tail.get(..end)?)?;
    Some((int, start + end))
}

/// Decode a fixed point value, scaling to a specific power of
/// ten.
///
/// See <https://gitlab.freedesktop.org/freetype/freetype/-/blob/80a507a6b8e3d2906ad2c8ba69329bd2fb2a85ef/src/psaux/psconv.c#L195>
fn decode_fixed(bytes: &[u8], mut power_ten: i32) -> Option<Fixed> {
    const LIMIT: i32 = 0xCCCCCCC;
    let mut idx = 0;
    let &first = bytes.get(idx)?;
    let sign = if first == b'-' || first == b'+' {
        idx += 1;
        if first == b'-' {
            -1
        } else {
            1
        }
    } else {
        1
    };
    let overflow = || Some(Fixed::from_bits(0x7FFFFFFF * sign));
    let mut integral = 0;
    if *bytes.get(idx)? != b'.' {
        let (int, end_idx) = decode_int_prefix(bytes, idx)?;
        if int > 0x7FFF {
            return overflow();
        }
        integral = (int << 16) as i32;
        idx = end_idx;
    }
    let mut decimal = 0;
    let mut divider = 1;
    if bytes.get(idx) == Some(&b'.') {
        idx += 1;
        while let Some(byte) = bytes.get(idx).copied() {
            if !byte.is_ascii_digit() {
                break;
            }
            let digit = (byte - b'0') as i32;
            if divider < LIMIT && decimal < LIMIT {
                decimal = decimal * 10 + digit;
                if integral == 0 && power_ten > 0 {
                    power_ten -= 1;
                } else {
                    divider *= 10;
                }
            }
            idx += 1;
        }
    }
    if bytes.get(idx).map(|b| b.to_ascii_lowercase()) == Some(b'e') {
        idx += 1;
        let (exponent, _) = decode_int_prefix(bytes, idx)?;
        if exponent > 1000 {
            return overflow();
        } else if exponent < -1000 {
            // underflow
            return Some(Fixed::ZERO);
        } else {
            power_ten = power_ten.checked_add(exponent as i32)?;
        }
    }
    if integral == 0 && decimal == 0 {
        return Some(Fixed::ZERO);
    }
    while power_ten > 0 {
        if integral >= LIMIT {
            return overflow();
        }
        integral *= 10;
        if decimal >= LIMIT {
            if divider == 1 {
                return overflow();
            }
            divider /= 10;
        } else {
            decimal *= 10;
        }
        power_ten -= 1;
    }
    while power_ten < 0 {
        integral /= 10;
        if divider < LIMIT {
            divider *= 10;
        } else {
            decimal /= 10;
        }
        if integral == 0 && decimal == 0 {
            return Some(Fixed::ZERO);
        }
        power_ten += 1;
    }
    if decimal != 0 {
        decimal = (Fixed::from_bits(decimal) / Fixed::from_bits(divider)).to_bits();
        integral += decimal;
    }
    Some(Fixed::from_bits(integral * sign))
}

#[cfg(test)]
mod tests {
    use super::charstring::test_helpers::*;
    use super::*;

    #[test]
    fn pfb_tags() {
        // Text segment tag
        let data = [0x80, 0x01, 0x01, 0x02, 0x00, 0x00];
        let (tag, len) = decode_pfb_tag(&data, 0).unwrap();
        assert_eq!(tag, PFB_TEXT_SEGMENT_TAG);
        assert_eq!(len, 513);
        // Binary segment tag
        let data = [0x80, 0x02, 0x01, 0x03, 0x00, 0x00];
        let (tag, len) = decode_pfb_tag(&data, 0).unwrap();
        assert_eq!(tag, PFB_BINARY_SEGMENT_TAG);
        assert_eq!(len, 769);
        // Invalid tag
        let data = [0x00; 6];
        assert!(decode_pfb_tag(&data, 0).is_none());
        // Not enough data
        let data = [0x00; 5];
        assert!(decode_pfb_tag(&data, 0).is_none());
    }

    #[test]
    fn pfb_segments() {
        let segments = [
            vec![0x01; 8],
            vec![0x02; 10],
            vec![0x03; 4],
            vec![0x04; 255],
        ];
        // Write each segment to a buffer
        let mut buf = vec![];
        for segment in &segments {
            buf.push(0x80);
            buf.push(0x02);
            buf.push(segment.len() as u8);
            buf.extend_from_slice(&[0; 3]);
            for byte in segment {
                buf.push(*byte);
            }
        }
        // Now parse and compare
        let mut parsed_count = 0;
        for (parsed, expected) in decode_pfb_binary_segments(&buf).zip(&segments) {
            assert_eq!(parsed, expected);
            parsed_count += 1;
        }
        assert_eq!(parsed_count, segments.len());
    }

    #[test]
    fn hex_decode() {
        check_hex_decode(
            b"743F8413F3636CA85A9FFEFB50B4BB27",
            &[
                116, 63, 132, 19, 243, 99, 108, 168, 90, 159, 254, 251, 80, 180, 187, 39,
            ],
        );
    }

    #[test]
    fn hex_decode_ignores_whitespace() {
        check_hex_decode(
            b"743F 8413F3636C\nA85A9FFEF\tB50B     4BB27",
            &[
                116, 63, 132, 19, 243, 99, 108, 168, 90, 159, 254, 251, 80, 180, 187, 39,
            ],
        );
    }

    #[test]
    fn hex_decode_truncate() {
        check_hex_decode(b"743F.8413F3636CA85A9FFEFB50B4BB27", &[116, 63]);
    }

    #[test]
    fn hex_decode_odd_chars() {
        check_hex_decode(b"743", &[116, 48]);
    }

    #[track_caller]
    fn check_hex_decode(hex: &[u8], expected: &[u8]) {
        let decoded = decode_hex(hex.iter().copied()).collect::<Vec<_>>();
        assert_eq!(decoded, expected);
    }

    #[test]
    fn decrypt_bytes() {
        let cipher = [
            0x74, 0x3f, 0x84, 0x13, 0xf3, 0x63, 0x6c, 0xa8, 0x5a, 0x9f, 0xfe, 0xfb, 0x50, 0xb4,
            0xbb, 0x27,
        ];
        let plain = decrypt(cipher.iter().copied(), EEXEC_SEED).collect::<Vec<_>>();
        // First 4 bytes are random garbage
        assert_eq!(&plain[4..], b"dup\n/Private");
    }

    #[test]
    fn find_eexec() {
        // Just a space
        assert_eq!(
            find_eexec_data(b"dup\n/Private\ncurrentfile eexec *&&FW"),
            Some(31)
        );
        // Multiple spaces
        assert_eq!(
            find_eexec_data(b"dup\n/Private\ncurrentfile eexec     *&&FW"),
            Some(35)
        );
        // New lines
        assert_eq!(
            find_eexec_data(b"dup\n/Private\ncurrentfile eexec\n\n*&&FW"),
            Some(32)
        );
        // Only skip \r when it precedes \n
        assert_eq!(
            find_eexec_data(b"dup\n/Private\ncurrentfile eexec\r\n\r*&&FW"),
            Some(33)
        );
        // Skip eexec in comments and strings
        assert_eq!(
            find_eexec_data(b"% eexec in comment\n(eexec in string) currentfile eexec $$$$"),
            Some(55)
        );
        // No eexec
        assert!(find_eexec_data(b"% eexec in comment\n(eexec in string) currentfile").is_none());
    }

    #[test]
    fn read_pfb_raw_dicts() {
        let dicts = RawDicts::new(font_test_data::type1::NOTO_SERIF_REGULAR_SUBSET_PFB).unwrap();
        check_noto_serif_base(dicts.base);
        check_noto_serif_private(&dicts.private);
    }

    #[test]
    fn read_pfa_raw_dicts() {
        let dicts = RawDicts::new(font_test_data::type1::NOTO_SERIF_REGULAR_SUBSET_PFA).unwrap();
        check_noto_serif_base(dicts.base);
        check_noto_serif_private(&dicts.private);
    }

    fn check_noto_serif_base(base: &[u8]) {
        const EXPECTED_PREFIX: &str = r#"%!PS-AdobeFont-1.0: NotoSerif-Regular 2.007; ttfautohint (v1.8) -l 8 -r 50 -G 200 -x 14 -D latn -f none -a qsq -X ""
%%Title: NotoSerif-Regular
%Version: 2.007; ttfautohint (v1.8) -l 8 -r 50 -G 200 -x 14 -D latn -f none -a qsq -X ""
%%CreationDate: Tue Feb 10 16:07:25 2026
%%Creator: www-data
%Copyright: Copyright 2015-2021 Google LLC. All Rights Reserved.
% Generated by FontForge 20190801 (http://fontforge.sf.net/)
%%EndComments

10 dict begin
/FontType 1 def
/FontMatrix [0.001 0 0 0.001 0 0 ]readonly def
/FontName /NotoSerif-Regular def
/FontBBox {5 0 989 775 }readonly def
"#;
        assert!(base.starts_with(EXPECTED_PREFIX.as_bytes()));
    }

    fn check_noto_serif_private(private: &[u8]) {
        const EXPECTED_PREFIX: &str = r#"dup
/Private 8 dict dup begin
/RD{string currentfile exch readstring pop}executeonly def
/ND{noaccess def}executeonly def
/NP{noaccess put}executeonly def
/MinFeature{16 16}ND
/password 5839 def
/BlueValues [0 0 536 536 714 714 770 770 ]ND
/OtherSubrs"#;
        assert!(private.starts_with(EXPECTED_PREFIX.as_bytes()))
    }

    #[test]
    fn parse_ints() {
        check_tokens(
            "% a comment\n20 -30 2#1011 10#-5 %another!\r 16#fC",
            &[
                Token::Int(20),
                Token::Int(-30),
                Token::Int(11),
                Token::Int(-5),
                Token::Int(252),
            ],
        );
    }

    #[test]
    fn parse_strings() {
        check_tokens(
            "(string (nested) 1) % and a hex string:\n <DEAD BEEF>",
            &[
                Token::LitString(b"string (nested) 1"),
                Token::HexString(b"DEAD BEEF"),
            ],
        );
    }

    #[test]
    fn parse_unterminated_strings() {
        check_tokens("(string (nested) 1", &[]);
        check_tokens("<DEAD BEEF", &[]);
    }

    #[test]
    fn parse_procs() {
        check_tokens(
            "{a {nested 20} proc } % and a\n {simple proc}",
            &[
                Token::Proc(b"a {nested 20} proc "),
                Token::Proc(b"simple proc"),
            ],
        );
    }

    #[test]
    fn parse_unterminated_procs() {
        check_tokens("{a {nested 20} proc", &[]);
    }

    #[test]
    fn parse_names() {
        check_tokens(
            "/FontMatrix\r %comment\n /CharStrings",
            &[Token::Name(b"FontMatrix"), Token::Name(b"CharStrings")],
        );
    }

    #[test]
    fn parse_binary_blobs() {
        check_tokens(
            "/.notdef 4 RD abcd ND\n5 11\n \t-| a83jnshf7 3 -|",
            &[
                // simulates a charstring: name followed by data
                Token::Name(b".notdef"),
                Token::Binary(b"abcd"),
                // simulates a subr: index followed by data
                Token::Int(5),
                Token::Binary(b"a83jnshf7 3"),
            ],
        )
    }

    #[test]
    fn parse_base_dict_prefix() {
        let dicts = RawDicts::new(font_test_data::type1::NOTO_SERIF_REGULAR_SUBSET_PFA).unwrap();
        let ts = parse_to_tokens(dicts.base);
        assert_eq!(
            &ts[..19],
            &[
                Token::Int(10),
                Token::Raw(b"dict"),
                Token::Raw(b"begin"),
                Token::Name(b"FontType"),
                Token::Int(1),
                Token::Raw(b"def"),
                Token::Name(b"FontMatrix"),
                Token::Raw(b"["),
                Token::Raw(b"0.001"),
                Token::Int(0),
                Token::Int(0),
                Token::Raw(b"0.001"),
                Token::Int(0),
                Token::Int(0),
                Token::Raw(b"]"),
                Token::Raw(b"readonly"),
                Token::Raw(b"def"),
                Token::Name(b"FontName"),
                Token::Name(b"NotoSerif-Regular"),
            ]
        );
    }

    #[track_caller]
    fn check_tokens(source: &str, expected: &[Token]) {
        let ts = parse_to_tokens(source.as_bytes());
        assert_eq!(ts, expected);
    }

    fn parse_to_tokens(data: &'_ [u8]) -> Vec<Token<'_>> {
        let mut tokens = vec![];
        let mut parser = Parser::new(data);
        while let Some(token) = parser.next() {
            tokens.push(token);
        }
        tokens
    }

    #[test]
    fn parse_fixed() {
        // Direct conversions (power_ten = 0)
        assert_eq!(decode_fixed(b"42.5", 0).unwrap(), Fixed::from_f64(42.5));
        assert_eq!(
            decode_fixed(b"0.0015", 0).unwrap(),
            Fixed::from_f64(0.001495361328125)
        );
        assert_eq!(
            decode_fixed(b"425.000e-1", 0).unwrap(),
            Fixed::from_f64(42.5)
        );
        assert_eq!(
            decode_fixed(b"1.5e-3", 0).unwrap(),
            Fixed::from_f64(0.001495361328125)
        );
        // Scaled by 1000 (power_ten = 3)
        assert_eq!(decode_fixed(b"1.5", 3).unwrap(), Fixed::from_f64(1500.0));
        assert_eq!(decode_fixed(b"0.001", 3).unwrap(), Fixed::from_f64(1.0));
        assert_eq!(
            decode_fixed(b"15000e-4", 3).unwrap(),
            Fixed::from_f64(1500.0)
        );
        assert_eq!(decode_fixed(b"1.000e-3", 3).unwrap(), Fixed::from_f64(1.0));
    }

    #[test]
    fn parse_font_matrix() {
        // Standard matrix for 1000 upem
        assert_eq!(
            Parser::new(b"[0.001 0 0 0.001 0 0]")
                .read_font_matrix()
                .unwrap()
                .matrix,
            FontMatrix([
                Fixed::ONE,
                Fixed::ZERO,
                Fixed::ZERO,
                Fixed::ONE,
                Fixed::ZERO,
                Fixed::ZERO
            ])
        );
        // Matrix with a stretch along the x axis and a small
        // offset
        assert_eq!(
            Parser::new(b"[0.002 0 0 0.001 1 2e1]")
                .read_font_matrix()
                .unwrap()
                .matrix,
            FontMatrix([
                Fixed::from_i32(2),
                Fixed::ZERO,
                Fixed::ZERO,
                Fixed::ONE,
                Fixed::from_bits(1000),
                Fixed::from_bits(20000)
            ])
        );
        // Matrix with modified upem
        assert_eq!(
            Parser::new(b"[0.001 0 0 0.0005 0.0 0.0]")
                .read_font_matrix()
                .unwrap(),
            ScaledFontMatrix {
                matrix: FontMatrix([
                    Fixed::from_i32(2),
                    Fixed::ZERO,
                    Fixed::ZERO,
                    Fixed::ONE,
                    Fixed::from_i32(0),
                    Fixed::from_i32(0)
                ]),
                scale: 2000,
            }
        );
    }

    #[test]
    fn parse_subrs() {
        let dicts = RawDicts::new(font_test_data::type1::NOTO_SERIF_REGULAR_SUBSET_PFA).unwrap();
        let mut parser = Parser::new(&dicts.private);
        let mut subrs = None;
        while let Some(token) = parser.next() {
            if let Token::Name(b"Subrs") = token {
                subrs = parser.read_subrs(4);
                break;
            }
        }
        let mut subrs = subrs.unwrap();
        // The decrypted subroutines extracted from FreeType
        let expected_subrs: [&[u8]; 5] = [
            &[142, 139, 12, 16, 12, 17, 12, 17, 12, 33, 11],
            &[139, 140, 12, 16, 11],
            &[139, 141, 12, 16, 11],
            &[11],
            &[140, 142, 12, 16, 12, 17, 10, 11],
        ];
        assert_eq!(subrs.index.len(), expected_subrs.len());
        assert!(subrs.is_dense);
        // These subrs are densely allocated but check binary search mode
        // as well
        for is_dense in [true, false] {
            subrs.is_dense = is_dense;
            for (idx, &expected) in expected_subrs.iter().enumerate() {
                let subr = subrs.get(idx as u32).unwrap();
                assert_eq!(subr, expected);
            }
        }
    }

    #[test]
    fn parse_empty_array_subrs() {
        let subrs = Parser::new(b"[ ]").read_subrs(4).unwrap();
        assert!(subrs.data.is_empty());
        assert!(subrs.index.is_empty());
    }

    #[test]
    fn parse_empty_subrs() {
        let subrs = Parser::new(b" 0 array\nND\n").read_subrs(4).unwrap();
        assert!(subrs.data.is_empty());
        assert!(subrs.index.is_empty());
    }

    #[test]
    fn parse_malformed_subrs() {
        assert!(Parser::new(b" 20 \nND\n").read_subrs(4).is_none());
    }

    #[test]
    fn parse_subrs_duplicate_def() {
        // Two definitions of subrs.. we want to keep the first
        // one which has two entries at 5 and 42
        let private = b"/Subrs 2 array dup 5 2 RD nd NP dup 42 2 RD ab NP ND\n/Subrs 1 array dup 0 2 RD xy NP ND";
        let font = Type1Font::from_dicts(b"", private).unwrap();
        assert_eq!(font.subrs.index.len(), 2);
        assert_eq!(font.subrs.index[0].0, 5);
        assert_eq!(font.subrs.index[1].0, 42);
    }

    #[test]
    fn parse_charstrings() {
        let dicts = RawDicts::new(font_test_data::type1::NOTO_SERIF_REGULAR_SUBSET_PFA).unwrap();
        let mut parser = Parser::new(&dicts.private);
        let mut charstrings = None;
        while let Some(token) = parser.next() {
            if let Token::Name(b"CharStrings") = token {
                charstrings = parser.read_charstrings(4);
                break;
            }
        }
        let charstrings = charstrings.unwrap();
        assert_eq!(charstrings.num_glyphs(), 9);
        assert!(charstrings.orig_notdef_index.is_none());
        let expected_names = [
            ".notdef",
            "H",
            "f",
            "i",
            "x",
            "f_f.liga",
            "f_f_i.liga",
            "f_i.liga",
            "H.c2sc",
        ];
        let names = (0..charstrings.num_glyphs())
            .map(|idx| charstrings.name(idx).unwrap())
            .collect::<Vec<_>>();
        assert_eq!(names, expected_names);
        // Prefix (up to 8 bytes), extracted from FreeType
        let expected_charstrings_prefix: [&[u8]; 9] = [
            &[139, 248, 236, 13, 14],
            &[177, 249, 173, 13, 139, 4, 247, 183],
            &[166, 248, 5, 13, 139, 4, 247, 201],
            &[162, 247, 212, 13, 247, 30, 249, 16],
            &[144, 248, 214, 13, 139, 4, 247, 130],
            &[166, 249, 88, 13, 139, 4, 247, 181],
            &[166, 250, 126, 13, 139, 4, 247, 181],
            &[166, 249, 43, 13, 139, 4, 247, 181],
            &[180, 249, 60, 13, 139, 4, 247, 141],
        ];
        for (idx, &expected) in expected_charstrings_prefix.iter().enumerate() {
            let charstring = charstrings.get(idx as u32).unwrap();
            assert_eq!(&charstring[..expected.len()], expected);
        }
    }

    #[test]
    fn parse_charstrings_duplicate_def() {
        // Two definitions of charstrings.. we want to keep the first
        // one which has three glyphs: .notdef, H and I
        let private = b"/CharStrings 2 /.notdef 2 RD nd ND /H 2 RD ab ND /I 2 RD cd ND def\n/CharStrings 1 /B 2 RD xy ND def";
        let font = Type1Font::from_dicts(b"", private).unwrap();
        assert_eq!(font.num_glyphs(), 3);
        assert_eq!(font.charstrings.name(0).unwrap(), ".notdef");
        assert_eq!(font.charstrings.name(1).unwrap(), "H");
        assert_eq!(font.charstrings.name(2).unwrap(), "I");
    }

    #[test]
    fn parse_charstrings_missing_notdef() {
        let mut parser = Parser::new(b"1 /H 2 RD ab ND /B 2 RD xy ND");
        let charstrings = parser.read_charstrings(-1).unwrap();
        assert_eq!(charstrings.num_glyphs(), 3);
        assert_eq!(charstrings.orig_notdef_index, Some(2));
        let expected_glyphs: &[(&str, &[u8])] =
            &[(".notdef", NOTDEF_GLYPH), ("B", b"xy"), ("H", b"ab")];
        check_charstrings(&charstrings, expected_glyphs);
    }

    #[test]
    fn parse_charstrings_notdef_moved() {
        let mut parser = Parser::new(b"1 /H 2 RD ab ND /.notdef 2 RD nd ND /B 2 RD xy ND");
        let charstrings = parser.read_charstrings(-1).unwrap();
        assert_eq!(charstrings.num_glyphs(), 3);
        assert_eq!(charstrings.orig_notdef_index, Some(1));
        let expected_glyphs: &[(&str, &[u8])] = &[(".notdef", b"nd"), ("H", b"ab"), ("B", b"xy")];
        check_charstrings(&charstrings, expected_glyphs);
    }

    #[track_caller]
    fn check_charstrings(charstrings: &Charstrings, expected_glyphs: &[(&str, &[u8])]) {
        for (idx, expected) in expected_glyphs.iter().enumerate() {
            let idx = idx as u32;
            let name = charstrings.name(idx).unwrap();
            let data = charstrings.get(idx).unwrap();
            assert_eq!((name, data), *expected);
        }
    }

    #[test]
    fn parse_weight_vector() {
        let mut parser = Parser::new(b"[0 0.125, 1.25 -0.87]");
        let weights = parser
            .read_weight_vector()
            .unwrap()
            .drain(..)
            .map(|w| w.to_f32())
            .collect::<Vec<_>>();
        assert_eq!(weights, &[0.0, 0.125, 1.25, -0.8699951]);
    }

    #[test]
    fn parse_type1_font_pfb() {
        check_type1_font(
            &Type1Font::new(font_test_data::type1::NOTO_SERIF_REGULAR_SUBSET_PFB).unwrap(),
        );
    }

    #[test]
    fn parse_type1_font_pfa() {
        check_type1_font(
            &Type1Font::new(font_test_data::type1::NOTO_SERIF_REGULAR_SUBSET_PFA).unwrap(),
        );
    }

    #[track_caller]
    fn check_type1_font(font: &Type1Font) {
        assert_eq!(font.num_glyphs(), 9);
        assert_eq!(font.subrs.index.len(), 5);
        assert_eq!(
            font.matrix,
            ScaledFontMatrix {
                matrix: FontMatrix::IDENTITY,
                scale: 1000
            }
        );
        assert!(font
            .glyph_names()
            .map(|(_, name)| name)
            .take(4)
            .eq([".notdef", "H", "f", "i"].into_iter()))
    }

    #[test]
    fn parse_encoding() {
        assert!(matches!(
            Type1Font::new(font_test_data::type1::NOTO_SERIF_REGULAR_SUBSET_PFA)
                .unwrap()
                .encoding,
            Encoding::Predefined(PredefinedEncoding::Standard),
        ));
    }

    #[test]
    fn parse_known_encodings() {
        for (blob, encoding) in [
            (
                "StandardEncoding",
                Encoding::Predefined(PredefinedEncoding::Standard),
            ),
            (
                "ExpertEncoding",
                Encoding::Predefined(PredefinedEncoding::Expert),
            ),
            (
                "ISOLatin1Encoding",
                Encoding::Predefined(PredefinedEncoding::IsoLatin1),
            ),
        ] {
            assert_eq!(
                Parser::new(blob.as_bytes())
                    .read_encoding(&Charstrings::default())
                    .unwrap(),
                encoding
            );
        }
    }

    #[test]
    fn parse_custom_dense_encoding() {
        let mut map = Vec::new();
        map.resize(256, ".notdef".to_string());
        let mut parser = Parser::new(b"[/.notdef /A /b /.notdef /comma /at]");
        parser.read_dense_encoding(|idx, name| {
            map[idx as usize] = name.to_string();
        });
        for (ch, entry) in map.iter().enumerate() {
            let expected = match ch {
                1 => "A",
                2 => "b",
                4 => "comma",
                5 => "at",
                _ => ".notdef",
            };
            assert_eq!(entry, expected);
        }
    }

    #[test]
    fn parse_custom_sparse_encoding() {
        let mut map = Vec::new();
        map.resize(256, ".notdef".to_string());
        let mut parser = Parser::new(CUSTOM_SPARSE_ENCODING.as_bytes());
        parser.read_sparse_encoding(|idx, name| {
            map[idx as usize] = name.to_string();
        });
        for (ch, entry) in map.iter().enumerate() {
            let expected = match ch {
                66 => "B",
                97 => "a",
                64 => "at",
                44 => "comma",
                56 => "eight",
                _ => ".notdef",
            };
            assert_eq!(entry, expected);
        }
    }

    const CUSTOM_SPARSE_ENCODING: &str = r#"
        array
        0 1 255 {1 index exch /.notdef put} for
        dup 66 /B put
        dup 97 /a put
        dup 64 /at put
        dup 44 /comma put
        dup 56 /eight put
        readonly def    
    "#;

    #[test]
    fn eval_charstrings() {
        let font = Type1Font::new(font_test_data::type1::NOTO_SERIF_REGULAR_SUBSET_PFA).unwrap();
        let expected_eval_prefix = [
            "M38,0 L329,0 L329,42 L316,42 C293,42 274,46 258,54 C242,63 234,83 234,114",
            "M27,0 L336,0 L336,42 L298,42 C275,42 256,46 240,54 C224,63 216,83 216,114",
            "M161,636 C176,636 190,641 201,650 C212,659 218,675 218,698 C218,721 212,738 201,746",
            "M5,0 L243,0 L243,42 L240,42 C218,42 202,44 192,50 C183,54 178,62 178,73",
            "M27,0 L316,0 L316,42 L298,42 C275,42 256,46 240,54 C224,63 216,83 216,114",
            "M27,0 L316,0 L316,42 L298,42 C275,42 256,46 240,54 C224,63 216,83 216,114",
            "M27,0 L316,0 L316,42 L298,42 C275,42 256,46 240,54 C224,63 216,83 216,114",
            "M41,0 L290,0 L290,42 L269,42 C254,42 240,45 229,52 C218,58 212,73 212,98",
        ];
        // -1 to ignore the .notdef glyph
        assert_eq!(font.num_glyphs() as usize - 1, expected_eval_prefix.len());
        let mut commands = CaptureCommandSink::default();
        for (gid, expected_prefix) in (1..font.num_glyphs()).zip(&expected_eval_prefix) {
            commands.0.clear();
            font.evaluate_charstring(gid.into(), &mut commands).unwrap();
            assert!(commands.to_svg().starts_with(expected_prefix));
        }
    }

    #[test]
    fn eval_charstring_widths() {
        let font = Type1Font::new(font_test_data::type1::NOTO_SERIF_REGULAR_SUBSET_PFA).unwrap();
        let expected_widths = [
            600.0, 793.0, 369.0, 320.0, 578.0, 708.0, 1002.0, 663.0, 680.0,
        ];
        let mut commands = CaptureCommandSink::default();
        let widths = (0..font.num_glyphs())
            .map(|gid| {
                commands.0.clear();
                font.evaluate_charstring(gid.into(), &mut commands)
                    .unwrap()
                    .unwrap()
                    .to_f32()
            })
            .collect::<Vec<_>>();
        assert_eq!(widths, expected_widths);
    }

    #[test]
    fn csctx_seac_components() {
        let font = Type1Font::new(font_test_data::type1::NOTO_SERIF_REGULAR_SUBSET_PFA).unwrap();
        // Standard encoding for 'x' and 'i'
        let x_code = 120;
        let i_code = 105;
        let [x_data, i_data] = font.seac_components(x_code, i_code).unwrap();
        let name_to_gid = |name| {
            font.glyph_names()
                .find_map(|(gid, gname)| (name == gname).then_some(gid.to_u32()))
                .unwrap()
        };
        assert_eq!(x_data, font.charstrings.get(name_to_gid("x")).unwrap());
        assert_eq!(i_data, font.charstrings.get(name_to_gid("i")).unwrap());
    }

    #[test]
    fn csctx_subrs() {
        let font = Type1Font::new(font_test_data::type1::NOTO_SERIF_REGULAR_SUBSET_PFA).unwrap();
        assert!(!font.subrs.index.is_empty());
        for subr_idx in 0..font.subrs.index.len() {
            assert_eq!(
                font.subrs.get(subr_idx as u32).unwrap(),
                font.subr(subr_idx as _).unwrap()
            )
        }
    }
}

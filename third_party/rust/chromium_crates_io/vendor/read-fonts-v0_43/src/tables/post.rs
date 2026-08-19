//! the [post (PostScript)](https://docs.microsoft.com/en-us/typography/opentype/spec/post#header) table

include!("../../generated/generated_post.rs");

#[allow(clippy::needless_lifetimes)] // 'a is used with experimental_traverse feature below
impl<'a> Post<'a> {
    /// The number of glyph names covered by this table
    pub fn num_names(&self) -> usize {
        match self.version() {
            Version16Dot16::VERSION_1_0 => DEFAULT_GLYPH_NAMES.len(),
            Version16Dot16::VERSION_2_0 => self.num_glyphs().unwrap_or_default() as usize,
            _ => 0,
        }
    }

    /// Returns the name for the given glyph.
    ///
    /// Note that this is a relatively expensive operation, as it may require
    /// a linear scan through the string data to find the target name. If you
    /// need to iterate over all glyph names or collect them into a map for
    /// faster access, use [`Self::glyph_names`] instead.
    pub fn glyph_name(&self, glyph_id: GlyphId16) -> Option<&'a str> {
        let glyph_id = glyph_id.to_u16() as usize;
        match self.version() {
            Version16Dot16::VERSION_1_0 => DEFAULT_GLYPH_NAMES.get(glyph_id).copied(),
            Version16Dot16::VERSION_2_0 => {
                let idx = self.glyph_name_index()?.get(glyph_id)?.get() as usize;
                if idx < DEFAULT_GLYPH_NAMES.len() {
                    return DEFAULT_GLYPH_NAMES.get(idx).copied();
                }
                let idx = idx - DEFAULT_GLYPH_NAMES.len();
                self.string_data()?.get(idx)?.ok().map(|s| s.0)
            }
            _ => None,
        }
    }

    /// Return an iterator over the glyph names in this table.
    pub fn glyph_names(&self) -> GlyphNames<'a> {
        let num_names = self.num_names() as u32;
        let kind = match self.version() {
            Version16Dot16::VERSION_1_0 => GlyphNameIterKind::V1(self.clone(), 0),
            Version16Dot16::VERSION_2_0 => GlyphNameIterKind::V2 {
                post: self.clone(),
                idx: 0,
                checkpoint_stride: (num_names as usize).div_ceil(NUM_CHECKPOINTS + 1).max(1),
                checkpoints: [UNSET_CHECKPOINT; NUM_CHECKPOINTS],
                last_actual_idx: None,
                last_offset: 0,
            },
            _ => GlyphNameIterKind::None,
        };
        GlyphNames { num_names, kind }
    }

    //FIXME: how do we want to traverse this? I want to stop needing to
    // add special cases for things...
    #[cfg(feature = "experimental_traverse")]
    fn traverse_string_data(&self) -> FieldType<'a> {
        FieldType::I8(-42) // meaningless value
    }
}

/// A string in the post table.
///
/// This is basically just a newtype that knows how to parse from a Pascal-style
/// string.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct PString<'a>(&'a str);

impl<'a> PString<'a> {
    pub fn as_str(&self) -> &'a str {
        self.0
    }
}

impl std::ops::Deref for PString<'_> {
    type Target = str;
    fn deref(&self) -> &Self::Target {
        self.0
    }
}

impl PartialEq<&str> for PString<'_> {
    fn eq(&self, other: &&str) -> bool {
        self.0 == *other
    }
}

impl ReadArgs for PString<'_> {
    type Args = ();
}

impl<'a> FontRead<'a> for PString<'a> {
    fn read_with_args(data: FontData<'a>, _: ()) -> Result<Self, ReadError> {
        let len: u8 = data.read_at(0)?;
        let pstring = data
            .as_bytes()
            .get(1..len as usize + 1)
            .ok_or(ReadError::OutOfBounds)?;

        if pstring.is_ascii() {
            Ok(PString(std::str::from_utf8(pstring).unwrap()))
        } else {
            //FIXME not really sure how we want to handle this?
            Err(ReadError::MalformedData("Must be valid ascii"))
        }
    }
}

impl VarSize for PString<'_> {
    type Size = u8;
}

const NUM_CHECKPOINTS: usize = 16;
const UNSET_CHECKPOINT: u32 = u32::MAX;

/// Iterator over the glyph names in a post table.
#[derive(Clone)]
pub struct GlyphNames<'a> {
    num_names: u32,
    kind: GlyphNameIterKind<'a>,
}

#[derive(Clone)]
enum GlyphNameIterKind<'a> {
    None,
    V1(Post<'a>, u32),
    V2 {
        post: Post<'a>,
        idx: u32,
        // The number of indices between checkpoints
        checkpoint_stride: usize,
        // The offset of each checkpoint; checkpoint 0 is implicit
        checkpoints: [u32; NUM_CHECKPOINTS],
        // The last actual index and that was scanned, for monotonic fast path
        last_actual_idx: Option<usize>,
        // The offset associated with the last scanned index
        last_offset: usize,
    },
}

impl<'a> Iterator for GlyphNames<'a> {
    type Item = (GlyphId, &'a str);

    fn next(&mut self) -> Option<Self::Item> {
        match &mut self.kind {
            GlyphNameIterKind::None => None,
            GlyphNameIterKind::V1(post, idx) => {
                if *idx >= self.num_names {
                    return None;
                }
                let gid = GlyphId16::new(*idx as u16);
                let name = post.glyph_name(gid)?;
                *idx += 1;
                Some((gid.into(), name))
            }
            GlyphNameIterKind::V2 {
                post,
                idx,
                checkpoint_stride,
                checkpoints,
                last_actual_idx,
                last_offset,
            } => {
                if *idx >= self.num_names {
                    return None;
                }
                let stride = *checkpoint_stride;
                let gid = GlyphId16::new(*idx as u16);
                let mut actual_idx = post.glyph_name_index()?.get(*idx as usize)?.get() as usize;
                let name = if actual_idx < DEFAULT_GLYPH_NAMES.len() {
                    DEFAULT_GLYPH_NAMES.get(actual_idx).copied()?
                } else {
                    actual_idx -= DEFAULT_GLYPH_NAMES.len();
                    let string_data = post.data.slice(post.string_data_byte_range())?;
                    // Checkpoint 0 is implicit and always at offset 0; the
                    // array stores logical checkpoints 1..=NUM_CHECKPOINTS.
                    let target_slot = (actual_idx / stride).min(NUM_CHECKPOINTS);
                    // Find the the starting location for our scan
                    let (mut scan_idx, mut offset) = {
                        // Search backward from the target slot to find the
                        // nearest checkpoint that has been set
                        let mut slot = target_slot;
                        while slot > 0 && checkpoints[slot - 1] == UNSET_CHECKPOINT {
                            slot -= 1;
                        }
                        if slot == 0 {
                            // Fallback to implicit checkpoint 0
                            (0, 0)
                        } else {
                            // Otherwise, start scanning from the nearest
                            // checkpoint
                            (slot * stride, checkpoints[slot - 1] as usize)
                        }
                    };
                    // See if we can use the monotonic fast path.
                    if let Some(last_idx) = *last_actual_idx {
                        // Start scanning from the last index if that provides
                        // a smaller search space than the nearest checkpoint
                        if last_idx <= actual_idx && last_idx > scan_idx {
                            scan_idx = last_idx;
                            offset = *last_offset;
                        }
                    }
                    // Now do the linear scan over the string data
                    while scan_idx < actual_idx {
                        let item_len = PString::read_len_at(string_data, offset)?;
                        offset = offset.checked_add(item_len)?;
                        scan_idx += 1;
                        // If this index is a checkpoint, record the offset
                        // for future scans
                        if scan_idx % stride == 0 {
                            let slot = (scan_idx / stride).min(NUM_CHECKPOINTS);
                            if slot > 0 {
                                checkpoints[slot - 1] = u32::try_from(offset).ok()?;
                            }
                        }
                    }
                    if actual_idx % stride == 0 && target_slot > 0 {
                        checkpoints[target_slot - 1] = u32::try_from(offset).ok()?;
                    }
                    // Record the last index and offset for future scans
                    *last_actual_idx = Some(actual_idx);
                    *last_offset = offset;
                    PString::read(string_data.split_off(offset)?).ok()?.0
                };
                *idx += 1;
                Some((gid.into(), name))
            }
        }
    }
}

/// The 258 glyph names defined for Macintosh TrueType fonts
#[rustfmt::skip]
pub static DEFAULT_GLYPH_NAMES: [&str; 258] = [
    ".notdef", ".null", "nonmarkingreturn", "space", "exclam", "quotedbl", "numbersign", "dollar",
    "percent", "ampersand", "quotesingle", "parenleft", "parenright", "asterisk", "plus", "comma",
    "hyphen", "period", "slash", "zero", "one", "two", "three", "four", "five", "six", "seven",
    "eight", "nine", "colon", "semicolon", "less", "equal", "greater", "question", "at", "A", "B",
    "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M", "N", "O", "P", "Q", "R", "S", "T", "U",
    "V", "W", "X", "Y", "Z", "bracketleft", "backslash", "bracketright", "asciicircum",
    "underscore", "grave", "a", "b", "c", "d", "e", "f", "g", "h", "i", "j", "k", "l", "m", "n",
    "o", "p", "q", "r", "s", "t", "u", "v", "w", "x", "y", "z", "braceleft", "bar", "braceright",
    "asciitilde", "Adieresis", "Aring", "Ccedilla", "Eacute", "Ntilde", "Odieresis", "Udieresis",
    "aacute", "agrave", "acircumflex", "adieresis", "atilde", "aring", "ccedilla", "eacute",
    "egrave", "ecircumflex", "edieresis", "iacute", "igrave", "icircumflex", "idieresis", "ntilde",
    "oacute", "ograve", "ocircumflex", "odieresis", "otilde", "uacute", "ugrave", "ucircumflex",
    "udieresis", "dagger", "degree", "cent", "sterling", "section", "bullet", "paragraph",
    "germandbls", "registered", "copyright", "trademark", "acute", "dieresis", "notequal", "AE",
    "Oslash", "infinity", "plusminus", "lessequal", "greaterequal", "yen", "mu", "partialdiff",
    "summation", "product", "pi", "integral", "ordfeminine", "ordmasculine", "Omega", "ae",
    "oslash", "questiondown", "exclamdown", "logicalnot", "radical", "florin", "approxequal",
    "Delta", "guillemotleft", "guillemotright", "ellipsis", "nonbreakingspace", "Agrave", "Atilde",
    "Otilde", "OE", "oe", "endash", "emdash", "quotedblleft", "quotedblright", "quoteleft",
    "quoteright", "divide", "lozenge", "ydieresis", "Ydieresis", "fraction", "currency",
    "guilsinglleft", "guilsinglright", "fi", "fl", "daggerdbl", "periodcentered", "quotesinglbase",
    "quotedblbase", "perthousand", "Acircumflex", "Ecircumflex", "Aacute", "Edieresis", "Egrave",
    "Iacute", "Icircumflex", "Idieresis", "Igrave", "Oacute", "Ocircumflex", "apple", "Ograve",
    "Uacute", "Ucircumflex", "Ugrave", "dotlessi", "circumflex", "tilde", "macron", "breve",
    "dotaccent", "ring", "cedilla", "hungarumlaut", "ogonek", "caron", "Lslash", "lslash",
    "Scaron", "scaron", "Zcaron", "zcaron", "brokenbar", "Eth", "eth", "Yacute", "yacute", "Thorn",
    "thorn", "minus", "multiply", "onesuperior", "twosuperior", "threesuperior", "onehalf",
    "onequarter", "threequarters", "franc", "Gbreve", "gbreve", "Idotaccent", "Scedilla",
    "scedilla", "Cacute", "cacute", "Ccaron", "ccaron", "dcroat",
];

#[cfg(test)]
mod tests {
    use super::*;
    use font_test_data::{bebuffer::BeBuffer, post as test_data};

    #[test]
    fn test_post() {
        let table = Post::read(test_data::SIMPLE.into()).unwrap();
        assert_eq!(table.version(), Version16Dot16::VERSION_2_0);
        assert_eq!(table.underline_position(), FWord::new(-75));
        assert_eq!(table.glyph_name(GlyphId16::new(1)), Some(".notdef"));
        assert_eq!(table.glyph_name(GlyphId16::new(2)), Some("space"));
        assert_eq!(table.glyph_name(GlyphId16::new(7)), Some("hello"));
        assert_eq!(table.glyph_name(GlyphId16::new(8)), Some("hi"));
        assert_eq!(table.glyph_name(GlyphId16::new(9)), Some("hola"));
    }

    fn make_basic_post(version: Version16Dot16, include_num_glyphs: bool) -> BeBuffer {
        let buf = BeBuffer::new()
            .push(version)
            .push(Fixed::from_i32(5))
            .extend([FWord::new(6), FWord::new(7)]) //underline pos/thickness
            .push(0u32) // isFixedPitch
            .extend([7u32, 8, 9, 10]); // min/max mem x
        if include_num_glyphs {
            buf.push(0u16)
        } else {
            buf
        }
    }

    #[test]
    fn parse_versioned_fields_v1() {
        // v1, even if it has the extra field will not read it:

        let buf = make_basic_post(Version16Dot16::VERSION_1_0, true);
        let postv1 = Post::read(buf.data().into()).unwrap();
        assert!(postv1.num_glyphs().is_none());
    }

    #[test]
    fn parse_versioned_fields_v2() {
        let buf = make_basic_post(Version16Dot16::VERSION_2_0, false);
        let postv2 = Post::read(buf.data().into()).unwrap();
        // v2 will fail to read if data is missing
        assert!(postv2.num_glyphs().is_none());

        // but read if data is present
        let buf = make_basic_post(Version16Dot16::VERSION_2_0, true);
        let postv2 = Post::read(buf.data().into()).unwrap();
        // v2 will fail to read if data is missing
        assert_eq!(postv2.num_glyphs(), Some(0));
    }

    #[test]
    fn parse_versioned_fields_v3() {
        // v3 will again not read since this field is not compatible
        let buf = make_basic_post(Version16Dot16::VERSION_3_0, true);
        let postv3 = Post::read(buf.data().into()).unwrap();
        assert!(postv3.num_glyphs().is_none());
    }

    #[test]
    fn num_names_defaults_to_zero_without_num_glyphs() {
        let buf = make_basic_post(Version16Dot16::VERSION_2_0, false);
        let post = Post::read(buf.data().into()).unwrap();
        // Just don't panic
        assert_eq!(post.num_names(), 0);
    }

    #[test]
    fn glyph_name_missing_string_data_returns_none() {
        let buf = BeBuffer::new()
            .push(Version16Dot16::VERSION_2_0)
            .push(Fixed::from_i32(5))
            .extend([FWord::new(6), FWord::new(7)])
            .push(0u32)
            .extend([7u32, 8, 9, 10])
            .push(1u16)
            .push(258u16);
        let post = Post::read(buf.data().into()).unwrap();
        // Just don't panic
        assert_eq!(post.glyph_name(GlyphId16::new(0)), None);
    }

    #[test]
    fn glyph_names_matches_naive_on_varied_synthetic_v2_data() {
        let num_glyphs = 2_000u16;
        let orders = [
            test_data::GlyphNameOrder::Monotonic,
            test_data::GlyphNameOrder::MostlyMonotonicWithBackrefs,
            test_data::GlyphNameOrder::AllPointToLast,
        ];
        for order in orders {
            let (bytes, expected_custom_names) =
                test_data::v2_with_varied_glyph_names(num_glyphs, 63, order);
            let post = Post::read(bytes.as_slice().into()).unwrap();
            let from_naive: Vec<_> = (0..num_glyphs)
                .map(|gid| post.glyph_name(GlyphId16::new(gid)).unwrap())
                .collect();
            let from_iter: Vec<_> = post.glyph_names().map(|(_, name)| name).collect();
            assert_eq!(from_iter, from_naive);
            for (name, expected) in from_iter.iter().zip(expected_custom_names.iter()) {
                assert_eq!(*name, expected.as_str());
            }
        }
    }
}

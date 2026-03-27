//! Unified access to CFF/CFF2 fonts.

use super::super::{
    super::{cff, cff2},
    charstring::{self, CommandSink},
    dict::{self, ScaledFontMatrix},
    BlendState, Charset, Error, FdSelect, Index,
};
use super::HintingParams;
use crate::{tables::variations::ItemVariationStore, FontRead, ReadError};
use core::ops::Range;
use types::{F2Dot14, Fixed, GlyphId};

/// A CFF or CFF2 font.
///
/// The source data may be a raw CFF blob as embedded in a PDF or the content
/// of a CFF or CFF2 table in an OpenType font.
#[derive(Clone)]
pub struct CffFontRef<'a> {
    data: &'a [u8],
    is_cff2: bool,
    global_subrs: Index<'a>,
    top_dict: TopDict<'a>,
}

impl<'a> CffFontRef<'a> {
    /// Creates a new font for the given CFF data.
    pub fn new_cff(data: &'a [u8], top_dict_index: u32) -> Result<Self, Error> {
        let cff = cff::Cff::read(data.into())?;
        let top_dict_data = cff.top_dicts().get(top_dict_index as usize)?;
        Self::new(
            data,
            false,
            top_dict_data,
            cff.strings().into(),
            cff.global_subrs().into(),
        )
    }

    /// Creates a new font for the given CFF2 data.
    pub fn new_cff2(data: &'a [u8]) -> Result<Self, Error> {
        let cff = cff2::Cff2::read(data.into())?;
        Self::new(
            data,
            true,
            cff.top_dict_data(),
            Index::Empty,
            cff.global_subrs().into(),
        )
    }

    fn new(
        data: &'a [u8],
        is_cff2: bool,
        top_dict_data: &'a [u8],
        strings: Index<'a>,
        global_subrs: Index<'a>,
    ) -> Result<Self, Error> {
        let top_dict = TopDict::new(data, top_dict_data, strings, is_cff2)?;
        Ok(Self {
            data,
            is_cff2,
            global_subrs,
            top_dict,
        })
    }

    /// Returns the raw CFF blob.
    pub fn data(&self) -> &'a [u8] {
        self.data
    }

    /// Returns the CFF version (either 1 or 2).
    pub fn version(&self) -> u16 {
        if self.is_cff2 {
            2
        } else {
            1
        }
    }

    /// Returns true if this is a CID-keyed font.
    pub fn is_cid(&self) -> bool {
        matches!(&self.top_dict.kind, CffFontKind::Cid { .. })
    }

    /// Returns the global subroutine index.
    pub fn global_subrs(&self) -> &Index<'a> {
        &self.global_subrs
    }

    /// Returns the number of glyphs in the CFF font.
    pub fn num_glyphs(&self) -> u32 {
        self.top_dict.charstrings.count()
    }

    /// Returns the charstring index.
    pub fn charstrings(&self) -> &Index<'a> {
        &self.top_dict.charstrings
    }

    /// Returns the mapping for glyph identifiers.
    ///
    /// For a CID font, this maps between CIDs and glyph identifiers.
    /// Otherwise maps between SIDs and glyph identifiers.
    pub fn charset(&self) -> Option<Charset<'a>> {
        Charset::new(
            self.data.into(),
            self.top_dict.charset_offset.get()?,
            self.top_dict.charstrings.count(),
        )
        .ok()
    }

    /// Returns the top level font matrix.
    pub fn matrix(&self) -> Option<&ScaledFontMatrix> {
        self.top_dict.matrix.as_ref()
    }

    /// Returns the item variation store.
    ///
    /// Only present for CFF2 variable fonts.
    pub fn var_store(&self) -> Option<&ItemVariationStore<'a>> {
        self.top_dict.var_store.as_ref()
    }

    /// Returns the number of available subfonts.
    pub fn num_subfonts(&self) -> u16 {
        match &self.top_dict.kind {
            CffFontKind::Sid { .. } => 1,
            CffFontKind::Cid { fd_array, .. } | CffFontKind::Cff2 { fd_array, .. } => {
                fd_array.count() as u16
            }
        }
    }

    /// Returns the subfont index for the given glyph.
    pub fn subfont_index(&self, gid: GlyphId) -> Option<u16> {
        match &self.top_dict.kind {
            CffFontKind::Sid { .. } => Some(0),
            CffFontKind::Cid { fd_select, .. } | CffFontKind::Cff2 { fd_select, .. } => fd_select
                .as_ref()
                .map_or(Some(0), |fds| fds.font_index(gid)),
        }
    }

    /// Returns the subfont with the given index and normalized variation
    /// coordinates.
    pub fn subfont(&self, index: u16, coords: &[F2Dot14]) -> Result<CffSubfont, Error> {
        let blend = self.blend_state(0, coords);
        match &self.top_dict.kind {
            CffFontKind::Sid { private_dict, .. } => CffSubfont::new(
                self.data,
                private_dict.start as usize..private_dict.end as usize,
                blend,
                None,
            ),
            CffFontKind::Cid { fd_array, .. } | CffFontKind::Cff2 { fd_array, .. } => {
                let font_dict = FontDict::new(fd_array.get(index as usize)?)?;
                CffSubfont::new(
                    self.data,
                    font_dict.private_dict_range,
                    blend,
                    font_dict.matrix,
                )
            }
        }
    }

    /// Returns the subfont and hinting parameters for the given index and
    /// normalized variation coordinates.
    pub fn subfont_hinted(
        &self,
        index: u16,
        coords: &[F2Dot14],
    ) -> Result<(CffSubfont, HintingParams), Error> {
        let blend = self.blend_state(0, coords);
        match &self.top_dict.kind {
            CffFontKind::Sid { private_dict, .. } => CffSubfont::new_hinted(
                self.data,
                private_dict.start as usize..private_dict.end as usize,
                blend,
                None,
            ),
            CffFontKind::Cid { fd_array, .. } | CffFontKind::Cff2 { fd_array, .. } => {
                let font_dict = FontDict::new(fd_array.get(index as usize)?)?;
                CffSubfont::new_hinted(
                    self.data,
                    font_dict.private_dict_range,
                    blend,
                    font_dict.matrix,
                )
            }
        }
    }

    /// Evaluates the charstring for the requested glyph and sends the results
    /// to the given sink.
    pub fn evaluate_charstring(
        &self,
        subfont: &CffSubfont,
        coords: &[F2Dot14],
        gid: GlyphId,
        sink: &mut impl CommandSink,
    ) -> Result<Option<Fixed>, Error> {
        let charstrings = self.top_dict.charstrings.clone();
        let blend = self.blend_state(subfont.vs_index, coords);
        let subrs = if subfont.subrs_offset != 0 {
            let data = self
                .data
                .get(subfont.subrs_offset as usize..)
                .ok_or(ReadError::OutOfBounds)?;
            Index::new(data, self.is_cff2)?
        } else {
            Index::Empty
        };
        let charstring_data = charstrings.get(gid.to_u32() as usize)?;
        let ctx = (self.data, &charstrings, &self.global_subrs, &subrs);
        if let Some(width) = charstring::evaluate(&ctx, blend, charstring_data, sink)? {
            Ok(Some(width + subfont.nominal_width))
        } else {
            Ok(subfont.default_width)
        }
    }

    /// Returns a blend state for the given variation store index and
    /// normalized coordinates.
    fn blend_state(&self, vs_index: u16, coords: &'a [F2Dot14]) -> Option<BlendState<'a>> {
        self.top_dict
            .var_store
            .as_ref()
            .and_then(|store| BlendState::new(store.clone(), coords, vs_index).ok())
    }
}

/// An SID or CID font.
#[derive(Clone)]
enum CffFontKind<'a> {
    /// A CFF font.
    Sid {
        /// Index for resolving glyph names.
        _strings: Index<'a>,
        /// Byte range of the private dict from the base of the font data.
        private_dict: Range<u32>,
    },
    /// A CFF font with an externally defined encoding.
    Cid {
        /// Maps from glyph identifiers to font dict indices.
        fd_select: Option<FdSelect<'a>>,
        /// Index containing font dicts.
        fd_array: Index<'a>,
    },
    /// A CFF2 font.
    Cff2 {
        /// Maps from glyph identifiers to font dict indices.
        fd_select: Option<FdSelect<'a>>,
        /// Index containing font dicts.
        fd_array: Index<'a>,
    },
}

/// Metadata for a CFF subfont.
///
/// A subfont is the collection of data read from both the
/// [Font DICT](https://adobe-type-tools.github.io/font-tech-notes/pdfs/5176.CFF.pdf#page=28)
/// and [Private Dict](https://adobe-type-tools.github.io/font-tech-notes/pdfs/5176.CFF.pdf#page=24)
/// structures. These determine the set of subroutines, metrics and hinting
/// parameters for some group of glyphs.
///
/// Use [CffFontRef::subfont_index] to determine the subfont index for a
/// particular glyph and then [CffFontRef::subfont] (or
/// [CffFontRef::subfont_hinted]) to retrieve the associated subfont.
#[derive(Copy, Clone, Default, Debug)]
pub struct CffSubfont {
    subrs_offset: u32,
    default_width: Option<Fixed>,
    nominal_width: Fixed,
    matrix: Option<ScaledFontMatrix>,
    vs_index: u16,
}

impl CffSubfont {
    fn new(
        data: &[u8],
        range: Range<usize>,
        blend: Option<BlendState>,
        matrix: Option<ScaledFontMatrix>,
    ) -> Result<Self, Error> {
        let mut subfont = Self {
            matrix,
            ..Default::default()
        };
        let data = data.get(range.clone()).ok_or(ReadError::OutOfBounds)?;
        for entry in dict::entries(data, blend) {
            match entry? {
                dict::Entry::SubrsOffset(offset) => {
                    subfont.subrs_offset = range
                        .start
                        .checked_add(offset)
                        .ok_or(ReadError::OutOfBounds)?
                        as u32;
                }
                dict::Entry::VariationStoreIndex(index) => subfont.vs_index = index,
                dict::Entry::DefaultWidthX(width) => subfont.default_width = Some(width),
                dict::Entry::NominalWidthX(width) => subfont.nominal_width = width,
                _ => {}
            }
        }
        Ok(subfont)
    }

    fn new_hinted(
        data: &[u8],
        range: Range<usize>,
        blend: Option<BlendState>,
        matrix: Option<ScaledFontMatrix>,
    ) -> Result<(Self, HintingParams), Error> {
        let mut subfont = Self {
            matrix,
            ..Default::default()
        };
        let mut params = HintingParams::default();
        let data = data.get(range.clone()).ok_or(ReadError::OutOfBounds)?;
        for entry in dict::entries(data, blend) {
            match entry? {
                dict::Entry::SubrsOffset(offset) => {
                    subfont.subrs_offset = range
                        .start
                        .checked_add(offset)
                        .ok_or(ReadError::OutOfBounds)?
                        as u32;
                }
                dict::Entry::VariationStoreIndex(index) => subfont.vs_index = index,
                dict::Entry::DefaultWidthX(width) => subfont.default_width = Some(width),
                dict::Entry::NominalWidthX(width) => subfont.nominal_width = width,
                dict::Entry::BlueValues(values) => params.blues = values,
                dict::Entry::FamilyBlues(values) => params.family_blues = values,
                dict::Entry::OtherBlues(values) => params.other_blues = values,
                dict::Entry::FamilyOtherBlues(values) => params.family_other_blues = values,
                dict::Entry::BlueScale(value) => params.blue_scale = value,
                dict::Entry::BlueShift(value) => params.blue_shift = value,
                dict::Entry::BlueFuzz(value) => params.blue_fuzz = value,
                dict::Entry::LanguageGroup(group) => params.language_group = group,
                _ => {}
            }
        }
        Ok((subfont, params))
    }

    /// Returns the offset to the local subroutine index from the start of the
    /// CFF blob.
    pub fn subrs_offset(&self) -> u32 {
        self.subrs_offset
    }

    /// Returns the default advance width.
    ///
    /// The advance value for charstrings that do not contain a width.
    pub fn default_width(&self) -> Option<Fixed> {
        self.default_width
    }

    /// Returns the nominal advance width.
    ///
    /// The base advance value for charstrings that do contain a width. This
    /// should be added to the width in the charstring.
    pub fn nominal_width(&self) -> Fixed {
        self.nominal_width
    }

    /// Returns the font matrix.
    pub fn matrix(&self) -> Option<&ScaledFontMatrix> {
        self.matrix.as_ref()
    }

    /// Returns the default variation store index.
    pub fn vs_index(&self) -> u16 {
        self.vs_index
    }
}

/// Use in-band signaling for missing offsets to keep the struct size small.
#[derive(Copy, Clone)]
struct MaybeOffset(u32);

impl MaybeOffset {
    fn get(self) -> Option<usize> {
        (self.0 != u32::MAX).then_some(self.0 as usize)
    }
}

impl Default for MaybeOffset {
    fn default() -> Self {
        Self(u32::MAX)
    }
}

#[derive(Clone)]
struct TopDict<'a> {
    charstrings: Index<'a>,
    charset_offset: MaybeOffset,
    _encoding_offset: MaybeOffset,
    matrix: Option<ScaledFontMatrix>,
    var_store: Option<ItemVariationStore<'a>>,
    kind: CffFontKind<'a>,
}

impl<'a> TopDict<'a> {
    fn new(
        cff_data: &'a [u8],
        top_dict_data: &[u8],
        strings: Index<'a>,
        is_cff2: bool,
    ) -> Result<Self, Error> {
        let mut has_ros = false;
        let mut charstrings = None;
        let mut charset_offset = MaybeOffset::default();
        let mut encoding_offset = MaybeOffset::default();
        let mut fd_array = None;
        let mut fd_select = None;
        let mut private_dict_range = 0..0;
        let mut matrix = None;
        let mut var_store = None;
        for entry in dict::entries(top_dict_data, None) {
            match entry? {
                dict::Entry::Ros { .. } => has_ros = true,
                dict::Entry::CharstringsOffset(offset) => {
                    charstrings = cff_data
                        .get(offset..)
                        .and_then(|data| Index::new(data, is_cff2).ok());
                }
                dict::Entry::Charset(offset) => charset_offset = MaybeOffset(offset as u32),
                dict::Entry::Encoding(offset) => encoding_offset = MaybeOffset(offset as u32),
                dict::Entry::FdArrayOffset(offset) => {
                    fd_array = cff_data
                        .get(offset..)
                        .and_then(|data| Index::new(data, is_cff2).ok());
                }
                dict::Entry::FdSelectOffset(offset) => {
                    fd_select = cff_data
                        .get(offset..)
                        .and_then(|data| FdSelect::read(data.into()).ok());
                }
                dict::Entry::PrivateDictRange(range) => private_dict_range = range,
                dict::Entry::FontMatrix(font_matrix) => {
                    // FreeType always normalizes this and the scaling factor
                    // is dynamic so it won't make a difference to our users
                    matrix = Some(font_matrix.normalize());
                }
                dict::Entry::VariationStoreOffset(offset) if is_cff2 => {
                    // IVS is preceded by a 2 byte length, but ensure that
                    // we don't overflow
                    // See <https://github.com/googlefonts/fontations/issues/1223>
                    let offset = offset.checked_add(2).ok_or(ReadError::OutOfBounds)?;
                    var_store = Some(ItemVariationStore::read(
                        cff_data.get(offset..).unwrap_or_default().into(),
                    )?);
                }
                _ => {}
            }
        }
        let charstrings = charstrings.ok_or(Error::MissingCharstrings)?;
        let kind = if let Some(fd_array) = fd_array {
            if is_cff2 {
                CffFontKind::Cff2 {
                    fd_array,
                    fd_select,
                }
            } else {
                CffFontKind::Cid {
                    fd_array,
                    fd_select,
                }
            }
        } else {
            if has_ros || is_cff2 {
                // The font dict array is required for CID-keyed and CFF2 fonts
                return Err(Error::MissingFdArray);
            }
            CffFontKind::Sid {
                _strings: strings,
                private_dict: private_dict_range.start as u32..private_dict_range.end as u32,
            }
        };
        Ok(Self {
            charset_offset,
            _encoding_offset: encoding_offset,
            charstrings,
            matrix,
            kind,
            var_store,
        })
    }
}

#[derive(Default)]
struct FontDict {
    private_dict_range: Range<usize>,
    matrix: Option<ScaledFontMatrix>,
}

impl FontDict {
    fn new(dict_data: &[u8]) -> Result<Self, Error> {
        let mut range = None;
        let mut matrix = None;
        for entry in dict::entries(dict_data, None) {
            match entry? {
                dict::Entry::PrivateDictRange(dict_range) => {
                    range = Some(dict_range);
                }
                dict::Entry::FontMatrix(font_matrix) => {
                    matrix = Some(font_matrix);
                }
                _ => {}
            }
        }
        Ok(Self {
            private_dict_range: range.ok_or(Error::MissingPrivateDict)?,
            matrix,
        })
    }
}

#[cfg(test)]
mod tests {
    use super::{super::super::charstring::test_helpers::*, super::Blues, *};
    use crate::{tables::postscript::dict::FontMatrix, FontData, FontRef, TableProvider};
    use font_test_data::bebuffer::BeBuffer;

    #[test]
    fn read_cff_static() {
        let font = FontRef::new(font_test_data::NOTO_SERIF_DISPLAY_TRIMMED).unwrap();
        let cff = CffFontRef::new_cff(font.cff().unwrap().offset_data().as_bytes(), 0).unwrap();
        assert_eq!(cff.version(), 1);
        assert!(cff.var_store().is_none());
        let CffFontKind::Sid { private_dict, .. } = &cff.top_dict.kind else {
            panic!("this is an SID font");
        };
        assert!(!private_dict.is_empty());
        assert_eq!(cff.num_glyphs(), 5);
        assert_eq!(cff.num_subfonts(), 1);
        assert_eq!(cff.subfont_index(GlyphId::new(1)), Some(0));
        assert_eq!(cff.global_subrs.count(), 17);
    }

    #[test]
    fn read_cff2_static() {
        let font = FontRef::new(font_test_data::CANTARELL_VF_TRIMMED).unwrap();
        let cff = CffFontRef::new_cff2(font.cff2().unwrap().offset_data().as_bytes()).unwrap();
        assert_eq!(cff.version(), 2);
        assert!(cff.var_store().is_some());
        let CffFontKind::Cff2 { fd_array, .. } = &cff.top_dict.kind else {
            panic!("this is a CFF2 font");
        };
        assert_eq!(fd_array.count(), 1);
        assert_eq!(cff.num_glyphs(), 6);
        assert_eq!(cff.num_subfonts(), 1);
        assert_eq!(cff.subfont_index(GlyphId::new(1)), Some(0));
        assert_eq!(cff.global_subrs.count(), 0);
    }

    #[test]
    fn read_example_cff2_table() {
        let cff = CffFontRef::new_cff2(font_test_data::cff2::EXAMPLE).unwrap();
        assert_eq!(cff.version(), 2);
        assert!(cff.var_store().is_some());
        let CffFontKind::Cff2 { fd_array, .. } = &cff.top_dict.kind else {
            panic!("this is a CFF2 font");
        };
        assert_eq!(fd_array.count(), 1);
        assert_eq!(cff.num_glyphs(), 2);
        assert_eq!(cff.num_subfonts(), 1);
        assert_eq!(cff.subfont_index(GlyphId::new(1)), Some(0));
        assert_eq!(cff.global_subrs.count(), 0);
    }

    #[test]
    fn charset() {
        let font = FontRef::new(font_test_data::NOTO_SERIF_DISPLAY_TRIMMED).unwrap();
        let cff = CffFontRef::new_cff(font.cff().unwrap().offset_data().as_bytes(), 0).unwrap();
        let charset = cff.charset().unwrap();
        let glyph_names = charset
            .iter()
            .map(|(gid, sid)| {
                (
                    gid.to_u32(),
                    std::str::from_utf8(sid.standard_string().unwrap().bytes()).unwrap(),
                )
            })
            .collect::<Vec<_>>();
        let expected = [(0, ".notdef"), (1, "i"), (2, "j"), (3, "k"), (4, "l")];
        assert_eq!(glyph_names, expected)
    }

    // Fuzzer caught add with overflow when computing offset to
    // var store.
    // See <https://issues.oss-fuzz.com/issues/377574377>
    #[test]
    fn top_dict_ivs_offset_overflow() {
        // A top DICT with a var store offset of -1 which will cause an
        // overflow
        let top_dict = BeBuffer::new()
            .push(29u8) // integer operator
            .push(-1i32) // integer value
            .push(24u8) // var store offset operator
            .to_vec();
        // Just don't panic with overflow
        assert!(TopDict::new(&[], &top_dict, Index::Empty, true).is_err());
    }

    /// Fuzzer caught add with overflow when computing subrs offset.
    /// See <https://issues.oss-fuzz.com/issues/377965575>
    #[test]
    fn subrs_offset_overflow() {
        // A private DICT with an overflowing subrs offset
        let private_dict = BeBuffer::new()
            .push(0u32) // pad so that range doesn't start with 0 and we overflow
            .push(29u8) // integer operator
            .push(-1i32) // integer value
            .push(19u8) // subrs offset operator
            .to_vec();
        // Just don't panic with overflow
        assert!(CffSubfont::new(&private_dict, 4..private_dict.len(), None, None).is_err());
    }

    /// Ensure we don't reject an empty Private DICT
    #[test]
    fn empty_private_dict() {
        let font = FontRef::new(font_test_data::MATERIAL_ICONS_SUBSET).unwrap();
        let cff = CffFontRef::new_cff(font.cff().unwrap().offset_data().as_bytes(), 0).unwrap();
        let CffFontKind::Sid { private_dict, .. } = &cff.top_dict.kind else {
            panic!("this is an SID font");
        };
        assert!(private_dict.is_empty());
    }

    // We were overwriting family_other_blues with family_blues.
    #[test]
    fn capture_family_other_blues() {
        let private_dict_data = &font_test_data::cff2::EXAMPLE[0x4f..=0xc0];
        let store =
            ItemVariationStore::read(FontData::new(&font_test_data::cff2::EXAMPLE[18..])).unwrap();
        let coords = &[F2Dot14::from_f32(0.0)];
        let blend_state = BlendState::new(store, coords, 0).unwrap();
        let (_subfont, hint_params) = CffSubfont::new_hinted(
            private_dict_data,
            0..private_dict_data.len(),
            Some(blend_state),
            None,
        )
        .unwrap();
        assert_eq!(
            hint_params.family_other_blues,
            Blues::new([-249.0, -239.0].map(Fixed::from_f64).into_iter())
        )
    }

    #[test]
    fn subfont_cff() {
        let font = FontRef::new(font_test_data::NOTO_SERIF_DISPLAY_TRIMMED).unwrap();
        let cff = CffFontRef::new_cff(font.cff().unwrap().offset_data().as_bytes(), 0).unwrap();
        let subfont = cff.subfont(0, &[]).unwrap();
        assert_eq!(subfont.default_width, None);
        assert_eq!(subfont.nominal_width, Fixed::from_i32(598));
        assert_eq!(subfont.vs_index, 0);
        assert_eq!(subfont.matrix, None);
    }

    fn make_blues<const N: usize>(values: [i32; N]) -> Blues {
        Blues::new(values.map(Fixed::from_i32).into_iter())
    }

    #[test]
    fn hinted_subfont_cff() {
        let font = FontRef::new(font_test_data::NOTO_SERIF_DISPLAY_TRIMMED).unwrap();
        let cff = CffFontRef::new_cff(font.cff().unwrap().offset_data().as_bytes(), 0).unwrap();
        let (subfont, hinting) = cff.subfont_hinted(0, &[]).unwrap();
        assert_eq!(subfont.default_width, None);
        assert_eq!(subfont.nominal_width, Fixed::from_i32(598));
        assert_eq!(subfont.vs_index, 0);
        assert_eq!(subfont.matrix, None);
        let expected_hinting = HintingParams {
            blues: make_blues([-15, 0, 536, 547, 571, 582, 714, 726, 760, 772]),
            family_blues: Blues::default(),
            other_blues: make_blues([-255, -240]),
            family_other_blues: Blues::default(),
            blue_scale: Fixed::from_f64(0.0500030517578125),
            blue_shift: Fixed::from_f64(7.0),
            blue_fuzz: Fixed::ZERO,
            language_group: 0,
        };
        assert_eq!(hinting, expected_hinting);
    }

    #[test]
    fn subfont_cff2() {
        let font = FontRef::new(font_test_data::CANTARELL_VF_TRIMMED).unwrap();
        let cff = CffFontRef::new_cff2(font.cff2().unwrap().offset_data().as_bytes()).unwrap();
        let subfont = cff.subfont(0, &[]).unwrap();
        assert_eq!(subfont.default_width, None);
        assert_eq!(subfont.nominal_width, Fixed::ZERO);
        assert_eq!(subfont.vs_index, 0);
        assert_eq!(subfont.matrix, None);
    }

    #[test]
    fn hinted_subfont_cff2() {
        let font = FontRef::new(font_test_data::CANTARELL_VF_TRIMMED).unwrap();
        let cff = CffFontRef::new_cff2(font.cff2().unwrap().offset_data().as_bytes()).unwrap();
        let (subfont, hinting) = cff.subfont_hinted(0, &[]).unwrap();
        assert_eq!(subfont.default_width, None);
        assert_eq!(subfont.nominal_width, Fixed::ZERO);
        assert_eq!(subfont.vs_index, 0);
        assert_eq!(subfont.matrix, None);
        let expected_hinting = HintingParams {
            blues: make_blues([-10, 0, 482, 492, 694, 704, 739, 749]),
            family_blues: Blues::default(),
            other_blues: make_blues([-227, -217]),
            family_other_blues: Blues::default(),
            blue_scale: Fixed::from_f64(0.0625),
            blue_shift: Fixed::from_f64(7.0),
            blue_fuzz: Fixed::ONE,
            language_group: 0,
        };
        assert_eq!(hinting, expected_hinting);
    }

    #[test]
    fn subfont_matrix() {
        let font = FontRef::new(font_test_data::MATERIAL_ICONS_SUBSET_MATRIX).unwrap();
        let cff = CffFontRef::new_cff(font.cff().unwrap().offset_data().as_bytes(), 0).unwrap();
        let subfont = cff.subfont(0, &[]).unwrap();
        assert_eq!(subfont.default_width, None);
        assert_eq!(subfont.nominal_width, Fixed::ZERO);
        assert_eq!(subfont.vs_index, 0);
        let expected_matrix = FontMatrix([
            Fixed::from_i32(5),
            Fixed::ZERO,
            Fixed::ZERO,
            Fixed::from_i32(5),
            Fixed::ZERO,
            Fixed::ZERO,
        ]);
        let expected_scale = 10;
        assert_eq!(
            subfont.matrix,
            Some(ScaledFontMatrix {
                matrix: expected_matrix,
                scale: expected_scale
            })
        );
    }

    #[test]
    fn eval_charstring_cff() {
        let font = FontRef::new(font_test_data::NOTO_SERIF_DISPLAY_TRIMMED).unwrap();
        let cff = CffFontRef::new_cff(font.cff().unwrap().offset_data().as_bytes(), 0).unwrap();
        let mut sink = CharstringCommandCounter::default();
        let subfont = cff.subfont(0, &[]).unwrap();
        cff.evaluate_charstring(&subfont, &[], GlyphId::new(2), &mut sink)
            .unwrap();
        // Charstring eval is tested elsewhere so just make sure we're processing the
        // *correct* charstring.
        assert_eq!(sink.0, 18);
    }

    #[test]
    fn eval_charstring_cff2() {
        let font = FontRef::new(font_test_data::CANTARELL_VF_TRIMMED).unwrap();
        let cff = CffFontRef::new_cff2(font.cff2().unwrap().offset_data().as_bytes()).unwrap();
        let mut sink = CharstringCommandCounter::default();
        let subfont = cff.subfont(0, &[]).unwrap();
        cff.evaluate_charstring(&subfont, &[], GlyphId::new(2), &mut sink)
            .unwrap();
        // Charstring eval is tested elsewhere so just make sure we're processing the
        // *correct* charstring.
        assert_eq!(sink.0, 10);
    }
}

//! OpenType Layout common table formats

#[path = "./lookupflag.rs"]
mod lookupflag;
mod script;

use core::cmp::Ordering;

pub use lookupflag::LookupFlag;
pub use script::{ScriptTags, SelectedScript, UNICODE_TO_NEW_OPENTYPE_SCRIPT_TAGS};

use super::variations::DeltaSetIndex;

#[cfg(test)]
#[path = "../tests/layout.rs"]
mod spec_tests;

include!("../../generated/generated_layout.rs");

impl<'a, T: FontRead<'a>> Lookup<'a, T> {
    pub fn get_subtable(&self, offset: Offset16) -> Result<T, ReadError> {
        self.resolve_offset(offset)
    }

    #[cfg(feature = "experimental_traverse")]
    fn traverse_lookup_flag(&self) -> traversal::FieldType<'a> {
        self.lookup_flag().to_bits().into()
    }
}

/// A trait that abstracts the behaviour of an extension subtable
///
/// This is necessary because GPOS and GSUB have different concrete types
/// for their extension lookups.
pub trait ExtensionLookup<'a, T: FontRead<'a>>: FontRead<'a> {
    fn extension(&self) -> Result<T, ReadError>;
}

/// an array of subtables, maybe behind extension lookups
///
/// This is used to implement more ergonomic access to lookup subtables for
/// GPOS & GSUB lookup tables.
pub enum Subtables<'a, T: FontRead<'a>, Ext: ExtensionLookup<'a, T>> {
    Subtable(ArrayOfOffsets<'a, T>),
    Extension(ArrayOfOffsets<'a, Ext>),
}

impl<'a, T: FontRead<'a> + 'a, Ext: ExtensionLookup<'a, T> + 'a> Subtables<'a, T, Ext> {
    /// create a new subtables array given offsets to non-extension subtables
    pub(crate) fn new(offsets: &'a [BigEndian<Offset16>], data: FontData<'a>) -> Self {
        Subtables::Subtable(ArrayOfOffsets::new(offsets, data, ()))
    }

    /// create a new subtables array given offsets to extension subtables
    pub(crate) fn new_ext(offsets: &'a [BigEndian<Offset16>], data: FontData<'a>) -> Self {
        Subtables::Extension(ArrayOfOffsets::new(offsets, data, ()))
    }

    /// The number of subtables in this collection
    pub fn len(&self) -> usize {
        match self {
            Subtables::Subtable(inner) => inner.len(),
            Subtables::Extension(inner) => inner.len(),
        }
    }

    pub fn is_empty(&self) -> bool {
        self.len() == 0
    }

    /// Return the subtable at the given index
    pub fn get(&self, idx: usize) -> Result<T, ReadError> {
        match self {
            Subtables::Subtable(inner) => inner.get(idx),
            Subtables::Extension(inner) => inner.get(idx).and_then(|ext| ext.extension()),
        }
    }

    /// Return an iterator over all the subtables in the collection
    pub fn iter(&self) -> impl Iterator<Item = Result<T, ReadError>> + 'a {
        let (left, right) = match self {
            Subtables::Subtable(inner) => (Some(inner.iter()), None),
            Subtables::Extension(inner) => (
                None,
                Some(inner.iter().map(|ext| ext.and_then(|ext| ext.extension()))),
            ),
        };
        left.into_iter()
            .flatten()
            .chain(right.into_iter().flatten())
    }
}

/// An enum for different possible tables referenced by [Feature::feature_params_offset]
pub enum FeatureParams<'a> {
    StylisticSet(StylisticSetParams<'a>),
    Size(SizeParams<'a>),
    CharacterVariant(CharacterVariantParams<'a>),
}

impl ReadArgs for FeatureParams<'_> {
    type Args = Tag;
}

impl<'a> FontReadWithArgs<'a> for FeatureParams<'a> {
    fn read_with_args(bytes: FontData<'a>, args: &Tag) -> Result<FeatureParams<'a>, ReadError> {
        match *args {
            t if t == Tag::new(b"size") => SizeParams::read(bytes).map(Self::Size),
            // to whoever is debugging this dumb bug I wrote: I'm sorry.
            t if &t.to_raw()[..2] == b"ss" => {
                StylisticSetParams::read(bytes).map(Self::StylisticSet)
            }
            t if &t.to_raw()[..2] == b"cv" => {
                CharacterVariantParams::read(bytes).map(Self::CharacterVariant)
            }
            // NOTE: what even is our error condition here? an offset exists but
            // we don't know the tag?
            _ => Err(ReadError::InvalidFormat(0xdead)),
        }
    }
}

#[cfg(feature = "experimental_traverse")]
impl<'a> SomeTable<'a> for FeatureParams<'a> {
    fn type_name(&self) -> &str {
        match self {
            FeatureParams::StylisticSet(table) => table.type_name(),
            FeatureParams::Size(table) => table.type_name(),
            FeatureParams::CharacterVariant(table) => table.type_name(),
        }
    }

    fn get_field(&self, idx: usize) -> Option<Field<'a>> {
        match self {
            FeatureParams::StylisticSet(table) => table.get_field(idx),
            FeatureParams::Size(table) => table.get_field(idx),
            FeatureParams::CharacterVariant(table) => table.get_field(idx),
        }
    }
}

impl FeatureTableSubstitutionRecord {
    pub fn alternate_feature<'a>(&self, data: FontData<'a>) -> Result<Feature<'a>, ReadError> {
        self.alternate_feature_offset()
            .resolve_with_args(data, &Tag::new(b"NULL"))
    }
}

impl<'a> CoverageTable<'a> {
    pub fn iter(&self) -> impl Iterator<Item = GlyphId16> + 'a {
        // all one expression so that we have a single return type
        let (iter1, iter2) = match self {
            CoverageTable::Format1(t) => (Some(t.glyph_array().iter().map(|g| g.get())), None),
            CoverageTable::Format2(t) => {
                let iter = t.range_records().iter().flat_map(RangeRecord::iter);
                (None, Some(iter))
            }
        };

        iter1
            .into_iter()
            .flatten()
            .chain(iter2.into_iter().flatten())
    }

    /// If this glyph is in the coverage table, returns its index
    pub fn get(&self, gid: impl Into<GlyphId>) -> Option<u16> {
        match self {
            CoverageTable::Format1(sub) => sub.get(gid),
            CoverageTable::Format2(sub) => sub.get(gid),
        }
    }
}

impl CoverageFormat1<'_> {
    /// If this glyph is in the coverage table, returns its index
    pub fn get(&self, gid: impl Into<GlyphId>) -> Option<u16> {
        let gid16: GlyphId16 = gid.into().try_into().ok()?;
        let be_glyph: BigEndian<GlyphId16> = gid16.into();
        self.glyph_array()
            .binary_search(&be_glyph)
            .ok()
            .map(|idx| idx as _)
    }
}

impl CoverageFormat2<'_> {
    /// If this glyph is in the coverage table, returns its index
    pub fn get(&self, gid: impl Into<GlyphId>) -> Option<u16> {
        let gid: GlyphId16 = gid.into().try_into().ok()?;
        self.range_records()
            .binary_search_by(|rec| {
                if rec.end_glyph_id() < gid {
                    Ordering::Less
                } else if rec.start_glyph_id() > gid {
                    Ordering::Greater
                } else {
                    Ordering::Equal
                }
            })
            .ok()
            .map(|idx| {
                let rec = &self.range_records()[idx];
                rec.start_coverage_index() + gid.to_u16() - rec.start_glyph_id().to_u16()
            })
    }
}

impl RangeRecord {
    fn iter(&self) -> impl Iterator<Item = GlyphId16> + '_ {
        (self.start_glyph_id().to_u16()..=self.end_glyph_id().to_u16()).map(GlyphId16::new)
    }
}

impl DeltaFormat {
    pub(crate) fn value_count(self, start_size: u16, end_size: u16) -> usize {
        let range_len = end_size.saturating_add(1).saturating_sub(start_size) as usize;
        let val_per_word = match self {
            DeltaFormat::Local2BitDeltas => 8,
            DeltaFormat::Local4BitDeltas => 4,
            DeltaFormat::Local8BitDeltas => 2,
            _ => return 0,
        };

        let count = range_len / val_per_word;
        let extra = (range_len % val_per_word).min(1);
        count + extra
    }
}

// we as a 'format' in codegen, and the generic error type for an invalid format
// stores the value as an i64, so we need this conversion.
impl From<DeltaFormat> for i64 {
    fn from(value: DeltaFormat) -> Self {
        value as u16 as _
    }
}

impl<'a> ClassDefFormat1<'a> {
    /// Get the class for this glyph id
    pub fn get(&self, gid: GlyphId16) -> u16 {
        if gid < self.start_glyph_id() {
            return 0;
        }
        let idx = gid.to_u16() - self.start_glyph_id().to_u16();
        self.class_value_array()
            .get(idx as usize)
            .map(|x| x.get())
            .unwrap_or(0)
    }

    /// Iterate over each glyph and its class.
    pub fn iter(&self) -> impl Iterator<Item = (GlyphId16, u16)> + 'a {
        let start = self.start_glyph_id();
        self.class_value_array()
            .iter()
            .enumerate()
            .map(move |(i, val)| {
                let gid = start.to_u16().saturating_add(i as u16);
                (GlyphId16::new(gid), val.get())
            })
    }
}

impl<'a> ClassDefFormat2<'a> {
    /// Get the class for this glyph id
    pub fn get(&self, gid: GlyphId16) -> u16 {
        let records = self.class_range_records();
        let ix = match records.binary_search_by(|rec| rec.start_glyph_id().cmp(&gid)) {
            Ok(ix) => ix,
            Err(ix) => ix.saturating_sub(1),
        };
        if let Some(record) = records.get(ix) {
            if (record.start_glyph_id()..=record.end_glyph_id()).contains(&gid) {
                return record.class();
            }
        }
        0
    }

    /// Iterate over each glyph and its class.
    pub fn iter(&self) -> impl Iterator<Item = (GlyphId16, u16)> + 'a {
        self.class_range_records().iter().flat_map(|range| {
            let start = range.start_glyph_id().to_u16();
            let end = range.end_glyph_id().to_u16();
            (start..=end).map(|gid| (GlyphId16::new(gid), range.class()))
        })
    }
}

impl ClassDef<'_> {
    /// Get the class for this glyph id
    pub fn get(&self, gid: GlyphId16) -> u16 {
        match self {
            ClassDef::Format1(table) => table.get(gid),
            ClassDef::Format2(table) => table.get(gid),
        }
    }

    /// Iterate over each glyph and its class.
    ///
    /// This will not include class 0 unless it has been explicitly assigned.
    pub fn iter(&self) -> impl Iterator<Item = (GlyphId16, u16)> + '_ {
        let (one, two) = match self {
            ClassDef::Format1(inner) => (Some(inner.iter()), None),
            ClassDef::Format2(inner) => (None, Some(inner.iter())),
        };
        one.into_iter().flatten().chain(two.into_iter().flatten())
    }
}

impl<'a> Device<'a> {
    /// Iterate over the decoded values for this device
    pub fn iter(&self) -> impl Iterator<Item = i8> + 'a {
        let format = self.delta_format();
        let mut n = (self.end_size() - self.start_size()) as usize + 1;
        let deltas_per_word = match format {
            DeltaFormat::Local2BitDeltas => 8,
            DeltaFormat::Local4BitDeltas => 4,
            DeltaFormat::Local8BitDeltas => 2,
            _ => 0,
        };

        self.delta_value().iter().flat_map(move |val| {
            let iter = iter_packed_values(val.get(), format, n);
            n = n.saturating_sub(deltas_per_word);
            iter
        })
    }
}

fn iter_packed_values(raw: u16, format: DeltaFormat, n: usize) -> impl Iterator<Item = i8> {
    let mut decoded = [None; 8];
    let (mask, sign_mask, bits) = match format {
        DeltaFormat::Local2BitDeltas => (0b11, 0b10, 2usize),
        DeltaFormat::Local4BitDeltas => (0b1111, 0b1000, 4),
        DeltaFormat::Local8BitDeltas => (0b1111_1111, 0b1000_0000, 8),
        _ => (0, 0, 0),
    };

    let max_per_word = 16 / bits;
    #[allow(clippy::needless_range_loop)] // enumerate() feels weird here
    for i in 0..n.min(max_per_word) {
        let mask = mask << ((16 - bits) - i * bits);
        let val = (raw & mask) >> ((16 - bits) - i * bits);
        let sign = val & sign_mask != 0;

        let val = if sign {
            // it is 2023 and I am googling to remember how twos compliment works
            -((((!val) & mask) + 1) as i8)
        } else {
            val as i8
        };
        decoded[i] = Some(val)
    }
    decoded.into_iter().flatten()
}

impl From<VariationIndex<'_>> for DeltaSetIndex {
    fn from(src: VariationIndex) -> DeltaSetIndex {
        DeltaSetIndex {
            outer: src.delta_set_outer_index(),
            inner: src.delta_set_inner_index(),
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn coverage_get_format1() {
        // manually generated, corresponding to the glyphs (1, 7, 13, 27, 44);
        const COV1_DATA: FontData = FontData::new(&[0, 1, 0, 5, 0, 1, 0, 7, 0, 13, 0, 27, 0, 44]);

        let coverage = CoverageFormat1::read(COV1_DATA).unwrap();
        assert_eq!(coverage.get(GlyphId::new(1)), Some(0));
        assert_eq!(coverage.get(GlyphId::new(2)), None);
        assert_eq!(coverage.get(GlyphId::new(7)), Some(1));
        assert_eq!(coverage.get(GlyphId::new(27)), Some(3));
        assert_eq!(coverage.get(GlyphId::new(45)), None);
    }

    #[test]
    fn coverage_get_format2() {
        // manually generated, corresponding to glyphs (5..10) and (30..40).
        const COV2_DATA: FontData =
            FontData::new(&[0, 2, 0, 2, 0, 5, 0, 9, 0, 0, 0, 30, 0, 39, 0, 5]);
        let coverage = CoverageFormat2::read(COV2_DATA).unwrap();
        assert_eq!(coverage.get(GlyphId::new(2)), None);
        assert_eq!(coverage.get(GlyphId::new(7)), Some(2));
        assert_eq!(coverage.get(GlyphId::new(9)), Some(4));
        assert_eq!(coverage.get(GlyphId::new(10)), None);
        assert_eq!(coverage.get(GlyphId::new(32)), Some(7));
        assert_eq!(coverage.get(GlyphId::new(39)), Some(14));
        assert_eq!(coverage.get(GlyphId::new(40)), None);
    }

    #[test]
    fn classdef_get_format2() {
        let classdef = ClassDef::read(FontData::new(
            font_test_data::gdef::MARKATTACHCLASSDEF_TABLE,
        ))
        .unwrap();
        assert!(matches!(classdef, ClassDef::Format2(..)));
        let gid_class_pairs = [
            (616, 1),
            (617, 1),
            (618, 1),
            (624, 1),
            (625, 1),
            (626, 1),
            (652, 2),
            (653, 2),
            (654, 2),
            (655, 2),
            (661, 2),
        ];
        for (gid, class) in gid_class_pairs {
            assert_eq!(classdef.get(GlyphId16::new(gid)), class);
        }
        for (gid, class) in classdef.iter() {
            assert_eq!(classdef.get(gid), class);
        }
    }

    #[test]
    fn delta_decode() {
        // these examples come from the spec
        assert_eq!(
            iter_packed_values(0x123f, DeltaFormat::Local4BitDeltas, 4).collect::<Vec<_>>(),
            &[1, 2, 3, -1]
        );

        assert_eq!(
            iter_packed_values(0x5540, DeltaFormat::Local2BitDeltas, 5).collect::<Vec<_>>(),
            &[1, 1, 1, 1, 1]
        );
    }

    #[test]
    fn delta_decode_all() {
        // manually generated with write-fonts
        let bytes: &[u8] = &[0, 7, 0, 13, 0, 3, 1, 244, 30, 245, 101, 8, 42, 0];
        let device = Device::read(bytes.into()).unwrap();
        assert_eq!(
            device.iter().collect::<Vec<_>>(),
            &[1i8, -12, 30, -11, 101, 8, 42]
        );
    }
}

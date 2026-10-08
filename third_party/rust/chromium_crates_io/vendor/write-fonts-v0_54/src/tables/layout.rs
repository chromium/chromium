//! OpenType layout.

use std::{collections::HashSet, hash::Hash};

pub use read_fonts::tables::layout::LookupFlag;
use read_fonts::FontRead;

pub mod builders;
#[cfg(test)]
mod spec_tests;

include!("../../generated/generated_layout.rs");

/// A macro to implement the [LookupSubtable] trait.
macro_rules! lookup_type {
    (gpos, $ty:ty, $val:expr) => {
        impl LookupSubtable for $ty {
            const TYPE: LookupType = LookupType::Gpos($val);
        }
    };

    (gsub, $ty:ty, $val:expr) => {
        impl LookupSubtable for $ty {
            const TYPE: LookupType = LookupType::Gsub($val);
        }
    };
}

/// A macro to define a newtype around an existing table, that defers all
/// impls to that table.
///
/// We use this to ensure that shared lookup types (Sequence/Chain
/// lookups) can be given different lookup ids for each of GSUB/GPOS.
macro_rules! table_newtype {
    ($name:ident, $inner:ident, $read_type:path) => {
        /// A typed wrapper around a shared table.
        ///
        /// This is used so that we can associate the correct lookup ids for
        /// lookups that are shared between GPOS/GSUB.
        ///
        /// You can access the inner type via `Deref` or the `as_inner` method.
        #[derive(Clone, Debug, Default, PartialEq, Eq, PartialOrd, Ord, Hash)]
        #[cfg_attr(feature = "serde", derive(serde::Serialize, serde::Deserialize))]
        pub struct $name($inner);

        impl $name {
            /// Return a reference to the inner type.
            pub fn as_inner(&self) -> &$inner {
                &self.0
            }
        }

        impl std::ops::Deref for $name {
            type Target = $inner;
            fn deref(&self) -> &Self::Target {
                &self.0
            }
        }

        impl std::ops::DerefMut for $name {
            fn deref_mut(&mut self) -> &mut Self::Target {
                &mut self.0
            }
        }

        impl FontWrite for $name {
            fn write_into(&self, writer: &mut TableWriter) {
                self.0.write_into(writer)
            }

            fn table_type(&self) -> crate::table_type::TableType {
                self.0.table_type()
            }
        }

        impl Validate for $name {
            fn validate_impl(&self, ctx: &mut ValidationCtx) {
                self.0.validate_impl(ctx)
            }
        }

        impl<'a> FromObjRef<$read_type> for $name {
            fn from_obj_ref(obj: &$read_type, _data: FontData) -> Self {
                Self(FromObjRef::from_obj_ref(obj, _data))
            }
        }

        impl<'a> FromTableRef<$read_type> for $name {}

        impl From<$inner> for $name {
            fn from(src: $inner) -> $name {
                $name(src)
            }
        }
    };
}

pub(crate) use lookup_type;
pub(crate) use table_newtype;

impl FontWrite for LookupFlag {
    fn write_into(&self, writer: &mut TableWriter) {
        self.to_bits().write_into(writer)
    }
}

impl<T: LookupSubtable + FontWrite> FontWrite for Lookup<T> {
    fn write_into(&self, writer: &mut TableWriter) {
        T::TYPE.write_into(writer);
        self.lookup_flag.write_into(writer);
        u16::try_from(self.subtables.len())
            .unwrap()
            .write_into(writer);
        self.subtables.write_into(writer);
        self.mark_filtering_set.write_into(writer);
    }

    fn table_type(&self) -> crate::table_type::TableType {
        T::TYPE.into()
    }
}

impl Lookup<SequenceContext> {
    /// Convert this untyped SequenceContext into its GSUB or GPOS specific version
    pub fn into_concrete<T: From<SequenceContext>>(self) -> Lookup<T> {
        let Lookup {
            lookup_flag,
            subtables,
            mark_filtering_set,
        } = self;
        let subtables = subtables
            .into_iter()
            .map(|offset| OffsetMarker::new(offset.into_inner().into()))
            .collect();
        Lookup {
            lookup_flag,
            subtables,
            mark_filtering_set,
        }
    }
}

impl Lookup<ChainedSequenceContext> {
    /// Convert this untyped SequenceContext into its GSUB or GPOS specific version
    pub fn into_concrete<T: From<ChainedSequenceContext>>(self) -> Lookup<T> {
        let Lookup {
            lookup_flag,
            subtables,
            mark_filtering_set,
        } = self;
        let subtables = subtables
            .into_iter()
            .map(|offset| OffsetMarker::new(offset.into_inner().into()))
            .collect();
        Lookup {
            lookup_flag,
            subtables,
            mark_filtering_set,
        }
    }
}

/// A utility trait for writing lookup tables.
///
/// This allows us to attach the numerical lookup type to the appropriate concrete
/// types, so that we can write it as needed without passing it around.
pub trait LookupSubtable {
    /// The lookup type of this layout subtable.
    const TYPE: LookupType;
}

/// Raw values for the different layout subtables
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum LookupType {
    Gpos(u16),
    Gsub(u16),
}

impl LookupType {
    pub(crate) const GSUB_EXT_TYPE: u16 = 7;
    pub(crate) const GPOS_EXT_TYPE: u16 = 9;
    pub(crate) const PAIR_POS: u16 = 2;
    pub(crate) const MARK_TO_BASE: u16 = 4;

    pub(crate) fn to_raw(self) -> u16 {
        match self {
            LookupType::Gpos(val) => val,
            LookupType::Gsub(val) => val,
        }
    }

    pub(crate) fn promote(self) -> Self {
        match self {
            LookupType::Gpos(Self::GPOS_EXT_TYPE) | LookupType::Gsub(Self::GSUB_EXT_TYPE) => {
                panic!("should never be promoting an extension subtable")
            }
            LookupType::Gpos(_) => LookupType::Gpos(Self::GPOS_EXT_TYPE),
            LookupType::Gsub(_) => LookupType::Gsub(Self::GSUB_EXT_TYPE),
        }
    }
}

impl FontWrite for LookupType {
    fn write_into(&self, writer: &mut TableWriter) {
        self.to_raw().write_into(writer)
    }
}

#[derive(Clone, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
#[cfg_attr(feature = "serde", derive(serde::Serialize, serde::Deserialize))]
pub enum FeatureParams {
    StylisticSet(StylisticSetParams),
    Size(SizeParams),
    CharacterVariant(CharacterVariantParams),
}

impl FontWrite for FeatureParams {
    fn write_into(&self, writer: &mut TableWriter) {
        match self {
            FeatureParams::StylisticSet(table) => table.write_into(writer),
            FeatureParams::Size(table) => table.write_into(writer),
            FeatureParams::CharacterVariant(table) => table.write_into(writer),
        }
    }
}

impl Validate for FeatureParams {
    fn validate_impl(&self, ctx: &mut ValidationCtx) {
        match self {
            Self::StylisticSet(table) => table.validate_impl(ctx),
            Self::Size(table) => table.validate_impl(ctx),
            Self::CharacterVariant(table) => table.validate_impl(ctx),
        }
    }
}

impl FromObjRef<read_fonts::tables::layout::FeatureParams<'_>> for FeatureParams {
    fn from_obj_ref(from: &read_fonts::tables::layout::FeatureParams, data: FontData) -> Self {
        use read_fonts::tables::layout::FeatureParams as FromType;
        match from {
            FromType::Size(thing) => Self::Size(SizeParams::from_obj_ref(thing, data)),
            FromType::StylisticSet(thing) => {
                Self::StylisticSet(FromObjRef::from_obj_ref(thing, data))
            }
            FromType::CharacterVariant(thing) => {
                Self::CharacterVariant(FromObjRef::from_obj_ref(thing, data))
            }
        }
    }
}

impl FromTableRef<read_fonts::tables::layout::FeatureParams<'_>> for FeatureParams {}

impl ClassDefFormat1 {
    fn iter(&self) -> impl Iterator<Item = (GlyphId16, u16)> + '_ {
        self.class_value_array.iter().enumerate().map(|(i, cls)| {
            (
                GlyphId16::new(self.start_glyph_id.to_u16().saturating_add(i as u16)),
                *cls,
            )
        })
    }
}

impl ClassRangeRecord {
    fn validate_glyph_range(&self, ctx: &mut ValidationCtx) {
        if self.start_glyph_id > self.end_glyph_id {
            ctx.report(format!(
                "start_glyph_id {} larger than end_glyph_id {}",
                self.start_glyph_id, self.end_glyph_id
            ));
        }
    }

    fn contains(&self, gid: GlyphId16) -> bool {
        (self.start_glyph_id..=self.end_glyph_id).contains(&gid)
    }
}

impl ClassDefFormat2 {
    fn iter(&self) -> impl Iterator<Item = (GlyphId16, u16)> + '_ {
        self.class_range_records.iter().flat_map(|rcd| {
            (rcd.start_glyph_id.to_u16()..=rcd.end_glyph_id.to_u16())
                .map(|gid| (GlyphId16::new(gid), rcd.class))
        })
    }
}

impl ClassDef {
    pub fn iter(&self) -> impl Iterator<Item = (GlyphId16, u16)> + '_ {
        let (one, two) = match self {
            Self::Format1(table) => (Some(table.iter()), None),
            Self::Format2(table) => (None, Some(table.iter())),
        };

        one.into_iter().flatten().chain(two.into_iter().flatten())
    }

    /// Return the glyph class for the provided glyph.
    ///
    /// Glyphs which have not been assigned a class are given class 0
    pub fn get(&self, glyph: GlyphId16) -> u16 {
        self.get_raw(glyph).unwrap_or(0)
    }

    // exposed for testing
    fn get_raw(&self, glyph: GlyphId16) -> Option<u16> {
        match self {
            ClassDef::Format1(table) => glyph
                .to_u16()
                .checked_sub(table.start_glyph_id.to_u16())
                .and_then(|idx| table.class_value_array.get(idx as usize))
                .copied(),
            ClassDef::Format2(table) => table
                .class_range_records
                .iter()
                .find_map(|rec| rec.contains(glyph).then_some(rec.class)),
        }
    }

    pub fn class_count(&self) -> u16 {
        //TODO: implement a good integer set!!
        self.iter()
            .map(|(_gid, cls)| cls)
            .chain(std::iter::once(0))
            .collect::<HashSet<_>>()
            .len()
            .try_into()
            .unwrap()
    }

    /// Returns `true` if no glyphs are explicitly assigned to a class in this table
    pub fn is_empty(&self) -> bool {
        match self {
            Self::Format1(table) => table.class_value_array.is_empty(),
            Self::Format2(table) => table.class_range_records.is_empty(),
        }
    }
}

impl CoverageFormat1 {
    fn iter(&self) -> impl Iterator<Item = GlyphId16> + '_ {
        self.glyph_array.iter().copied()
    }

    fn len(&self) -> usize {
        self.glyph_array.len()
    }
}

impl CoverageFormat2 {
    fn iter(&self) -> impl Iterator<Item = GlyphId16> + '_ {
        self.range_records
            .iter()
            .flat_map(|rcd| iter_gids(rcd.start_glyph_id, rcd.end_glyph_id))
    }

    fn len(&self) -> usize {
        self.range_records
            .iter()
            .map(|rcd| {
                rcd.end_glyph_id
                    .to_u16()
                    .saturating_sub(rcd.start_glyph_id.to_u16()) as usize
                    + 1
            })
            .sum()
    }
}

impl CoverageTable {
    pub fn iter(&self) -> impl Iterator<Item = GlyphId16> + '_ {
        let (one, two) = match self {
            Self::Format1(table) => (Some(table.iter()), None),
            Self::Format2(table) => (None, Some(table.iter())),
        };

        one.into_iter().flatten().chain(two.into_iter().flatten())
    }

    pub fn len(&self) -> usize {
        match self {
            Self::Format1(table) => table.len(),
            Self::Format2(table) => table.len(),
        }
    }

    pub fn is_empty(&self) -> bool {
        self.len() == 0
    }
}

impl FromIterator<GlyphId16> for CoverageTable {
    fn from_iter<T: IntoIterator<Item = GlyphId16>>(iter: T) -> Self {
        let glyphs = iter.into_iter().collect::<Vec<_>>();
        builders::CoverageTableBuilder::from_glyphs(glyphs).build()
    }
}

impl From<Vec<GlyphId16>> for CoverageTable {
    fn from(value: Vec<GlyphId16>) -> Self {
        builders::CoverageTableBuilder::from_glyphs(value).build()
    }
}

impl FromIterator<(GlyphId16, u16)> for ClassDef {
    fn from_iter<T: IntoIterator<Item = (GlyphId16, u16)>>(iter: T) -> Self {
        builders::ClassDefBuilderImpl::from_iter(iter).build()
    }
}

impl RangeRecord {
    /// An iterator over records for this array of glyphs.
    ///
    /// # Note
    ///
    /// this function expects that glyphs are already sorted.
    pub fn iter_for_glyphs(glyphs: &[GlyphId16]) -> impl Iterator<Item = RangeRecord> + '_ {
        let mut cur_range = glyphs.first().copied().map(|g| (g, g));
        let mut len = 0u16;
        let mut iter = glyphs.iter().skip(1).copied();

        #[allow(clippy::while_let_on_iterator)]
        std::iter::from_fn(move || {
            while let Some(glyph) = iter.next() {
                match cur_range {
                    None => return None,
                    Some((a, b)) if are_sequential(b, glyph) => cur_range = Some((a, glyph)),
                    Some((a, b)) => {
                        let result = RangeRecord {
                            start_glyph_id: a,
                            end_glyph_id: b,
                            start_coverage_index: len,
                        };
                        cur_range = Some((glyph, glyph));
                        len += 1 + b.to_u16().saturating_sub(a.to_u16());
                        return Some(result);
                    }
                }
            }
            cur_range
                .take()
                .map(|(start_glyph_id, end_glyph_id)| RangeRecord {
                    start_glyph_id,
                    end_glyph_id,
                    start_coverage_index: len,
                })
        })
    }
}

fn iter_gids(gid1: GlyphId16, gid2: GlyphId16) -> impl Iterator<Item = GlyphId16> {
    (gid1.to_u16()..=gid2.to_u16()).map(GlyphId16::new)
}

fn are_sequential(gid1: GlyphId16, gid2: GlyphId16) -> bool {
    gid2.to_u16().saturating_sub(gid1.to_u16()) == 1
}

impl Device {
    pub fn new(start_size: u16, end_size: u16, values: &[i8]) -> Self {
        debug_assert_eq!(
            (start_size..=end_size).count(),
            values.len(),
            "device range and values must match"
        );
        let delta_format: DeltaFormat = values
            .iter()
            .map(|val| match val {
                -2..=1 => DeltaFormat::Local2BitDeltas,
                -8..=7 => DeltaFormat::Local4BitDeltas,
                _ => DeltaFormat::Local8BitDeltas,
            })
            .max()
            .unwrap_or_default();
        let delta_value = encode_delta(delta_format, values);

        Device {
            start_size,
            end_size,
            delta_format,
            delta_value,
        }
    }
}

impl DeviceOrVariationIndex {
    /// Create a new [`Device`] subtable
    pub fn device(start_size: u16, end_size: u16, values: &[i8]) -> Self {
        DeviceOrVariationIndex::Device(Device::new(start_size, end_size, values))
    }
}

impl FontWrite for PendingVariationIndex {
    fn write_into(&self, _writer: &mut TableWriter) {
        panic!(
            "Attempted to write PendingVariationIndex.\n\
            VariationIndex tables should always be resolved before compilation.\n\
            Please report this bug at <https://github.com/googlefonts/fontations/issues>"
        )
    }
}

fn encode_delta(format: DeltaFormat, values: &[i8]) -> Vec<u16> {
    let (chunk_size, mask, bits) = match format {
        DeltaFormat::Local2BitDeltas => (8, 0b11, 2),
        DeltaFormat::Local4BitDeltas => (4, 0b1111, 4),
        DeltaFormat::Local8BitDeltas => (2, 0b11111111, 8),
        _ => panic!("invalid format"),
    };
    values
        .chunks(chunk_size)
        .map(|chunk| encode_chunk(chunk, mask, bits))
        .collect()
}

fn encode_chunk(chunk: &[i8], mask: u8, bits: usize) -> u16 {
    let mut out = 0u16;
    for (i, val) in chunk.iter().enumerate() {
        out |= ((val.to_be_bytes()[0] & mask) as u16) << ((16 - bits) - i * bits);
    }
    out
}

impl From<VariationIndex> for u32 {
    fn from(value: VariationIndex) -> Self {
        ((value.delta_set_outer_index as u32) << 16) | value.delta_set_inner_index as u32
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    #[should_panic(expected = "array exceeds max length")]
    fn array_len_smoke_test() {
        let table = ScriptList {
            script_records: vec![ScriptRecord {
                script_tag: Tag::new(b"hihi"),
                script: OffsetMarker::new(Script {
                    default_lang_sys: NullableOffsetMarker::new(None),
                    lang_sys_records: vec![LangSysRecord {
                        lang_sys_tag: Tag::new(b"coco"),
                        lang_sys: OffsetMarker::new(LangSys {
                            required_feature_index: 0xffff,
                            feature_indices: vec![69; (u16::MAX) as usize + 5],
                        }),
                    }],
                }),
            }],
        };

        table.validate().unwrap();
    }

    #[test]
    #[should_panic(expected = "larger than end_glyph_id")]
    fn validate_classdef_ranges() {
        let classdef = ClassDefFormat2::new(vec![ClassRangeRecord::new(
            GlyphId16::new(12),
            GlyphId16::new(3),
            7,
        )]);

        classdef.validate().unwrap();
    }

    #[test]
    fn delta_encode() {
        let inp = [1i8, 2, 3, -1];
        let result = encode_delta(DeltaFormat::Local4BitDeltas, &inp);
        assert_eq!(result.len(), 1);
        assert_eq!(result[0], 0x123f_u16);

        let inp = [1i8, 1, 1, 1, 1];
        let result = encode_delta(DeltaFormat::Local2BitDeltas, &inp);
        assert_eq!(result.len(), 1);
        assert_eq!(result[0], 0x5540_u16);
    }
}

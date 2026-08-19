//! Common utilities and helpers for constructing layout tables

use std::collections::{BTreeMap, HashMap, HashSet};

use read_fonts::collections::IntSet;
use types::GlyphId16;

use super::{
    ClassDef, ClassDefFormat1, ClassDefFormat2, ClassRangeRecord, CoverageFormat1, CoverageFormat2,
    CoverageTable, Device, DeviceOrVariationIndex, Lookup, LookupFlag, PendingVariationIndex,
    RangeRecord,
};
use crate::tables::{
    gdef::CaretValue,
    variations::{ivs_builder::VariationStoreBuilder, VariationRegion},
};

/// A simple trait for building GPOS/GSUB lookups and subtables.
///
// This exists because we use it to implement `LookupBuilder<T>`
pub trait Builder {
    /// The type produced by this builder.
    ///
    /// In the case of lookups, this is always a `Vec<Subtable>`, because a single
    /// builder may produce multiple subtables in some instances.
    type Output;
    /// Finalize the builder, producing the output.
    ///
    /// # Note:
    ///
    /// The var_store is only used in GPOS, but we pass it everywhere.
    /// This is annoying but feels like the lesser of two evils. It's easy to
    /// ignore this argument where it isn't used, and this makes the logic
    /// in LookupBuilder simpler, since it is identical for GPOS/GSUB.
    ///
    /// It would be nice if this could then be Option<&mut T>, but that type is
    /// annoying to work with, as Option<&mut _> doesn't impl Copy, so you need
    /// to do a dance anytime you use it.
    fn build(self, var_store: &mut VariationStoreBuilder) -> Self::Output;
}

pub(crate) type FilterSetId = u16;

#[derive(Clone, Debug, Default)]
pub struct LookupBuilder<T> {
    pub flags: LookupFlag,
    pub mark_set: Option<FilterSetId>,
    pub subtables: Vec<T>,
}

/// An opinionated builder for `ClassDef`s.
///
/// This ensures that class ids are assigned based on the size of the class.
///
/// If you need to know the values assigned to particular classes, call the
/// [`ClassDefBuilder::build_with_mapping`] method, which will build the final
/// [`ClassDef`] table, and will also return a map from the original class sets
/// to the final assigned class id values.
///
/// If you don't care about this, you can also construct a `ClassDef` from any
/// iterator over `(GlyphId16, u16)` tuples, using collect:
///
/// ```
/// # use write_fonts::{types::GlyphId16, tables::layout::ClassDef};
/// let gid1 = GlyphId16::new(1);
/// let gid2 = GlyphId16::new(2);
/// let gid3 = GlyphId16::new(2);
/// let my_class: ClassDef = [(gid1, 2), (gid2, 3), (gid3, 4)].into_iter().collect();
/// ```
#[derive(Clone, Debug, Default, PartialEq, Eq)]
#[cfg_attr(feature = "serde", derive(serde::Serialize, serde::Deserialize))]
pub struct ClassDefBuilder {
    classes: HashSet<IntSet<GlyphId16>>,
    all_glyphs: IntSet<GlyphId16>,
    use_class_0: bool,
}

/// A builder for [CoverageTable] tables.
///
/// This will choose the best format based for the included glyphs.
#[derive(Debug, Default, PartialEq, Eq)]
pub struct CoverageTableBuilder {
    // invariant: is always sorted
    glyphs: Vec<GlyphId16>,
}

/// A value with a default position and optionally variations or a device table.
///
/// This is used in the API for types like [`ValueRecordBuilder`] and
/// [`AnchorBuilder`].
///
/// [`ValueRecordBuilder`]: crate::tables::gpos::builders::ValueRecordBuilder
/// [`AnchorBuilder`]: crate::tables::gpos::builders::AnchorBuilder
#[derive(Clone, Debug, Default, PartialEq, Eq, PartialOrd, Ord)]
#[cfg_attr(feature = "serde", derive(serde::Serialize, serde::Deserialize))]
pub struct Metric {
    /// The value at the default location
    pub default: i16,
    /// An optional device table or delta set
    pub device_or_deltas: DeviceOrDeltas,
}

/// Either a `Device` table or a set of deltas.
///
/// This stores deltas directly; during compilation, the deltas are bundled
/// into some [`ItemVariationStore`], and referenced by a [`VariationIndex`].
///
/// [`ItemVariationStore`]: crate::tables::variations::ItemVariationStore
/// [`VariationIndex`]: super::VariationIndex
#[derive(Clone, Debug, Default, PartialEq, Eq, PartialOrd, Ord)]
#[cfg_attr(feature = "serde", derive(serde::Serialize, serde::Deserialize))]
#[allow(missing_docs)]
pub enum DeviceOrDeltas {
    Device(Device),
    Deltas(Vec<(VariationRegion, i16)>),
    #[default]
    None,
}

/// A value in the GDEF ligature caret list
#[derive(Clone, Debug, PartialEq, Eq, PartialOrd, Ord)]
#[cfg_attr(feature = "serde", derive(serde::Serialize, serde::Deserialize))]
pub enum CaretValueBuilder {
    /// An X or Y value (in design units) with optional deltas
    Coordinate {
        /// The value at the default location
        default: i16,
        /// An optional device table or delta set
        deltas: DeviceOrDeltas,
    },
    /// The index of a contour point to be used as the caret location.
    ///
    /// This format is rarely used.
    PointIndex(u16),
}

impl ClassDefBuilder {
    /// Create a new `ClassDefBuilder`.
    pub fn new() -> Self {
        Self::default()
    }

    /// Create a new `ClassDefBuilder` that will assign glyphs to class 0.
    ///
    /// In general, class 0 is a sentinel value returned when a glyph is not
    /// assigned to any other class; however in some cases (specifically in
    /// GPOS type two lookups) the `ClassDef` has an accompanying [`CoverageTable`],
    /// which means that class 0 can be used, since it is known that the class
    /// is only checked if a glyph is known to have _some_ class.
    pub fn new_using_class_0() -> Self {
        Self {
            use_class_0: true,
            ..Default::default()
        }
    }

    pub(crate) fn can_add(&self, cls: &IntSet<GlyphId16>) -> bool {
        self.classes.contains(cls) || cls.iter().all(|gid| !self.all_glyphs.contains(gid))
    }

    /// Check that this class can be added to this classdef, and add it if so.
    ///
    /// returns `true` if the class is added, and `false` otherwise.
    pub fn checked_add(&mut self, cls: IntSet<GlyphId16>) -> bool {
        if self.can_add(&cls) {
            self.all_glyphs.extend(cls.iter());
            self.classes.insert(cls);
            true
        } else {
            false
        }
    }

    /// Returns a compiled [`ClassDef`], as well as a mapping from our glyph sets
    /// to the final class ids.
    ///
    /// This sorts the classes, ensuring that larger classes are first.
    ///
    /// (This is needed when subsequent structures are ordered based on the
    /// final order of class assignments.)
    pub fn build_with_mapping(self) -> (ClassDef, HashMap<IntSet<GlyphId16>, u16>) {
        let mut classes = self.classes.into_iter().collect::<Vec<_>>();
        // we match the sort order used by fonttools, see:
        // <https://github.com/fonttools/fonttools/blob/9a46f9d3ab01e3/Lib/fontTools/otlLib/builder.py#L2677>
        classes.sort_unstable_by_key(|cls| {
            (
                std::cmp::Reverse(cls.len()),
                cls.iter().next().unwrap_or_default().to_u16(),
            )
        });
        classes.dedup();
        let add_one = u16::from(!self.use_class_0);
        let mapping = classes
            .into_iter()
            .enumerate()
            .map(|(i, cls)| (cls, i as u16 + add_one))
            .collect::<HashMap<_, _>>();
        let class_def = mapping
            .iter()
            .flat_map(|(cls, id)| cls.iter().map(move |gid| (gid, *id)))
            .collect();

        (class_def, mapping)
    }

    /// Build a final [`ClassDef`] table.
    pub fn build(self) -> ClassDef {
        self.build_with_mapping().0
    }
}

/// Builder logic for classdefs.
///
/// This handles the actual serialization, picking the best format based on the
/// included glyphs.
///
/// This will choose the best format based for the included glyphs.
#[derive(Debug, PartialEq, Eq)]
pub(super) struct ClassDefBuilderImpl {
    items: BTreeMap<GlyphId16, u16>,
}

impl ClassDefBuilderImpl {
    fn prefer_format_1(&self) -> bool {
        const U16_LEN: usize = std::mem::size_of::<u16>();
        const FORMAT1_HEADER_LEN: usize = U16_LEN * 3;
        const FORMAT2_HEADER_LEN: usize = U16_LEN * 2;
        const CLASS_RANGE_RECORD_LEN: usize = U16_LEN * 3;
        // format 2 is the most efficient way to represent an empty classdef
        if self.items.is_empty() {
            return false;
        }
        // calculate our format2 size:
        let first = self.items.keys().next().map(|g| g.to_u16()).unwrap();
        let last = self.items.keys().next_back().map(|g| g.to_u16()).unwrap();
        let format1_array_len = (last - first) as usize + 1;
        let len_format1 = FORMAT1_HEADER_LEN + format1_array_len * U16_LEN;
        let len_format2 =
            FORMAT2_HEADER_LEN + iter_class_ranges(&self.items).count() * CLASS_RANGE_RECORD_LEN;

        len_format1 < len_format2
    }

    pub fn build(&self) -> ClassDef {
        if self.prefer_format_1() {
            let first = self.items.keys().next().map(|g| g.to_u16()).unwrap_or(0);
            let last = self.items.keys().next_back().map(|g| g.to_u16());
            let class_value_array = (first..=last.unwrap_or_default())
                .map(|g| self.items.get(&GlyphId16::new(g)).copied().unwrap_or(0))
                .collect();
            ClassDef::Format1(ClassDefFormat1 {
                start_glyph_id: self
                    .items
                    .keys()
                    .next()
                    .copied()
                    .unwrap_or(GlyphId16::NOTDEF),
                class_value_array,
            })
        } else {
            ClassDef::Format2(ClassDefFormat2 {
                class_range_records: iter_class_ranges(&self.items).collect(),
            })
        }
    }
}

impl CoverageTableBuilder {
    /// Create a new builder from a vec of `GlyphId`.
    pub fn from_glyphs(mut glyphs: Vec<GlyphId16>) -> Self {
        glyphs.sort_unstable();
        glyphs.dedup();
        CoverageTableBuilder { glyphs }
    }

    /// Add a `GlyphId` to this coverage table.
    ///
    /// Returns the coverage index of the added glyph.
    ///
    /// If the glyph already exists, this returns its current index.
    pub fn add(&mut self, glyph: GlyphId16) -> u16 {
        match self.glyphs.binary_search(&glyph) {
            Ok(ix) => ix as u16,
            Err(ix) => {
                self.glyphs.insert(ix, glyph);
                // if we're over u16::MAX glyphs, crash
                ix.try_into().unwrap()
            }
        }
    }

    //NOTE: it would be nice if we didn't do this intermediate step and instead
    //wrote out bytes directly, but the current approach is simpler.
    /// Convert this builder into the appropriate [CoverageTable] variant.
    pub fn build(self) -> CoverageTable {
        if should_choose_coverage_format_2(&self.glyphs) {
            CoverageTable::Format2(CoverageFormat2 {
                range_records: RangeRecord::iter_for_glyphs(&self.glyphs).collect(),
            })
        } else {
            CoverageTable::Format1(CoverageFormat1 {
                glyph_array: self.glyphs,
            })
        }
    }
}

impl<T: Default> LookupBuilder<T> {
    pub fn new(flags: LookupFlag, mark_set: Option<FilterSetId>) -> Self {
        LookupBuilder {
            flags,
            mark_set,
            subtables: vec![Default::default()],
        }
    }

    pub fn new_with_lookups(
        flags: LookupFlag,
        mark_set: Option<FilterSetId>,
        subtables: Vec<T>,
    ) -> Self {
        Self {
            flags,
            mark_set,
            subtables,
        }
    }

    //TODO: if we keep this, make it unwrap and ensure we always have a subtable
    pub fn last_mut(&mut self) -> Option<&mut T> {
        self.subtables.last_mut()
    }

    pub fn force_subtable_break(&mut self) {
        self.subtables.push(Default::default())
    }

    pub fn iter_subtables(&self) -> impl Iterator<Item = &T> + '_ {
        self.subtables.iter()
    }
}

impl<U> LookupBuilder<U> {
    /// A helper method for converting from (say) ContextBuilder to PosContextBuilder
    pub fn convert<T: From<U>>(self) -> LookupBuilder<T> {
        let LookupBuilder {
            flags,
            mark_set,
            subtables,
        } = self;
        LookupBuilder {
            flags,
            mark_set,
            subtables: subtables.into_iter().map(Into::into).collect(),
        }
    }
}

impl<U, T> Builder for LookupBuilder<T>
where
    T: Builder<Output = Vec<U>>,
    U: Default,
{
    type Output = Lookup<U>;

    fn build(self, var_store: &mut VariationStoreBuilder) -> Self::Output {
        let subtables = self
            .subtables
            .into_iter()
            .flat_map(|b| b.build(var_store).into_iter())
            .collect();
        let mut out = Lookup::new(self.flags, subtables);
        out.mark_filtering_set = self.mark_set;
        out
    }
}

impl Metric {
    /// Returns `true` if the default value is `0` and there is no device or deltas
    pub fn is_zero(&self) -> bool {
        self.default == 0 && !self.has_device_or_deltas()
    }

    /// `true` if this metric has either a device table or deltas
    pub fn has_device_or_deltas(&self) -> bool {
        !self.device_or_deltas.is_none()
    }
}

impl DeviceOrDeltas {
    /// Returns `true` if there is no device table or variation index
    pub fn is_none(&self) -> bool {
        *self == DeviceOrDeltas::None
    }

    /// Compile the device or deltas into their final form.
    ///
    /// In the case of a device, this generates a [`Device`] table; in the
    /// case of deltas this adds them to the `VariationStoreBuilder`, and returns
    /// a [`PendingVariationIndex`] that must be remapped after the builder is
    /// finished, using the returned [`VariationIndexRemapping`].
    ///
    /// [`PendingVariationIndex`]: super::PendingVariationIndex
    /// [`VariationIndexRemapping`]: crate::tables::variations::ivs_builder::VariationIndexRemapping
    pub fn build(self, var_store: &mut VariationStoreBuilder) -> Option<DeviceOrVariationIndex> {
        match self {
            DeviceOrDeltas::Device(dev) => Some(DeviceOrVariationIndex::Device(dev)),
            DeviceOrDeltas::Deltas(deltas) => {
                let temp_id = var_store.add_deltas(deltas);
                Some(DeviceOrVariationIndex::PendingVariationIndex(
                    PendingVariationIndex::new(temp_id),
                ))
            }
            DeviceOrDeltas::None => None,
        }
    }
}

impl CaretValueBuilder {
    /// Build the final [`CaretValue`] table.
    pub fn build(self, var_store: &mut VariationStoreBuilder) -> CaretValue {
        match self {
            Self::Coordinate { default, deltas } => match deltas.build(var_store) {
                Some(deltas) => CaretValue::format_3(default, deltas),
                None => CaretValue::format_1(default),
            },
            Self::PointIndex(index) => CaretValue::format_2(index),
        }
    }
}

impl From<i16> for Metric {
    fn from(src: i16) -> Metric {
        Metric {
            default: src,
            device_or_deltas: DeviceOrDeltas::None,
        }
    }
}

impl From<Option<Device>> for DeviceOrDeltas {
    fn from(src: Option<Device>) -> DeviceOrDeltas {
        src.map(DeviceOrDeltas::Device).unwrap_or_default()
    }
}

impl From<Device> for DeviceOrDeltas {
    fn from(value: Device) -> Self {
        DeviceOrDeltas::Device(value)
    }
}

impl From<Vec<(VariationRegion, i16)>> for DeviceOrDeltas {
    fn from(src: Vec<(VariationRegion, i16)>) -> DeviceOrDeltas {
        if src.is_empty() {
            DeviceOrDeltas::None
        } else {
            DeviceOrDeltas::Deltas(src)
        }
    }
}
impl FromIterator<(GlyphId16, u16)> for ClassDefBuilderImpl {
    fn from_iter<T: IntoIterator<Item = (GlyphId16, u16)>>(iter: T) -> Self {
        Self {
            items: iter.into_iter().filter(|(_, cls)| *cls != 0).collect(),
        }
    }
}

impl FromIterator<GlyphId16> for CoverageTableBuilder {
    fn from_iter<T: IntoIterator<Item = GlyphId16>>(iter: T) -> Self {
        let glyphs = iter.into_iter().collect::<Vec<_>>();
        CoverageTableBuilder::from_glyphs(glyphs)
    }
}

fn iter_class_ranges(
    values: &BTreeMap<GlyphId16, u16>,
) -> impl Iterator<Item = ClassRangeRecord> + '_ {
    let mut iter = values.iter();
    let mut prev = None;

    #[allow(clippy::while_let_on_iterator)]
    std::iter::from_fn(move || {
        while let Some((gid, class)) = iter.next() {
            match prev.take() {
                None => prev = Some((*gid, *gid, *class)),
                Some((start, end, pclass))
                    if super::are_sequential(end, *gid) && pclass == *class =>
                {
                    prev = Some((start, *gid, pclass))
                }
                Some((start_glyph_id, end_glyph_id, pclass)) => {
                    prev = Some((*gid, *gid, *class));
                    return Some(ClassRangeRecord {
                        start_glyph_id,
                        end_glyph_id,
                        class: pclass,
                    });
                }
            }
        }
        prev.take()
            .map(|(start_glyph_id, end_glyph_id, class)| ClassRangeRecord {
                start_glyph_id,
                end_glyph_id,
                class,
            })
    })
}

fn should_choose_coverage_format_2(glyphs: &[GlyphId16]) -> bool {
    let format2_len = 4 + RangeRecord::iter_for_glyphs(glyphs).count() * 6;
    let format1_len = 4 + glyphs.len() * 2;
    format2_len < format1_len
}

#[cfg(test)]
mod tests {
    use std::ops::RangeInclusive;

    use read_fonts::collections::IntSet;

    use crate::tables::layout::DeltaFormat;

    use super::*;

    #[test]
    fn classdef_format() {
        let builder: ClassDefBuilderImpl = [(3u16, 4u16), (4, 6), (5, 1), (9, 5), (10, 2), (11, 3)]
            .map(|(gid, cls)| (GlyphId16::new(gid), cls))
            .into_iter()
            .collect();

        assert!(builder.prefer_format_1());

        let builder: ClassDefBuilderImpl = [(1u16, 1u16), (3, 4), (9, 5), (10, 2), (11, 3)]
            .map(|(gid, cls)| (GlyphId16::new(gid), cls))
            .into_iter()
            .collect();

        assert!(builder.prefer_format_1());
    }

    #[test]
    fn classdef_prefer_format2() {
        fn iter_class_items(
            start: u16,
            end: u16,
            cls: u16,
        ) -> impl Iterator<Item = (GlyphId16, u16)> {
            (start..=end).map(move |gid| (GlyphId16::new(gid), cls))
        }

        // 3 ranges of 4 glyphs at 6 bytes a range should be smaller than writing
        // out the 3 * 4 classes directly
        let builder: ClassDefBuilderImpl = iter_class_items(5, 8, 3)
            .chain(iter_class_items(9, 12, 4))
            .chain(iter_class_items(13, 16, 5))
            .collect();

        assert!(!builder.prefer_format_1());
    }

    #[test]
    fn delta_format_dflt() {
        let some: DeltaFormat = Default::default();
        assert_eq!(some, DeltaFormat::Local2BitDeltas);
    }

    fn make_glyph_vec<const N: usize>(gids: [u16; N]) -> Vec<GlyphId16> {
        gids.into_iter().map(GlyphId16::new).collect()
    }

    #[test]
    fn coverage_builder() {
        let coverage = make_glyph_vec([1u16, 2, 9, 3, 6, 9])
            .into_iter()
            .collect::<CoverageTableBuilder>();
        assert_eq!(coverage.glyphs, make_glyph_vec([1, 2, 3, 6, 9]));
    }

    fn make_class<const N: usize>(gid_class_pairs: [(u16, u16); N]) -> ClassDef {
        gid_class_pairs
            .iter()
            .map(|(gid, cls)| (GlyphId16::new(*gid), *cls))
            .collect::<ClassDefBuilderImpl>()
            .build()
    }

    #[test]
    fn class_def_builder_zero() {
        // even if class 0 is provided, we don't need to assign explicit entries for it
        let class = make_class([(4, 0), (5, 1)]);
        assert!(class.get_raw(GlyphId16::new(4)).is_none());
        assert_eq!(class.get_raw(GlyphId16::new(5)), Some(1));
        assert!(class.get_raw(GlyphId16::new(100)).is_none());
    }

    // https://github.com/googlefonts/fontations/issues/923
    // an empty classdef should always be format 2
    #[test]
    fn class_def_builder_empty() {
        let builder = ClassDefBuilderImpl::from_iter([]);
        let built = builder.build();

        assert_eq!(
            built,
            ClassDef::Format2(ClassDefFormat2 {
                class_range_records: vec![]
            })
        )
    }

    #[test]
    fn class_def_small() {
        let class = make_class([(1, 1), (2, 1), (3, 1)]);

        assert_eq!(
            class,
            ClassDef::Format2(ClassDefFormat2 {
                class_range_records: vec![ClassRangeRecord {
                    start_glyph_id: GlyphId16::new(1),
                    end_glyph_id: GlyphId16::new(3),
                    class: 1
                }]
            })
        )
    }

    #[test]
    fn classdef_f2_get() {
        fn make_f2_class<const N: usize>(range: [RangeInclusive<u16>; N]) -> ClassDef {
            ClassDefFormat2::new(
                range
                    .into_iter()
                    .enumerate()
                    .map(|(i, range)| {
                        ClassRangeRecord::new(
                            GlyphId16::new(*range.start()),
                            GlyphId16::new(*range.end()),
                            (1 + i) as _,
                        )
                    })
                    .collect(),
            )
            .into()
        }

        let cls = make_f2_class([1..=1, 2..=9]);
        assert_eq!(cls.get(GlyphId16::new(2)), 2);
        assert_eq!(cls.get(GlyphId16::new(20)), 0);
    }

    fn make_glyph_class<const N: usize>(glyphs: [u16; N]) -> IntSet<GlyphId16> {
        glyphs.into_iter().map(GlyphId16::new).collect()
    }

    #[test]
    fn smoke_test_class_builder() {
        let mut builder = ClassDefBuilder::new();
        builder.checked_add(make_glyph_class([6, 10]));
        let cls = builder.build();
        assert_eq!(cls.get(GlyphId16::new(6)), 1);

        let mut builder = ClassDefBuilder::new_using_class_0();
        builder.checked_add(make_glyph_class([6, 10]));
        let cls = builder.build();
        assert_eq!(cls.get(GlyphId16::new(6)), 0);
        assert_eq!(cls.get(GlyphId16::new(10)), 0);
    }

    #[test]
    fn classdef_assign_order() {
        // - longer classes before short ones
        // - if tied, lowest glyph id first

        let mut builder = ClassDefBuilder::default();
        builder.checked_add(make_glyph_class([7, 8, 9]));
        builder.checked_add(make_glyph_class([1, 12]));
        builder.checked_add(make_glyph_class([3, 4]));
        let cls = builder.build();
        assert_eq!(cls.get(GlyphId16::new(9)), 1);
        assert_eq!(cls.get(GlyphId16::new(1)), 2);
        assert_eq!(cls.get(GlyphId16::new(4)), 3);
        // notdef
        assert_eq!(cls.get(GlyphId16::new(5)), 0);
    }

    #[test]
    fn we_handle_dupes() {
        let mut builder = ClassDefBuilder::default();
        let c1 = make_glyph_class([1, 2, 3, 4]);
        let c2 = make_glyph_class([4, 3, 2, 1, 1]);
        let c3 = make_glyph_class([1, 5, 6, 7]);
        assert!(builder.checked_add(c1.clone()));
        assert!(builder.checked_add(c2.clone()));
        assert!(!builder.checked_add(c3.clone()));

        let (_, map) = builder.build_with_mapping();
        assert_eq!(map.get(&c1), map.get(&c2));
        assert!(!map.contains_key(&c3));
    }
}

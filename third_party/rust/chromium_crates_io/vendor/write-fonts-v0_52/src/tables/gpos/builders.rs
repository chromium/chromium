//! GPOS subtable builders

use std::collections::{BTreeMap, HashMap};

use read_fonts::collections::IntSet;
use types::GlyphId16;

use crate::tables::{
    layout::{
        builders::{Builder, ClassDefBuilder, DeviceOrDeltas, Metric},
        CoverageTable,
    },
    variations::ivs_builder::VariationStoreBuilder,
};

use super::{
    AnchorTable, BaseArray, BaseRecord, Class1Record, Class2Record, ComponentRecord,
    CursivePosFormat1, EntryExitRecord, LigatureArray, LigatureAttach, Mark2Array, Mark2Record,
    MarkArray, MarkBasePosFormat1, MarkLigPosFormat1, MarkMarkPosFormat1, MarkRecord, PairPos,
    PairSet, PairValueRecord, SinglePos, ValueFormat, ValueRecord,
};

type GlyphSet = IntSet<GlyphId16>;

/// A builder for [`ValueRecord`]s, which may contain raw deltas or device tables.
#[derive(Clone, Debug, Default, PartialEq, Eq, PartialOrd, Ord)]
#[cfg_attr(feature = "serde", derive(serde::Serialize, serde::Deserialize))]
pub struct ValueRecordBuilder {
    /// The x advance, plus a possible device table or set of deltas
    pub x_advance: Option<Metric>,
    /// The y advance, plus a possible device table or set of deltas
    pub y_advance: Option<Metric>,
    /// The x placement, plus a possible device table or set of deltas
    pub x_placement: Option<Metric>,
    /// The y placement, plus a possible device table or set of deltas
    pub y_placement: Option<Metric>,
}

/// A builder for [`AnchorTable`]s, which may contain raw deltas or device tables.
#[derive(Clone, Debug, Default, PartialEq, Eq, PartialOrd, Ord)]
#[cfg_attr(feature = "serde", derive(serde::Serialize, serde::Deserialize))]
pub struct AnchorBuilder {
    /// The x coordinate, plus a possible device table or set of deltas
    pub x: Metric,
    /// The y coordinate, plus a possible device table or set of deltas
    pub y: Metric,
    /// The countourpoint, in a format 2 anchor.
    ///
    /// This is a rarely used format.
    pub contourpoint: Option<u16>,
}

impl ValueRecordBuilder {
    /// Create a new all-zeros `ValueRecordBuilder`
    pub fn new() -> Self {
        Default::default()
    }

    /// Duplicates the x-advance value to x-placement, required for RTL rules.
    ///
    /// This is only necessary when a record was originally created without
    /// knowledge of the writing direction, and then later needs to be modified.
    pub fn make_rtl_compatible(&mut self) {
        if self.x_placement.is_none() {
            self.x_placement.clone_from(&self.x_advance);
        }
    }

    // these methods just match the existing builder methods on `ValueRecord`
    /// Builder style method to set the default x_placement value
    pub fn with_x_placement(mut self, val: i16) -> Self {
        self.x_placement
            .get_or_insert_with(Default::default)
            .default = val;
        self
    }

    /// Builder style method to set the default y_placement value
    pub fn with_y_placement(mut self, val: i16) -> Self {
        self.y_placement
            .get_or_insert_with(Default::default)
            .default = val;
        self
    }

    /// Builder style method to set the default x_placement value
    pub fn with_x_advance(mut self, val: i16) -> Self {
        self.x_advance.get_or_insert_with(Default::default).default = val;
        self
    }

    /// Builder style method to set the default y_placement value
    pub fn with_y_advance(mut self, val: i16) -> Self {
        self.y_advance.get_or_insert_with(Default::default).default = val;
        self
    }

    /// Builder style method to set the device or deltas for x_placement
    ///
    /// The argument can be a `Device` table or a `Vec<(VariationRegion, i16)>`
    pub fn with_x_placement_device(mut self, val: impl Into<DeviceOrDeltas>) -> Self {
        self.x_placement
            .get_or_insert_with(Default::default)
            .device_or_deltas = val.into();
        self
    }

    /// Builder style method to set the device or deltas for y_placement
    ///
    /// The argument can be a `Device` table or a `Vec<(VariationRegion, i16)>`
    pub fn with_y_placement_device(mut self, val: impl Into<DeviceOrDeltas>) -> Self {
        self.y_placement
            .get_or_insert_with(Default::default)
            .device_or_deltas = val.into();
        self
    }

    /// Builder style method to set the device or deltas for x_advance
    ///
    /// The argument can be a `Device` table or a `Vec<(VariationRegion, i16)>`
    pub fn with_x_advance_device(mut self, val: impl Into<DeviceOrDeltas>) -> Self {
        self.x_advance
            .get_or_insert_with(Default::default)
            .device_or_deltas = val.into();
        self
    }

    /// Builder style method to set the device or deltas for y_advance
    ///
    /// The argument can be a `Device` table or a `Vec<(VariationRegion, i16)>`
    pub fn with_y_advance_device(mut self, val: impl Into<DeviceOrDeltas>) -> Self {
        self.y_advance
            .get_or_insert_with(Default::default)
            .device_or_deltas = val.into();
        self
    }

    /// Clear any fields that exist but are 'empty' (`0` default value, no device or deltas)
    pub fn clear_zeros(mut self) -> Self {
        self.x_advance = self.x_advance.filter(|m| !m.is_zero());
        self.y_advance = self.y_advance.filter(|m| !m.is_zero());
        self.x_placement = self.x_placement.filter(|m| !m.is_zero());
        self.y_placement = self.y_placement.filter(|m| !m.is_zero());
        self
    }

    /// Compute the `ValueFormat` for this record.
    pub fn format(&self) -> ValueFormat {
        const EMPTY: ValueFormat = ValueFormat::empty();
        use ValueFormat as VF;

        let get_flags = |field: &Option<Metric>, def_flag, dev_flag| {
            let field = field.as_ref();
            let def_flag = if field.is_some() { def_flag } else { EMPTY };
            let dev_flag = field
                .and_then(|fld| (!fld.device_or_deltas.is_none()).then_some(dev_flag))
                .unwrap_or(EMPTY);
            (def_flag, dev_flag)
        };

        let (x_adv, x_adv_dev) = get_flags(&self.x_advance, VF::X_ADVANCE, VF::X_ADVANCE_DEVICE);
        let (y_adv, y_adv_dev) = get_flags(&self.y_advance, VF::Y_ADVANCE, VF::Y_ADVANCE_DEVICE);
        let (x_place, x_place_dev) =
            get_flags(&self.x_placement, VF::X_PLACEMENT, VF::X_PLACEMENT_DEVICE);
        let (y_place, y_place_dev) =
            get_flags(&self.y_placement, VF::Y_PLACEMENT, VF::Y_PLACEMENT_DEVICE);
        x_adv | y_adv | x_place | y_place | x_adv_dev | y_adv_dev | x_place_dev | y_place_dev
    }

    /// `true` if we are not null, but our set values are all 0
    pub fn is_all_zeros(&self) -> bool {
        let device_mask = ValueFormat::X_PLACEMENT_DEVICE
            | ValueFormat::Y_PLACEMENT_DEVICE
            | ValueFormat::X_ADVANCE_DEVICE
            | ValueFormat::Y_ADVANCE_DEVICE;

        let format = self.format();
        if format.is_empty() || format.intersects(device_mask) {
            return false;
        }
        let all_values = [
            &self.x_placement,
            &self.y_placement,
            &self.x_advance,
            &self.y_advance,
        ];
        all_values
            .iter()
            .all(|v| v.as_ref().map(|v| v.is_zero()).unwrap_or(true))
    }

    /// Build the final [`ValueRecord`], compiling deltas if needed.
    pub fn build(self, var_store: &mut VariationStoreBuilder) -> ValueRecord {
        let mut result = ValueRecord::new();
        result.x_advance = self.x_advance.as_ref().map(|val| val.default);
        result.y_advance = self.y_advance.as_ref().map(|val| val.default);
        result.x_placement = self.x_placement.as_ref().map(|val| val.default);
        result.y_placement = self.y_placement.as_ref().map(|val| val.default);
        result.x_advance_device = self
            .x_advance
            .and_then(|val| val.device_or_deltas.build(var_store))
            .into();
        result.y_advance_device = self
            .y_advance
            .and_then(|val| val.device_or_deltas.build(var_store))
            .into();
        result.x_placement_device = self
            .x_placement
            .and_then(|val| val.device_or_deltas.build(var_store))
            .into();
        result.y_placement_device = self
            .y_placement
            .and_then(|val| val.device_or_deltas.build(var_store))
            .into();

        result
    }
}

impl AnchorBuilder {
    /// Create a new [`AnchorBuilder`].
    pub fn new(x: i16, y: i16) -> Self {
        AnchorBuilder {
            x: x.into(),
            y: y.into(),
            contourpoint: None,
        }
    }

    /// Builder style method to set the device or deltas for the x value
    ///
    /// The argument can be a `Device` table or a `Vec<(VariationRegion, i16)>`
    pub fn with_x_device(mut self, val: impl Into<DeviceOrDeltas>) -> Self {
        self.x.device_or_deltas = val.into();
        self
    }

    /// Builder style method to set the device or deltas for the y value
    ///
    /// The argument can be a `Device` table or a `Vec<(VariationRegion, i16)>`
    pub fn with_y_device(mut self, val: impl Into<DeviceOrDeltas>) -> Self {
        self.y.device_or_deltas = val.into();
        self
    }

    /// Builder-style method to set the contourpoint.
    ///
    /// This is for the little-used format2 AnchorTable; it will be ignored
    /// if any device or deltas have been set.
    pub fn with_contourpoint(mut self, idx: u16) -> Self {
        self.contourpoint = Some(idx);
        self
    }

    /// Build the final [`AnchorTable`], adding deltas to the varstore if needed.
    pub fn build(self, var_store: &mut VariationStoreBuilder) -> AnchorTable {
        let x = self.x.default;
        let y = self.y.default;
        let x_dev = self.x.device_or_deltas.build(var_store);
        let y_dev = self.y.device_or_deltas.build(var_store);
        if x_dev.is_some() || y_dev.is_some() {
            AnchorTable::format_3(x, y, x_dev, y_dev)
        } else if let Some(point) = self.contourpoint {
            AnchorTable::format_2(x, y, point)
        } else {
            AnchorTable::format_1(x, y)
        }
    }
}

/// A builder for [`SinglePos`] subtables.
#[derive(Clone, Debug, Default, PartialEq, Eq)]
#[cfg_attr(feature = "serde", derive(serde::Serialize, serde::Deserialize))]
pub struct SinglePosBuilder {
    items: BTreeMap<GlyphId16, ValueRecordBuilder>,
}

impl SinglePosBuilder {
    /// Returns the number of rules in the lookup.
    pub fn len(&self) -> usize {
        self.items.len()
    }

    /// Returns `true` if no rules have been added to the builder.
    pub fn is_empty(&self) -> bool {
        self.items.is_empty()
    }

    /// Add a new single-pos rule to this builder.
    pub fn insert(&mut self, glyph: GlyphId16, record: ValueRecordBuilder) {
        self.items.insert(glyph, record);
    }

    /// Check whether this glyph already has an assigned value in this builder.
    pub fn can_add(&self, glyph: GlyphId16, value: &ValueRecordBuilder) -> bool {
        self.items
            .get(&glyph)
            .map(|existing| existing == value)
            .unwrap_or(true)
    }
}

impl Builder for SinglePosBuilder {
    type Output = Vec<SinglePos>;

    fn build(self, var_store: &mut VariationStoreBuilder) -> Self::Output {
        fn build_subtable(items: BTreeMap<GlyphId16, &ValueRecord>) -> SinglePos {
            let first = *items.values().next().unwrap();
            let use_format_1 = first.format().is_empty() || items.values().all(|val| val == &first);
            let coverage: CoverageTable = items.keys().copied().collect();
            if use_format_1 {
                SinglePos::format_1(coverage.clone(), first.clone())
            } else {
                SinglePos::format_2(coverage, items.into_values().cloned().collect())
            }
        }
        const NEW_SUBTABLE_COST: usize = 10;
        let items = self
            .items
            .into_iter()
            .map(|(glyph, anchor)| (glyph, anchor.build(var_store)))
            .collect::<BTreeMap<_, _>>();

        // list of sets of glyph ids which will end up in their own subtables
        let mut subtables = Vec::new();
        let mut group_by_record: HashMap<&ValueRecord, BTreeMap<GlyphId16, &ValueRecord>> =
            Default::default();

        // first group by specific record; glyphs that share a record can use
        // the more efficient format-1 subtable type
        for (gid, value) in &items {
            group_by_record
                .entry(value)
                .or_default()
                .insert(*gid, value);
        }
        let mut group_by_format: HashMap<ValueFormat, BTreeMap<GlyphId16, &ValueRecord>> =
            Default::default();
        for (value, glyphs) in group_by_record {
            // if this saves us size, use format 1
            if glyphs.len() * value.encoded_size() > NEW_SUBTABLE_COST {
                subtables.push(glyphs);
                // else split based on value format; each format will be its own
                // format 2 table
            } else {
                group_by_format
                    .entry(value.format())
                    .or_default()
                    .extend(glyphs.into_iter());
            }
        }
        subtables.extend(group_by_format.into_values());

        let mut output = subtables
            .into_iter()
            .map(build_subtable)
            .collect::<Vec<_>>();

        // finally sort the subtables: first in decreasing order of size,
        // using first glyph id to break ties (matches feaLib)
        output.sort_unstable_by_key(|table| match table {
            SinglePos::Format1(table) => cmp_coverage_key(&table.coverage),
            SinglePos::Format2(table) => cmp_coverage_key(&table.coverage),
        });
        output
    }
}

fn cmp_coverage_key(coverage: &CoverageTable) -> impl Ord {
    (std::cmp::Reverse(coverage.len()), coverage.iter().next())
}

/// A builder for GPOS type 2 (PairPos) subtables
///
/// This builder can build both glyph and class-based kerning subtables.
#[derive(Clone, Debug, Default, PartialEq)]
#[cfg_attr(feature = "serde", derive(serde::Serialize, serde::Deserialize))]
pub struct PairPosBuilder {
    pairs: GlyphPairPosBuilder,
    classes: ClassPairPosBuilder,
}

#[derive(Clone, Debug, Default, PartialEq)]
#[cfg_attr(feature = "serde", derive(serde::Serialize, serde::Deserialize))]
struct GlyphPairPosBuilder(
    BTreeMap<GlyphId16, BTreeMap<GlyphId16, (ValueRecordBuilder, ValueRecordBuilder)>>,
);

#[derive(Clone, Debug, PartialEq)]
#[cfg_attr(feature = "serde", derive(serde::Serialize, serde::Deserialize))]
struct ClassPairPosSubtable {
    items:
        BTreeMap<IntSet<GlyphId16>, BTreeMap<GlyphSet, (ValueRecordBuilder, ValueRecordBuilder)>>,
    classdef_1: ClassDefBuilder,
    classdef_2: ClassDefBuilder,
}

impl Default for ClassPairPosSubtable {
    fn default() -> Self {
        Self {
            items: Default::default(),
            classdef_1: ClassDefBuilder::new_using_class_0(),
            classdef_2: Default::default(),
        }
    }
}

#[derive(Clone, Debug, Default, PartialEq)]
#[cfg_attr(feature = "serde", derive(serde::Serialize, serde::Deserialize))]
struct ClassPairPosBuilder(Vec<ClassPairPosSubtable>);

impl ClassPairPosBuilder {
    fn insert(
        &mut self,
        class1: GlyphSet,
        record1: ValueRecordBuilder,
        class2: GlyphSet,
        record2: ValueRecordBuilder,
    ) {
        if self.0.last().map(|last| last.can_add(&class1, &class2)) != Some(true) {
            self.0.push(Default::default())
        }
        self.0
            .last_mut()
            .unwrap()
            .add(class1, class2, record1, record2);
    }
}

impl ClassPairPosSubtable {
    fn can_add(&self, class1: &GlyphSet, class2: &GlyphSet) -> bool {
        self.classdef_1.can_add(class1) && self.classdef_2.can_add(class2)
    }

    fn add(
        &mut self,
        class1: GlyphSet,
        class2: GlyphSet,
        record1: ValueRecordBuilder,
        record2: ValueRecordBuilder,
    ) {
        self.classdef_1.checked_add(class1.clone());
        self.classdef_2.checked_add(class2.clone());
        self.items
            .entry(class1)
            .or_default()
            .insert(class2, (record1, record2));
    }

    // determine the union of each of the two value formats
    //
    // we need a to ensure that the value format we use can represent all
    // of the fields present in any of the value records in this subtable.
    //
    // see https://github.com/fonttools/fonttools/blob/770917d89e9/Lib/fontTools/otlLib/builder.py#L2066
    fn compute_value_formats(&self) -> (ValueFormat, ValueFormat) {
        self.items.values().flat_map(|v| v.values()).fold(
            (ValueFormat::empty(), ValueFormat::empty()),
            |(acc1, acc2), (f1, f2)| (acc1 | f1.format(), acc2 | f2.format()),
        )
    }
}

impl PairPosBuilder {
    /// Returns `true` if no rules have been added to this builder
    pub fn is_empty(&self) -> bool {
        self.pairs.0.is_empty() && self.classes.0.is_empty()
    }

    /// The number of rules in the builder
    pub fn len(&self) -> usize {
        self.pairs.0.values().map(|vals| vals.len()).sum::<usize>()
            + self
                .classes
                .0
                .iter()
                .map(|sub| sub.items.values().len())
                .sum::<usize>()
    }

    /// Insert a new kerning pair
    pub fn insert_pair(
        &mut self,
        glyph1: GlyphId16,
        record1: ValueRecordBuilder,
        glyph2: GlyphId16,
        record2: ValueRecordBuilder,
    ) {
        // "When specific kern pair rules conflict, the first rule specified is used,
        // and later conflicting rule are skipped"
        // https://adobe-type-tools.github.io/afdko/OpenTypeFeatureFileSpecification.html#6bii-enumerating-pairs
        // E.g.:
        //   @A = [A Aacute Agrave]
        //   feature kern {
        //     pos A B 100;
        //     enum pos @A B -50;
        //   } kern;
        // should result in a A B kerning value of 100, not -50.
        // https://github.com/googlefonts/fontc/issues/550
        self.pairs
            .0
            .entry(glyph1)
            .or_default()
            .entry(glyph2)
            .or_insert((record1, record2));
    }

    /// Insert a new class-based kerning rule.
    pub fn insert_classes(
        &mut self,
        class1: GlyphSet,
        record1: ValueRecordBuilder,
        class2: GlyphSet,
        record2: ValueRecordBuilder,
    ) {
        self.classes.insert(class1, record1, class2, record2)
    }
}

impl Builder for PairPosBuilder {
    type Output = Vec<PairPos>;

    fn build(self, var_store: &mut VariationStoreBuilder) -> Self::Output {
        let mut out = self.pairs.build(var_store);
        out.extend(self.classes.build(var_store));
        out
    }
}

impl Builder for GlyphPairPosBuilder {
    type Output = Vec<PairPos>;

    fn build(self, var_store: &mut VariationStoreBuilder) -> Self::Output {
        let mut split_by_format = BTreeMap::<_, BTreeMap<_, Vec<_>>>::default();
        for (g1, map) in self.0 {
            for (g2, (v1, v2)) in map {
                split_by_format
                    .entry((v1.format(), v2.format()))
                    .or_default()
                    .entry(g1)
                    .or_default()
                    .push(PairValueRecord::new(
                        g2,
                        v1.build(var_store),
                        v2.build(var_store),
                    ));
            }
        }

        split_by_format
            .into_values()
            .map(|map| {
                let coverage = map.keys().copied().collect();
                let pair_sets = map.into_values().map(PairSet::new).collect();
                PairPos::format_1(coverage, pair_sets)
            })
            .collect()
    }
}

impl Builder for ClassPairPosBuilder {
    type Output = Vec<PairPos>;

    fn build(self, var_store: &mut VariationStoreBuilder) -> Self::Output {
        self.0.into_iter().map(|sub| sub.build(var_store)).collect()
    }
}

impl Builder for ClassPairPosSubtable {
    type Output = PairPos;

    fn build(self, var_store: &mut VariationStoreBuilder) -> Self::Output {
        assert!(!self.items.is_empty(), "filter before here");
        let (format1, format2) = self.compute_value_formats();
        // we have a set of classes/values with a single valueformat

        // an empty record, if some pair of classes have no entry
        let empty_record = Class2Record::new(
            ValueRecord::new().with_explicit_value_format(format1),
            ValueRecord::new().with_explicit_value_format(format2),
        );

        let (class1def, class1map) = self.classdef_1.build_with_mapping();
        let (class2def, class2map) = self.classdef_2.build_with_mapping();

        let coverage = self.items.keys().flat_map(GlyphSet::iter).collect();

        let mut out = vec![Class1Record::default(); self.items.len()];
        for (cls1, stuff) in self.items {
            let idx = class1map.get(&cls1).unwrap();
            let mut records = vec![empty_record.clone(); class2map.len() + 1];
            for (class, (v1, v2)) in stuff {
                let idx = class2map.get(&class).unwrap();
                records[*idx as usize] = Class2Record::new(
                    v1.build(var_store).with_explicit_value_format(format1),
                    v2.build(var_store).with_explicit_value_format(format2),
                );
            }
            out[*idx as usize] = Class1Record::new(records);
        }
        PairPos::format_2(coverage, class1def, class2def, out)
    }
}

/// A builder for GPOS Lookup Type 3, Cursive Attachment
#[derive(Clone, Debug, Default, PartialEq, Eq)]
#[cfg_attr(feature = "serde", derive(serde::Serialize, serde::Deserialize))]
pub struct CursivePosBuilder {
    // (entry, exit)
    items: BTreeMap<GlyphId16, (Option<AnchorBuilder>, Option<AnchorBuilder>)>,
}

impl CursivePosBuilder {
    /// Returns the number of rules in the lookup.
    pub fn len(&self) -> usize {
        self.items.len()
    }

    /// Returns `true` if no rules have been added to the builder.
    pub fn is_empty(&self) -> bool {
        self.items.is_empty()
    }

    /// Insert a new entry/exit anchor pair for a glyph.
    pub fn insert(
        &mut self,
        glyph: GlyphId16,
        entry: Option<AnchorBuilder>,
        exit: Option<AnchorBuilder>,
    ) {
        self.items.insert(glyph, (entry, exit));
    }
}

impl Builder for CursivePosBuilder {
    type Output = Vec<CursivePosFormat1>;

    fn build(self, var_store: &mut VariationStoreBuilder) -> Self::Output {
        let coverage = self.items.keys().copied().collect();
        let records = self
            .items
            .into_values()
            .map(|(entry, exit)| {
                EntryExitRecord::new(
                    entry.map(|x| x.build(var_store)),
                    exit.map(|x| x.build(var_store)),
                )
            })
            .collect();
        vec![CursivePosFormat1::new(coverage, records)]
    }
}

// shared between several tables
#[derive(Clone, Debug, Default, PartialEq, Eq)]
#[cfg_attr(feature = "serde", derive(serde::Serialize, serde::Deserialize))]
struct MarkList {
    // (class id, anchor)
    glyphs: BTreeMap<GlyphId16, (u16, AnchorBuilder)>,
    // map class names to their idx for this table
    classes: HashMap<String, u16>,
}

impl MarkList {
    /// If this glyph is already part of another class, return the previous class name
    ///
    /// Otherwise return the u16 id for this class, in this lookup.
    fn insert(
        &mut self,
        glyph: GlyphId16,
        class: &str,
        anchor: AnchorBuilder,
    ) -> Result<u16, PreviouslyAssignedClass> {
        let next_id = self.classes.len().try_into().unwrap();
        let id = self.classes.get(class).copied().unwrap_or_else(|| {
            self.classes.insert(class.to_owned(), next_id);
            next_id
        });
        if let Some(prev) = self
            .glyphs
            .insert(glyph, (id, anchor))
            .filter(|prev| prev.0 != id)
        {
            let class = self
                .classes
                .iter()
                .find_map(|(name, idx)| (*idx == prev.0).then(|| name.clone()))
                .unwrap();

            return Err(PreviouslyAssignedClass {
                glyph_id: glyph,
                class,
            });
        }
        Ok(id)
    }

    fn glyphs(&self) -> impl Iterator<Item = GlyphId16> + Clone + '_ {
        self.glyphs.keys().copied()
    }

    fn get_class(&self, class_name: &str) -> u16 {
        *self
            .classes
            .get(class_name)
            // this is internal API, we uphold this
            .expect("marks added before bases")
    }
}

impl Builder for MarkList {
    type Output = (CoverageTable, MarkArray);

    fn build(self, var_store: &mut VariationStoreBuilder) -> Self::Output {
        let coverage = self.glyphs().collect();
        let array = MarkArray::new(
            self.glyphs
                .into_values()
                .map(|(class, anchor)| MarkRecord::new(class, anchor.build(var_store)))
                .collect(),
        );
        (coverage, array)
    }
}

/// A builder for GPOS Lookup Type 4, Mark-to-Base
#[derive(Clone, Debug, Default, PartialEq, Eq)]
#[cfg_attr(feature = "serde", derive(serde::Serialize, serde::Deserialize))]
pub struct MarkToBaseBuilder {
    marks: MarkList,
    bases: BTreeMap<GlyphId16, Vec<(u16, AnchorBuilder)>>,
}

/// An error indicating a given glyph has been assigned to multiple mark classes
#[derive(Clone, Debug, Default)]
pub struct PreviouslyAssignedClass {
    /// The ID of the glyph in conflict
    pub glyph_id: GlyphId16,
    /// The name of the previous class
    pub class: String,
}

impl std::error::Error for PreviouslyAssignedClass {}

impl std::fmt::Display for PreviouslyAssignedClass {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(
            f,
            "Glyph '{}' was previously assigned to class '{}'",
            self.glyph_id.to_u16(),
            self.class
        )
    }
}

impl MarkToBaseBuilder {
    /// Returns the number of rules in the lookup.
    pub fn len(&self) -> usize {
        self.bases.len()
    }

    /// Returns `true` if no rules have been added to the builder.
    pub fn is_empty(&self) -> bool {
        self.bases.is_empty()
    }

    /// Add a new mark glyph.
    ///
    /// If this glyph already exists in another mark class, we return the
    /// previous class; this is likely an error.
    pub fn insert_mark(
        &mut self,
        glyph: GlyphId16,
        class: &str,
        anchor: AnchorBuilder,
    ) -> Result<u16, PreviouslyAssignedClass> {
        self.marks.insert(glyph, class, anchor)
    }

    /// Insert a new base glyph.
    pub fn insert_base(&mut self, glyph: GlyphId16, class: &str, anchor: AnchorBuilder) {
        let class = self.marks.get_class(class);
        self.bases.entry(glyph).or_default().push((class, anchor))
    }

    /// Returns an iterator over all of the base glyphs
    pub fn base_glyphs(&self) -> impl Iterator<Item = GlyphId16> + Clone + '_ {
        self.bases.keys().copied()
    }

    /// Returns an iterator over all of the mark glyphs
    pub fn mark_glyphs(&self) -> impl Iterator<Item = GlyphId16> + Clone + '_ {
        self.marks.glyphs()
    }
}

impl Builder for MarkToBaseBuilder {
    type Output = Vec<MarkBasePosFormat1>;

    fn build(self, var_store: &mut VariationStoreBuilder) -> Self::Output {
        let MarkToBaseBuilder { marks, bases } = self;
        let n_classes = marks.classes.len();

        let (mark_coverage, mark_array) = marks.build(var_store);
        let base_coverage = bases.keys().copied().collect();
        let base_records = bases
            .into_values()
            .map(|anchors| {
                let mut anchor_offsets = vec![None; n_classes];
                for (class, anchor) in anchors {
                    anchor_offsets[class as usize] = Some(anchor.build(var_store));
                }
                BaseRecord::new(anchor_offsets)
            })
            .collect();
        let base_array = BaseArray::new(base_records);
        vec![MarkBasePosFormat1::new(
            mark_coverage,
            base_coverage,
            mark_array,
            base_array,
        )]
    }
}

/// A builder for GPOS Lookup Type 5, Mark-to-Ligature
#[derive(Clone, Debug, Default, PartialEq, Eq)]
#[cfg_attr(feature = "serde", derive(serde::Serialize, serde::Deserialize))]
pub struct MarkToLigBuilder {
    marks: MarkList,
    ligatures: BTreeMap<GlyphId16, Vec<BTreeMap<String, AnchorBuilder>>>,
}

impl MarkToLigBuilder {
    /// Returns the number of rules in the lookup.
    pub fn len(&self) -> usize {
        self.ligatures.len()
    }

    /// Returns `true` if no rules have been added to the builder.
    pub fn is_empty(&self) -> bool {
        self.ligatures.is_empty()
    }

    /// Add a new mark glyph.
    ///
    /// If this glyph already exists in another mark class, we return the
    /// previous class; this is likely an error.
    pub fn insert_mark(
        &mut self,
        glyph: GlyphId16,
        class: &str,
        anchor: AnchorBuilder,
    ) -> Result<u16, PreviouslyAssignedClass> {
        self.marks.insert(glyph, class, anchor)
    }

    /// Add a ligature base, providing a set of anchors for each component.
    ///
    /// There must be an item in the vec for each component in the ligature
    /// glyph, but the anchors can be sparse; null anchors will be added for
    /// any classes that are missing.
    ///
    /// NOTE: this API is designed for use from a FEA compiler, as it closely
    /// mimics how the FEA source represents these rules where you process each
    /// component in order, with all the marks defined for that component)
    /// but this is less useful for public API, where you are more often dealing
    /// with marks a class at a time. For that reason we provide an alternative
    /// public method below.
    pub fn add_ligature_components_directly(
        &mut self,
        glyph: GlyphId16,
        components: Vec<BTreeMap<String, AnchorBuilder>>,
    ) {
        self.ligatures.insert(glyph, components);
    }

    /// Add ligature anchors for a specific mark class.
    ///
    /// This can be called multiple times for the same ligature glyph, to add anchors
    /// for multiple mark classes; however the number of components must be equal
    /// on each call for a given glyph id.
    ///
    /// If a component has no anchor for a given mark class, you must include an
    /// explicit 'None' in the appropriate ordering.
    pub fn insert_ligature(
        &mut self,
        glyph: GlyphId16,
        class: &str,
        components: Vec<Option<AnchorBuilder>>,
    ) {
        let component_list = self.ligatures.entry(glyph).or_default();
        if component_list.is_empty() {
            component_list.resize(components.len(), Default::default());
        } else if component_list.len() != components.len() {
            log::warn!("mismatched component lengths for anchors in glyph {glyph}");
        }
        for (i, anchor) in components.into_iter().enumerate() {
            if let Some(anchor) = anchor {
                component_list[i].insert(class.to_owned(), anchor);
            }
        }
    }

    /// Returns an iterator over all of the mark glyphs
    pub fn mark_glyphs(&self) -> impl Iterator<Item = GlyphId16> + Clone + '_ {
        self.marks.glyphs()
    }

    /// Returns an iterator over all of the ligature glyphs
    pub fn lig_glyphs(&self) -> impl Iterator<Item = GlyphId16> + Clone + '_ {
        self.ligatures.keys().copied()
    }
}

impl Builder for MarkToLigBuilder {
    type Output = Vec<MarkLigPosFormat1>;

    fn build(self, var_store: &mut VariationStoreBuilder) -> Self::Output {
        let MarkToLigBuilder { marks, ligatures } = self;
        let n_classes = marks.classes.len();

        // LigArray:
        // - [LigatureAttach] (one per ligature glyph)
        //    - [ComponentRecord] (one per component)
        //    - [Anchor] (one per mark-class)
        let ligature_coverage = ligatures.keys().copied().collect();
        let ligature_array = ligatures
            .into_values()
            .map(|components| {
                let comp_records = components
                    .into_iter()
                    .map(|anchors| {
                        let mut anchor_offsets = vec![None; n_classes];
                        for (class, anchor) in anchors {
                            let class_idx = marks.get_class(&class);
                            anchor_offsets[class_idx as usize] = Some(anchor.build(var_store));
                        }
                        ComponentRecord::new(anchor_offsets)
                    })
                    .collect();
                LigatureAttach::new(comp_records)
            })
            .collect();
        let ligature_array = LigatureArray::new(ligature_array);
        let (mark_coverage, mark_array) = marks.build(var_store);
        vec![MarkLigPosFormat1::new(
            mark_coverage,
            ligature_coverage,
            mark_array,
            ligature_array,
        )]
    }
}

/// A builder for GPOS Type 6 (Mark-to-Mark)
#[derive(Clone, Debug, Default, PartialEq, Eq)]
#[cfg_attr(feature = "serde", derive(serde::Serialize, serde::Deserialize))]
pub struct MarkToMarkBuilder {
    attaching_marks: MarkList,
    base_marks: BTreeMap<GlyphId16, Vec<(u16, AnchorBuilder)>>,
}

impl MarkToMarkBuilder {
    /// Returns the number of rules in the lookup.
    pub fn len(&self) -> usize {
        self.base_marks.len()
    }

    /// Returns `true` if no rules have been added to the builder.
    pub fn is_empty(&self) -> bool {
        self.base_marks.is_empty()
    }

    /// Add a new mark1 (combining) glyph.
    ///
    /// If this glyph already exists in another mark class, we return the
    /// previous class; this is likely an error.
    pub fn insert_mark1(
        &mut self,
        glyph: GlyphId16,
        class: &str,
        anchor: AnchorBuilder,
    ) -> Result<u16, PreviouslyAssignedClass> {
        self.attaching_marks.insert(glyph, class, anchor)
    }

    /// Insert a new mark2 (base) glyph
    pub fn insert_mark2(&mut self, glyph: GlyphId16, class: &str, anchor: AnchorBuilder) {
        let id = self.attaching_marks.get_class(class);
        self.base_marks.entry(glyph).or_default().push((id, anchor))
    }

    /// Returns an iterator over all of the mark1 glyphs
    pub fn mark1_glyphs(&self) -> impl Iterator<Item = GlyphId16> + Clone + '_ {
        self.attaching_marks.glyphs()
    }

    /// Returns an iterator over all of the mark2 glyphs
    pub fn mark2_glyphs(&self) -> impl Iterator<Item = GlyphId16> + Clone + '_ {
        self.base_marks.keys().copied()
    }
}

impl Builder for MarkToMarkBuilder {
    type Output = Vec<MarkMarkPosFormat1>;

    fn build(self, var_store: &mut VariationStoreBuilder) -> Self::Output {
        let MarkToMarkBuilder {
            attaching_marks,
            base_marks,
        } = self;
        let n_classes = attaching_marks.classes.len();

        let (mark_coverage, mark_array) = attaching_marks.build(var_store);
        let mark2_coverage = base_marks.keys().copied().collect();
        let mark2_records = base_marks
            .into_values()
            .map(|anchors| {
                let mut anchor_offsets = vec![None; n_classes];
                for (class, anchor) in anchors {
                    anchor_offsets[class as usize] = Some(anchor.build(var_store));
                }
                Mark2Record::new(anchor_offsets)
            })
            .collect();
        let mark2array = Mark2Array::new(mark2_records);
        vec![MarkMarkPosFormat1::new(
            mark_coverage,
            mark2_coverage,
            mark_array,
            mark2array,
        )]
    }
}

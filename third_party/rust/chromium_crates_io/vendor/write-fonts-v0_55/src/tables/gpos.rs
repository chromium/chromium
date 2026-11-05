//! the [GPOS] table
//!
//! [GPOS]: https://docs.microsoft.com/en-us/typography/opentype/spec/gpos

include!("../../generated/generated_gpos.rs");

impl ReadArgs for PositionLookupList2 {
    type Args = ();
}

impl<'a> FontRead<'a> for PositionLookupList2 {
    fn read_with_args(data: FontData<'a>, _: ()) -> Result<Self, ReadError> {
        read_fonts::tables::gpos::PositionLookupList2::read(data).map(|x| x.to_owned_table())
    }
}

use std::collections::HashSet;

//use super::layout::value_record::ValueRecord;
use super::{
    layout::{
        ChainedSequenceContext, ClassDef, CoverageTable, DeviceOrVariationIndex, FeatureList,
        FeatureVariations, Lookup, LookupList, LookupSubtable, LookupType, ScriptList,
        SequenceContext, VariationIndex,
    },
    variations::{common_builder::RemapVarStore, ivs_builder::VariationIndexRemapping},
};

#[cfg(test)]
mod extended;
#[cfg(test)]
mod spec_tests;

pub mod builders;
mod value_record;
pub use value_record::ValueRecord;

/// A GPOS lookup list table.
pub type PositionLookupList = LookupList<PositionLookup>;

/// A lookup list with the original 16-bit offsets.
pub type OriginalPositionLookupList = PositionLookupList;

/// A lookup list with 32-bit offsets.
pub type PositionLookupList2 = super::layout::LookupList2<PositionLookup>;

super::layout::table_newtype!(
    PositionSequenceContext,
    SequenceContext,
    read_fonts::tables::layout::SequenceContext<'a>
);

super::layout::table_newtype!(
    PositionChainContext,
    ChainedSequenceContext,
    read_fonts::tables::layout::ChainedSequenceContext<'a>
);

impl Gpos {
    /// Creates a GPOS table using the original 16-bit header offsets.
    pub fn new(
        script_list: ScriptList,
        feature_list: FeatureList,
        lookup_list: PositionLookupList,
    ) -> Self {
        Self {
            script_list: Some(script_list).into(),
            feature_list: Some(feature_list).into(),
            lookup_list: Some(lookup_list).into(),
            ..Default::default()
        }
    }

    fn compute_version(&self) -> MajorMinor {
        if self.script_list2.is_some()
            || self.feature_list2.is_some()
            || self.lookup_list2.is_some()
        {
            MajorMinor::new(1, 2)
        } else if self.feature_variations.is_none() {
            MajorMinor::VERSION_1_0
        } else {
            MajorMinor::VERSION_1_1
        }
    }
}

super::layout::lookup_type!(gpos, SinglePos, 1);
super::layout::lookup_type!(gpos, PairPos, 2);
super::layout::lookup_type!(gpos, CursivePosFormat1, 3);
super::layout::lookup_type!(gpos, MarkBasePosFormat1, 4);
super::layout::lookup_type!(gpos, MarkLigPosFormat1, 5);
super::layout::lookup_type!(gpos, MarkMarkPosFormat1, 6);
super::layout::lookup_type!(gpos, PositionSequenceContext, 7);
super::layout::lookup_type!(gpos, PositionChainContext, 8);
super::layout::lookup_type!(gpos, ExtensionSubtable, 9);

impl<T: LookupSubtable + FontWrite> FontWrite for ExtensionPosFormat1<T> {
    fn write_into(&self, writer: &mut TableWriter) {
        1u16.write_into(writer);
        T::TYPE.write_into(writer);
        self.extension.write_into(writer);
    }
}

// these can't have auto impls because the traits don't support generics
impl ReadArgs for PositionLookup {
    type Args = ();
}

impl<'a> FontRead<'a> for PositionLookup {
    fn read_with_args(data: FontData<'a>, _: ()) -> Result<Self, ReadError> {
        read_fonts::tables::gpos::PositionLookup::read(data).map(|x| x.to_owned_table())
    }
}

impl ReadArgs for PositionLookupList {
    type Args = ();
}

impl<'a> FontRead<'a> for PositionLookupList {
    fn read_with_args(data: FontData<'a>, _: ()) -> Result<Self, ReadError> {
        read_fonts::tables::gpos::OriginalPositionLookupList::read(data).map(|x| x.to_owned_table())
    }
}

impl SinglePosFormat1 {
    fn compute_value_format(&self) -> ValueFormat {
        self.value_record.format()
    }
}

impl SinglePosFormat2 {
    fn compute_value_format(&self) -> ValueFormat {
        self.value_records
            .first()
            .map(ValueRecord::format)
            .unwrap_or(ValueFormat::empty())
    }
}

impl PairPosFormat1 {
    fn compute_value_format1(&self) -> ValueFormat {
        self.pair_sets
            .first()
            .and_then(|pairset| pairset.pair_value_records.first())
            .map(|rec| rec.value_record1.format())
            .unwrap_or(ValueFormat::empty())
    }

    fn compute_value_format2(&self) -> ValueFormat {
        self.pair_sets
            .first()
            .and_then(|pairset| pairset.pair_value_records.first())
            .map(|rec| rec.value_record2.format())
            .unwrap_or(ValueFormat::empty())
    }

    fn check_format_consistency(&self, ctx: &mut ValidationCtx) {
        let vf1 = self.compute_value_format1();
        let vf2 = self.compute_value_format2();
        ctx.with_array_items(self.pair_sets.iter(), |ctx, item| {
            ctx.in_field("pair_value_records", |ctx| {
                if item.pair_value_records.iter().any(|pairset| {
                    pairset.value_record1.format() != vf1 || pairset.value_record2.format() != vf2
                }) {
                    ctx.report("all ValueRecords must have same format")
                }
            })
        })
    }
}

impl PairPosFormat2 {
    fn compute_value_format1(&self) -> ValueFormat {
        self.class1_records
            .first()
            .and_then(|rec| rec.class2_records.first())
            .map(|rec| rec.value_record1.format())
            .unwrap_or(ValueFormat::empty())
    }

    fn compute_value_format2(&self) -> ValueFormat {
        self.class1_records
            .first()
            .and_then(|rec| rec.class2_records.first())
            .map(|rec| rec.value_record2.format())
            .unwrap_or(ValueFormat::empty())
    }

    fn compute_class1_count(&self) -> u16 {
        self.class_def1.class_count().try_into().unwrap_or(0)
    }

    fn compute_class2_count(&self) -> u16 {
        self.class_def2.class_count().try_into().unwrap_or(0)
    }

    fn check_length_and_format_conformance(&self, ctx: &mut ValidationCtx) {
        let n_class_1s = self.class_def1.class_count();
        let n_class_2s = self.class_def2.class_count();
        if n_class_1s > u16::MAX as u32 || n_class_2s > u16::MAX as u32 {
            ctx.report("pair positioning class counts must fit in 16 bits");
        }
        let format_1 = self.compute_value_format1();
        let format_2 = self.compute_value_format2();
        if self.class1_records.len() != n_class_1s as usize {
            ctx.report("class1_records length must match number of class1 classes");
        }
        ctx.in_field("class1_records", |ctx| {
            ctx.with_array_items(self.class1_records.iter(), |ctx, c1rec| {
                if c1rec.class2_records.len() != n_class_2s as usize {
                    ctx.report("class2_records length must match number of class2 classes ");
                }
                if c1rec.class2_records.iter().any(|rec| {
                    rec.value_record1.format() != format_1 || rec.value_record2.format() != format_2
                }) {
                    ctx.report("all value records should report the same format");
                }
            })
        });
    }
}

impl MarkBasePosFormat1 {
    fn compute_mark_class_count(&self) -> u16 {
        self.mark_array.class_count()
    }
}

impl MarkMarkPosFormat1 {
    fn compute_mark_class_count(&self) -> u16 {
        self.mark1_array.class_count()
    }
}

impl MarkLigPosFormat1 {
    fn compute_mark_class_count(&self) -> u16 {
        self.mark_array.class_count()
    }
}

impl MarkArray {
    fn class_count(&self) -> u16 {
        self.mark_records
            .iter()
            .map(|rec| rec.mark_class)
            .collect::<HashSet<_>>()
            .len() as u16
    }
}

impl RemapVarStore<VariationIndex> for ValueRecord {
    fn remap_variation_indices(&mut self, key_map: &VariationIndexRemapping) {
        for table in [
            self.x_placement_device.as_mut(),
            self.y_placement_device.as_mut(),
            self.x_advance_device.as_mut(),
            self.y_advance_device.as_mut(),
        ]
        .into_iter()
        .flatten()
        {
            table.remap_variation_indices(key_map)
        }
    }
}

impl RemapVarStore<VariationIndex> for DeviceOrVariationIndex {
    fn remap_variation_indices(&mut self, key_map: &VariationIndexRemapping) {
        if let DeviceOrVariationIndex::PendingVariationIndex(table) = self {
            *self = key_map.get(table.delta_set_id).unwrap().into();
        }
    }
}

impl RemapVarStore<VariationIndex> for AnchorTable {
    fn remap_variation_indices(&mut self, key_map: &VariationIndexRemapping) {
        if let AnchorTable::Format3(table) = self {
            table
                .x_device
                .as_mut()
                .into_iter()
                .chain(table.y_device.as_mut())
                .for_each(|x| x.remap_variation_indices(key_map))
        }
    }
}

impl RemapVarStore<VariationIndex> for Gpos {
    fn remap_variation_indices(&mut self, key_map: &VariationIndexRemapping) {
        if let Some(list) = self.lookup_list.as_mut() {
            for lookup in &mut list.lookups {
                lookup.remap_variation_indices(key_map);
            }
        }
        if let Some(list) = self.lookup_list2.as_mut() {
            for lookup in &mut list.lookups {
                lookup.remap_variation_indices(key_map);
            }
        }
    }
}

impl RemapVarStore<VariationIndex> for PositionLookupList {
    fn remap_variation_indices(&mut self, key_map: &VariationIndexRemapping) {
        for lookup in &mut self.lookups {
            lookup.remap_variation_indices(key_map)
        }
    }
}

impl RemapVarStore<VariationIndex> for PositionLookup {
    fn remap_variation_indices(&mut self, key_map: &VariationIndexRemapping) {
        match self {
            PositionLookup::Single(lookup) => lookup.remap_variation_indices(key_map),
            PositionLookup::Pair(lookup) => lookup.remap_variation_indices(key_map),
            PositionLookup::Cursive(lookup) => lookup.remap_variation_indices(key_map),
            PositionLookup::MarkToBase(lookup) => lookup.remap_variation_indices(key_map),
            PositionLookup::MarkToLig(lookup) => lookup.remap_variation_indices(key_map),
            PositionLookup::MarkToMark(lookup) => lookup.remap_variation_indices(key_map),
            PositionLookup::Extension(lookup) => lookup.remap_variation_indices(key_map),

            // don't contain any metrics directly
            PositionLookup::Contextual(_) | PositionLookup::ChainContextual(_) => (),
        }
    }
}

impl RemapVarStore<VariationIndex> for ExtensionSubtable {
    fn remap_variation_indices(&mut self, key_map: &VariationIndexRemapping) {
        match self {
            ExtensionSubtable::Single(table) => table.remap_variation_indices(key_map),
            ExtensionSubtable::Pair(table) => table.remap_variation_indices(key_map),
            ExtensionSubtable::Cursive(table) => table.remap_variation_indices(key_map),
            ExtensionSubtable::MarkToBase(table) => table.remap_variation_indices(key_map),
            ExtensionSubtable::MarkToLig(table) => table.remap_variation_indices(key_map),
            ExtensionSubtable::MarkToMark(table) => table.remap_variation_indices(key_map),

            // don't contain any metrics directly
            ExtensionSubtable::Contextual(_) | ExtensionSubtable::ChainContextual(_) => (),
        }
    }
}

impl<T: RemapVarStore<VariationIndex>> RemapVarStore<VariationIndex> for ExtensionPosFormat1<T> {
    fn remap_variation_indices(&mut self, key_map: &VariationIndexRemapping) {
        self.extension.as_mut().remap_variation_indices(key_map)
    }
}

impl<T: RemapVarStore<VariationIndex>> RemapVarStore<VariationIndex> for Lookup<T> {
    fn remap_variation_indices(&mut self, key_map: &VariationIndexRemapping) {
        for subtable in &mut self.subtables {
            subtable.remap_variation_indices(key_map)
        }
    }
}

impl RemapVarStore<VariationIndex> for SinglePos {
    fn remap_variation_indices(&mut self, key_map: &VariationIndexRemapping) {
        match self {
            SinglePos::Format1(table) => table.remap_variation_indices(key_map),
            SinglePos::Format2(table) => table.remap_variation_indices(key_map),
            SinglePos::Format3(table) => table.remap_variation_indices(key_map),
            SinglePos::Format4(table) => table.remap_variation_indices(key_map),
        }
    }
}

impl RemapVarStore<VariationIndex> for SinglePosFormat1 {
    fn remap_variation_indices(&mut self, key_map: &VariationIndexRemapping) {
        self.value_record.remap_variation_indices(key_map);
    }
}

impl RemapVarStore<VariationIndex> for SinglePosFormat2 {
    fn remap_variation_indices(&mut self, key_map: &VariationIndexRemapping) {
        for rec in &mut self.value_records {
            rec.remap_variation_indices(key_map);
        }
    }
}

impl RemapVarStore<VariationIndex> for PairPosFormat1 {
    fn remap_variation_indices(&mut self, key_map: &VariationIndexRemapping) {
        for pairset in &mut self.pair_sets {
            for pairrec in &mut pairset.pair_value_records {
                pairrec.value_record1.remap_variation_indices(key_map);
                pairrec.value_record2.remap_variation_indices(key_map);
            }
        }
    }
}

impl RemapVarStore<VariationIndex> for PairPosFormat2 {
    fn remap_variation_indices(&mut self, key_map: &VariationIndexRemapping) {
        for class1rec in &mut self.class1_records {
            for class2rec in &mut class1rec.class2_records {
                class2rec.value_record1.remap_variation_indices(key_map);
                class2rec.value_record2.remap_variation_indices(key_map);
            }
        }
    }
}

impl RemapVarStore<VariationIndex> for PairPos {
    fn remap_variation_indices(&mut self, key_map: &VariationIndexRemapping) {
        match self {
            PairPos::Format1(table) => table.remap_variation_indices(key_map),
            PairPos::Format2(table) => table.remap_variation_indices(key_map),
            PairPos::Format3(table) => table.remap_variation_indices(key_map),
            PairPos::Format4(table) => table.remap_variation_indices(key_map),
        }
    }
}

impl RemapVarStore<VariationIndex> for MarkBasePosFormat1 {
    fn remap_variation_indices(&mut self, key_map: &VariationIndexRemapping) {
        self.mark_array.as_mut().remap_variation_indices(key_map);
        for rec in &mut self.base_array.as_mut().base_records {
            for anchor in &mut rec.base_anchors {
                if let Some(anchor) = anchor.as_mut() {
                    anchor.remap_variation_indices(key_map);
                }
            }
        }
    }
}

impl RemapVarStore<VariationIndex> for MarkMarkPosFormat1 {
    fn remap_variation_indices(&mut self, key_map: &VariationIndexRemapping) {
        self.mark1_array.as_mut().remap_variation_indices(key_map);
        for rec in &mut self.mark2_array.as_mut().mark2_records {
            for anchor in &mut rec.mark2_anchors {
                if let Some(anchor) = anchor.as_mut() {
                    anchor.remap_variation_indices(key_map);
                }
            }
        }
    }
}

impl RemapVarStore<VariationIndex> for MarkLigPosFormat1 {
    fn remap_variation_indices(&mut self, key_map: &VariationIndexRemapping) {
        self.mark_array.as_mut().remap_variation_indices(key_map);
        for lig in &mut self.ligature_array.as_mut().ligature_attaches {
            for rec in &mut lig.component_records {
                for anchor in &mut rec.ligature_anchors {
                    if let Some(anchor) = anchor.as_mut() {
                        anchor.remap_variation_indices(key_map);
                    }
                }
            }
        }
    }
}

impl RemapVarStore<VariationIndex> for CursivePosFormat1 {
    fn remap_variation_indices(&mut self, key_map: &VariationIndexRemapping) {
        for rec in &mut self.entry_exit_record {
            for anchor in [rec.entry_anchor.as_mut(), rec.exit_anchor.as_mut()]
                .into_iter()
                .flatten()
            {
                anchor.remap_variation_indices(key_map);
            }
        }
    }
}

impl RemapVarStore<VariationIndex> for MarkArray {
    fn remap_variation_indices(&mut self, key_map: &VariationIndexRemapping) {
        for rec in &mut self.mark_records {
            rec.mark_anchor.remap_variation_indices(key_map);
        }
    }
}

super::layout::lookup_type!(gpos, CursivePos, 3);
super::layout::lookup_type!(gpos, CursivePosFormat2, 3);
super::layout::original_lookup!(PositionLookup, Cursive, CursivePosFormat1, CursivePos);

super::layout::lookup_type!(gpos, MarkBasePos, 4);
super::layout::lookup_type!(gpos, MarkBasePosFormat2, 4);
super::layout::original_lookup!(PositionLookup, MarkToBase, MarkBasePosFormat1, MarkBasePos);

super::layout::lookup_type!(gpos, MarkLigPos, 5);
super::layout::lookup_type!(gpos, MarkLigPosFormat2, 5);
super::layout::original_lookup!(PositionLookup, MarkToLig, MarkLigPosFormat1, MarkLigPos);

super::layout::lookup_type!(gpos, MarkMarkPos, 6);
super::layout::lookup_type!(gpos, MarkMarkPosFormat2, 6);
super::layout::original_lookup!(PositionLookup, MarkToMark, MarkMarkPosFormat1, MarkMarkPos);

impl SinglePosFormat3 {
    fn compute_value_format(&self) -> ValueFormat {
        self.value_record.format()
    }
}

impl SinglePosFormat4 {
    fn compute_value_format(&self) -> ValueFormat {
        self.value_records
            .first()
            .map(ValueRecord::format)
            .unwrap_or(ValueFormat::empty())
    }

    fn check_format_consistency(&self, ctx: &mut ValidationCtx) {
        let format = self.compute_value_format();
        if self
            .value_records
            .iter()
            .any(|record| record.format() != format)
        {
            ctx.report("all ValueRecords must have same format");
        }
    }
}

impl PairPosFormat3 {
    fn compute_value_format1(&self) -> ValueFormat {
        self.pair_sets
            .iter()
            .find_map(|pairset| pairset.pair_value_records.first())
            .map(|rec| rec.value_record1.format())
            .unwrap_or(ValueFormat::empty())
    }

    fn compute_value_format2(&self) -> ValueFormat {
        self.pair_sets
            .iter()
            .find_map(|pairset| pairset.pair_value_records.first())
            .map(|rec| rec.value_record2.format())
            .unwrap_or(ValueFormat::empty())
    }

    fn check_format_consistency(&self, ctx: &mut ValidationCtx) {
        let vf1 = self.compute_value_format1();
        let vf2 = self.compute_value_format2();
        ctx.with_array_items(self.pair_sets.iter(), |ctx, item| {
            ctx.in_field("pair_value_records", |ctx| {
                if item.pair_value_records.iter().any(|pairset| {
                    pairset.value_record1.format() != vf1 || pairset.value_record2.format() != vf2
                }) {
                    ctx.report("all ValueRecords must have same format")
                }
            })
        })
    }
}

impl PairPosFormat4 {
    fn compute_value_format1(&self) -> ValueFormat {
        self.class1_records
            .first()
            .and_then(|rec| rec.class2_records.first())
            .map(|rec| rec.value_record1.format())
            .unwrap_or(ValueFormat::empty())
    }

    fn compute_value_format2(&self) -> ValueFormat {
        self.class1_records
            .first()
            .and_then(|rec| rec.class2_records.first())
            .map(|rec| rec.value_record2.format())
            .unwrap_or(ValueFormat::empty())
    }

    fn compute_class1_count(&self) -> u16 {
        self.class1_records
            .len()
            .try_into()
            .expect("class count exceeds 16 bits")
    }

    fn compute_class2_count(&self) -> u16 {
        self.class1_records
            .first()
            .map_or(0, |row| row.class2_records.len())
            .try_into()
            .expect("class count exceeds 16 bits")
    }

    fn check_length_and_format_conformance(&self, ctx: &mut ValidationCtx) {
        let n_class_1s = self.class1_records.len();
        let n_class_2s = self
            .class1_records
            .first()
            .map_or(0, |row| row.class2_records.len());
        if n_class_1s > u16::MAX as usize || n_class_2s > u16::MAX as usize {
            ctx.report("pair positioning class counts must fit in 16 bits");
        }
        let format_1 = self.compute_value_format1();
        let format_2 = self.compute_value_format2();
        if self
            .class_def1
            .iter()
            .any(|(_, class)| class as usize >= n_class_1s)
            || self
                .class_def2
                .iter()
                .any(|(_, class)| class as usize >= n_class_2s)
        {
            ctx.report("class definitions must index the value record matrix");
        }
        ctx.in_field("class1_records", |ctx| {
            ctx.with_array_items(self.class1_records.iter(), |ctx, c1rec| {
                if c1rec.class2_records.len() != n_class_2s {
                    ctx.report("class2_records length must match number of class2 classes ");
                }
                if c1rec.class2_records.iter().any(|rec| {
                    rec.value_record1.format() != format_1 || rec.value_record2.format() != format_2
                }) {
                    ctx.report("all value records should report the same format");
                }
            })
        });
    }
}

impl MarkBasePosFormat2 {
    fn compute_mark_class_count(&self) -> u16 {
        self.mark_array
            .class_count()
            .try_into()
            .expect("mark class count exceeds 16 bits")
    }

    fn check_class_dimensions(&self, ctx: &mut ValidationCtx) {
        let classes = self.mark_array.class_count() as usize;
        if (self
            .base_array
            .base_records
            .iter()
            .map(|rec| rec.base_anchors.len()))
        .any(|len| len != classes)
        {
            ctx.report("anchor arrays must match the mark class count");
        }
    }
}

impl MarkMarkPosFormat2 {
    fn compute_mark_class_count(&self) -> u16 {
        self.mark1_array
            .class_count()
            .try_into()
            .expect("mark class count exceeds 16 bits")
    }

    fn check_class_dimensions(&self, ctx: &mut ValidationCtx) {
        let classes = self.mark1_array.class_count() as usize;
        if (self
            .mark2_array
            .mark2_records
            .iter()
            .map(|rec| rec.mark2_anchors.len()))
        .any(|len| len != classes)
        {
            ctx.report("anchor arrays must match the mark class count");
        }
    }
}

impl MarkLigPosFormat2 {
    fn compute_mark_class_count(&self) -> u16 {
        self.mark_array
            .class_count()
            .try_into()
            .expect("mark class count exceeds 16 bits")
    }

    fn check_class_dimensions(&self, ctx: &mut ValidationCtx) {
        let classes = self.mark_array.class_count() as usize;
        if (self
            .ligature_array
            .ligature_attaches
            .iter()
            .flat_map(|lig| lig.component_records.iter())
            .map(|rec| rec.ligature_anchors.len()))
        .any(|len| len != classes)
        {
            ctx.report("anchor arrays must match the mark class count");
        }
    }
}

impl MarkArray2 {
    fn class_count(&self) -> u32 {
        self.mark_records
            .iter()
            .map(|rec| u32::from(rec.mark_class) + 1)
            .max()
            .unwrap_or(0)
    }

    fn check_class_count(&self, ctx: &mut ValidationCtx) {
        if self.class_count() > u16::MAX as u32 {
            ctx.report("mark class count must fit in 16 bits");
        }
    }
}

impl RemapVarStore<VariationIndex> for SinglePosFormat3 {
    fn remap_variation_indices(&mut self, key_map: &VariationIndexRemapping) {
        self.value_record.remap_variation_indices(key_map);
    }
}

impl RemapVarStore<VariationIndex> for SinglePosFormat4 {
    fn remap_variation_indices(&mut self, key_map: &VariationIndexRemapping) {
        for rec in &mut self.value_records {
            rec.remap_variation_indices(key_map);
        }
    }
}

impl RemapVarStore<VariationIndex> for PairPosFormat3 {
    fn remap_variation_indices(&mut self, key_map: &VariationIndexRemapping) {
        for pairset in &mut self.pair_sets {
            for pairrec in &mut pairset.pair_value_records {
                pairrec.value_record1.remap_variation_indices(key_map);
                pairrec.value_record2.remap_variation_indices(key_map);
            }
        }
    }
}

impl RemapVarStore<VariationIndex> for PairPosFormat4 {
    fn remap_variation_indices(&mut self, key_map: &VariationIndexRemapping) {
        for class1rec in &mut self.class1_records {
            for class2rec in &mut class1rec.class2_records {
                class2rec.value_record1.remap_variation_indices(key_map);
                class2rec.value_record2.remap_variation_indices(key_map);
            }
        }
    }
}

impl RemapVarStore<VariationIndex> for MarkBasePosFormat2 {
    fn remap_variation_indices(&mut self, key_map: &VariationIndexRemapping) {
        self.mark_array.as_mut().remap_variation_indices(key_map);
        for rec in &mut self.base_array.as_mut().base_records {
            for anchor in &mut rec.base_anchors {
                if let Some(anchor) = anchor.as_mut() {
                    anchor.remap_variation_indices(key_map);
                }
            }
        }
    }
}

impl RemapVarStore<VariationIndex> for MarkMarkPosFormat2 {
    fn remap_variation_indices(&mut self, key_map: &VariationIndexRemapping) {
        self.mark1_array.as_mut().remap_variation_indices(key_map);
        for rec in &mut self.mark2_array.as_mut().mark2_records {
            for anchor in &mut rec.mark2_anchors {
                if let Some(anchor) = anchor.as_mut() {
                    anchor.remap_variation_indices(key_map);
                }
            }
        }
    }
}

impl RemapVarStore<VariationIndex> for MarkLigPosFormat2 {
    fn remap_variation_indices(&mut self, key_map: &VariationIndexRemapping) {
        self.mark_array.as_mut().remap_variation_indices(key_map);
        for lig in &mut self.ligature_array.as_mut().ligature_attaches {
            for rec in &mut lig.component_records {
                for anchor in &mut rec.ligature_anchors {
                    if let Some(anchor) = anchor.as_mut() {
                        anchor.remap_variation_indices(key_map);
                    }
                }
            }
        }
    }
}

impl RemapVarStore<VariationIndex> for CursivePosFormat2 {
    fn remap_variation_indices(&mut self, key_map: &VariationIndexRemapping) {
        for rec in &mut self.entry_exit_record {
            for anchor in [rec.entry_anchor.as_mut(), rec.exit_anchor.as_mut()]
                .into_iter()
                .flatten()
            {
                anchor.remap_variation_indices(key_map);
            }
        }
    }
}

impl RemapVarStore<VariationIndex> for MarkArray2 {
    fn remap_variation_indices(&mut self, key_map: &VariationIndexRemapping) {
        for rec in &mut self.mark_records {
            rec.mark_anchor.remap_variation_indices(key_map);
        }
    }
}

impl RemapVarStore<VariationIndex> for CursivePos {
    fn remap_variation_indices(&mut self, key_map: &VariationIndexRemapping) {
        match self {
            Self::Format1(t) => t.remap_variation_indices(key_map),
            Self::Format2(t) => t.remap_variation_indices(key_map),
        }
    }
}

impl RemapVarStore<VariationIndex> for MarkBasePos {
    fn remap_variation_indices(&mut self, key_map: &VariationIndexRemapping) {
        match self {
            Self::Format1(t) => t.remap_variation_indices(key_map),
            Self::Format2(t) => t.remap_variation_indices(key_map),
        }
    }
}

impl RemapVarStore<VariationIndex> for MarkMarkPos {
    fn remap_variation_indices(&mut self, key_map: &VariationIndexRemapping) {
        match self {
            Self::Format1(t) => t.remap_variation_indices(key_map),
            Self::Format2(t) => t.remap_variation_indices(key_map),
        }
    }
}

impl RemapVarStore<VariationIndex> for MarkLigPos {
    fn remap_variation_indices(&mut self, key_map: &VariationIndexRemapping) {
        match self {
            Self::Format1(t) => t.remap_variation_indices(key_map),
            Self::Format2(t) => t.remap_variation_indices(key_map),
        }
    }
}

#[cfg(test)]
mod tests {

    use read_fonts::tables::{gpos as read_gpos, layout::LookupFlag};

    use crate::tables::layout::VariationIndex;

    use super::*;

    #[test]
    fn remap_variation_indices_through_extension() {
        use crate::tables::variations::{
            ivs_builder::VariationStoreBuilder, RegionAxisCoordinates, VariationRegion,
        };
        use font_types::F2Dot14;

        let region = VariationRegion::new(vec![RegionAxisCoordinates {
            start_coord: F2Dot14::from_f32(0.0),
            peak_coord: F2Dot14::from_f32(1.0),
            end_coord: F2Dot14::from_f32(1.0),
        }]);
        let mut var_store = VariationStoreBuilder::new(1);
        let temp_id = var_store.add_deltas(vec![(region, -20)]);
        let (_, key_map) = var_store.build();

        let value_record = ValueRecord::new()
            .with_x_advance(-20)
            .with_x_advance_device(DeviceOrVariationIndex::pending_variation_index(temp_id));
        let pair_set = PairSet::new(vec![PairValueRecord::new(
            GlyphId16::new(2),
            value_record,
            ValueRecord::default(),
        )]);
        let pair_pos = PairPos::format_1(
            CoverageTable::format_1(vec![GlyphId16::new(1)]),
            vec![pair_set],
        );

        // the same subtable, in a plain lookup and wrapped in an extension lookup
        let mut plain =
            PositionLookup::Pair(Lookup::new(LookupFlag::default(), vec![pair_pos.clone()]));
        let mut extension = PositionLookup::Extension(Lookup::new(
            LookupFlag::default(),
            vec![ExtensionSubtable::Pair(ExtensionPosFormat1::new(
                <PairPos as LookupSubtable>::TYPE.to_raw(),
                pair_pos,
            ))],
        ));

        plain.remap_variation_indices(&key_map);
        extension.remap_variation_indices(&key_map);

        // writing a PendingVariationIndex panics, so dump_table succeeding is the assertion
        let plain_bytes = crate::dump_table(&plain).unwrap();
        let ext_bytes = crate::dump_table(&extension).unwrap();

        let parsed = read_gpos::PositionLookup::read(FontData::new(&ext_bytes)).unwrap();
        let read_gpos::PositionSubtables::Pair(subtables) = parsed.subtables().unwrap() else {
            panic!("expected a pair lookup");
        };
        let read_gpos::PairPos::Format1(subtable) = subtables.get(0).unwrap() else {
            panic!("expected format 1");
        };
        let pair_set = subtable.pair_sets().get(0).unwrap();
        let record = &pair_set.pair_value_records().get(0).unwrap();
        let read_fonts::tables::layout::DeviceOrVariationIndex::VariationIndex(var_idx) =
            record.value_record1.x_advance_device().unwrap().unwrap()
        else {
            panic!("expected a VariationIndex");
        };
        let expected = key_map.get(temp_id).unwrap();
        assert_eq!(
            (
                var_idx.delta_set_outer_index(),
                var_idx.delta_set_inner_index()
            ),
            (
                expected.delta_set_outer_index,
                expected.delta_set_inner_index
            )
        );
        // the extension wrapper is the only difference, so the inner bytes must match
        assert_eq!(plain_bytes.len() + 8, ext_bytes.len());
    }

    // adapted from/motivated by https://github.com/fonttools/fonttools/issues/471
    #[test]
    fn gpos_1_zero() {
        let cov_one = CoverageTable::format_1(vec![GlyphId16::new(2)]);
        let cov_two = CoverageTable::format_1(vec![GlyphId16::new(4)]);
        let sub1 = SinglePos::format_1(cov_one, ValueRecord::default());
        let sub2 = SinglePos::format_1(cov_two, ValueRecord::default().with_x_advance(500));
        let lookup = Lookup::new(LookupFlag::default(), vec![sub1, sub2]);
        let bytes = crate::dump_table(&lookup).unwrap();

        let parsed = read_gpos::PositionLookup::read(FontData::new(&bytes)).unwrap();
        let read_gpos::PositionLookup::Single(table) = parsed else {
            panic!("something has gone seriously wrong");
        };

        assert_eq!(table.lookup_flag(), LookupFlag::empty());
        assert_eq!(table.sub_table_count(), 2);
        let read_gpos::SinglePos::Format1(sub1) = table.subtables().get(0).unwrap() else {
            panic!("wrong table type");
        };
        let read_gpos::SinglePos::Format1(sub2) = table.subtables().get(1).unwrap() else {
            panic!("wrong table type");
        };

        assert_eq!(sub1.value_format(), ValueFormat::empty());
        assert!(sub1.value_record().is_empty());

        assert_eq!(sub2.value_format(), ValueFormat::X_ADVANCE);
        assert_eq!(sub2.value_record().x_advance(), Some(500));
        assert_eq!(sub2.value_record().y_advance(), None);
    }

    // shared between a pair of tests below
    fn make_rec(i: u16) -> ValueRecord {
        // '0' here is shorthand for 'no device table'
        if i == 0 {
            return ValueRecord::new().with_explicit_value_format(ValueFormat::X_ADVANCE_DEVICE);
        }
        ValueRecord::new().with_x_advance_device(VariationIndex::new(0xff, i))
    }

    #[test]
    fn compile_devices_pairpos2() {
        let class1 = ClassDef::from_iter([(GlyphId16::new(5), 0), (GlyphId16::new(6), 1)]);
        // class 0 is 'all the rest', here, always implicitly present
        let class2 = ClassDef::from_iter([(GlyphId16::new(8), 1)]);

        // two c1recs, each with two c2recs
        let class1recs = vec![
            Class1Record::new(vec![
                Class2Record::new(make_rec(0), make_rec(0)),
                Class2Record::new(make_rec(1), make_rec(2)),
            ]),
            Class1Record::new(vec![
                Class2Record::new(make_rec(0), make_rec(0)),
                Class2Record::new(make_rec(2), make_rec(3)),
            ]),
        ];
        let coverage = class1.iter().map(|(gid, _)| gid).collect();
        let a_table = PairPos::format_2(coverage, class1, class2, class1recs);

        let bytes = crate::dump_table(&a_table).unwrap();
        let read_back = PairPosFormat2::read(bytes.as_slice().into()).unwrap();

        assert!(read_back.class1_records[0].class2_records[0]
            .value_record1
            .x_advance_device
            .is_none());
        assert!(read_back.class1_records[1].class2_records[1]
            .value_record1
            .x_advance_device
            .is_some());

        let DeviceOrVariationIndex::VariationIndex(dev2) = read_back.class1_records[0]
            .class2_records[1]
            .value_record2
            .x_advance_device
            .as_ref()
            .unwrap()
        else {
            panic!("not a variation index")
        };
        assert_eq!(dev2.delta_set_inner_index, 2);
    }

    #[should_panic(expected = "all value records should report the same format")]
    #[test]
    fn validate_bad_pairpos2() {
        let class1 = ClassDef::from_iter([(GlyphId16::new(5), 0), (GlyphId16::new(6), 1)]);
        // class 0 is 'all the rest', here, always implicitly present
        let class2 = ClassDef::from_iter([(GlyphId16::new(8), 1)]);
        let coverage = class1.iter().map(|(gid, _)| gid).collect();

        // two c1recs, each with two c2recs
        let class1recs = vec![
            Class1Record::new(vec![
                Class2Record::new(make_rec(0), make_rec(0)),
                Class2Record::new(make_rec(1), make_rec(2)),
            ]),
            Class1Record::new(vec![
                Class2Record::new(make_rec(0), make_rec(0)),
                // this is now the wrong type
                Class2Record::new(make_rec(2), make_rec(3).with_x_advance(0x514)),
            ]),
        ];
        let ppf2 = PairPos::format_2(coverage, class1, class2, class1recs);
        crate::dump_table(&ppf2).unwrap();
    }

    #[test]
    fn validate_pairpos1() {
        let coverage: CoverageTable = [1, 2].into_iter().map(GlyphId16::new).collect();
        let good_table = PairPosFormat1::new(
            coverage.clone(),
            vec![
                PairSet::new(vec![PairValueRecord::new(
                    GlyphId16::new(5),
                    ValueRecord::new().with_x_advance(5),
                    ValueRecord::new(),
                )]),
                PairSet::new(vec![PairValueRecord::new(
                    GlyphId16::new(1),
                    ValueRecord::new().with_x_advance(42),
                    ValueRecord::new(),
                )]),
            ],
        );

        let bad_table = PairPosFormat1::new(
            coverage,
            vec![
                PairSet::new(vec![PairValueRecord::new(
                    GlyphId16::new(5),
                    ValueRecord::new().with_x_advance(5),
                    ValueRecord::new(),
                )]),
                PairSet::new(vec![PairValueRecord::new(
                    GlyphId16::new(1),
                    //this is a different format, which is not okay
                    ValueRecord::new().with_x_placement(42),
                    ValueRecord::new(),
                )]),
            ],
        );

        assert!(crate::dump_table(&good_table).is_ok());
        assert!(matches!(
            crate::dump_table(&bad_table),
            Err(crate::error::Error::ValidationFailed(_))
        ));
    }
}

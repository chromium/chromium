//! impl subset() for layout common tables

use std::{cmp::Ordering, mem};

use crate::fnv::FnvHashMap;
use crate::{
    offset::SerializeSubset,
    offset_array::SubsetOffsetArray,
    serialize::{OffsetWhence, SerializeErrorFlags, Serializer},
    CollectVariationIndices, NameIdClosure, Plan, Serialize, SubsetState, SubsetTable,
};
use write_fonts::{
    read::{
        collections::IntSet,
        tables::{
            gsub::Gsub,
            layout::{
                CharacterVariantParams, ClassDef, ClassDefFormat1, ClassDefFormat2,
                ClassRangeRecord, Condition, ConditionFormat1, ConditionSet, CoverageFormat1,
                CoverageFormat2, CoverageTable, DeltaFormat, Device, DeviceOrVariationIndex,
                Feature, FeatureList, FeatureParams, FeatureRecord, FeatureTableSubstitution,
                FeatureTableSubstitutionRecord, FeatureVariationRecord, FeatureVariations,
                Intersect, LangSys, LangSysRecord, LookupList, RangeRecord, Script, ScriptList,
                ScriptRecord, SizeParams, StylisticSetParams, VariationIndex,
            },
        },
        types::{GlyphId, GlyphId16, NameId},
        ArrayOfOffsets, FontData, FontRead, FontRef, MinByteRange, ReadError, TopLevelTable,
    },
    types::{FixedSize, Offset16, Offset32, Tag},
};

const MAX_SCRIPTS: u16 = 500;
const MAX_LANGSYS: u16 = 2000;
const MAX_FEATURE_INDICES: u16 = 1500;
const MAX_LOOKUP_VISIT_COUNT: u16 = 35000;
const MAX_LANGSYS_FEATURE_COUNT: u16 = 5000;

impl NameIdClosure for StylisticSetParams<'_> {
    fn collect_name_ids(&self, plan: &mut Plan) {
        plan.name_ids.insert(self.ui_name_id());
    }
}

impl NameIdClosure for SizeParams<'_> {
    fn collect_name_ids(&self, plan: &mut Plan) {
        plan.name_ids.insert(NameId::new(self.name_entry()));
    }
}

impl NameIdClosure for CharacterVariantParams<'_> {
    fn collect_name_ids(&self, plan: &mut Plan) {
        plan.name_ids.insert(self.feat_ui_label_name_id());
        plan.name_ids.insert(self.feat_ui_tooltip_text_name_id());
        plan.name_ids.insert(self.sample_text_name_id());

        let first_name_id = self.first_param_ui_label_name_id();
        let num_named_params = self.num_named_parameters();
        if first_name_id == NameId::COPYRIGHT_NOTICE
            || num_named_params == 0
            || num_named_params >= 0x7FFF
        {
            return;
        }

        let last_name_id = first_name_id.to_u16() as u32 + num_named_params as u32 - 1;
        plan.name_ids.insert_range(first_name_id..=NameId::new(last_name_id as u16));
    }
}

impl NameIdClosure for Feature<'_> {
    fn collect_name_ids(&self, plan: &mut Plan) {
        let Some(Ok(feature_params)) = self.feature_params() else {
            return;
        };
        match feature_params {
            FeatureParams::StylisticSet(table) => table.collect_name_ids(plan),
            FeatureParams::Size(table) => table.collect_name_ids(plan),
            FeatureParams::CharacterVariant(table) => table.collect_name_ids(plan),
        }
    }
}

impl<'a> SubsetTable<'a> for DeviceOrVariationIndex<'a> {
    type ArgsForSubset = &'a FnvHashMap<u32, (u32, i32)>;
    type Output = ();

    fn subset(
        &self,
        plan: &Plan,
        s: &mut Serializer,
        args: &FnvHashMap<u32, (u32, i32)>,
    ) -> Result<(), SerializeErrorFlags> {
        match self {
            Self::Device(item) => item.subset(plan, s, ()),
            Self::VariationIndex(item) => item.subset(plan, s, args),
        }
    }
}

impl SubsetTable<'_> for Device<'_> {
    type ArgsForSubset = ();
    type Output = ();
    fn subset(
        &self,
        _plan: &Plan,
        s: &mut Serializer,
        _args: (),
    ) -> Result<(), SerializeErrorFlags> {
        s.embed_bytes(self.min_table_bytes()).map(|_| ())
    }
}

impl<'a> SubsetTable<'a> for VariationIndex<'a> {
    type ArgsForSubset = &'a FnvHashMap<u32, (u32, i32)>;
    type Output = ();

    fn subset(
        &self,
        _plan: &Plan,
        s: &mut Serializer,
        args: &FnvHashMap<u32, (u32, i32)>,
    ) -> Result<(), SerializeErrorFlags> {
        let var_idx =
            ((self.delta_set_outer_index() as u32) << 16) + self.delta_set_inner_index() as u32;
        let Some((new_idx, _)) = args.get(&var_idx) else {
            return Err(SerializeErrorFlags::SERIALIZE_ERROR_OTHER);
        };

        s.embed(*new_idx)?;
        s.embed(self.delta_format()).map(|_| ())
    }
}

impl CollectVariationIndices for DeviceOrVariationIndex<'_> {
    fn collect_variation_indices(&self, plan: &Plan, varidx_set: &mut IntSet<u32>) {
        match self {
            Self::Device(_item) => (),
            Self::VariationIndex(item) => item.collect_variation_indices(plan, varidx_set),
        }
    }
}

impl CollectVariationIndices for VariationIndex<'_> {
    fn collect_variation_indices(&self, _plan: &Plan, varidx_set: &mut IntSet<u32>) {
        if self.delta_format() == DeltaFormat::VariationIndex {
            let var_idx =
                ((self.delta_set_outer_index() as u32) << 16) + self.delta_set_inner_index() as u32;
            varidx_set.insert(var_idx);
        }
    }
}

pub(crate) struct ClassDefSubsetStruct<'a> {
    pub(crate) remap_class: bool,
    pub(crate) keep_empty_table: bool,
    pub(crate) use_class_zero: bool,
    pub(crate) glyph_filter: Option<&'a CoverageTable<'a>>,
}

impl<'a> SubsetTable<'a> for ClassDef<'a> {
    type ArgsForSubset = &'a ClassDefSubsetStruct<'a>;
    // class_map: Option<FnvHashMap<u16, u16>>
    type Output = Option<FnvHashMap<u16, u16>>;
    fn subset(
        &self,
        plan: &Plan,
        s: &mut Serializer,
        args: Self::ArgsForSubset,
    ) -> Result<Self::Output, SerializeErrorFlags> {
        match self {
            Self::Format1(item) => item.subset(plan, s, args),
            Self::Format2(item) => item.subset(plan, s, args),
        }
    }
}

impl<'a> SubsetTable<'a> for ClassDefFormat1<'a> {
    type ArgsForSubset = &'a ClassDefSubsetStruct<'a>;
    // class_map: Option<FnvHashMap<u16, u16>>
    type Output = Option<FnvHashMap<u16, u16>>;
    fn subset(
        &self,
        plan: &Plan,
        s: &mut Serializer,
        args: Self::ArgsForSubset,
    ) -> Result<Self::Output, SerializeErrorFlags> {
        let glyph_map = &plan.glyph_map_gsub;

        let start = self.start_glyph_id().to_u32();
        let end = start + self.glyph_count() as u32 - 1;
        let end = plan.glyphset_gsub.last().unwrap().to_u32().min(end);

        let class_values = self.class_value_array();
        let mut retained_classes = IntSet::empty();

        let cap = glyph_map.len().min(self.glyph_count() as usize);
        let mut new_gid_classes = Vec::with_capacity(cap);

        for g in start..=end {
            let gid = GlyphId::from(g);
            let Some(new_gid) = glyph_map.get(&gid) else {
                continue;
            };

            if let Some(glyph_filter) = args.glyph_filter {
                if glyph_filter.get(gid).is_none() {
                    continue;
                }
            }

            let Some(class) = class_values.get((g - start) as usize) else {
                return Err(s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR));
            };

            let class = class.get();
            if class == 0 {
                continue;
            }

            retained_classes.insert(class);
            new_gid_classes.push((new_gid.to_u32() as u16, class));
        }

        let use_class_zero = if args.use_class_zero {
            let glyph_count = if let Some(glyph_filter) = args.glyph_filter {
                glyph_map.keys().filter(|&g| glyph_filter.get(*g).is_some()).count()
            } else {
                glyph_map.len()
            };
            glyph_count <= new_gid_classes.len()
        } else {
            false
        };

        if !args.keep_empty_table && new_gid_classes.is_empty() {
            return Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY);
        }

        classdef_remap_and_serialize(
            args.remap_class,
            &retained_classes,
            use_class_zero,
            &mut new_gid_classes,
            s,
        )
    }
}

impl<'a> SubsetTable<'a> for ClassDefFormat2<'a> {
    type ArgsForSubset = &'a ClassDefSubsetStruct<'a>;
    // class_map: Option<FnvHashMap<u16, u16>>
    type Output = Option<FnvHashMap<u16, u16>>;
    fn subset(
        &self,
        plan: &Plan,
        s: &mut Serializer,
        args: Self::ArgsForSubset,
    ) -> Result<Self::Output, SerializeErrorFlags> {
        let glyph_map = &plan.glyph_map_gsub;
        let glyph_set = &plan.glyphset_gsub;

        let mut retained_classes = IntSet::empty();

        let population = self.population();
        let cap = glyph_map.len().min(population);
        let mut new_gid_classes = Vec::with_capacity(cap);

        let num_bits = 16 - self.class_range_count().leading_zeros() as u64;
        if population as u64 > glyph_set.len() * num_bits {
            for g in glyph_set.iter() {
                if g.to_u32() > 0xFFFF_u32 {
                    break;
                }
                let Some(new_gid) = glyph_map.get(&g) else {
                    continue;
                };
                if let Some(glyph_filter) = args.glyph_filter {
                    if glyph_filter.get(g).is_none() {
                        continue;
                    }
                }

                let class = self.get(GlyphId16::from(g.to_u32() as u16));
                if class == 0 {
                    continue;
                }

                retained_classes.insert(class);
                new_gid_classes.push((new_gid.to_u32() as u16, class));
            }
        } else {
            for record in self.class_range_records() {
                let class = record.class();
                if class == 0 {
                    continue;
                }

                let start = record.start_glyph_id().to_u32();
                let end = record.end_glyph_id().to_u32().min(glyph_set.last().unwrap().to_u32());
                for g in start..=end {
                    let gid = GlyphId::from(g);
                    let Some(new_gid) = glyph_map.get(&gid) else {
                        continue;
                    };
                    if let Some(glyph_filter) = args.glyph_filter {
                        if glyph_filter.get(gid).is_none() {
                            continue;
                        }
                    }

                    retained_classes.insert(class);
                    new_gid_classes.push((new_gid.to_u32() as u16, class));
                }
            }
        }

        new_gid_classes.sort_by(|a, b| a.0.cmp(&b.0));
        let use_class_zero = if args.use_class_zero {
            let glyph_count = if let Some(glyph_filter) = args.glyph_filter {
                glyph_map.keys().filter(|&g| glyph_filter.get(*g).is_some()).count()
            } else {
                glyph_map.len()
            };
            glyph_count <= new_gid_classes.len()
        } else {
            false
        };

        if !args.keep_empty_table && new_gid_classes.is_empty() {
            return Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY);
        }

        classdef_remap_and_serialize(
            args.remap_class,
            &retained_classes,
            use_class_zero,
            &mut new_gid_classes,
            s,
        )
    }
}

fn classdef_remap_and_serialize(
    remap_class: bool,
    retained_classes: &IntSet<u16>,
    use_class_zero: bool,
    new_gid_classes: &mut [(u16, u16)],
    s: &mut Serializer,
) -> Result<Option<FnvHashMap<u16, u16>>, SerializeErrorFlags> {
    if !remap_class {
        return ClassDef::serialize(s, new_gid_classes).map(|()| None);
    }

    let mut class_map = FnvHashMap::default();
    if !use_class_zero {
        class_map.insert(0_u16, 0_u16);
    }

    let mut new_idx = if use_class_zero { 0_u16 } else { 1 };
    for class in retained_classes.iter() {
        class_map.insert(class, new_idx);
        new_idx += 1;
    }

    for (_, class) in new_gid_classes.iter_mut() {
        let Some(new_class) = class_map.get(class) else {
            return Err(SerializeErrorFlags::SERIALIZE_ERROR_OTHER);
        };
        *class = *new_class;
    }
    ClassDef::serialize(s, new_gid_classes).map(|()| Some(class_map))
}

impl<'a> Serialize<'a> for ClassDef<'a> {
    type Args = &'a [(u16, u16)];
    fn serialize(
        s: &mut Serializer,
        new_gid_classes: &[(u16, u16)],
    ) -> Result<(), SerializeErrorFlags> {
        let mut glyph_min = 0;
        let mut glyph_max = 0;
        let mut prev_g = 0;
        let mut prev_class = 0;

        let mut num_glyphs = 0_u16;
        let mut num_ranges = 1_u16;
        for (g, class) in new_gid_classes.iter().filter(|(_, class)| *class != 0) {
            num_glyphs += 1;
            if num_glyphs == 1 {
                glyph_min = *g;
                glyph_max = *g;
                prev_g = *g;
                prev_class = *class;
                continue;
            }

            glyph_max = glyph_max.max(*g);
            if *g != prev_g + 1 || *class != prev_class {
                num_ranges += 1;
            }

            prev_g = *g;
            prev_class = *class;
        }

        if num_glyphs > 0 && (glyph_max - glyph_min + 1) < num_ranges * 3 {
            ClassDefFormat1::serialize(s, new_gid_classes)
        } else {
            ClassDefFormat2::serialize(s, new_gid_classes)
        }
    }
}

impl<'a> Serialize<'a> for ClassDefFormat1<'a> {
    type Args = &'a [(u16, u16)];
    fn serialize(
        s: &mut Serializer,
        new_gid_classes: &[(u16, u16)],
    ) -> Result<(), SerializeErrorFlags> {
        // format 1
        s.embed(1_u16)?;
        // start_glyph
        let start_glyph_pos = s.embed(0_u16)?;
        // glyph count
        let glyph_count_pos = s.embed(0_u16)?;

        let mut num = 0;
        let mut glyph_min = 0;
        let mut glyph_max = 0;
        for (g, _) in new_gid_classes.iter().filter(|(_, class)| *class != 0) {
            if num == 0 {
                glyph_min = *g;
                glyph_max = *g;
            } else {
                glyph_max = *g.max(&glyph_max);
            }
            num += 1;
        }

        if num == 0 {
            return Ok(());
        }

        s.copy_assign(start_glyph_pos, glyph_min);

        let glyph_count = glyph_max - glyph_min + 1;
        s.copy_assign(glyph_count_pos, glyph_count);

        let pos = s.allocate_size((glyph_count as usize) * 2, true)?;
        for (g, class) in new_gid_classes.iter().filter(|(_, class)| *class != 0) {
            let idx = (*g - glyph_min) as usize;
            s.copy_assign(pos + idx * 2, *class);
        }
        Ok(())
    }
}

impl<'a> Serialize<'a> for ClassDefFormat2<'a> {
    type Args = &'a [(u16, u16)];
    fn serialize(
        s: &mut Serializer,
        new_gid_classes: &[(u16, u16)],
    ) -> Result<(), SerializeErrorFlags> {
        // format 2
        s.embed(2_u16)?;
        //classRange count
        let range_count_pos = s.embed(0_u16)?;

        let mut num = 0_u16;
        let mut prev_g = 0;
        let mut prev_class = 0;

        let mut num_ranges = 0_u16;
        let mut pos = 0;
        for (g, class) in new_gid_classes.iter().filter(|(_, class)| *class != 0) {
            num += 1;
            if num == 1 {
                prev_g = *g;
                prev_class = *class;

                pos = s.allocate_size(ClassRangeRecord::RAW_BYTE_LEN, true)?;
                s.copy_assign(pos, prev_g);
                s.copy_assign(pos + 2, prev_g);
                s.copy_assign(pos + 4, prev_class);

                num_ranges += 1;
                continue;
            }

            if *g != prev_g + 1 || *class != prev_class {
                num_ranges += 1;
                // update last_gid of previous record
                s.copy_assign(pos + 2, prev_g);

                pos = s.allocate_size(ClassRangeRecord::RAW_BYTE_LEN, true)?;
                s.copy_assign(pos, *g);
                s.copy_assign(pos + 2, *g);
                s.copy_assign(pos + 4, *class);
            }

            prev_class = *class;
            prev_g = *g;
        }

        if num == 0 {
            return Ok(());
        }

        // update end glyph of the last record
        s.copy_assign(pos + 2, prev_g);
        // update range count
        s.copy_assign(range_count_pos, num_ranges);
        Ok(())
    }
}

impl<'a> SubsetTable<'a> for CoverageTable<'a> {
    type ArgsForSubset = ();
    type Output = ();
    fn subset(
        &self,
        plan: &Plan,
        s: &mut Serializer,
        args: Self::ArgsForSubset,
    ) -> Result<(), SerializeErrorFlags> {
        match self {
            CoverageTable::Format1(sub) => sub.subset(plan, s, args),
            CoverageTable::Format2(sub) => sub.subset(plan, s, args),
        }
    }
}

impl<'a> SubsetTable<'a> for CoverageFormat1<'a> {
    type ArgsForSubset = ();
    type Output = ();
    fn subset(
        &self,
        plan: &Plan,
        s: &mut Serializer,
        _args: Self::ArgsForSubset,
    ) -> Result<Self::Output, SerializeErrorFlags> {
        let glyph_count = (self.glyph_count() as usize).min(plan.font_num_glyphs);
        let Some(glyph_array) = self.glyph_array().get(0..glyph_count) else {
            return Err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR);
        };

        let num_bits = 16 - (glyph_count as u16).leading_zeros() as usize;
        // if/else branches return the same result, it's just an optimization that
        // we pick the faster approach depending on the number of glyphs
        let retained_glyphs: Vec<GlyphId> =
            if glyph_count > (plan.glyphset_gsub.len() as usize) * num_bits {
                plan.glyphset_gsub
                    .iter()
                    .filter_map(|old_gid| {
                        glyph_array
                            .binary_search_by(|g| g.get().to_u32().cmp(&old_gid.to_u32()))
                            .ok()
                            .and_then(|_| plan.glyph_map_gsub.get(&old_gid))
                            .copied()
                    })
                    .collect()
            } else {
                glyph_array
                    .iter()
                    .filter_map(|g| plan.glyph_map_gsub.get(&GlyphId::from(g.get())))
                    .copied()
                    .collect()
            };

        if retained_glyphs.is_empty() {
            return Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY);
        }
        CoverageTable::serialize(s, &retained_glyphs)
    }
}

impl<'a> SubsetTable<'a> for CoverageFormat2<'a> {
    type ArgsForSubset = ();
    type Output = ();
    fn subset(
        &self,
        plan: &Plan,
        s: &mut Serializer,
        _args: Self::ArgsForSubset,
    ) -> Result<Self::Output, SerializeErrorFlags> {
        let range_count = self.range_count();
        if range_count as usize > plan.font_num_glyphs {
            return Err(s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR));
        }
        let num_bits = 16 - range_count.leading_zeros() as usize;
        // if/else branches return the same result, it's just an optimization that
        // we pick the faster approach depending on the number of glyphs
        let retained_glyphs: Vec<GlyphId> = if self.population()
            > plan.glyph_map_gsub.len() * num_bits
        {
            let range_records = self.range_records();
            plan.glyphset_gsub
                .iter()
                .filter_map(|g| {
                    range_records
                        .binary_search_by(|rec| {
                            if rec.end_glyph_id().to_u32() < g.to_u32() {
                                Ordering::Less
                            } else if rec.start_glyph_id().to_u32() > g.to_u32() {
                                Ordering::Greater
                            } else {
                                Ordering::Equal
                            }
                        })
                        .ok()
                        .and_then(|_| plan.glyph_map_gsub.get(&g))
                })
                .copied()
                .collect()
        } else {
            self.range_records()
                .iter()
                .flat_map(|r| r.iter().filter_map(|g| plan.glyph_map_gsub.get(&GlyphId::from(g))))
                .copied()
                .collect()
        };

        if retained_glyphs.is_empty() {
            return Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY);
        }

        CoverageTable::serialize(s, &retained_glyphs)
    }
}

impl<'a> Serialize<'a> for CoverageTable<'a> {
    type Args = &'a [GlyphId];
    fn serialize(s: &mut Serializer, glyphs: &[GlyphId]) -> Result<(), SerializeErrorFlags> {
        if glyphs.is_empty() {
            return CoverageFormat1::serialize(s, glyphs);
        }

        let glyph_count = glyphs.len();
        let mut num_ranges = 1_u16;
        let mut last = glyphs[0].to_u32();

        for g in glyphs.iter().skip(1) {
            let gid = g.to_u32();
            if last + 1 != gid {
                num_ranges += 1;
            }

            last = gid;
        }

        // TODO: add support for unsorted glyph list??
        // ref: <https://github.com/harfbuzz/harfbuzz/blob/59001aa9527c056ad08626cfec9a079b65d8aec8/src/OT/Layout/Common/Coverage.hh#L143>
        if glyph_count <= num_ranges as usize * 3 {
            CoverageFormat1::serialize(s, glyphs)
        } else {
            CoverageFormat2::serialize(s, (glyphs, num_ranges))
        }
    }
}

impl<'a> Serialize<'a> for CoverageFormat1<'a> {
    type Args = &'a [GlyphId];
    fn serialize(s: &mut Serializer, glyphs: &[GlyphId]) -> Result<(), SerializeErrorFlags> {
        //format
        s.embed(1_u16)?;

        // count
        let count = glyphs.len();
        s.embed(count as u16)?;

        let pos = s.allocate_size(count * 2, true)?;
        for (idx, g) in glyphs.iter().enumerate() {
            s.copy_assign(pos + idx * 2, g.to_u32() as u16);
        }
        Ok(())
    }
}

impl<'a> Serialize<'a> for CoverageFormat2<'a> {
    type Args = (&'a [GlyphId], u16);
    fn serialize(s: &mut Serializer, args: Self::Args) -> Result<(), SerializeErrorFlags> {
        let (glyphs, range_count) = args;
        //format
        s.embed(2_u16)?;

        //range_count
        s.embed(range_count)?;

        // range records
        let pos = s.allocate_size((range_count as usize) * RangeRecord::RAW_BYTE_LEN, true)?;
        let mut last = glyphs[0].to_u32() as u16;
        // copy start glyph of first record
        s.copy_assign(pos, last);
        // copy coverage index of firstr ecord
        s.copy_assign(pos + 4, 0_u16);

        let mut range = 0;
        for (idx, g) in glyphs.iter().enumerate().skip(1) {
            let g = g.to_u32() as u16;
            let range_pos = pos + range * RangeRecord::RAW_BYTE_LEN;
            if last + 1 != g {
                //end glyph
                s.copy_assign(range_pos + 2, last);
                range += 1;

                let new_range_pos = range_pos + RangeRecord::RAW_BYTE_LEN;
                // start glyph of next record
                s.copy_assign(new_range_pos, g);
                // coverage index of next record
                s.copy_assign(new_range_pos + 4, idx as u16);
            }
            last = g;
        }

        let last_range_pos = pos + range * RangeRecord::RAW_BYTE_LEN;
        // end glyph
        s.copy_assign(last_range_pos + 2, last);
        Ok(())
    }
}

/// Return glyphs and their indices in the input Coverage table that intersect
/// with the input glyph set returned glyphs are mapped into new glyph ids
pub(crate) fn intersected_glyphs_and_indices(
    coverage: &CoverageTable,
    glyph_set: &IntSet<GlyphId>,
    glyph_map: &FnvHashMap<GlyphId, GlyphId>,
) -> (Vec<GlyphId>, IntSet<u16>) {
    let count = match coverage {
        CoverageTable::Format1(t) => t.glyph_count(),
        CoverageTable::Format2(t) => t.range_count(),
    };
    let num_bits = 32 - count.leading_zeros();

    let coverage_population = coverage.population();
    let glyph_set_len = glyph_set.len();
    let cap = coverage_population.min(glyph_set_len as usize);
    let mut glyphs = Vec::with_capacity(cap);
    let mut indices = IntSet::empty();

    if coverage_population as u32 > (glyph_set_len as u32) * num_bits {
        for (idx, g) in glyph_set
            .iter()
            .filter_map(|g| coverage.get(g).map(|idx| (idx, g)))
            .filter_map(|(idx, g)| glyph_map.get(&g).map(|new_g| (idx, *new_g)))
        {
            glyphs.push(g);
            indices.insert(idx);
        }
    } else {
        for (i, g) in coverage
            .iter()
            .enumerate()
            .filter_map(|(i, g)| glyph_map.get(&GlyphId::from(g)).map(|&new_g| (i, new_g)))
        {
            glyphs.push(g);
            indices.insert(i as u16);
        }
    }
    (glyphs, indices)
}

/// Return indices of glyphs in the input Coverage table that intersect with the
/// input glyph set
pub(crate) fn intersected_coverage_indices(
    coverage: &CoverageTable,
    glyph_set: &IntSet<GlyphId>,
) -> IntSet<u16> {
    let count = match coverage {
        CoverageTable::Format1(t) => t.glyph_count(),
        CoverageTable::Format2(t) => t.range_count(),
    };
    let num_bits = 32 - count.leading_zeros();

    let coverage_population = coverage.population();
    let glyph_set_len = glyph_set.len();

    if coverage_population as u32 > (glyph_set_len as u32) * num_bits {
        glyph_set.iter().filter_map(|g| coverage.get(g)).collect()
    } else {
        coverage
            .iter()
            .enumerate()
            .filter_map(|(i, g)| glyph_set.contains(GlyphId::from(g)).then_some(i as u16))
            .collect()
    }
}

/// Return a set of feature indices that have alternate features defined in
/// FeatureVariations table and the alternate version(s) intersect the set of
/// lookup indices
pub(crate) fn collect_features_with_retained_subs(
    feature_variations: &FeatureVariations,
    lookup_indices: &IntSet<u16>,
) -> IntSet<u16> {
    let font_data = feature_variations.offset_data();
    let mut out = IntSet::empty();
    for subs in feature_variations
        .feature_variation_records()
        .iter()
        .filter_map(|rec| rec.feature_table_substitution(font_data))
    {
        let Ok(subs) = subs else {
            return IntSet::empty();
        };

        for rec in subs.substitutions() {
            if rec.alternate_feature_offset().is_null() {
                continue;
            }
            let Ok(sub_f) = rec.alternate_feature(subs.offset_data()) else {
                return IntSet::empty();
            };
            if !feature_intersects_lookups(&sub_f, lookup_indices) {
                continue;
            }
            out.insert(rec.feature_index());
        }
    }
    out
}

fn feature_intersects_lookups(f: &Feature, lookup_indices: &IntSet<u16>) -> bool {
    f.lookup_list_indices().iter().any(|i| lookup_indices.contains(i.get()))
}

pub(crate) fn prune_features(
    feature_list: &FeatureList,
    alternate_features: &IntSet<u16>,
    lookup_indices: &IntSet<u16>,
    feature_indices: IntSet<u16>,
) -> IntSet<u16> {
    let mut out = IntSet::empty();
    let feature_records = feature_list.feature_records();
    for i in feature_indices.iter() {
        let Some(feature_rec) = feature_records.get(i as usize) else {
            continue;
        };
        if feature_rec.feature_offset().is_null() {
            continue;
        }
        let feature_tag = feature_rec.feature_tag();
        // never drop feature "pref"
        // ref: https://github.com/harfbuzz/harfbuzz/blob/fc6231726e514f96bfbb098283aab332fc6b45fb/src/hb-ot-layout-gsubgpos.hh#L4822
        if feature_tag == Tag::new(b"pref") {
            out.insert(i);
            continue;
        }

        let Ok(feature) = feature_rec.feature(feature_list.offset_data()) else {
            return out;
        };
        // always keep "size" feature even if it's empty
        // ref: https://github.com/fonttools/fonttools/blob/e857fe5ef7b25e92fd829a445357e45cde16eb04/Lib/fontTools/subset/__init__.py#L1627
        if !feature.feature_params_offset().is_null() && feature_tag == Tag::new(b"size") {
            out.insert(i);
            continue;
        }

        if !feature_intersects_lookups(&feature, lookup_indices) && !alternate_features.contains(i)
        {
            continue;
        }
        out.insert(i);
    }
    out
}

pub(crate) fn find_duplicate_features(
    feature_list: &FeatureList,
    lookup_indices: &IntSet<u16>,
    feature_indices: IntSet<u16>,
) -> FnvHashMap<u16, u16> {
    let mut out = FnvHashMap::default();
    if feature_indices.is_empty() {
        return out;
    }

    let feature_recs = feature_list.feature_records();
    let mut unique_features = FnvHashMap::default();
    for i in feature_indices.iter() {
        let Some(rec) = feature_recs.get(i as usize) else {
            continue;
        };

        let Ok(f) = rec.feature(feature_list.offset_data()) else {
            return out;
        };

        let t = u32::from_be_bytes(rec.feature_tag().to_be_bytes());

        let same_tag_features = unique_features.entry(t).or_insert(IntSet::empty());
        if same_tag_features.is_empty() {
            same_tag_features.insert(i);
            out.insert(i, i);
            continue;
        }

        for other_f_idx in same_tag_features.iter() {
            let Some(other_rec) = feature_recs.get(other_f_idx as usize) else {
                continue;
            };

            let Ok(other_f) = other_rec.feature(feature_list.offset_data()) else {
                return out;
            };

            let f_iter = f
                .lookup_list_indices()
                .iter()
                .filter_map(|i| lookup_indices.contains(i.get()).then_some(i.get()));
            let other_f_iter = other_f
                .lookup_list_indices()
                .iter()
                .filter_map(|i| lookup_indices.contains(i.get()).then_some(i.get()));
            if !f_iter.eq(other_f_iter) {
                continue;
            } else {
                out.insert(i, other_f_idx);
                break;
            }
        }

        let o = out.entry(i).or_insert(i);
        // no duplicate for this index
        if *o == i {
            same_tag_features.insert(i);
        }
    }
    out
}

pub(crate) struct PruneLangSysContext<'a> {
    script_count: u16,
    langsys_feature_count: u16,
    // IN: retained feature indices map:
    // duplicate features will be mapped to the same value
    feature_index_map: &'a FnvHashMap<u16, u16>,
    // OUT: retained feature indices after pruning
    feature_indices: IntSet<u16>,
    // OUT: retained script->langsys map after pruning
    script_langsys_map: FnvHashMap<u16, IntSet<u16>>,
}

impl<'a> PruneLangSysContext<'a> {
    pub(crate) fn new(feature_index_map: &'a FnvHashMap<u16, u16>) -> Self {
        Self {
            script_count: 0,
            langsys_feature_count: 0,
            feature_index_map,
            feature_indices: IntSet::empty(),
            script_langsys_map: FnvHashMap::default(),
        }
    }

    fn visit_script(&mut self) -> bool {
        let ret = self.script_count < MAX_SCRIPTS;
        self.script_count += 1;
        ret
    }

    fn visit_langsys(&mut self, feature_count: u16) -> bool {
        self.langsys_feature_count += feature_count;
        self.langsys_feature_count < MAX_LANGSYS_FEATURE_COUNT
    }

    fn collect_langsys_features(&mut self, langsys: &LangSys) {
        let required_feature_index = langsys.required_feature_index();
        if required_feature_index == 0xFFFF_u16 && langsys.feature_index_count() == 0 {
            return;
        }

        if required_feature_index != 0xFFFF_u16
            && self.feature_index_map.contains_key(&required_feature_index)
        {
            self.feature_indices.insert(required_feature_index);
        }

        self.feature_indices.extend_unsorted(
            langsys
                .feature_indices()
                .iter()
                .filter_map(|i| self.feature_index_map.contains_key(&i.get()).then_some(i.get())),
        );
    }

    fn check_equal(&self, la: &LangSys, lb: &LangSys) -> bool {
        if la.required_feature_index() != lb.required_feature_index() {
            return false;
        }

        let iter_a =
            la.feature_indices().iter().filter_map(|i| self.feature_index_map.get(&i.get()));
        let iter_b =
            lb.feature_indices().iter().filter_map(|i| self.feature_index_map.get(&i.get()));

        iter_a.eq(iter_b)
    }

    fn add_script_langsys(&mut self, script_index: u16, langsys_index: u16) {
        let langsys_indices =
            self.script_langsys_map.entry(script_index).or_insert(IntSet::empty());
        langsys_indices.insert(langsys_index);
    }

    pub(crate) fn prune_script_langsys(&mut self, script_index: u16, script: &Script) {
        if script.lang_sys_count() == 0 && script.default_lang_sys_offset().is_null() {
            return;
        }

        if !self.visit_script() {
            return;
        }

        if let Some(Ok(default_langsys)) = script.default_lang_sys() {
            if self.visit_langsys(default_langsys.feature_index_count()) {
                self.collect_langsys_features(&default_langsys);
            }

            for (i, langsys_rec) in script.lang_sys_records().iter().enumerate() {
                if langsys_rec.lang_sys_offset().is_null() {
                    continue;
                }
                let Ok(l) = langsys_rec.lang_sys(script.offset_data()) else {
                    return;
                };
                if !self.visit_langsys(l.feature_index_count()) {
                    return;
                }

                if self.check_equal(&l, &default_langsys) {
                    continue;
                }
                self.collect_langsys_features(&l);
                self.add_script_langsys(script_index, i as u16);
            }
        } else {
            for (i, langsys_rec) in script.lang_sys_records().iter().enumerate() {
                if langsys_rec.lang_sys_offset().is_null() {
                    continue;
                }
                let Ok(l) = langsys_rec.lang_sys(script.offset_data()) else {
                    return;
                };
                if !self.visit_langsys(l.feature_index_count()) {
                    return;
                }
                self.collect_langsys_features(&l);
                self.add_script_langsys(script_index, i as u16);
            }
        }
    }

    pub(crate) fn script_langsys_map(&mut self) -> FnvHashMap<u16, IntSet<u16>> {
        mem::take(&mut self.script_langsys_map)
    }

    pub(crate) fn feature_indices(&mut self) -> IntSet<u16> {
        mem::take(&mut self.feature_indices)
    }

    pub(crate) fn prune_langsys(
        &mut self,
        script_list: &ScriptList,
        layout_scripts: &IntSet<Tag>,
    ) -> (FnvHashMap<u16, IntSet<u16>>, IntSet<u16>) {
        for (i, script_rec) in script_list.script_records().iter().enumerate() {
            if script_rec.script_offset().is_null() {
                continue;
            }
            let script_tag = script_rec.script_tag();
            if !layout_scripts.contains(script_tag) {
                continue;
            }

            let Ok(script) = script_rec.script(script_list.offset_data()) else {
                return (self.script_langsys_map(), self.feature_indices());
            };
            self.prune_script_langsys(i as u16, &script);
        }
        (self.script_langsys_map(), self.feature_indices())
    }
}

// remap feature indices: old-> new
// mapping contains unique old feature indices -> new indices mapping only, used
// by FeatureList subsetting mapping_w_duplicate contains all retained feature
// indices in ScriptList/FeatureVariations subsetting
pub(crate) fn remap_feature_indices(
    feature_indices: &IntSet<u16>,
    duplicate_feature_map: &FnvHashMap<u16, u16>,
) -> (FnvHashMap<u16, u16>, FnvHashMap<u16, u16>) {
    let mut mapping = FnvHashMap::default();
    let mut mapping_w_duplicates = FnvHashMap::default();
    let mut i = 0_u16;
    for f_idx in feature_indices.iter() {
        let unique_f_idx = duplicate_feature_map.get(&f_idx).unwrap_or(&f_idx);
        if let Some(new_idx) = mapping.get(unique_f_idx) {
            mapping_w_duplicates.insert(f_idx, *new_idx);
        } else {
            mapping.insert(f_idx, i);
            mapping_w_duplicates.insert(f_idx, i);
            i += 1;
        }
    }
    (mapping, mapping_w_duplicates)
}

pub(crate) struct SubsetLayoutContext {
    script_count: u16,
    langsys_count: u16,
    feature_index_count: u16,
    lookup_count: u16,
    table_tag: Tag,
}

impl SubsetLayoutContext {
    pub(crate) fn new(table_tag: Tag) -> Self {
        Self {
            script_count: 0,
            langsys_count: 0,
            feature_index_count: 0,
            lookup_count: 0,
            table_tag,
        }
    }

    fn visit_script(&mut self) -> bool {
        if self.script_count >= MAX_SCRIPTS {
            return false;
        }
        self.script_count += 1;
        true
    }

    fn visit_langsys(&mut self) -> bool {
        if self.langsys_count >= MAX_LANGSYS {
            return false;
        }
        self.langsys_count += 1;
        true
    }

    fn visit_feature_index(&mut self, count: u16) -> bool {
        let Some(sum) = self.feature_index_count.checked_add(count) else {
            return false;
        };
        self.feature_index_count = sum;
        self.feature_index_count < MAX_FEATURE_INDICES
    }

    fn visit_lookup(&mut self) -> bool {
        if self.lookup_count >= MAX_LOOKUP_VISIT_COUNT {
            return false;
        }
        self.lookup_count += 1;
        true
    }
}

impl<'a> SubsetTable<'a> for ScriptList<'_> {
    type ArgsForSubset = &'a mut SubsetLayoutContext;
    type Output = ();
    fn subset(
        &self,
        plan: &Plan,
        s: &mut Serializer,
        c: &mut SubsetLayoutContext,
    ) -> Result<(), SerializeErrorFlags> {
        let script_count_pos = s.embed(0_u16)?;
        let mut num_records = 0_u16;
        let font_data = self.offset_data();
        for (i, script_record) in self.script_records().iter().enumerate() {
            if script_record.script_offset().is_null() {
                continue;
            }
            let tag = script_record.script_tag();
            if !plan.layout_scripts.contains(tag) {
                continue;
            }

            if !c.visit_script() {
                break;
            }

            let snap = s.snapshot();
            match script_record.subset(plan, s, (c, font_data, i)) {
                Ok(()) => num_records += 1,
                Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY) => s.revert_snapshot(snap),
                Err(e) => return Err(e),
            }
        }
        if num_records != 0 {
            s.copy_assign(script_count_pos, num_records);
        }
        Ok(())
    }
}

impl<'a> SubsetTable<'a> for ScriptRecord {
    type ArgsForSubset = (&'a mut SubsetLayoutContext, FontData<'a>, usize);
    type Output = ();
    fn subset(
        &self,
        plan: &Plan,
        s: &mut Serializer,
        args: Self::ArgsForSubset,
    ) -> Result<(), SerializeErrorFlags> {
        let tag = self.script_tag();
        s.embed(tag)?;
        let script_offset_pos = s.embed(0_u16)?;

        let (c, font_data, script_index) = args;
        let Ok(script) = self.script(font_data) else {
            return Err(s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR));
        };

        Offset16::serialize_subset(&script, s, plan, (c, script_index, tag), script_offset_pos)
    }
}

impl<'a> SubsetTable<'a> for Script<'_> {
    type ArgsForSubset = (&'a mut SubsetLayoutContext, usize, Tag);
    type Output = ();
    fn subset(
        &self,
        plan: &Plan,
        s: &mut Serializer,
        args: Self::ArgsForSubset,
    ) -> Result<(), SerializeErrorFlags> {
        let default_langsys_offset_pos = s.embed(0_u16)?;
        let langsys_count_pos = s.embed(0_u16)?;
        let mut langsys_count = 0_u16;

        let (c, script_index, script_tag) = args;
        let has_default_langsys = if let Some(default_langsys) = self
            .default_lang_sys()
            .transpose()
            .map_err(|_| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR))?
        {
            s.push()?;
            let ret = default_langsys.subset(plan, s, c);
            if s.in_error() {
                return Err(s.error());
            }

            // harfbuzz ref: <https://github.com/harfbuzz/harfbuzz/blob/567a0307fa65db03d51a3bcf19d995e57ffc1d24/src/hb-ot-layout-common.hh#L1200>
            // If there is a DFLT script table, it must have a default language system table
            if ret.is_err() && script_tag != Tag::new(b"DFLT") {
                s.pop_discard();
                false
            } else {
                let Some(obj_idx) = s.pop_pack(true) else {
                    return Err(s.error());
                };
                s.add_link(
                    default_langsys_offset_pos..default_langsys_offset_pos + Offset16::RAW_BYTE_LEN,
                    obj_idx,
                    OffsetWhence::Head,
                    0,
                    false,
                )?;
                true
            }
        } else {
            false
        };

        let script_langsys_map = if c.table_tag == Gsub::TAG {
            &plan.gsub_script_langsys
        } else {
            &plan.gpos_script_langsys
        };

        if let Some(retained_langsys_idxes) = script_langsys_map.get(&(script_index as u16)) {
            let langsys_records = self.lang_sys_records();
            for i in retained_langsys_idxes.iter() {
                let Some(langsys_rec) = langsys_records.get(i as usize) else {
                    continue;
                };

                if !c.visit_langsys() {
                    break;
                }

                let snap = s.snapshot();
                match langsys_rec.subset(plan, s, (c, self.offset_data())) {
                    Ok(()) => langsys_count += 1,
                    Err(e) => {
                        if s.in_error() {
                            return Err(e);
                        }
                        s.revert_snapshot(snap);
                        continue;
                    }
                };
            }
        }

        if has_default_langsys || langsys_count != 0 || c.table_tag == Gsub::TAG {
            s.copy_assign(langsys_count_pos, langsys_count);
            Ok(())
        } else {
            Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY)
        }
    }
}

impl<'a> SubsetTable<'a> for LangSysRecord {
    type ArgsForSubset = (&'a mut SubsetLayoutContext, FontData<'a>);
    type Output = ();
    fn subset(
        &self,
        plan: &Plan,
        s: &mut Serializer,
        args: Self::ArgsForSubset,
    ) -> Result<Self::Output, SerializeErrorFlags> {
        let tag = self.lang_sys_tag();
        s.embed(tag)?;
        let langsys_offset_pos = s.embed(0_u16)?;

        let (c, font_data) = args;
        let Ok(langsys) = self.lang_sys(font_data) else {
            return Err(s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR));
        };

        Offset16::serialize_subset(&langsys, s, plan, c, langsys_offset_pos)
    }
}

impl<'a> SubsetTable<'a> for LangSys<'a> {
    type ArgsForSubset = &'a mut SubsetLayoutContext;
    type Output = ();
    fn subset(
        &self,
        plan: &Plan,
        s: &mut Serializer,
        c: &mut SubsetLayoutContext,
    ) -> Result<Self::Output, SerializeErrorFlags> {
        // reserved field
        s.embed(0_u16)?;

        let feature_index_map = if c.table_tag == Gsub::TAG {
            &plan.gsub_features_w_duplicates
        } else {
            &plan.gpos_features_w_duplicates
        };
        // required feature index
        let required_feature_idx = self.required_feature_index();
        let new_required_idx = *feature_index_map.get(&required_feature_idx).unwrap_or(&0xFFFF_u16);
        s.embed(new_required_idx)?;

        let mut index_count = 0_u16;
        let index_count_pos = s.embed(index_count)?;

        if !c.visit_feature_index(self.feature_index_count()) {
            return Err(SerializeErrorFlags::SERIALIZE_ERROR_OTHER);
        }
        for new_idx in self.feature_indices().iter().filter_map(|i| feature_index_map.get(&i.get()))
        {
            s.embed(*new_idx)?;
            index_count += 1;
        }

        if index_count == 0 && new_required_idx == 0xFFFF {
            return Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY);
        }
        s.copy_assign(index_count_pos, index_count);
        Ok(())
    }
}

impl<'a> SubsetTable<'a> for FeatureList<'_> {
    type ArgsForSubset = &'a mut SubsetLayoutContext;
    type Output = ();
    fn subset(
        &self,
        plan: &Plan,
        s: &mut Serializer,
        c: &mut SubsetLayoutContext,
    ) -> Result<(), SerializeErrorFlags> {
        let feature_count_pos = s.embed(0_u16)?;
        let mut num_records = 0_u16;
        let font_data = self.offset_data();
        let feature_index_map =
            if c.table_tag == Gsub::TAG { &plan.gsub_features } else { &plan.gpos_features };
        for (_, feature_record) in self
            .feature_records()
            .iter()
            .enumerate()
            .filter(|&(i, _)| feature_index_map.contains_key(&(i as u16)))
        {
            feature_record.subset(plan, s, (c, font_data))?;
            num_records += 1;
        }
        if num_records != 0 {
            s.copy_assign(feature_count_pos, num_records);
        }
        Ok(())
    }
}

impl<'a> SubsetTable<'a> for FeatureRecord {
    type ArgsForSubset = (&'a mut SubsetLayoutContext, FontData<'a>);
    type Output = ();
    fn subset(
        &self,
        plan: &Plan,
        s: &mut Serializer,
        args: Self::ArgsForSubset,
    ) -> Result<(), SerializeErrorFlags> {
        let tag = self.feature_tag();
        s.embed(tag)?;
        let feature_offset_pos = s.embed(0_u16)?;

        let (c, font_data) = args;
        let Ok(feature) = self.feature(font_data) else {
            return Err(s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR));
        };

        Offset16::serialize_subset(&feature, s, plan, c, feature_offset_pos)
    }
}

impl<'a> SubsetTable<'a> for Feature<'_> {
    type ArgsForSubset = &'a mut SubsetLayoutContext;
    type Output = ();
    fn subset(
        &self,
        plan: &Plan,
        s: &mut Serializer,
        c: &mut SubsetLayoutContext,
    ) -> Result<(), SerializeErrorFlags> {
        //FeatureParams offset
        let feature_params_offset_pos = s.embed(0_u16)?;
        let lookup_count_pos = s.embed(0_u16)?;
        let mut lookup_count = 0_u16;
        let lookup_index_map =
            if c.table_tag == Gsub::TAG { &plan.gsub_lookups } else { &plan.gpos_lookups };

        for idx in self.lookup_list_indices().iter().filter_map(|i| lookup_index_map.get(&i.get()))
        {
            if !c.visit_lookup() {
                break;
            }
            s.embed(*idx)?;
            lookup_count += 1;
        }

        if lookup_count != 0 {
            s.copy_assign(lookup_count_pos, lookup_count);
        }

        if let Some(feature_params) = self
            .feature_params()
            .transpose()
            .map_err(|_| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR))?
        {
            Offset16::serialize_subset(&feature_params, s, plan, (), feature_params_offset_pos)?;
        }
        Ok(())
    }
}

impl SubsetTable<'_> for FeatureParams<'_> {
    type ArgsForSubset = ();
    type Output = ();
    fn subset(
        &self,
        _plan: &Plan,
        s: &mut Serializer,
        _args: (),
    ) -> Result<(), SerializeErrorFlags> {
        let ret = match self {
            FeatureParams::StylisticSet(table) => s.embed_bytes(table.min_table_bytes()),
            FeatureParams::Size(table) => s.embed_bytes(table.min_table_bytes()),
            FeatureParams::CharacterVariant(table) => s.embed_bytes(table.min_table_bytes()),
        };
        ret.map(|_| ())
    }
}

impl<
        'a,
        T: FontRead<'a>
            + SubsetTable<
                'a,
                ArgsForSubset = (&'a SubsetState, &'a FontRef<'a>, &'a FnvHashMap<u16, u16>),
            >,
    > SubsetTable<'a> for LookupList<'a, T>
{
    type ArgsForSubset = (&'a SubsetState, &'a FontRef<'a>, &'a FnvHashMap<u16, u16>);
    type Output = ();
    fn subset(
        &self,
        plan: &Plan,
        s: &mut Serializer,
        args: Self::ArgsForSubset,
    ) -> Result<(), SerializeErrorFlags> {
        let lookup_index_map = args.2;
        let lookup_count = lookup_index_map.len() as u16;
        s.embed(lookup_count)?;

        let lookup_offsets = self.lookups();
        for i in (0..self.lookup_count()).filter(|idx| lookup_index_map.contains_key(idx)) {
            lookup_offsets.subset_offset(i as usize, s, plan, args)?;
        }
        Ok(())
    }
}

impl<'a> SubsetTable<'a> for FeatureVariations<'_> {
    type ArgsForSubset = &'a mut SubsetLayoutContext;
    type Output = ();
    fn subset(
        &self,
        plan: &Plan,
        s: &mut Serializer,
        c: &mut SubsetLayoutContext,
    ) -> Result<(), SerializeErrorFlags> {
        let feature_index_map = if c.table_tag == Gsub::TAG {
            &plan.gsub_features_w_duplicates
        } else {
            &plan.gpos_features_w_duplicates
        };
        let num_retained_records = num_variation_record_to_retain(self, feature_index_map, s)?;
        if num_retained_records == 0 {
            return Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY);
        }

        s.embed(self.version())?;
        s.embed(num_retained_records)?;

        let font_data = self.offset_data();

        let variation_records = self.feature_variation_records();
        for i in 0..num_retained_records {
            variation_records[i as usize].subset(plan, s, (font_data, feature_index_map, c))?;
        }
        Ok(())
    }
}

// Prune empty records at the end only
// ref: <https://github.com/fonttools/fonttools/blob/3c1822544d608f87c41fc8fb9ba41ea129257aa8/Lib/fontTools/subset/__init__.py#L1782>
fn num_variation_record_to_retain(
    feature_variations: &FeatureVariations,
    feature_index_map: &FnvHashMap<u16, u16>,
    s: &mut Serializer,
) -> Result<u32, SerializeErrorFlags> {
    let num_records = feature_variations.feature_variation_record_count();
    let variation_records = feature_variations.feature_variation_records();
    let font_data = feature_variations.offset_data();

    for i in (0..num_records).rev() {
        let Some(feature_substitution) = variation_records[i as usize]
            .feature_table_substitution(font_data)
            .transpose()
            .map_err(|_| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR))?
        else {
            continue;
        };

        if feature_substitution
            .substitutions()
            .iter()
            .any(|subs| feature_index_map.contains_key(&subs.feature_index()))
        {
            return Ok(i + 1);
        }
    }
    Ok(0)
}

impl<'a> SubsetTable<'a> for FeatureVariationRecord {
    type ArgsForSubset = (FontData<'a>, &'a FnvHashMap<u16, u16>, &'a mut SubsetLayoutContext);
    type Output = ();
    fn subset(
        &self,
        plan: &Plan,
        s: &mut Serializer,
        args: Self::ArgsForSubset,
    ) -> Result<Self::Output, SerializeErrorFlags> {
        let (font_data, feature_index_map, c) = args;
        let condition_set_offset_pos = s.embed(0_u32)?;
        if let Some(condition_set) = self
            .condition_set(font_data)
            .transpose()
            .map_err(|_| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR))?
        {
            Offset32::serialize_subset(&condition_set, s, plan, (), condition_set_offset_pos)?;
        }

        let feature_substitutions_offset_pos = s.embed(0_u32)?;
        if let Some(feature_subs) = self
            .feature_table_substitution(font_data)
            .transpose()
            .map_err(|_| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR))?
        {
            Offset32::serialize_subset(
                &feature_subs,
                s,
                plan,
                (feature_index_map, c),
                feature_substitutions_offset_pos,
            )?;
        }

        Ok(())
    }
}

impl SubsetTable<'_> for ConditionSet<'_> {
    type ArgsForSubset = ();
    type Output = ();
    fn subset(
        &self,
        plan: &Plan,
        s: &mut Serializer,
        _args: Self::ArgsForSubset,
    ) -> Result<Self::Output, SerializeErrorFlags> {
        let count_pos = s.embed(0_u16)?;
        let mut count = 0_u16;

        let conditions = self.conditions();
        let condition_count = self.condition_count() as usize;
        for i in 0..condition_count {
            match conditions.subset_offset(i, s, plan, ()) {
                Ok(()) => count += 1,
                Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY) => continue,
                Err(e) => return Err(e),
            }
        }

        if count != 0 {
            s.copy_assign(count_pos, count);
        }
        Ok(())
    }
}

impl SubsetTable<'_> for Condition<'_> {
    type ArgsForSubset = ();
    type Output = ();
    fn subset(
        &self,
        plan: &Plan,
        s: &mut Serializer,
        _args: Self::ArgsForSubset,
    ) -> Result<Self::Output, SerializeErrorFlags> {
        match self {
            Self::Format1AxisRange(item) => item.subset(plan, s, ()),
            // TODO: support other formats
            _ => Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY),
        }
    }
}

impl SubsetTable<'_> for ConditionFormat1<'_> {
    type ArgsForSubset = ();
    type Output = ();
    fn subset(
        &self,
        _plan: &Plan,
        s: &mut Serializer,
        _args: Self::ArgsForSubset,
    ) -> Result<Self::Output, SerializeErrorFlags> {
        s.embed_bytes(self.min_table_bytes()).map(|_| ())
    }
}

impl<'a> SubsetTable<'a> for FeatureTableSubstitution<'_> {
    type ArgsForSubset = (&'a FnvHashMap<u16, u16>, &'a mut SubsetLayoutContext);
    type Output = ();
    fn subset(
        &self,
        plan: &Plan,
        s: &mut Serializer,
        args: Self::ArgsForSubset,
    ) -> Result<Self::Output, SerializeErrorFlags> {
        s.embed(self.version())?;

        // substitution count
        let subs_count_pos = s.embed(0_u16)?;
        let mut subs_count = 0_u16;

        let (feature_index_map, c) = args;
        let font_data = self.offset_data();
        for sub in self.substitutions() {
            match sub.subset(plan, s, (feature_index_map, c, font_data)) {
                Ok(()) => subs_count += 1,
                Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY) => continue,
                Err(e) => return Err(e),
            }
        }

        if subs_count != 0 {
            s.copy_assign(subs_count_pos, subs_count);
        }
        Ok(())
    }
}

impl<'a> SubsetTable<'a> for FeatureTableSubstitutionRecord {
    type ArgsForSubset = (&'a FnvHashMap<u16, u16>, &'a mut SubsetLayoutContext, FontData<'a>);
    type Output = ();
    fn subset(
        &self,
        plan: &Plan,
        s: &mut Serializer,
        args: Self::ArgsForSubset,
    ) -> Result<Self::Output, SerializeErrorFlags> {
        if self.alternate_feature_offset().is_null() {
            return Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY);
        }
        let (feature_index_map, c, font_data) = args;
        let Some(new_feature_indx) = feature_index_map.get(&self.feature_index()) else {
            return Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY);
        };

        let alternate_feature = self
            .alternate_feature(font_data)
            .map_err(|_| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR))?;
        s.embed(*new_feature_indx)?;

        let feature_offset_pos = s.embed(0_u32)?;
        Offset32::serialize_subset(&alternate_feature, s, plan, c, feature_offset_pos)
    }
}

impl<'a, T> SubsetTable<'a> for ArrayOfOffsets<'a, T, Offset16>
where
    T: SubsetTable<
            'a,
            ArgsForSubset = (&'a SubsetState, &'a FontRef<'a>, &'a FnvHashMap<u16, u16>),
        > + Intersect
        + FontRead<'a>
        + 'a,
{
    type ArgsForSubset = T::ArgsForSubset;
    type Output = u16;
    fn subset(
        &self,
        plan: &Plan,
        s: &mut Serializer,
        args: Self::ArgsForSubset,
    ) -> Result<Self::Output, SerializeErrorFlags> {
        let mut count = 0_u16;
        for sub in self.iter().filter_map(|table| match table {
            Err(ReadError::NullOffset) => None,
            other => Some(other),
        }) {
            let sub =
                sub.map_err(|_| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR))?;
            if !sub
                .intersects(&plan.glyphset_gsub)
                .map_err(|_| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR))?
            {
                continue;
            }
            let snap = s.snapshot();
            let offset_pos = s.embed(0_u16)?;
            match Offset16::serialize_subset(&sub, s, plan, args, offset_pos) {
                Ok(_) => count += 1,
                Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY) => s.revert_snapshot(snap),
                Err(e) => {
                    s.revert_snapshot(snap);
                    return Err(e);
                }
            }
        }
        Ok(count)
    }
}

#[cfg(test)]
mod test {
    use super::*;
    use write_fonts::{
        read::{
            tables::gpos::{PairPos, PositionSubtables},
            FontRef, TableProvider,
        },
        types::GlyphId,
    };

    #[test]
    fn test_subset_gpos_classdefs() {
        let font = FontRef::new(include_bytes!("../test-data/fonts/AdobeVFPrototype.otf")).unwrap();
        let gpos = font.gpos().unwrap();
        let gpos_lookup = gpos.lookup_list().unwrap().lookups().get(0).unwrap();

        let Ok(PositionSubtables::Pair(subtables)) = gpos_lookup.subtables() else {
            panic!("invalid lookup!")
        };
        let Ok(PairPos::Format2(pair_pos2)) = subtables.get(1) else { panic!("invalid lookup!") };

        let class_def1 = pair_pos2.class_def1().unwrap();
        let class_def2 = pair_pos2.class_def2().unwrap();
        let coverage = pair_pos2.coverage().unwrap();

        let mut plan = Plan::default();
        plan.glyphset_gsub.insert(GlyphId::NOTDEF);
        plan.glyphset_gsub.insert(GlyphId::from(34_u32));
        plan.glyphset_gsub.insert(GlyphId::from(35_u32));

        plan.glyph_map_gsub.insert(GlyphId::NOTDEF, GlyphId::NOTDEF);
        plan.glyph_map_gsub.insert(GlyphId::from(34_u32), GlyphId::from(1_u32));
        plan.glyph_map_gsub.insert(GlyphId::from(35_u32), GlyphId::from(2_u32));

        let mut s = Serializer::new(1024);
        assert_eq!(s.start_serialize(), Ok(()));
        //test ClassDef1: remap_class: true, keep_empty_table: true, use_class_zero:
        // true, use glyph_filter:Some(&Coverage)
        let ret = class_def1.subset(
            &plan,
            &mut s,
            &ClassDefSubsetStruct {
                remap_class: true,
                keep_empty_table: true,
                use_class_zero: true,
                glyph_filter: Some(&coverage),
            },
        );
        assert!(ret.is_ok());
        assert!(!s.in_error());
        s.end_serialize();

        let subsetted_data = s.copy_bytes();
        let expected_bytes: [u8; 8] = [0x00, 0x01, 0x00, 0x02, 0x00, 0x01, 0x00, 0x01];
        assert_eq!(subsetted_data, expected_bytes);

        let ret_hashmap = ret.unwrap().unwrap();
        assert_eq!(ret_hashmap.len(), 2);
        assert_eq!(ret_hashmap.get(&2), Some(&0));
        assert_eq!(ret_hashmap.get(&44), Some(&1));

        // test subset ClassDef2:
        // remap_class: true, keep_empty_table: true, use_class_zero: false, no
        // glyph_filter: None
        let mut s = Serializer::new(1024);
        assert_eq!(s.start_serialize(), Ok(()));
        let ret = class_def2.subset(
            &plan,
            &mut s,
            &ClassDefSubsetStruct {
                remap_class: true,
                keep_empty_table: true,
                use_class_zero: false,
                glyph_filter: None,
            },
        );
        assert!(ret.is_ok());
        assert!(!s.in_error());
        s.end_serialize();

        let subsetted_data = s.copy_bytes();
        let expected_bytes: [u8; 10] = [0x00, 0x01, 0x00, 0x01, 0x00, 0x02, 0x00, 0x02, 0x00, 0x01];
        assert_eq!(subsetted_data, expected_bytes);

        let ret_hashmap = ret.unwrap().unwrap();
        assert_eq!(ret_hashmap.len(), 3);
        assert_eq!(ret_hashmap.get(&0), Some(&0));
        assert_eq!(ret_hashmap.get(&1), Some(&1));
        assert_eq!(ret_hashmap.get(&5), Some(&2));
    }

    #[test]
    fn test_subset_gdef_glyph_classdef() {
        let font =
            FontRef::new(include_bytes!("../test-data/fonts/IndicTestHowrah-Regular.ttf")).unwrap();
        let gdef = font.gdef().unwrap();
        let glyph_class_def = gdef.glyph_class_def().unwrap().unwrap();

        let mut plan = Plan::default();
        plan.glyphset_gsub.insert(GlyphId::NOTDEF);
        plan.glyphset_gsub.insert(GlyphId::from(68_u32));

        plan.glyph_map_gsub.insert(GlyphId::NOTDEF, GlyphId::NOTDEF);
        plan.glyph_map_gsub.insert(GlyphId::from(68_u32), GlyphId::from(1_u32));

        let mut s = Serializer::new(1024);
        assert_eq!(s.start_serialize(), Ok(()));
        //remap_class: false, keep_empty_table: false, use_class_zero: true, no
        // glyph_filter:None
        let ret = glyph_class_def.subset(
            &plan,
            &mut s,
            &ClassDefSubsetStruct {
                remap_class: false,
                keep_empty_table: false,
                use_class_zero: true,
                glyph_filter: None,
            },
        );
        assert!(ret.is_ok());
        assert!(ret.unwrap().is_none());

        assert!(!s.in_error());
        s.end_serialize();

        let subsetted_data = s.copy_bytes();
        let expected_bytes: [u8; 8] = [0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01];
        assert_eq!(subsetted_data, expected_bytes);
    }

    #[test]
    fn test_subset_coverage_format2_to_format1() {
        let font =
            FontRef::new(include_bytes!("../test-data/fonts/IndicTestHowrah-Regular.ttf")).unwrap();
        let gdef = font.gdef().unwrap();
        let attach_list = gdef.attach_list().unwrap().unwrap();
        let coverage = attach_list.coverage().unwrap();

        let mut plan = Plan { font_num_glyphs: 611, ..Default::default() };
        plan.glyphset_gsub.insert(GlyphId::NOTDEF);
        plan.glyphset_gsub.insert(GlyphId::from(68_u32));

        plan.glyph_map_gsub.insert(GlyphId::NOTDEF, GlyphId::NOTDEF);
        plan.glyph_map_gsub.insert(GlyphId::from(68_u32), GlyphId::from(3_u32));

        let mut s = Serializer::new(1024);
        assert_eq!(s.start_serialize(), Ok(()));
        let ret = coverage.subset(&plan, &mut s, ());
        assert!(ret.is_ok());

        assert!(!s.in_error());
        s.end_serialize();

        let subsetted_data = s.copy_bytes();
        let expected_bytes: [u8; 6] = [0x00, 0x01, 0x00, 0x01, 0x00, 0x03];
        assert_eq!(subsetted_data, expected_bytes);
    }

    #[test]
    fn test_subset_coverage_format1() {
        let font = FontRef::new(include_bytes!("../test-data/fonts/AdobeVFPrototype.otf")).unwrap();
        let gpos = font.gpos().unwrap();
        let gpos_lookup = gpos.lookup_list().unwrap().lookups().get(0).unwrap();

        let Ok(PositionSubtables::Pair(subtables)) = gpos_lookup.subtables() else {
            panic!("invalid lookup!")
        };
        let Ok(PairPos::Format1(pair_pos1)) = subtables.get(0) else { panic!("invalid lookup!") };

        let coverage = pair_pos1.coverage().unwrap();

        let mut plan = Plan { font_num_glyphs: 313, ..Default::default() };
        plan.glyphset_gsub.insert(GlyphId::NOTDEF);
        plan.glyphset_gsub.insert(GlyphId::from(34_u32));
        plan.glyphset_gsub.insert(GlyphId::from(35_u32));
        plan.glyphset_gsub.insert(GlyphId::from(36_u32));
        plan.glyphset_gsub.insert(GlyphId::from(56_u32));

        plan.glyph_map_gsub.insert(GlyphId::NOTDEF, GlyphId::NOTDEF);
        plan.glyph_map_gsub.insert(GlyphId::from(34_u32), GlyphId::from(1_u32));
        plan.glyph_map_gsub.insert(GlyphId::from(35_u32), GlyphId::from(2_u32));
        plan.glyph_map_gsub.insert(GlyphId::from(36_u32), GlyphId::from(3_u32));
        plan.glyph_map_gsub.insert(GlyphId::from(56_u32), GlyphId::from(4_u32));

        let mut s = Serializer::new(1024);
        assert_eq!(s.start_serialize(), Ok(()));
        let ret = coverage.subset(&plan, &mut s, ());
        assert!(ret.is_ok());
        assert!(!s.in_error());
        s.end_serialize();

        let subsetted_data = s.copy_bytes();
        let expected_bytes: [u8; 8] = [0x00, 0x01, 0x00, 0x02, 0x00, 0x02, 0x00, 0x04];
        assert_eq!(subsetted_data, expected_bytes);
    }

    #[test]
    fn test_singlepos_closure_lookups() {
        let font = FontRef::new(include_bytes!(
            "../test-data/fonts/gpos_chaining1_multiple_subrules_f1.otf"
        ))
        .unwrap();
        let gpos_lookup_list = font.gpos().unwrap().lookup_list().unwrap();
        let mut lookup_indices = IntSet::empty();
        lookup_indices.insert(1_u16);

        let mut glyphs = IntSet::empty();
        // glyphs set doesn't intersect with any subtable in lookup index=1,
        // lookup_indices set will be emptied
        glyphs.insert(GlyphId::from(3_u32));
        assert!(gpos_lookup_list.closure_lookups(&glyphs, &mut lookup_indices).is_ok());
        assert!(lookup_indices.is_empty());

        //reset
        lookup_indices.insert(1);
        glyphs.clear();

        // make glyphs intersect with lookup index=1
        glyphs.insert(GlyphId::from(48_u32));

        // no new lookup indices are added, lookup_indices set remains the same
        assert!(gpos_lookup_list.closure_lookups(&glyphs, &mut lookup_indices).is_ok());
        assert_eq!(lookup_indices.len(), 1);
        assert!(lookup_indices.contains(1_u16));
    }

    #[test]
    fn test_context_format1_closure_lookups() {
        let font = FontRef::new(include_bytes!(
            "../test-data/fonts/gpos_chaining1_multiple_subrules_f1.otf"
        ))
        .unwrap();
        let gpos_lookup_list = font.gpos().unwrap().lookup_list().unwrap();
        let mut lookup_indices = IntSet::empty();
        lookup_indices.insert(4_u16);

        let mut glyphs = IntSet::empty();
        // glyphs set doesn't intersect with any subtable in lookup index=4,
        // lookup_indices set will be emptied
        glyphs.insert(GlyphId::from(3_u32));
        assert!(gpos_lookup_list.closure_lookups(&glyphs, &mut lookup_indices).is_ok());
        assert!(lookup_indices.is_empty());

        //reset
        lookup_indices.insert(4);
        glyphs.clear();

        // make glyphs intersect with subtable index=1
        // input coverage glyph
        glyphs.insert(GlyphId::from(49_u32));
        // backtrack glyph
        glyphs.insert(GlyphId::from(48_u32));
        // input glyph
        glyphs.insert(GlyphId::from(50_u32));
        // lookahead glyph
        glyphs.insert(GlyphId::from(51_u32));

        assert!(gpos_lookup_list.closure_lookups(&glyphs, &mut lookup_indices).is_ok());
        assert_eq!(lookup_indices.len(), 2);
        assert!(lookup_indices.contains(4_u16));
        assert!(lookup_indices.contains(1_u16));
    }

    #[test]
    fn test_context_format2_closure_lookups() {
        let font = FontRef::new(include_bytes!(
            "../test-data/fonts/gpos_chaining2_multiple_subrules_f1.otf"
        ))
        .unwrap();
        let gpos_lookup_list = font.gpos().unwrap().lookup_list().unwrap();
        let mut lookup_indices = IntSet::empty();
        lookup_indices.insert(4_u16);

        let mut glyphs = IntSet::empty();
        // glyphs set doesn't intersect with any subtable in lookup index=4,
        // lookup_indices set will be emptied
        glyphs.insert(GlyphId::from(47_u32));
        assert!(gpos_lookup_list.closure_lookups(&glyphs, &mut lookup_indices).is_ok());
        assert!(lookup_indices.is_empty());

        //reset
        lookup_indices.insert(4);
        glyphs.clear();

        // make glyphs intersect with subtable index=1
        // input coverage glyph
        glyphs.insert(GlyphId::from(49_u32));
        glyphs.insert(GlyphId::from(50_u32));
        // backtrack glyph
        glyphs.insert(GlyphId::from(48_u32));
        // lookahead glyph
        glyphs.insert(GlyphId::from(51_u32));
        glyphs.insert(GlyphId::from(52_u32));

        assert!(gpos_lookup_list.closure_lookups(&glyphs, &mut lookup_indices).is_ok());
        assert_eq!(lookup_indices.len(), 2);
        assert!(lookup_indices.contains(4_u16));
        assert!(lookup_indices.contains(1_u16));
    }

    #[test]
    fn test_context_format3_closure_lookups() {
        let font = FontRef::new(include_bytes!("../test-data/fonts/Amiri-Regular.ttf")).unwrap();
        let gpos_lookup_list = font.gpos().unwrap().lookup_list().unwrap();
        let mut lookup_indices = IntSet::empty();
        lookup_indices.insert(2_u16);

        let mut glyphs = IntSet::empty();
        // glyphs set doesn't intersect with any subtable in lookup index=2,
        // lookup_indices set will be emptied
        glyphs.insert(GlyphId::from(3_u32));
        assert!(gpos_lookup_list.closure_lookups(&glyphs, &mut lookup_indices).is_ok());
        assert!(lookup_indices.is_empty());

        //reset
        lookup_indices.insert(2);
        glyphs.clear();

        // make glyphs intersect with subtable index=0
        // input glyph
        glyphs.insert(GlyphId::from(6053_u32));
        // lookahead glyph
        glyphs.insert(GlyphId::from(580_u32));

        // make glyphs intersect with subtable index=2
        // input glyph
        glyphs.insert(GlyphId::from(2033_u32));
        // lookahead glyph
        glyphs.insert(GlyphId::from(435_u32));

        assert!(gpos_lookup_list.closure_lookups(&glyphs, &mut lookup_indices).is_ok());
        assert_eq!(lookup_indices.len(), 3);
        assert!(lookup_indices.contains(3_u16));
        assert!(lookup_indices.contains(4_u16));
    }

    #[test]
    fn test_subset_script_list() {
        use write_fonts::read::tables::gpos::Gpos;
        let font = FontRef::new(include_bytes!("../test-data/fonts/Amiri-Regular.ttf")).unwrap();
        let gpos_script_list = font.gpos().unwrap().script_list().unwrap();

        let mut plan = Plan::default();
        plan.gpos_features_w_duplicates.insert(0_u16, 0_u16);
        plan.gpos_features_w_duplicates.insert(2_u16, 1_u16);

        plan.layout_scripts.invert();

        let mut s = Serializer::new(1024);
        assert_eq!(s.start_serialize(), Ok(()));

        let mut c = SubsetLayoutContext::new(Gpos::TAG);
        gpos_script_list.subset(&plan, &mut s, &mut c).unwrap();
        assert!(!s.in_error());
        s.end_serialize();

        let subsetted_data = s.copy_bytes();
        let expected_data: [u8; 58] = [
            0x00, 0x03, 0x44, 0x46, 0x4c, 0x54, 0x00, 0x2c, 0x61, 0x72, 0x61, 0x62, 0x00, 0x20,
            0x6c, 0x61, 0x74, 0x6e, 0x00, 0x14, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff,
            0x00, 0x01, 0x00, 0x01, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0x00, 0x01,
            0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0x00, 0x02, 0x00, 0x00,
            0x00, 0x01,
        ];

        assert_eq!(subsetted_data, expected_data);
    }

    #[test]
    fn test_subset_feature_list() {
        use write_fonts::read::tables::gpos::Gpos;
        let font = FontRef::new(include_bytes!("../test-data/fonts/Amiri-Regular.ttf")).unwrap();
        let gpos_feature_list = font.gpos().unwrap().feature_list().unwrap();

        let mut plan = Plan::default();
        plan.gpos_features.insert(0_u16, 0_u16);
        plan.gpos_features.insert(2_u16, 1_u16);

        plan.gpos_lookups.insert(82, 1);
        plan.gpos_lookups.insert(57, 0);

        let mut s = Serializer::new(1024);
        assert_eq!(s.start_serialize(), Ok(()));

        let mut c = SubsetLayoutContext::new(Gpos::TAG);
        gpos_feature_list.subset(&plan, &mut s, &mut c).unwrap();
        assert!(!s.in_error());
        s.end_serialize();

        let subsetted_data = s.copy_bytes();
        let expected_data: [u8; 26] = [
            0x00, 0x02, 0x63, 0x75, 0x72, 0x73, 0x00, 0x14, 0x6b, 0x65, 0x72, 0x6e, 0x00, 0x0e,
            0x00, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00,
        ];

        assert_eq!(subsetted_data, expected_data);
    }
}

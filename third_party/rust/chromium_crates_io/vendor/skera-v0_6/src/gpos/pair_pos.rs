//! impl subset() for PairPos subtable

use crate::{
    gpos::value_record::{collect_variation_indices, compute_effective_format},
    layout::{intersected_coverage_indices, intersected_glyphs_and_indices, ClassDefSubsetStruct},
    offset::{SerializeSerialize, SerializeSubset},
    offset_array::SubsetOffsetArray,
    serialize::{SerializeErrorFlags, Serializer},
    CollectVariationIndices, Plan, SubsetFlags, SubsetState, SubsetTable,
};

use crate::fnv::FnvHashMap;
use write_fonts::{
    read::{
        collections::IntSet,
        tables::{
            gpos::{
                PairPos, PairPosFormat1, PairPosFormat2, PairSet, PairValueRecord, ValueFormat,
            },
            layout::CoverageTable,
        },
        types::GlyphId,
        FontData, FontRef, ReadError, TableProvider,
    },
    types::Offset16,
};

impl<'a> SubsetTable<'a> for PairPos<'_> {
    type ArgsForSubset = (&'a SubsetState, &'a FontRef<'a>, &'a FnvHashMap<u16, u16>);
    type Output = ();
    fn subset(
        &self,
        plan: &Plan,
        s: &mut Serializer,
        args: Self::ArgsForSubset,
    ) -> Result<Self::Output, SerializeErrorFlags> {
        let args = (args.0, args.1);
        match self {
            Self::Format1(item) => item.subset(plan, s, args),
            Self::Format2(item) => item.subset(plan, s, args),
        }
    }
}

fn compute_effective_pair_formats_1(
    pair_pos: &PairPosFormat1,
    glyph_set: &IntSet<GlyphId>,
    strip_hints: bool,
    strip_empty: bool,
) -> Result<(ValueFormat, ValueFormat), ReadError> {
    let mut new_format1 = ValueFormat::empty();
    let mut new_format2 = ValueFormat::empty();

    let orig_format1 = pair_pos.value_format1();
    let orig_format2 = pair_pos.value_format2();

    let coverage = pair_pos.coverage()?;
    let pair_sets = pair_pos.pair_sets();
    let partset_idxes = intersected_coverage_indices(&coverage, glyph_set);
    for i in partset_idxes.iter() {
        let pair_set = match pair_sets.get(i as usize) {
            Err(ReadError::NullOffset) => continue,
            other => other,
        }?;
        for pair_value_rec in pair_set.pair_value_records().iter() {
            let pair_value_rec = pair_value_rec?;
            let second_glyph = pair_value_rec.second_glyph();
            if !glyph_set.contains(GlyphId::from(second_glyph)) {
                continue;
            }

            new_format1 |=
                compute_effective_format(pair_value_rec.value_record1(), strip_hints, strip_empty);
            new_format2 |=
                compute_effective_format(pair_value_rec.value_record2(), strip_hints, strip_empty);
        }
        if new_format1 == orig_format1 && new_format2 == orig_format2 {
            break;
        }
    }

    Ok((new_format1, new_format2))
}

impl SubsetTable<'_> for PairSet<'_> {
    type ArgsForSubset = (ValueFormat, ValueFormat);
    type Output = ();
    fn subset(
        &self,
        plan: &Plan,
        s: &mut Serializer,
        args: Self::ArgsForSubset,
    ) -> Result<(), SerializeErrorFlags> {
        // pairvalue count
        let pairvalue_count_pos = s.embed(0_u16)?;
        let mut count = 0_u16;

        let glyph_map = &plan.glyph_map_gsub;
        let font_data = self.offset_data();
        let (new_format1, new_format2) = args;

        for pairvalue_rec in self.pair_value_records().iter() {
            let pairvalue_rec = pairvalue_rec
                .map_err(|_| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR))?;
            let Some(gid) = glyph_map.get(&GlyphId::from(pairvalue_rec.second_glyph())) else {
                continue;
            };
            pairvalue_rec.subset(plan, s, (gid, new_format1, new_format2, font_data))?;
            count += 1;
        }

        if count == 0 {
            return Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY);
        }
        s.copy_assign(pairvalue_count_pos, count);
        Ok(())
    }
}

impl<'a> SubsetTable<'a> for PairValueRecord {
    type ArgsForSubset = (&'a GlyphId, ValueFormat, ValueFormat, FontData<'a>);
    type Output = ();
    fn subset(
        &self,
        plan: &Plan,
        s: &mut Serializer,
        args: Self::ArgsForSubset,
    ) -> Result<(), SerializeErrorFlags> {
        let (new_gid, new_format1, new_format2, font_data) = args;
        // second glyph
        s.embed(new_gid.to_u32() as u16)?;

        //value records
        self.value_record1()
            .subset(plan, s, (new_format1, font_data))?;
        self.value_record2()
            .subset(plan, s, (new_format2, font_data))
    }
}

impl<'a> SubsetTable<'a> for PairPosFormat1<'_> {
    type ArgsForSubset = (&'a SubsetState, &'a FontRef<'a>);
    type Output = ();
    fn subset(
        &self,
        plan: &Plan,
        s: &mut Serializer,
        args: Self::ArgsForSubset,
    ) -> Result<(), SerializeErrorFlags> {
        if self.coverage_offset().is_null() {
            return Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY);
        }
        let (subset_state, font) = args;
        let glyph_map = &plan.glyph_map_gsub;
        let glyph_set = &plan.glyphset_gsub;
        // format
        s.embed(self.pos_format())?;

        // coverage offset
        let cov_offset_pos = s.embed(0_u16)?;

        // value_formats
        let (new_format1, new_format2) = if plan
            .subset_flags
            .contains(SubsetFlags::SUBSET_FLAGS_NO_HINTING)
        {
            // do not strip hints for VF unless it has no GDEF varstore after subsetting
            let strip_hints = if font.fvar().is_ok() {
                !subset_state.has_gdef_varstore
            } else {
                true
            };

            compute_effective_pair_formats_1(self, glyph_set, strip_hints, true)
                .map_err(|_| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR))?
        } else {
            (self.value_format1(), self.value_format2())
        };
        s.embed(new_format1)?;
        s.embed(new_format2)?;

        // pairset count
        let pairset_count_pos = s.embed(0_u16)?;
        let mut pairset_count = 0_u16;

        let coverage = self
            .coverage()
            .map_err(|_| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR))?;
        let pair_sets = self.pair_sets();

        let (glyphs, pairset_idxes) =
            intersected_glyphs_and_indices(&coverage, glyph_set, glyph_map);
        if glyphs.is_empty() {
            return Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY);
        }

        let mut retained_glyphs = Vec::with_capacity(glyphs.len());
        for (i, g) in pairset_idxes.iter().zip(glyphs) {
            match pair_sets.subset_offset(i as usize, s, plan, (new_format1, new_format2)) {
                Ok(()) => {
                    pairset_count += 1;
                    retained_glyphs.push(g);
                }
                Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY) => (),
                Err(e) => {
                    return Err(e);
                }
            }
        }

        s.copy_assign(pairset_count_pos, pairset_count);
        Offset16::serialize_serialize::<CoverageTable>(s, &retained_glyphs, cov_offset_pos)
    }
}

fn compute_effective_pair_formats_2(
    pair_pos: &PairPosFormat2,
    class1_idxes: &[u16],
    class2_idxes: &[u16],
    strip_hints: bool,
    strip_empty: bool,
) -> Result<(ValueFormat, ValueFormat), ReadError> {
    let mut new_format1 = ValueFormat::empty();
    let mut new_format2 = ValueFormat::empty();

    let orig_format1 = pair_pos.value_format1();
    let orig_format2 = pair_pos.value_format2();

    let class1_records = pair_pos.class1_records();
    for i in class1_idxes {
        let class1_rec = class1_records.get(*i as usize)?;
        let class2_records = class1_rec.class2_records();

        for j in class2_idxes {
            let class2_rec = class2_records.get(*j as usize)?;

            new_format1 |=
                compute_effective_format(class2_rec.value_record1(), strip_hints, strip_empty);
            new_format2 |=
                compute_effective_format(class2_rec.value_record2(), strip_hints, strip_empty);
        }
        if new_format1 == orig_format1 && new_format2 == orig_format2 {
            break;
        }
    }
    Ok((new_format1, new_format2))
}

impl<'a> SubsetTable<'a> for PairPosFormat2<'_> {
    type ArgsForSubset = (&'a SubsetState, &'a FontRef<'a>);
    type Output = ();
    fn subset(
        &self,
        plan: &Plan,
        s: &mut Serializer,
        args: Self::ArgsForSubset,
    ) -> Result<(), SerializeErrorFlags> {
        if self.coverage_offset().is_null()
            || self.class_def1_offset().is_null()
            || self.class_def2_offset().is_null()
        {
            return Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY);
        }
        let Ok(coverage) = self.coverage() else {
            return Err(s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR));
        };

        let glyphs: Vec<GlyphId> = coverage
            .intersect_set(&plan.glyphset_gsub)
            .iter()
            .filter_map(|g| plan.glyph_map_gsub.get(&g))
            .copied()
            .collect();
        if glyphs.is_empty() {
            return Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY);
        }

        // format
        s.embed(self.pos_format())?;

        // coverage offset
        let cov_offset_pos = s.embed(0_u16)?;

        // value format
        let value_format1_pos = s.embed(0_u16)?;
        let value_format2_pos = s.embed(0_u16)?;

        // classdef1 offset
        let classdef1_offset_pos = s.embed(0_u16)?;
        let class_def1 = self
            .class_def1()
            .map_err(|_| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR))?;
        let class1_map = match Offset16::serialize_subset(
            &class_def1,
            s,
            plan,
            &ClassDefSubsetStruct {
                remap_class: true,
                keep_empty_table: true,
                use_class_zero: true,
                glyph_filter: Some(&coverage),
            },
            classdef1_offset_pos,
        ) {
            Ok(Some(out)) => out,
            _ => FnvHashMap::default(),
        };

        if class1_map.is_empty() {
            return Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY);
        }
        let class1_count = class1_map.len() as u16;

        // classdef2 offset
        let classdef2_offset_pos = s.embed(0_u16)?;
        let class_def2 = self
            .class_def2()
            .map_err(|_| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR))?;

        let class2_map = match Offset16::serialize_subset(
            &class_def2,
            s,
            plan,
            &ClassDefSubsetStruct {
                remap_class: true,
                keep_empty_table: true,
                use_class_zero: false,
                glyph_filter: None,
            },
            classdef2_offset_pos,
        ) {
            Ok(Some(out)) => out,
            _ => FnvHashMap::default(),
        };

        // If only Class2 0 left, no need to keep anything.
        if class2_map.len() <= 1 {
            return Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY);
        }
        let class2_count = class2_map.len() as u16;

        // class1_count
        s.embed(class1_count)?;
        // class2_count
        s.embed(class2_count)?;

        // value formats
        let (subset_state, font) = args;
        let class1_idxes: Vec<u16> = (0..self.class1_count())
            .filter(|i| class1_map.contains_key(i))
            .collect();
        let class2_idxes: Vec<u16> = (0..self.class2_count())
            .filter(|i| class2_map.contains_key(i))
            .collect();

        let (new_format1, new_format2) = if plan
            .subset_flags
            .contains(SubsetFlags::SUBSET_FLAGS_NO_HINTING)
        {
            // do not strip hints for VF unless it has no GDEF varstore after subsetting
            let strip_hints = if font.fvar().is_ok() {
                !subset_state.has_gdef_varstore
            } else {
                true
            };

            compute_effective_pair_formats_2(self, &class1_idxes, &class2_idxes, strip_hints, true)
                .map_err(|_| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR))?
        } else {
            (self.value_format1(), self.value_format2())
        };

        s.copy_assign(value_format1_pos, new_format1);
        s.copy_assign(value_format2_pos, new_format2);

        // serialize value records
        let font_data = self.offset_data();
        let class1_records = self.class1_records();
        for i in class1_idxes {
            let Ok(class1_record) = class1_records.get(i as usize) else {
                return Err(s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR));
            };
            let class2_records = class1_record.class2_records();
            for j in &class2_idxes {
                let Ok(class2_rec) = class2_records.get(*j as usize) else {
                    return Err(s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR));
                };

                class2_rec
                    .value_record1()
                    .subset(plan, s, (new_format1, font_data))?;
                class2_rec
                    .value_record2()
                    .subset(plan, s, (new_format2, font_data))?;
            }
        }

        // this can be moved, put it at last so we have the same binary data with Harfbuzz subsetter
        Offset16::serialize_serialize::<CoverageTable>(s, &glyphs, cov_offset_pos)
    }
}

impl CollectVariationIndices for PairSet<'_> {
    fn collect_variation_indices(&self, plan: &Plan, varidx_set: &mut IntSet<u32>) {
        let glyph_set = &plan.glyphset_gsub;
        let font_data = self.offset_data();
        for pairvalue_record in self.pair_value_records().iter() {
            let Ok(pairvalue_record) = pairvalue_record else {
                return;
            };

            if !glyph_set.contains(GlyphId::from(pairvalue_record.second_glyph())) {
                continue;
            }

            collect_variation_indices(
                pairvalue_record.value_record1(),
                font_data,
                plan,
                varidx_set,
            );
            collect_variation_indices(
                pairvalue_record.value_record2(),
                font_data,
                plan,
                varidx_set,
            );
        }
    }
}

impl CollectVariationIndices for PairPos<'_> {
    fn collect_variation_indices(&self, plan: &Plan, varidx_set: &mut IntSet<u32>) {
        match self {
            Self::Format1(item) => item.collect_variation_indices(plan, varidx_set),
            Self::Format2(item) => item.collect_variation_indices(plan, varidx_set),
        }
    }
}

impl CollectVariationIndices for PairPosFormat1<'_> {
    fn collect_variation_indices(&self, plan: &Plan, varidx_set: &mut IntSet<u32>) {
        let value_format1 = self.value_format1();
        let value_format2 = self.value_format2();

        if !value_format1.intersects(ValueFormat::ANY_DEVICE_OR_VARIDX)
            && !value_format2.intersects(ValueFormat::ANY_DEVICE_OR_VARIDX)
        {
            return;
        }

        let Ok(coverage) = self.coverage() else {
            return;
        };

        let glyph_set = &plan.glyphset_gsub;
        let pair_sets = self.pair_sets();
        let pairset_idxes = intersected_coverage_indices(&coverage, glyph_set);
        for i in pairset_idxes.iter() {
            if let Ok(pair_set) = pair_sets.get(i as usize) {
                pair_set.collect_variation_indices(plan, varidx_set);
            }
        }
    }
}

impl CollectVariationIndices for PairPosFormat2<'_> {
    fn collect_variation_indices(&self, plan: &Plan, varidx_set: &mut IntSet<u32>) {
        let value_format1 = self.value_format1();
        let value_format2 = self.value_format2();

        if !value_format1.intersects(ValueFormat::ANY_DEVICE_OR_VARIDX)
            && !value_format2.intersects(ValueFormat::ANY_DEVICE_OR_VARIDX)
        {
            return;
        }
        let Ok(coverage) = self.coverage() else {
            return;
        };

        let glyph_set = &plan.glyphset_gsub;
        let cov_glyphs = coverage.intersect_set(glyph_set);
        if cov_glyphs.is_empty() {
            return;
        };

        let Ok(classdef1) = self.class_def1() else {
            return;
        };

        let Ok(classdef2) = self.class_def2() else {
            return;
        };

        let class1_set = classdef1.intersect_classes(&cov_glyphs);
        if class1_set.is_empty() {
            return;
        }
        let mut class2_set = classdef2.intersect_classes(glyph_set);
        if class2_set.is_empty() {
            return;
        }
        class2_set.insert(0);

        let font_data = self.offset_data();
        let class1_records = self.class1_records();
        for class1 in class1_set.iter() {
            let Ok(class1_record) = class1_records.get(class1 as usize) else {
                return;
            };
            let class2_records = class1_record.class2_records();
            for class2 in class2_set.iter() {
                let Ok(class2_rec) = class2_records.get(class2 as usize) else {
                    return;
                };
                collect_variation_indices(class2_rec.value_record1(), font_data, plan, varidx_set);
                collect_variation_indices(class2_rec.value_record2(), font_data, plan, varidx_set);
            }
        }
    }
}

#[cfg(test)]
mod test {
    use super::*;
    use write_fonts::read::{FontRef, TableProvider};

    #[test]
    fn test_subset_pairpos_format1() {
        use write_fonts::read::tables::gpos::PositionSubtables;

        let font = FontRef::new(include_bytes!("../../test-data/fonts/Amiri-Regular.ttf")).unwrap();
        let gpos_lookups = font.gpos().unwrap().lookup_list().unwrap();
        let lookup = gpos_lookups.lookups().get(56).unwrap();

        let PositionSubtables::Pair(sub_tables) = lookup.subtables().unwrap() else {
            panic!("Wrong type of lookup table!");
        };
        let pairpos_table = sub_tables.get(0).unwrap();

        let subset_state = SubsetState::default();
        let mut plan = Plan::default();

        plan.glyph_map_gsub
            .insert(GlyphId::from(6292_u32), GlyphId::from(3_u32));
        plan.glyph_map_gsub
            .insert(GlyphId::from(6298_u32), GlyphId::from(4_u32));

        plan.glyphset_gsub.insert(GlyphId::from(6292_u32));
        plan.glyphset_gsub.insert(GlyphId::from(6298_u32));

        let mut s = Serializer::new(1024);
        assert_eq!(s.start_serialize(), Ok(()));

        pairpos_table
            .subset(&plan, &mut s, (&subset_state, &font, &plan.gpos_lookups))
            .unwrap();
        assert!(!s.in_error());
        s.end_serialize();

        let subsetted_data = s.copy_bytes();
        let expected_data: [u8; 34] = [
            0x00, 0x01, 0x00, 0x0e, 0x00, 0x04, 0x00, 0x00, 0x00, 0x02, 0x00, 0x1c, 0x00, 0x16,
            0x00, 0x01, 0x00, 0x02, 0x00, 0x03, 0x00, 0x04, 0x00, 0x01, 0x00, 0x03, 0xff, 0xcf,
            0x00, 0x01, 0x00, 0x04, 0xff, 0xe8,
        ];

        assert_eq!(subsetted_data, expected_data);
    }

    #[test]
    fn test_subset_pairpos_format2() {
        use write_fonts::read::tables::gpos::PositionSubtables;

        let font = FontRef::new(include_bytes!("../../test-data/fonts/Amiri-Regular.ttf")).unwrap();
        let gpos_lookups = font.gpos().unwrap().lookup_list().unwrap();
        let lookup = gpos_lookups.lookups().get(82).unwrap();

        let PositionSubtables::Pair(sub_tables) = lookup.subtables().unwrap() else {
            panic!("Wrong type of lookup table!");
        };
        let pairpos_table = sub_tables.get(0).unwrap();

        let subset_state = SubsetState::default();
        let mut plan = Plan::default();

        //test case 1: ValueFormat remains the same
        plan.glyph_map_gsub
            .insert(GlyphId::from(40_u32), GlyphId::from(1_u32));
        plan.glyph_map_gsub
            .insert(GlyphId::from(72_u32), GlyphId::from(2_u32));
        plan.glyph_map_gsub
            .insert(GlyphId::from(168_u32), GlyphId::from(3_u32));
        plan.glyph_map_gsub
            .insert(GlyphId::from(6736_u32), GlyphId::from(4_u32));

        plan.glyphset_gsub.insert(GlyphId::from(40_u32));
        plan.glyphset_gsub.insert(GlyphId::from(72_u32));
        plan.glyphset_gsub.insert(GlyphId::from(168_u32));
        plan.glyphset_gsub.insert(GlyphId::from(6736_u32));

        let mut s = Serializer::new(1024);
        assert_eq!(s.start_serialize(), Ok(()));

        pairpos_table
            .subset(&plan, &mut s, (&subset_state, &font, &plan.gpos_lookups))
            .unwrap();
        assert!(!s.in_error());
        s.end_serialize();

        let subsetted_data = s.copy_bytes();
        let expected_data: [u8; 82] = [
            0x00, 0x02, 0x00, 0x2e, 0x00, 0x04, 0x00, 0x00, 0x00, 0x46, 0x00, 0x38, 0x00, 0x03,
            0x00, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0xff, 0xf2, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x01, 0x00, 0x01, 0x00, 0x04, 0x00, 0x00,
            0x00, 0x01, 0x00, 0x01, 0x00, 0x04, 0x00, 0x01, 0x00, 0x02, 0x00, 0x04, 0x00, 0x03,
            0x00, 0x01, 0x00, 0x01, 0x00, 0x03, 0x00, 0x02, 0x00, 0x01, 0x00, 0x01,
        ];

        assert_eq!(subsetted_data, expected_data);

        // test case 2: strip hints is enabled
        plan.subset_flags = SubsetFlags::SUBSET_FLAGS_NO_HINTING;
        plan.glyph_map_gsub.clear();
        plan.glyph_map_gsub
            .insert(GlyphId::from(72_u32), GlyphId::from(1_u32));
        plan.glyph_map_gsub
            .insert(GlyphId::from(168_u32), GlyphId::from(2_u32));
        plan.glyph_map_gsub
            .insert(GlyphId::from(6736_u32), GlyphId::from(3_u32));

        plan.glyphset_gsub.clear();
        plan.glyphset_gsub.insert(GlyphId::from(72_u32));
        plan.glyphset_gsub.insert(GlyphId::from(168_u32));
        plan.glyphset_gsub.insert(GlyphId::from(6736_u32));

        let mut s = Serializer::new(1024);
        assert_eq!(s.start_serialize(), Ok(()));

        pairpos_table
            .subset(&plan, &mut s, (&subset_state, &font, &plan.gpos_lookups))
            .unwrap();
        assert!(!s.in_error());
        s.end_serialize();

        let subsetted_data = s.copy_bytes();
        let expected_data: [u8; 48] = [
            0x00, 0x02, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x26, 0x00, 0x1a, 0x00, 0x02,
            0x00, 0x04, 0x00, 0x01, 0x00, 0x03, 0x00, 0x01, 0x00, 0x02, 0x00, 0x03, 0x00, 0x01,
            0x00, 0x01, 0x00, 0x03, 0x00, 0x01, 0x00, 0x03, 0x00, 0x02, 0x00, 0x01, 0x00, 0x01,
            0x00, 0x02, 0x00, 0x01, 0x00, 0x01,
        ];

        assert_eq!(subsetted_data, expected_data);
    }

    #[test]
    fn test_collect_variation_indices_pairpos_format1() {
        use write_fonts::read::tables::gpos::PositionSubtables;

        let font = FontRef::new(include_bytes!(
            "../../test-data/fonts/RobotoFlex-Variable.ttf"
        ))
        .unwrap();
        let gpos_lookups = font.gpos().unwrap().lookup_list().unwrap();
        let lookup = gpos_lookups.lookups().get(0).unwrap();

        let PositionSubtables::Pair(sub_tables) = lookup.subtables().unwrap() else {
            panic!("Wrong type of lookup table!");
        };
        let pairpos_table = sub_tables.get(0).unwrap();
        let mut plan = Plan::default();

        plan.glyphset_gsub.insert(GlyphId::from(3_u32));
        plan.glyphset_gsub.insert(GlyphId::from(11_u32));
        plan.glyphset_gsub.insert(GlyphId::from(55_u32));
        plan.glyphset_gsub.insert(GlyphId::from(57_u32));

        let mut varidx_set = IntSet::empty();
        pairpos_table.collect_variation_indices(&plan, &mut varidx_set);
        assert_eq!(varidx_set.len(), 5);
        assert!(varidx_set.contains(0x6f0013_u32));
        assert!(varidx_set.contains(0x3e0004_u32));
        assert!(varidx_set.contains(0x540010_u32));
        assert!(varidx_set.contains(0x1c0024_u32));
        assert!(varidx_set.contains(0x1c003c_u32));
    }

    #[test]
    fn test_collect_variation_indices_pairpos_format2() {
        use write_fonts::read::tables::gpos::PositionSubtables;

        let font = FontRef::new(include_bytes!(
            "../../test-data/fonts/RobotoFlex-Variable.ttf"
        ))
        .unwrap();
        let gpos_lookups = font.gpos().unwrap().lookup_list().unwrap();
        let lookup = gpos_lookups.lookups().get(0).unwrap();

        let PositionSubtables::Pair(sub_tables) = lookup.subtables().unwrap() else {
            panic!("Wrong type of lookup table!");
        };
        let pairpos_table = sub_tables.get(1).unwrap();
        let mut plan = Plan::default();

        plan.glyphset_gsub.insert(GlyphId::from(38_u32));
        plan.glyphset_gsub.insert(GlyphId::from(39_u32));
        plan.glyphset_gsub.insert(GlyphId::from(68_u32));
        plan.glyphset_gsub.insert(GlyphId::from(127_u32));

        let mut varidx_set = IntSet::empty();
        pairpos_table.collect_variation_indices(&plan, &mut varidx_set);
        assert_eq!(varidx_set.len(), 9);
        assert!(varidx_set.contains(0x12000f_u32));
        assert!(varidx_set.contains(0x3c0000_u32));
        assert!(varidx_set.contains(0x54001e_u32));
        assert!(varidx_set.contains(0x1c0031_u32));
        assert!(varidx_set.contains(0xb000b_u32));
        assert!(varidx_set.contains(0x1c0035_u32));
        assert!(varidx_set.contains(0x1c0022_u32));
        assert!(varidx_set.contains(0x100005_u32));
        assert!(varidx_set.contains(0x1c0036_u32));
    }
}

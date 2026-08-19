//! impl subset() for MarkBasePos subtable
use crate::fnv::FnvHashMap;
use crate::{
    gpos::mark_array::{collect_mark_record_varidx, get_mark_class_map},
    layout::{intersected_coverage_indices, intersected_glyphs_and_indices},
    offset::{SerializeSerialize, SerializeSubset},
    serialize::{SerializeErrorFlags, Serializer},
    CollectVariationIndices, Plan, SubsetState, SubsetTable,
};
use write_fonts::{
    read::{
        collections::IntSet,
        tables::{
            gpos::{BaseArray, BaseRecord, MarkBasePosFormat1},
            layout::CoverageTable,
        },
        FontData, FontRef,
    },
    types::{GlyphId, Offset16},
};

impl CollectVariationIndices for MarkBasePosFormat1<'_> {
    fn collect_variation_indices(&self, plan: &Plan, varidx_set: &mut IntSet<u32>) {
        let Ok(mark_coverage) = self.mark_coverage() else {
            return;
        };
        let Ok(mark_array) = self.mark_array() else {
            return;
        };

        let glyph_set = &plan.glyphset_gsub;
        let mark_array_data = mark_array.offset_data();
        let mark_records = mark_array.mark_records();

        let mark_record_idxes = intersected_coverage_indices(&mark_coverage, glyph_set);
        let mut retained_mark_classes = IntSet::empty();
        for i in mark_record_idxes.iter() {
            let Some(mark_record) = mark_records.get(i as usize) else {
                return;
            };
            let class = mark_record.mark_class();
            collect_mark_record_varidx(mark_record, plan, varidx_set, mark_array_data);
            retained_mark_classes.insert(class);
        }

        let Ok(base_coverage) = self.base_coverage() else {
            return;
        };
        let Ok(base_array) = self.base_array() else {
            return;
        };
        let base_array_data = base_array.offset_data();
        let base_records = base_array.base_records();
        let base_record_idxes = intersected_coverage_indices(&base_coverage, glyph_set);
        for i in base_record_idxes.iter() {
            let Ok(base_record) = base_records.get(i as usize) else {
                return;
            };

            let base_anchors = base_record.base_anchors(base_array_data);
            for j in retained_mark_classes.iter() {
                if let Some(Ok(anchor)) = base_anchors.get(j as usize) {
                    anchor.collect_variation_indices(plan, varidx_set);
                }
            }
        }
    }
}

impl<'a> SubsetTable<'a> for MarkBasePosFormat1<'_> {
    type ArgsForSubset = (&'a SubsetState, &'a FontRef<'a>, &'a FnvHashMap<u16, u16>);
    type Output = ();
    fn subset(
        &self,
        plan: &Plan,
        s: &mut Serializer,
        _args: Self::ArgsForSubset,
    ) -> Result<Self::Output, SerializeErrorFlags> {
        if self.mark_coverage_offset().is_null()
            || self.mark_array_offset().is_null()
            || self.base_coverage_offset().is_null()
            || self.base_array_offset().is_null()
        {
            return Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY);
        }
        let mark_coverage = self
            .mark_coverage()
            .map_err(|_| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR))?;

        let mark_array = self
            .mark_array()
            .map_err(|_| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR))?;

        let glyph_set = &plan.glyphset_gsub;
        let glyph_map = &plan.glyph_map_gsub;
        let mark_class_map = get_mark_class_map(&mark_coverage, &mark_array, glyph_set);
        if mark_class_map.is_empty() {
            return Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY);
        }

        // format
        s.embed(self.pos_format())?;

        // mark coverage offset
        let mark_cov_offset_pos = s.embed(0_u16)?;

        // base coverage offset
        let base_cov_offset_pos = s.embed(0_u16)?;

        // mark class count
        let mark_class_count = mark_class_map.len() as u16;
        s.embed(mark_class_count)?;

        // mark array offset
        let mark_array_offset_pos = s.embed(0_u16)?;
        let (mark_glyphs, mark_record_idxes) =
            intersected_glyphs_and_indices(&mark_coverage, glyph_set, glyph_map);

        Offset16::serialize_serialize::<CoverageTable>(s, &mark_glyphs, mark_cov_offset_pos)?;
        Offset16::serialize_subset(
            &mark_array,
            s,
            plan,
            (&mark_record_idxes, &mark_class_map),
            mark_array_offset_pos,
        )?;

        // base array offset
        let base_array_offset_pos = s.embed(0_u16)?;

        let base_coverage = self
            .base_coverage()
            .map_err(|_| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR))?;
        let base_array = self
            .base_array()
            .map_err(|_| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR))?;

        let (base_glyphs, base_record_idxes) =
            intersected_glyphs_and_indices(&base_coverage, glyph_set, glyph_map);
        if base_glyphs.is_empty() {
            return Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY);
        }

        let base_glyphs = Offset16::serialize_subset(
            &base_array,
            s,
            plan,
            (&base_glyphs, &base_record_idxes, &mark_class_map),
            base_array_offset_pos,
        )?;
        Offset16::serialize_serialize::<CoverageTable>(s, &base_glyphs, base_cov_offset_pos)
    }
}

impl<'a> SubsetTable<'a> for BaseArray<'_> {
    type ArgsForSubset = (&'a [GlyphId], &'a IntSet<u16>, &'a FnvHashMap<u16, u16>);
    type Output = Vec<GlyphId>;
    fn subset(
        &self,
        plan: &Plan,
        s: &mut Serializer,
        args: Self::ArgsForSubset,
    ) -> Result<Vec<GlyphId>, SerializeErrorFlags> {
        let (base_glyphs, base_record_idxes, mark_class_map) = args;
        let mut retained_base_glyphs = Vec::with_capacity(base_glyphs.len());
        // base count
        let base_count_pos = s.embed(0_u16)?;

        let font_data = self.offset_data();
        let base_records = self.base_records();
        for (g, i) in base_glyphs.iter().zip(base_record_idxes.iter()) {
            let base_record = base_records
                .get(i as usize)
                .map_err(|_| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR))?;

            match base_record.subset(plan, s, (mark_class_map, font_data)) {
                Ok(()) => retained_base_glyphs.push(*g),
                Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY) => (),
                Err(e) => return Err(e),
            }
        }

        let base_count = retained_base_glyphs.len() as u16;
        if base_count == 0 {
            return Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY);
        }
        s.copy_assign(base_count_pos, base_count);
        Ok(retained_base_glyphs)
    }
}

impl<'a> SubsetTable<'a> for BaseRecord<'_> {
    type ArgsForSubset = (&'a FnvHashMap<u16, u16>, FontData<'a>);
    type Output = ();
    fn subset(
        &self,
        plan: &Plan,
        s: &mut Serializer,
        args: Self::ArgsForSubset,
    ) -> Result<(), SerializeErrorFlags> {
        let (mark_class_map, font_data) = args;
        let base_anchors = self.base_anchors(font_data);
        let orig_mark_class_count = base_anchors.len() as u16;

        let mut has_effective_anchors = false;
        let snap = s.snapshot();
        for i in (0..orig_mark_class_count).filter(|class| mark_class_map.contains_key(class)) {
            let anchor_offset_pos = s.embed(0_u16)?;
            let Some(base_anchor) = base_anchors
                .get(i as usize)
                .transpose()
                .map_err(|_| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR))?
            else {
                continue;
            };
            Offset16::serialize_subset(&base_anchor, s, plan, (), anchor_offset_pos)?;
            if !has_effective_anchors {
                has_effective_anchors = true;
            }
        }

        if !has_effective_anchors {
            s.revert_snapshot(snap);
            return Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY);
        }
        Ok(())
    }
}

#[cfg(test)]
mod test {
    use super::*;
    use write_fonts::read::{types::GlyphId, FontRef, TableProvider};

    #[test]
    fn test_collect_variation_indices_markbasepos() {
        use write_fonts::read::tables::gpos::PositionSubtables;

        let font = FontRef::new(include_bytes!(
            "../../test-data/fonts/RobotoFlex-Variable.ttf"
        ))
        .unwrap();
        let gpos_lookups = font.gpos().unwrap().lookup_list().unwrap();
        let lookup = gpos_lookups.lookups().get(1).unwrap();

        let PositionSubtables::MarkToBase(sub_tables) = lookup.subtables().unwrap() else {
            panic!("Wrong type of lookup table!");
        };
        let pairpos_table = sub_tables.get(0).unwrap();
        let mut plan = Plan::default();

        plan.glyphset_gsub.insert(GlyphId::from(36_u32));
        plan.glyphset_gsub.insert(GlyphId::from(390_u32));
        plan.glyphset_gsub.insert(GlyphId::from(405_u32));

        let mut varidx_set = IntSet::empty();
        pairpos_table.collect_variation_indices(&plan, &mut varidx_set);
        assert_eq!(varidx_set.len(), 5);
        assert!(varidx_set.contains(0x160004_u32));
        assert!(varidx_set.contains(0x110002_u32));
        assert!(varidx_set.contains(0x82002a_u32));
        assert!(varidx_set.contains(0x820044_u32));
        assert!(varidx_set.contains(0x850013_u32));
    }

    #[test]
    fn test_subset_markbase_pos() {
        use write_fonts::read::tables::gpos::PositionSubtables;

        let font = FontRef::new(include_bytes!(
            "../../test-data/fonts/RobotoFlex-Variable.ttf"
        ))
        .unwrap();
        let gpos_lookups = font.gpos().unwrap().lookup_list().unwrap();
        let lookup = gpos_lookups.lookups().get(1).unwrap();

        let PositionSubtables::MarkToBase(sub_tables) = lookup.subtables().unwrap() else {
            panic!("Wrong type of lookup table!");
        };
        let markbasepos_table = sub_tables.get(0).unwrap();

        let subset_state = SubsetState::default();
        let mut plan = Plan::default();

        plan.glyph_map_gsub
            .insert(GlyphId::from(37_u32), GlyphId::from(1_u32));
        plan.glyph_map_gsub
            .insert(GlyphId::from(390_u32), GlyphId::from(3_u32));
        plan.glyph_map_gsub
            .insert(GlyphId::from(405_u32), GlyphId::from(4_u32));

        plan.glyphset_gsub.insert(GlyphId::from(37_u32));
        plan.glyphset_gsub.insert(GlyphId::from(390_u32));
        plan.glyphset_gsub.insert(GlyphId::from(405_u32));

        plan.layout_varidx_delta_map
            .insert(0x820044_u32, (0x40001_u32, 0));
        plan.layout_varidx_delta_map
            .insert(0x160004_u32, (0x10000_u32, 0));
        plan.layout_varidx_delta_map
            .insert(0x110002_u32, (0x0_u32, 0));
        plan.layout_varidx_delta_map
            .insert(0x820018_u32, (0x40000_u32, 0));

        let mut s = Serializer::new(1024);
        assert_eq!(s.start_serialize(), Ok(()));

        markbasepos_table
            .subset(&plan, &mut s, (&subset_state, &font, &plan.gpos_lookups))
            .unwrap();
        assert!(!s.in_error());
        s.end_serialize();

        let subsetted_data = s.copy_bytes();
        let expected_data: [u8; 106] = [
            0x00, 0x01, 0x00, 0x62, 0x00, 0x0c, 0x00, 0x02, 0x00, 0x32, 0x00, 0x12, 0x00, 0x01,
            0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x10, 0x00, 0x06, 0x00, 0x03, 0x02, 0x76,
            0x00, 0x00, 0x00, 0x14, 0x00, 0x00, 0x00, 0x03, 0x02, 0x76, 0x05, 0xb0, 0x00, 0x0a,
            0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x80, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x1a,
            0x00, 0x01, 0x00, 0x0a, 0x00, 0x03, 0x02, 0x00, 0x00, 0x00, 0x00, 0x0a, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x03, 0x02, 0x00, 0x04, 0x1c, 0x00, 0x10,
            0x00, 0x0a, 0x00, 0x01, 0x00, 0x00, 0x80, 0x00, 0x00, 0x04, 0x00, 0x01, 0x80, 0x00,
            0x00, 0x01, 0x00, 0x02, 0x00, 0x03, 0x00, 0x04,
        ];

        assert_eq!(subsetted_data, expected_data);
    }

    #[test]
    fn test_subset_markbase_pos_remove_empty_base_record() {
        use write_fonts::read::tables::gpos::PositionSubtables;

        let font = FontRef::new(include_bytes!(
            "../../test-data/fonts/RobotoFlex-Variable.ttf"
        ))
        .unwrap();
        let gpos_lookups = font.gpos().unwrap().lookup_list().unwrap();
        let lookup = gpos_lookups.lookups().get(1).unwrap();

        let PositionSubtables::MarkToBase(sub_tables) = lookup.subtables().unwrap() else {
            panic!("Wrong type of lookup table!");
        };
        let markbasepos_table = sub_tables.get(0).unwrap();

        let subset_state = SubsetState::default();
        let mut plan = Plan::default();

        plan.glyph_map_gsub
            .insert(GlyphId::from(36_u32), GlyphId::from(1_u32));
        plan.glyph_map_gsub
            .insert(GlyphId::from(37_u32), GlyphId::from(2_u32));
        plan.glyph_map_gsub
            .insert(GlyphId::from(404_u32), GlyphId::from(4_u32));
        plan.glyph_map_gsub
            .insert(GlyphId::from(409_u32), GlyphId::from(5_u32));

        plan.glyphset_gsub.insert(GlyphId::from(36_u32));
        plan.glyphset_gsub.insert(GlyphId::from(37_u32));
        plan.glyphset_gsub.insert(GlyphId::from(404_u32));
        plan.glyphset_gsub.insert(GlyphId::from(409_u32));

        plan.layout_varidx_delta_map
            .insert(0x40001_u32, (0x0_u32, 0));
        plan.layout_varidx_delta_map
            .insert(0x160004_u32, (0x30000_u32, 0));
        plan.layout_varidx_delta_map
            .insert(0x6f000c_u32, (0x40000_u32, 0));
        plan.layout_varidx_delta_map
            .insert(0x830015_u32, (0x50000_u32, 0));
        plan.layout_varidx_delta_map
            .insert(0x860011_u32, (0x70000_u32, 0));

        let mut s = Serializer::new(1024);
        assert_eq!(s.start_serialize(), Ok(()));

        markbasepos_table
            .subset(&plan, &mut s, (&subset_state, &font, &plan.gpos_lookups))
            .unwrap();
        assert!(!s.in_error());
        s.end_serialize();

        let subsetted_data = s.copy_bytes();
        let expected_data: [u8; 102] = [
            0x00, 0x01, 0x00, 0x5e, 0x00, 0x0c, 0x00, 0x02, 0x00, 0x2e, 0x00, 0x12, 0x00, 0x01,
            0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x06, 0x00, 0x03, 0x04, 0x57,
            0x00, 0x00, 0x00, 0x10, 0x00, 0x0a, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x05,
            0x00, 0x00, 0x80, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x1a, 0x00, 0x01, 0x00, 0x0a,
            0x00, 0x03, 0x02, 0x00, 0x00, 0x00, 0x00, 0x0a, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00,
            0x80, 0x00, 0x00, 0x03, 0x00, 0x46, 0x04, 0x1c, 0x00, 0x10, 0x00, 0x0a, 0x00, 0x03,
            0x00, 0x00, 0x80, 0x00, 0x00, 0x07, 0x00, 0x00, 0x80, 0x00, 0x00, 0x01, 0x00, 0x02,
            0x00, 0x04, 0x00, 0x05,
        ];

        assert_eq!(subsetted_data, expected_data);
    }
}

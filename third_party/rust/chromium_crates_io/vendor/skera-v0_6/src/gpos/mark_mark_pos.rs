//! impl subset() for MarkMarkPos subtable
use crate::fnv::FnvHashMap;
use crate::{
    gpos::mark_array::{collect_mark_record_varidx, get_mark_class_map},
    layout::{intersected_coverage_indices, intersected_glyphs_and_indices},
    offset::{SerializeSerialize, SerializeSubset},
    serialize::{SerializeErrorFlags, Serializer},
    CollectVariationIndices, Plan, SubsetState, SubsetTable,
};
use write_fonts::read::{
    collections::IntSet,
    tables::{
        gpos::{Mark2Array, Mark2Record, MarkMarkPosFormat1},
        layout::CoverageTable,
    },
    types::{GlyphId, Offset16},
    FontData, FontRef,
};

impl CollectVariationIndices for MarkMarkPosFormat1<'_> {
    fn collect_variation_indices(&self, plan: &Plan, varidx_set: &mut IntSet<u32>) {
        let Ok(mark1_coverage) = self.mark1_coverage() else {
            return;
        };
        let Ok(mark1_array) = self.mark1_array() else {
            return;
        };

        let glyph_set = &plan.glyphset_gsub;
        let mark1_array_data = mark1_array.offset_data();
        let mark1_records = mark1_array.mark_records();

        let mark1_record_idxes = intersected_coverage_indices(&mark1_coverage, glyph_set);
        let mut retained_mark_classes = IntSet::empty();
        for i in mark1_record_idxes.iter() {
            let Some(mark1_record) = mark1_records.get(i as usize) else {
                return;
            };
            let class = mark1_record.mark_class();
            collect_mark_record_varidx(mark1_record, plan, varidx_set, mark1_array_data);
            retained_mark_classes.insert(class);
        }

        let Ok(mark2_coverage) = self.mark2_coverage() else {
            return;
        };
        let Ok(mark2_array) = self.mark2_array() else {
            return;
        };
        let mark2_array_data = mark2_array.offset_data();
        let mark2_records = mark2_array.mark2_records();
        let mark2_record_idxes = intersected_coverage_indices(&mark2_coverage, glyph_set);
        for i in mark2_record_idxes.iter() {
            let Ok(mark2_record) = mark2_records.get(i as usize) else {
                return;
            };
            let mark2_anchors = mark2_record.mark2_anchors(mark2_array_data);
            for j in retained_mark_classes.iter() {
                if let Some(Ok(anchor)) = mark2_anchors.get(j as usize) {
                    anchor.collect_variation_indices(plan, varidx_set);
                }
            }
        }
    }
}

impl<'a> SubsetTable<'a> for MarkMarkPosFormat1<'_> {
    type ArgsForSubset = (&'a SubsetState, &'a FontRef<'a>, &'a FnvHashMap<u16, u16>);
    type Output = ();
    fn subset(
        &self,
        plan: &Plan,
        s: &mut Serializer,
        _args: Self::ArgsForSubset,
    ) -> Result<Self::Output, SerializeErrorFlags> {
        if self.mark1_coverage_offset().is_null()
            || self.mark1_array_offset().is_null()
            || self.mark2_coverage_offset().is_null()
            || self.mark2_array_offset().is_null()
        {
            return Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY);
        }
        let mark1_coverage = self
            .mark1_coverage()
            .map_err(|_| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR))?;

        let mark1_array = self
            .mark1_array()
            .map_err(|_| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR))?;

        let glyph_set = &plan.glyphset_gsub;
        let glyph_map = &plan.glyph_map_gsub;
        let mark_class_map = get_mark_class_map(&mark1_coverage, &mark1_array, glyph_set);
        if mark_class_map.is_empty() {
            return Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY);
        }

        // format
        s.embed(self.pos_format())?;

        // mark1 coverage offset
        let mark1_cov_offset_pos = s.embed(0_u16)?;

        // mark2 coverage offset
        let mark2_cov_offset_pos = s.embed(0_u16)?;

        // mark class count
        let mark_class_count = mark_class_map.len() as u16;
        s.embed(mark_class_count)?;

        // mark1 array offset
        let mark1_array_offset_pos = s.embed(0_u16)?;
        let (mark1_glyphs, mark1_record_idxes) =
            intersected_glyphs_and_indices(&mark1_coverage, glyph_set, glyph_map);

        Offset16::serialize_serialize::<CoverageTable>(s, &mark1_glyphs, mark1_cov_offset_pos)?;
        Offset16::serialize_subset(
            &mark1_array,
            s,
            plan,
            (&mark1_record_idxes, &mark_class_map),
            mark1_array_offset_pos,
        )?;

        // mark2 array offset
        let mark2_array_offset_pos = s.embed(0_u16)?;

        let mark2_coverage = self
            .mark2_coverage()
            .map_err(|_| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR))?;
        let mark2_array = self
            .mark2_array()
            .map_err(|_| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR))?;

        let (mark2_glyphs, mark2_record_idxes) =
            intersected_glyphs_and_indices(&mark2_coverage, glyph_set, glyph_map);
        if mark2_glyphs.is_empty() {
            return Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY);
        }

        let mark2_glyphs = Offset16::serialize_subset(
            &mark2_array,
            s,
            plan,
            (&mark2_glyphs, &mark2_record_idxes, &mark_class_map),
            mark2_array_offset_pos,
        )?;
        Offset16::serialize_serialize::<CoverageTable>(s, &mark2_glyphs, mark2_cov_offset_pos)
    }
}

impl<'a> SubsetTable<'a> for Mark2Array<'_> {
    type ArgsForSubset = (&'a [GlyphId], &'a IntSet<u16>, &'a FnvHashMap<u16, u16>);
    type Output = Vec<GlyphId>;
    fn subset(
        &self,
        plan: &Plan,
        s: &mut Serializer,
        args: Self::ArgsForSubset,
    ) -> Result<Vec<GlyphId>, SerializeErrorFlags> {
        let (mark2_glyphs, mark2_record_idxes, mark_class_map) = args;
        let mut retained_mark2_glyphs = Vec::with_capacity(mark2_glyphs.len());
        // mark2 count
        let mark2_count_pos = s.embed(0_u16)?;

        let font_data = self.offset_data();
        let mark2_records = self.mark2_records();
        for (g, i) in mark2_glyphs.iter().zip(mark2_record_idxes.iter()) {
            let mark2_record = mark2_records
                .get(i as usize)
                .map_err(|_| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR))?;

            match mark2_record.subset(plan, s, (mark_class_map, font_data)) {
                Ok(()) => retained_mark2_glyphs.push(*g),
                Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY) => (),
                Err(e) => return Err(e),
            }
        }

        let mark2_count = retained_mark2_glyphs.len() as u16;
        if mark2_count == 0 {
            return Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY);
        }
        s.copy_assign(mark2_count_pos, mark2_count);
        Ok(retained_mark2_glyphs)
    }
}

impl<'a> SubsetTable<'a> for Mark2Record<'_> {
    type ArgsForSubset = (&'a FnvHashMap<u16, u16>, FontData<'a>);
    type Output = ();
    fn subset(
        &self,
        plan: &Plan,
        s: &mut Serializer,
        args: Self::ArgsForSubset,
    ) -> Result<(), SerializeErrorFlags> {
        let (mark_class_map, font_data) = args;
        let mark2_anchors = self.mark2_anchors(font_data);
        let orig_mark_class_count = mark2_anchors.len() as u16;

        let mut has_effective_anchors = false;
        let snap = s.snapshot();
        for i in (0..orig_mark_class_count).filter(|class| mark_class_map.contains_key(class)) {
            let anchor_offset_pos = s.embed(0_u16)?;
            let Some(mark2_anchor) = mark2_anchors
                .get(i as usize)
                .transpose()
                .map_err(|_| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR))?
            else {
                continue;
            };
            Offset16::serialize_subset(&mark2_anchor, s, plan, (), anchor_offset_pos)?;
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
    fn test_collect_variation_indices_markmarkpos() {
        use write_fonts::read::tables::gpos::PositionSubtables;

        let font = FontRef::new(include_bytes!(
            "../../test-data/fonts/RobotoFlex-Variable.ttf"
        ))
        .unwrap();
        let gpos_lookups = font.gpos().unwrap().lookup_list().unwrap();
        let lookup = gpos_lookups.lookups().get(2).unwrap();

        let PositionSubtables::MarkToMark(sub_tables) = lookup.subtables().unwrap() else {
            panic!("Wrong type of lookup table!");
        };
        let markmarkpos_table = sub_tables.get(0).unwrap();
        let mut plan = Plan::default();

        plan.glyphset_gsub.insert(GlyphId::from(408_u32));
        plan.glyphset_gsub.insert(GlyphId::from(583_u32));

        let mut varidx_set = IntSet::empty();
        markmarkpos_table.collect_variation_indices(&plan, &mut varidx_set);
        assert_eq!(varidx_set.len(), 5);
        assert!(varidx_set.contains(0x300000_u32));
        assert!(varidx_set.contains(0x6f001a_u32));
        assert!(varidx_set.contains(0x110002_u32));
        assert!(varidx_set.contains(0x220000_u32));
        assert!(varidx_set.contains(0x86002e_u32));
    }

    #[test]
    fn test_subset_markmark_pos() {
        use write_fonts::read::tables::gpos::PositionSubtables;

        let font = FontRef::new(include_bytes!(
            "../../test-data/fonts/NotoNastaliqUrdu-Regular.ttf"
        ))
        .unwrap();
        let gpos_lookups = font.gpos().unwrap().lookup_list().unwrap();
        let lookup = gpos_lookups.lookups().get(24).unwrap();

        let PositionSubtables::MarkToMark(sub_tables) = lookup.subtables().unwrap() else {
            panic!("Wrong type of lookup table!");
        };
        let markmarkpos_table = sub_tables.get(0).unwrap();

        let subset_state = SubsetState::default();
        let mut plan = Plan::default();

        plan.glyph_map_gsub
            .insert(GlyphId::from(66_u32), GlyphId::from(2_u32));
        plan.glyph_map_gsub
            .insert(GlyphId::from(67_u32), GlyphId::from(3_u32));
        plan.glyph_map_gsub
            .insert(GlyphId::from(70_u32), GlyphId::from(4_u32));
        plan.glyph_map_gsub
            .insert(GlyphId::from(74_u32), GlyphId::from(5_u32));

        plan.glyphset_gsub.insert(GlyphId::from(66_u32));
        plan.glyphset_gsub.insert(GlyphId::from(67_u32));
        plan.glyphset_gsub.insert(GlyphId::from(70_u32));
        plan.glyphset_gsub.insert(GlyphId::from(74_u32));

        let mut s = Serializer::new(1024);
        assert_eq!(s.start_serialize(), Ok(()));

        markmarkpos_table
            .subset(&plan, &mut s, (&subset_state, &font, &plan.gpos_lookups))
            .unwrap();
        assert!(!s.in_error());
        s.end_serialize();

        let subsetted_data = s.copy_bytes();
        let expected_data: [u8; 98] = [
            0x00, 0x01, 0x00, 0x58, 0x00, 0x58, 0x00, 0x01, 0x00, 0x2e, 0x00, 0x0c, 0x00, 0x04,
            0x00, 0x1c, 0x00, 0x16, 0x00, 0x10, 0x00, 0x0a, 0x00, 0x01, 0x00, 0x99, 0xfd, 0xe8,
            0x00, 0x01, 0x00, 0x00, 0xfe, 0x63, 0x00, 0x01, 0x00, 0x00, 0xff, 0x1b, 0x00, 0x01,
            0x00, 0x00, 0xfe, 0xe0, 0x00, 0x04, 0x00, 0x00, 0x00, 0x24, 0x00, 0x00, 0x00, 0x1e,
            0x00, 0x00, 0x00, 0x18, 0x00, 0x00, 0x00, 0x12, 0x00, 0x01, 0x00, 0x9d, 0xff, 0x61,
            0x00, 0x01, 0x00, 0x00, 0xff, 0x9e, 0x00, 0x01, 0x00, 0x00, 0xff, 0xae, 0x00, 0x01,
            0x00, 0x00, 0xff, 0x74, 0x00, 0x02, 0x00, 0x01, 0x00, 0x02, 0x00, 0x05, 0x00, 0x00,
        ];
        assert_eq!(subsetted_data, expected_data);
    }
}

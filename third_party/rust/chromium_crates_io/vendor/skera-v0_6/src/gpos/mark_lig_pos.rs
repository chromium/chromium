//! impl subset() for MarkLigPos subtable
use crate::fnv::FnvHashMap;
use crate::{
    gpos::mark_array::{collect_mark_record_varidx, get_mark_class_map},
    layout::{intersected_coverage_indices, intersected_glyphs_and_indices},
    offset::{SerializeSerialize, SerializeSubset},
    offset_array::SubsetOffsetArray,
    serialize::{SerializeErrorFlags, Serializer},
    CollectVariationIndices, Plan, SubsetState, SubsetTable,
};
use write_fonts::read::{
    collections::IntSet,
    tables::{
        gpos::{ComponentRecord, LigatureArray, LigatureAttach, MarkLigPosFormat1},
        layout::CoverageTable,
    },
    types::{GlyphId, Offset16},
    FontData, FontRef, ReadError,
};

impl CollectVariationIndices for MarkLigPosFormat1<'_> {
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

        let Ok(lig_coverage) = self.ligature_coverage() else {
            return;
        };
        let Ok(lig_array) = self.ligature_array() else {
            return;
        };
        let lig_attaches = lig_array.ligature_attaches();
        let lig_attach_idxes = intersected_coverage_indices(&lig_coverage, glyph_set);
        for i in lig_attach_idxes.iter() {
            let lig_attach = match lig_attaches.get(i as usize) {
                Err(ReadError::NullOffset) => continue,
                Err(_) => return,
                Ok(lig_attach) => lig_attach,
            };

            let lig_attach_data = lig_attach.offset_data();
            for component in lig_attach.component_records().iter() {
                let Ok(component) = component else {
                    return;
                };

                let lig_anchors = component.ligature_anchors(lig_attach_data);
                for j in retained_mark_classes.iter() {
                    if let Some(Ok(anchor)) = lig_anchors.get(j as usize) {
                        anchor.collect_variation_indices(plan, varidx_set);
                    }
                }
            }
        }
    }
}

impl<'a> SubsetTable<'a> for MarkLigPosFormat1<'_> {
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
            || self.ligature_coverage_offset().is_null()
            || self.ligature_array_offset().is_null()
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

        // ligature coverage offset
        let lig_cov_offset_pos = s.embed(0_u16)?;

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

        // ligature array offset
        let lig_array_offset_pos = s.embed(0_u16)?;

        let lig_coverage = self
            .ligature_coverage()
            .map_err(|_| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR))?;
        let lig_array = self
            .ligature_array()
            .map_err(|_| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR))?;

        let (lig_glyphs, lig_attach_idxes) =
            intersected_glyphs_and_indices(&lig_coverage, glyph_set, glyph_map);
        if lig_glyphs.is_empty() {
            return Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY);
        }

        // Return non-empty lig glyphs for serializing coverage table
        // lig glyphs might have no anchor points defined for retained class of mark glyphs
        let lig_glyphs = Offset16::serialize_subset(
            &lig_array,
            s,
            plan,
            (&lig_glyphs, &lig_attach_idxes, &mark_class_map),
            lig_array_offset_pos,
        )?;
        Offset16::serialize_serialize::<CoverageTable>(s, &lig_glyphs, lig_cov_offset_pos)
    }
}

impl<'a> SubsetTable<'a> for LigatureArray<'_> {
    type ArgsForSubset = (&'a [GlyphId], &'a IntSet<u16>, &'a FnvHashMap<u16, u16>);
    type Output = Vec<GlyphId>;
    fn subset(
        &self,
        plan: &Plan,
        s: &mut Serializer,
        args: Self::ArgsForSubset,
    ) -> Result<Vec<GlyphId>, SerializeErrorFlags> {
        let (lig_glyphs, lig_attach_idxes, mark_class_map) = args;

        let mut retained_lig_glyphs = Vec::with_capacity(lig_glyphs.len());
        // ligature count
        let lig_count_pos = s.embed(0_u16)?;

        let lig_attaches = self.ligature_attaches();
        for (g, i) in lig_glyphs.iter().zip(lig_attach_idxes.iter()) {
            match lig_attaches.subset_offset(i as usize, s, plan, mark_class_map) {
                Ok(()) => retained_lig_glyphs.push(*g),
                Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY) => (),
                Err(e) => return Err(e),
            }
        }

        let lig_count = retained_lig_glyphs.len() as u16;
        if lig_count == 0 {
            return Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY);
        }

        s.copy_assign(lig_count_pos, lig_count);
        Ok(retained_lig_glyphs)
    }
}

impl<'a> SubsetTable<'a> for LigatureAttach<'_> {
    type ArgsForSubset = &'a FnvHashMap<u16, u16>;
    type Output = ();
    fn subset(
        &self,
        plan: &Plan,
        s: &mut Serializer,
        mark_class_map: &FnvHashMap<u16, u16>,
    ) -> Result<(), SerializeErrorFlags> {
        let snap = s.snapshot();
        s.embed(self.component_count())?;

        let component_records = self.component_records();
        let font_data = self.offset_data();
        let mut has_non_empty_rec = false;

        for component_rec in component_records.iter() {
            let component_rec = component_rec
                .map_err(|_| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR))?;
            match component_rec.subset(plan, s, (font_data, mark_class_map)) {
                Ok(()) => {
                    if !has_non_empty_rec {
                        has_non_empty_rec = true
                    }
                }
                Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY) => (),
                Err(e) => return Err(e),
            }
        }

        if !has_non_empty_rec {
            s.revert_snapshot(snap);
            return Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY);
        }
        Ok(())
    }
}

impl<'a> SubsetTable<'a> for ComponentRecord<'_> {
    type ArgsForSubset = (FontData<'a>, &'a FnvHashMap<u16, u16>);
    type Output = ();
    fn subset(
        &self,
        plan: &Plan,
        s: &mut Serializer,
        args: Self::ArgsForSubset,
    ) -> Result<(), SerializeErrorFlags> {
        let (font_data, mark_class_map) = args;
        let lig_anchors = self.ligature_anchors(font_data);
        let orig_mark_class_count = lig_anchors.len() as u16;

        let mut has_effective_anchors = false;
        for i in (0..orig_mark_class_count).filter(|class| mark_class_map.contains_key(class)) {
            let anchor_offset_pos = s.embed(0_u16)?;
            let Some(lig_anchor) = lig_anchors
                .get(i as usize)
                .transpose()
                .map_err(|_| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR))?
            else {
                continue;
            };
            Offset16::serialize_subset(&lig_anchor, s, plan, (), anchor_offset_pos)?;
            if !has_effective_anchors {
                has_effective_anchors = true;
            }
        }

        if !has_effective_anchors {
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
    fn test_collect_variation_indices_markligpos() {
        use write_fonts::read::tables::gpos::PositionSubtables;

        let font = FontRef::new(include_bytes!(
            "../../test-data/fonts/Comfortaa-Regular-new.ttf"
        ))
        .unwrap();
        let gpos_lookups = font.gpos().unwrap().lookup_list().unwrap();
        let lookup = gpos_lookups.lookups().get(2).unwrap();

        let PositionSubtables::MarkToLig(sub_tables) = lookup.subtables().unwrap() else {
            panic!("Wrong type of lookup table!");
        };
        let markligpos_table = sub_tables.get(0).unwrap();
        let mut plan = Plan::default();

        plan.glyphset_gsub.insert(GlyphId::from(396_u32));
        plan.glyphset_gsub.insert(GlyphId::from(839_u32));

        let mut varidx_set = IntSet::empty();
        markligpos_table.collect_variation_indices(&plan, &mut varidx_set);
        assert_eq!(varidx_set.len(), 3);
        assert!(varidx_set.contains(0x20065_u32));
        assert!(varidx_set.contains(0x20062_u32));
        assert!(varidx_set.contains(0x2004c_u32));
    }

    #[test]
    fn test_subset_marklig_pos() {
        use write_fonts::read::tables::gpos::PositionSubtables;

        let font = FontRef::new(include_bytes!("../../test-data/fonts/gpos5_font1.otf")).unwrap();
        let gpos_lookups = font.gpos().unwrap().lookup_list().unwrap();
        let lookup = gpos_lookups.lookups().get(0).unwrap();

        let PositionSubtables::MarkToLig(sub_tables) = lookup.subtables().unwrap() else {
            panic!("Wrong type of lookup table!");
        };
        let markligpos_table = sub_tables.get(0).unwrap();

        let subset_state = SubsetState::default();
        let mut plan = Plan::default();

        plan.glyph_map_gsub
            .insert(GlyphId::from(67_u32), GlyphId::from(1_u32));
        plan.glyph_map_gsub
            .insert(GlyphId::from(68_u32), GlyphId::from(2_u32));

        plan.glyphset_gsub.insert(GlyphId::from(67_u32));
        plan.glyphset_gsub.insert(GlyphId::from(68_u32));

        let mut s = Serializer::new(1024);
        assert_eq!(s.start_serialize(), Ok(()));

        markligpos_table
            .subset(&plan, &mut s, (&subset_state, &font, &plan.gpos_lookups))
            .unwrap();
        assert!(!s.in_error());
        s.end_serialize();

        let subsetted_data = s.copy_bytes();
        let expected_data: [u8; 58] = [
            0x00, 0x01, 0x00, 0x34, 0x00, 0x0c, 0x00, 0x01, 0x00, 0x28, 0x00, 0x12, 0x00, 0x01,
            0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x04, 0x00, 0x02, 0x00, 0x0c, 0x00, 0x06,
            0x00, 0x01, 0x00, 0x67, 0x00, 0x99, 0x00, 0x01, 0x00, 0x66, 0x00, 0x98, 0x00, 0x01,
            0x00, 0x00, 0x00, 0x06, 0x00, 0x01, 0x00, 0x96, 0x00, 0xc8, 0x00, 0x01, 0x00, 0x01,
            0x00, 0x02,
        ];
        assert_eq!(subsetted_data, expected_data);
    }

    #[test]
    fn test_subset_marklig_pos_remove_empty_ligattach() {
        use write_fonts::read::tables::gpos::PositionSubtables;

        let font = FontRef::new(include_bytes!(
            "../../test-data/fonts/NotoNastaliqUrdu-Regular.ttf"
        ))
        .unwrap();
        let gpos_lookups = font.gpos().unwrap().lookup_list().unwrap();
        let lookup = gpos_lookups.lookups().get(22).unwrap();

        let PositionSubtables::MarkToLig(sub_tables) = lookup.subtables().unwrap() else {
            panic!("Wrong type of lookup table!");
        };
        let markligpos_table = sub_tables.get(0).unwrap();

        let subset_state = SubsetState::default();
        let mut plan = Plan::default();

        plan.glyph_map_gsub
            .insert(GlyphId::from(30_u32), GlyphId::from(1_u32));
        plan.glyph_map_gsub
            .insert(GlyphId::from(987_u32), GlyphId::from(3_u32));
        plan.glyph_map_gsub
            .insert(GlyphId::from(216_u32), GlyphId::from(2_u32));

        plan.glyphset_gsub.insert(GlyphId::from(30_u32));
        plan.glyphset_gsub.insert(GlyphId::from(216_u32));
        plan.glyphset_gsub.insert(GlyphId::from(987_u32));

        let mut s = Serializer::new(1024);
        assert_eq!(s.start_serialize(), Ok(()));

        markligpos_table
            .subset(&plan, &mut s, (&subset_state, &font, &plan.gpos_lookups))
            .unwrap();
        assert!(!s.in_error());
        s.end_serialize();

        let subsetted_data = s.copy_bytes();
        let expected_data: [u8; 66] = [
            0x00, 0x01, 0x00, 0x3c, 0x00, 0x0c, 0x00, 0x01, 0x00, 0x30, 0x00, 0x12, 0x00, 0x01,
            0x00, 0x01, 0x00, 0x02, 0x00, 0x01, 0x00, 0x04, 0x00, 0x03, 0x00, 0x14, 0x00, 0x0e,
            0x00, 0x08, 0x00, 0x01, 0x00, 0xa6, 0x01, 0xfc, 0x00, 0x01, 0x02, 0x10, 0x02, 0xc9,
            0x00, 0x01, 0x03, 0x0a, 0x03, 0x5d, 0x00, 0x01, 0x00, 0x00, 0x00, 0x06, 0x00, 0x01,
            0x00, 0x00, 0x03, 0xd3, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01,
        ];
        assert_eq!(subsetted_data, expected_data);
    }
}

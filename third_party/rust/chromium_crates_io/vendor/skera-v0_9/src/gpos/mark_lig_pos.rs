//! impl subset() for MarkLigPos subtable

#[cfg(test)]
mod extended_tests;

use crate::FastHashMap;
use crate::{
    gpos::mark_array::{collect_retained_mark_varidx_and_classes, get_mark_class_map},
    layout::{
        for_each_intersected_coverage_index, for_each_intersected_glyph_and_index,
        intersected_glyphs_and_indices,
    },
    offset::{SerializeSerialize, SerializeSubset},
    offset_array::SubsetOffsetArray,
    serialize::{SerializeErrorFlags, SerializeResultEmpty, Serializer},
    CollectVariationIndices, Plan, SubsetState, SubsetTable,
};
use read_fonts::{
    collections::IntSet,
    tables::{
        gpos::{
            ComponentRecord, ComponentRecord2, LigatureArray, LigatureArray2, LigatureAttach,
            LigatureAttach2, MarkLigPos, MarkLigPosFormat1, MarkLigPosFormat2,
        },
        layout::CoverageTable,
    },
    types::{FixedSize, GlyphId, Offset16, Offset24, Offset32, Uint24},
    FontData, FontRef, MinByteRange, ReadError,
};

macro_rules! collect_mark_lig_variations {
    ($table:ident) => {
        impl CollectVariationIndices for $table<'_> {
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

                let retained_mark_classes = collect_retained_mark_varidx_and_classes(
                    &mark_coverage,
                    mark_records,
                    mark_array_data,
                    plan,
                    varidx_set,
                );

                let Ok(lig_coverage) = self.ligature_coverage() else {
                    return;
                };
                let Ok(lig_array) = self.ligature_array() else {
                    return;
                };
                let lig_attaches = lig_array.ligature_attaches();
                let _ = for_each_intersected_coverage_index(
                    &lig_coverage,
                    glyph_set,
                    lig_array.ligature_count(),
                    |index| {
                        let attach = match lig_attaches.get(index as usize) {
                            Err(ReadError::NullOffset) => return Ok(()),
                            other => other?,
                        };
                        for component in attach.component_records().iter() {
                            let component = component?;
                            let anchors = component.ligature_anchors(attach.offset_data());
                            for class in retained_mark_classes.iter() {
                                if let Some(Ok(anchor)) = anchors.get(class as usize) {
                                    anchor.collect_variation_indices(plan, varidx_set);
                                }
                            }
                        }
                        Ok::<(), ReadError>(())
                    },
                );
            }
        }
    };
}
collect_mark_lig_variations!(MarkLigPosFormat1);
collect_mark_lig_variations!(MarkLigPosFormat2);

macro_rules! subset_mark_lig {
    ($table:ident, $offset:ident) => {
        impl<'a> SubsetTable<'a> for $table<'_> {
            type ArgsForSubset = (&'a SubsetState, &'a FontRef<'a>, &'a FastHashMap<u16, u16>);
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
                let mark_class_map =
                    get_mark_class_map(&mark_coverage, mark_array.mark_records(), glyph_set);
                if mark_class_map.is_empty() {
                    return Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY);
                }

                if mark_array.min_table_bytes().is_empty()
                    || mark_class_map
                        .keys()
                        .any(|class| *class >= self.mark_class_count())
                {
                    return Err(s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR));
                }
                // format
                s.embed(self.pos_format())?;

                // mark coverage offset
                let mark_cov_offset_pos = s.allocate_size($offset::RAW_BYTE_LEN, true)?;

                // ligature coverage offset
                let lig_cov_offset_pos = s.allocate_size($offset::RAW_BYTE_LEN, true)?;

                // mark class count
                let mark_class_count = u16::try_from(mark_class_map.len())
                    .map_err(|_| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_INT_OVERFLOW))?;
                s.embed(mark_class_count)?;

                // mark array offset
                let mark_array_offset_pos = s.allocate_size($offset::RAW_BYTE_LEN, true)?;
                let (mark_glyphs, mark_record_idxes) = intersected_glyphs_and_indices(
                    &mark_coverage,
                    glyph_set,
                    glyph_map,
                    mark_array.mark_count(),
                );

                $offset::serialize_serialize::<CoverageTable>(
                    s,
                    &mark_glyphs,
                    mark_cov_offset_pos,
                )?;
                $offset::serialize_subset(
                    &mark_array,
                    s,
                    plan,
                    (&mark_record_idxes, &mark_class_map),
                    mark_array_offset_pos,
                )?;

                // ligature array offset
                let lig_array_offset_pos = s.allocate_size($offset::RAW_BYTE_LEN, true)?;

                let lig_coverage = self
                    .ligature_coverage()
                    .map_err(|_| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR))?;
                let lig_array = self
                    .ligature_array()
                    .map_err(|_| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR))?;

                // Return non-empty lig glyphs for serializing coverage table
                // lig glyphs might have no anchor points defined for retained class of mark glyphs
                let lig_glyphs = $offset::serialize_subset(
                    &lig_array,
                    s,
                    plan,
                    (&lig_coverage, &mark_class_map),
                    lig_array_offset_pos,
                )?;
                $offset::serialize_serialize::<CoverageTable>(s, &lig_glyphs, lig_cov_offset_pos)
            }
        }
    };
}
subset_mark_lig!(MarkLigPosFormat1, Offset16);
subset_mark_lig!(MarkLigPosFormat2, Offset32);

macro_rules! subset_lig_array {
    ($table:ident, $count:ident) => {
        impl<'a> SubsetTable<'a> for $table<'_> {
            type ArgsForSubset = (&'a CoverageTable<'a>, &'a FastHashMap<u16, u16>);
            type Output = Vec<GlyphId>;
            fn subset(
                &self,
                plan: &Plan,
                s: &mut Serializer,
                args: Self::ArgsForSubset,
            ) -> Result<Vec<GlyphId>, SerializeErrorFlags> {
                if self.min_table_bytes().is_empty() {
                    return Err(s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR));
                }
                let (lig_coverage, mark_class_map) = args;
                let glyph_set = &plan.glyphset_gsub;
                let lig_count = self.ligature_count();

                let mut retained_lig_glyphs = Vec::with_capacity(
                    lig_coverage
                        .population()
                        .min(glyph_set.len() as usize)
                        .min(u32::from(lig_count) as usize),
                );
                // ligature count
                let lig_count_pos = s.allocate_size($count::RAW_BYTE_LEN, true)?;

                let lig_attaches = self.ligature_attaches();
                for_each_intersected_glyph_and_index(
                    lig_coverage,
                    glyph_set,
                    &plan.glyph_map_gsub,
                    lig_count,
                    |i, g| {
                        if !lig_attaches
                            .subset_offset(i as usize, s, plan, mark_class_map)
                            .is_empty()?
                        {
                            retained_lig_glyphs.push(g);
                        }
                        Ok(())
                    },
                )?;

                if retained_lig_glyphs.is_empty() {
                    return Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY);
                }

                let lig_count = $count::try_from(retained_lig_glyphs.len())
                    .map_err(|_| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_INT_OVERFLOW))?;
                s.copy_assign(lig_count_pos, lig_count);
                Ok(retained_lig_glyphs)
            }
        }
    };
}
subset_lig_array!(LigatureArray, u16);
subset_lig_array!(LigatureArray2, Uint24);

macro_rules! subset_lig_attach {
    ($table:ident) => {
        impl<'a> SubsetTable<'a> for $table<'_> {
            type ArgsForSubset = &'a FastHashMap<u16, u16>;
            type Output = ();
            fn subset(
                &self,
                plan: &Plan,
                s: &mut Serializer,
                mark_class_map: &FastHashMap<u16, u16>,
            ) -> Result<(), SerializeErrorFlags> {
                if self.min_table_bytes().is_empty() {
                    return Err(s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR));
                }
                let snap = s.snapshot();
                s.embed(self.component_count())?;

                let component_records = self.component_records();
                let font_data = self.offset_data();
                let mut has_non_empty_rec = false;

                for component_rec in component_records.iter() {
                    let component_rec = component_rec
                        .map_err(|_| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR))?;
                    if !component_rec
                        .subset(plan, s, (font_data, mark_class_map))
                        .is_empty()?
                        && !has_non_empty_rec
                    {
                        has_non_empty_rec = true;
                    }
                }

                if !has_non_empty_rec {
                    s.revert_snapshot(snap);
                    return Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY);
                }
                Ok(())
            }
        }
    };
}
subset_lig_attach!(LigatureAttach);
subset_lig_attach!(LigatureAttach2);

macro_rules! subset_lig_component {
    ($record:ident, $offset:ident) => {
        impl<'a> SubsetTable<'a> for $record<'_> {
            type ArgsForSubset = (FontData<'a>, &'a FastHashMap<u16, u16>);
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
                for i in
                    (0..orig_mark_class_count).filter(|class| mark_class_map.contains_key(class))
                {
                    let anchor_offset_pos = s.allocate_size($offset::RAW_BYTE_LEN, true)?;
                    let Some(lig_anchor) = lig_anchors
                        .get(i as usize)
                        .transpose()
                        .map_err(|_| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR))?
                    else {
                        continue;
                    };
                    $offset::serialize_subset(&lig_anchor, s, plan, (), anchor_offset_pos)?;
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
    };
}
subset_lig_component!(ComponentRecord, Offset16);
subset_lig_component!(ComponentRecord2, Offset24);

impl<'a> SubsetTable<'a> for MarkLigPos<'_> {
    type ArgsForSubset = (&'a SubsetState, &'a FontRef<'a>, &'a FastHashMap<u16, u16>);
    type Output = ();
    fn subset(
        &self,
        plan: &Plan,
        s: &mut Serializer,
        args: Self::ArgsForSubset,
    ) -> Result<(), SerializeErrorFlags> {
        match self {
            Self::Format1(t) => t.subset(plan, s, args),
            Self::Format2(t) => t.subset(plan, s, args),
        }
    }
}

impl CollectVariationIndices for MarkLigPos<'_> {
    fn collect_variation_indices(&self, plan: &Plan, varidx_set: &mut IntSet<u32>) {
        match self {
            Self::Format1(t) => t.collect_variation_indices(plan, varidx_set),
            Self::Format2(t) => t.collect_variation_indices(plan, varidx_set),
        }
    }
}

#[cfg(test)]
mod test {
    use super::*;
    use read_fonts::{types::GlyphId, FontRef, TableProvider};

    #[test]
    fn test_collect_variation_indices_markligpos() {
        use read_fonts::tables::gpos::PositionSubtables;

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
        use read_fonts::tables::gpos::PositionSubtables;

        let font = FontRef::new(include_bytes!("../../test-data/fonts/gpos5_font1.otf")).unwrap();
        let gpos_lookups = font.gpos().unwrap().lookup_list().unwrap();
        let lookup = gpos_lookups.lookups().get(0).unwrap();

        let PositionSubtables::MarkToLig(sub_tables) = lookup.subtables().unwrap() else {
            panic!("Wrong type of lookup table!");
        };
        let markligpos_table = sub_tables.get(0).unwrap();

        let subset_state = SubsetState::default();
        let mut plan = Plan {
            glyph_map_gsub: vec![crate::INVALID_GID; 69],
            ..Default::default()
        };

        plan.glyph_map_gsub[67] = GlyphId::from(1_u32);
        plan.glyph_map_gsub[68] = GlyphId::from(2_u32);

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
        use read_fonts::tables::gpos::PositionSubtables;

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
        let mut plan = Plan {
            glyph_map_gsub: vec![crate::INVALID_GID; 988],
            ..Default::default()
        };

        plan.glyph_map_gsub[30] = GlyphId::from(1_u32);
        plan.glyph_map_gsub[216] = GlyphId::from(2_u32);
        plan.glyph_map_gsub[987] = GlyphId::from(3_u32);

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

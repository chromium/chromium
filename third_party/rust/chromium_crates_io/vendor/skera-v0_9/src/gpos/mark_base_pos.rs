//! impl subset() for MarkBasePos subtable

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
    serialize::{SerializeErrorFlags, SerializeResultEmpty, Serializer},
    CollectVariationIndices, Plan, SubsetState, SubsetTable,
};
use read_fonts::{
    collections::IntSet,
    tables::{
        gpos::{
            BaseArray, BaseArray2, BaseRecord, BaseRecord2, MarkBasePos, MarkBasePosFormat1,
            MarkBasePosFormat2,
        },
        layout::CoverageTable,
    },
    types::{FixedSize, GlyphId, Offset16, Offset24, Offset32, Uint24},
    FontData, FontRef, MinByteRange,
};

macro_rules! collect_mark_base_variations {
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

                let Ok(base_coverage) = self.base_coverage() else {
                    return;
                };
                let Ok(base_array) = self.base_array() else {
                    return;
                };
                let base_array_data = base_array.offset_data();
                let base_records = base_array.base_records();
                let _ = for_each_intersected_coverage_index(
                    &base_coverage,
                    glyph_set,
                    base_array.base_count(),
                    |index| {
                        let base_record = base_records.get(index as usize)?;
                        let anchors = base_record.base_anchors(base_array_data);
                        for class in retained_mark_classes.iter() {
                            if let Some(Ok(anchor)) = anchors.get(class as usize) {
                                anchor.collect_variation_indices(plan, varidx_set);
                            }
                        }
                        Ok::<(), read_fonts::ReadError>(())
                    },
                );
            }
        }
    };
}
collect_mark_base_variations!(MarkBasePosFormat1);
collect_mark_base_variations!(MarkBasePosFormat2);

macro_rules! subset_mark_base {
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

                // base coverage offset
                let base_cov_offset_pos = s.allocate_size($offset::RAW_BYTE_LEN, true)?;

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

                // base array offset
                let base_array_offset_pos = s.allocate_size($offset::RAW_BYTE_LEN, true)?;

                let base_coverage = self
                    .base_coverage()
                    .map_err(|_| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR))?;
                let base_array = self
                    .base_array()
                    .map_err(|_| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR))?;

                // Return non-empty base glyphs for serializing coverage table
                // base glyphs might have no anchor points defined for retained class of mark glyphs
                let base_glyphs = $offset::serialize_subset(
                    &base_array,
                    s,
                    plan,
                    (&base_coverage, &mark_class_map),
                    base_array_offset_pos,
                )?;
                $offset::serialize_serialize::<CoverageTable>(s, &base_glyphs, base_cov_offset_pos)
            }
        }
    };
}
subset_mark_base!(MarkBasePosFormat1, Offset16);
subset_mark_base!(MarkBasePosFormat2, Offset32);

macro_rules! subset_base_array {
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
                let (base_coverage, mark_class_map) = args;
                let glyph_set = &plan.glyphset_gsub;
                let base_count = self.base_count();
                let mut retained_base_glyphs = Vec::with_capacity(
                    base_coverage
                        .population()
                        .min(glyph_set.len() as usize)
                        .min(u32::from(base_count) as usize),
                );
                // base count
                let base_count_pos = s.allocate_size($count::RAW_BYTE_LEN, true)?;

                let font_data = self.offset_data();
                let base_records = self.base_records();
                for_each_intersected_glyph_and_index(
                    base_coverage,
                    glyph_set,
                    &plan.glyph_map_gsub,
                    base_count,
                    |i, g| {
                        let base_record = base_records.get(i as usize).map_err(|_| {
                            s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR)
                        })?;

                        if !base_record
                            .subset(plan, s, (mark_class_map, font_data))
                            .is_empty()?
                        {
                            retained_base_glyphs.push(g);
                        }
                        Ok(())
                    },
                )?;

                if retained_base_glyphs.is_empty() {
                    return Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY);
                }
                let base_count = $count::try_from(retained_base_glyphs.len())
                    .map_err(|_| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_INT_OVERFLOW))?;
                s.copy_assign(base_count_pos, base_count);
                Ok(retained_base_glyphs)
            }
        }
    };
}
subset_base_array!(BaseArray, u16);
subset_base_array!(BaseArray2, Uint24);

macro_rules! subset_base_record {
    ($record:ident, $offset:ident) => {
        impl<'a> SubsetTable<'a> for $record<'_> {
            type ArgsForSubset = (&'a FastHashMap<u16, u16>, FontData<'a>);
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
                for i in
                    (0..orig_mark_class_count).filter(|class| mark_class_map.contains_key(class))
                {
                    let anchor_offset_pos = s.allocate_size($offset::RAW_BYTE_LEN, true)?;
                    let Some(base_anchor) = base_anchors
                        .get(i as usize)
                        .transpose()
                        .map_err(|_| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR))?
                    else {
                        continue;
                    };
                    $offset::serialize_subset(&base_anchor, s, plan, (), anchor_offset_pos)?;
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
    };
}
subset_base_record!(BaseRecord, Offset16);
subset_base_record!(BaseRecord2, Offset24);

impl<'a> SubsetTable<'a> for MarkBasePos<'_> {
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

impl CollectVariationIndices for MarkBasePos<'_> {
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
    fn test_collect_variation_indices_markbasepos() {
        use read_fonts::tables::gpos::PositionSubtables;

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
        use read_fonts::tables::gpos::PositionSubtables;

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
        let mut plan = Plan {
            glyph_map_gsub: vec![crate::INVALID_GID; 406],
            ..Default::default()
        };

        plan.glyph_map_gsub[37] = GlyphId::from(1_u32);
        plan.glyph_map_gsub[390] = GlyphId::from(3_u32);
        plan.glyph_map_gsub[405] = GlyphId::from(4_u32);

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
        use read_fonts::tables::gpos::PositionSubtables;

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
        let mut plan = Plan {
            glyph_map_gsub: vec![crate::INVALID_GID; 410],
            ..Default::default()
        };

        plan.glyph_map_gsub[36] = GlyphId::from(1_u32);
        plan.glyph_map_gsub[37] = GlyphId::from(2_u32);
        plan.glyph_map_gsub[404] = GlyphId::from(4_u32);
        plan.glyph_map_gsub[409] = GlyphId::from(5_u32);

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

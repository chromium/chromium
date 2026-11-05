//! impl subset() for MarkMarkPos subtable

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
            Mark2Array, Mark2Array2, Mark2Record, Mark2Record2, MarkMarkPos, MarkMarkPosFormat1,
            MarkMarkPosFormat2,
        },
        layout::CoverageTable,
    },
    types::{FixedSize, GlyphId, Offset16, Offset24, Offset32},
    FontData, FontRef, MinByteRange,
};

macro_rules! collect_mark_mark_variations {
    ($table:ident) => {
        impl CollectVariationIndices for $table<'_> {
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

                let retained_mark_classes = collect_retained_mark_varidx_and_classes(
                    &mark1_coverage,
                    mark1_records,
                    mark1_array_data,
                    plan,
                    varidx_set,
                );

                let Ok(mark2_coverage) = self.mark2_coverage() else {
                    return;
                };
                let Ok(mark2_array) = self.mark2_array() else {
                    return;
                };
                let mark2_array_data = mark2_array.offset_data();
                let mark2_records = mark2_array.mark2_records();
                let _ = for_each_intersected_coverage_index(
                    &mark2_coverage,
                    glyph_set,
                    mark2_array.mark2_count(),
                    |index| {
                        let record = mark2_records.get(index as usize)?;
                        let anchors = record.mark2_anchors(mark2_array_data);
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
collect_mark_mark_variations!(MarkMarkPosFormat1);
collect_mark_mark_variations!(MarkMarkPosFormat2);

macro_rules! subset_mark_mark {
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
                let mark_class_map =
                    get_mark_class_map(&mark1_coverage, mark1_array.mark_records(), glyph_set);
                if mark_class_map.is_empty() {
                    return Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY);
                }

                if mark1_array.min_table_bytes().is_empty()
                    || mark_class_map
                        .keys()
                        .any(|class| *class >= self.mark_class_count())
                {
                    return Err(s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR));
                }
                // format
                s.embed(self.pos_format())?;

                // mark1 coverage offset
                let mark1_cov_offset_pos = s.allocate_size($offset::RAW_BYTE_LEN, true)?;

                // mark2 coverage offset
                let mark2_cov_offset_pos = s.allocate_size($offset::RAW_BYTE_LEN, true)?;

                // mark class count
                let mark_class_count = u16::try_from(mark_class_map.len())
                    .map_err(|_| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_INT_OVERFLOW))?;
                s.embed(mark_class_count)?;

                // mark1 array offset
                let mark1_array_offset_pos = s.allocate_size($offset::RAW_BYTE_LEN, true)?;
                let (mark1_glyphs, mark1_record_idxes) = intersected_glyphs_and_indices(
                    &mark1_coverage,
                    glyph_set,
                    glyph_map,
                    mark1_array.mark_count(),
                );

                $offset::serialize_serialize::<CoverageTable>(
                    s,
                    &mark1_glyphs,
                    mark1_cov_offset_pos,
                )?;
                $offset::serialize_subset(
                    &mark1_array,
                    s,
                    plan,
                    (&mark1_record_idxes, &mark_class_map),
                    mark1_array_offset_pos,
                )?;

                // mark2 array offset
                let mark2_array_offset_pos = s.allocate_size($offset::RAW_BYTE_LEN, true)?;

                let mark2_coverage = self
                    .mark2_coverage()
                    .map_err(|_| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR))?;
                let mark2_array = self
                    .mark2_array()
                    .map_err(|_| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR))?;

                // Return non-empty mark2 glyphs for serializing coverage table
                // mark2 glyphs might have no anchor points defined for retained class of mark1 glyphs
                let mark2_glyphs = $offset::serialize_subset(
                    &mark2_array,
                    s,
                    plan,
                    (&mark2_coverage, &mark_class_map),
                    mark2_array_offset_pos,
                )?;
                $offset::serialize_serialize::<CoverageTable>(
                    s,
                    &mark2_glyphs,
                    mark2_cov_offset_pos,
                )
            }
        }
    };
}
subset_mark_mark!(MarkMarkPosFormat1, Offset16);
subset_mark_mark!(MarkMarkPosFormat2, Offset32);

macro_rules! subset_mark2_array {
    ($table:ident) => {
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
                let (mark2_coverage, mark_class_map) = args;
                let glyph_set = &plan.glyphset_gsub;
                let mark2_count = self.mark2_count();
                let mut retained_mark2_glyphs = Vec::with_capacity(
                    mark2_coverage
                        .population()
                        .min(glyph_set.len() as usize)
                        .min(mark2_count as usize),
                );
                // mark2 count
                let mark2_count_pos = s.embed(0_u16)?;

                let font_data = self.offset_data();
                let mark2_records = self.mark2_records();
                for_each_intersected_glyph_and_index(
                    mark2_coverage,
                    glyph_set,
                    &plan.glyph_map_gsub,
                    mark2_count,
                    |i, g| {
                        let mark2_record = mark2_records.get(i as usize).map_err(|_| {
                            s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR)
                        })?;

                        if !mark2_record
                            .subset(plan, s, (mark_class_map, font_data))
                            .is_empty()?
                        {
                            retained_mark2_glyphs.push(g);
                        }
                        Ok(())
                    },
                )?;

                if retained_mark2_glyphs.is_empty() {
                    return Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY);
                }
                let mark2_count = u16::try_from(retained_mark2_glyphs.len())
                    .map_err(|_| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_INT_OVERFLOW))?;
                s.copy_assign(mark2_count_pos, mark2_count);
                Ok(retained_mark2_glyphs)
            }
        }
    };
}
subset_mark2_array!(Mark2Array);
subset_mark2_array!(Mark2Array2);

macro_rules! subset_mark2_record {
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
                let mark2_anchors = self.mark2_anchors(font_data);
                let orig_mark_class_count = mark2_anchors.len() as u16;

                let mut has_effective_anchors = false;
                let snap = s.snapshot();
                for i in
                    (0..orig_mark_class_count).filter(|class| mark_class_map.contains_key(class))
                {
                    let anchor_offset_pos = s.allocate_size($offset::RAW_BYTE_LEN, true)?;
                    let Some(mark2_anchor) = mark2_anchors
                        .get(i as usize)
                        .transpose()
                        .map_err(|_| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR))?
                    else {
                        continue;
                    };
                    $offset::serialize_subset(&mark2_anchor, s, plan, (), anchor_offset_pos)?;
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
subset_mark2_record!(Mark2Record, Offset16);
subset_mark2_record!(Mark2Record2, Offset24);

impl<'a> SubsetTable<'a> for MarkMarkPos<'_> {
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

impl CollectVariationIndices for MarkMarkPos<'_> {
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
    fn test_collect_variation_indices_markmarkpos() {
        use read_fonts::tables::gpos::PositionSubtables;

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
        use read_fonts::tables::gpos::PositionSubtables;

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
        let mut plan = Plan {
            glyph_map_gsub: vec![crate::INVALID_GID; 75],
            ..Default::default()
        };

        plan.glyph_map_gsub[66] = GlyphId::from(2_u32);
        plan.glyph_map_gsub[67] = GlyphId::from(3_u32);
        plan.glyph_map_gsub[70] = GlyphId::from(4_u32);
        plan.glyph_map_gsub[74] = GlyphId::from(5_u32);

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

//! impl subset() for CursivePos subtable

#[cfg(test)]
mod extended_tests;

use crate::FastHashMap;
use crate::{
    layout::{for_each_intersected_coverage_index, for_each_intersected_glyph_and_index},
    offset::{SerializeSerialize, SerializeSubset},
    serialize::{SerializeErrorFlags, SerializeResultEmpty, Serializer},
    CollectVariationIndices, Plan, SubsetState, SubsetTable,
};
use read_fonts::{
    collections::IntSet,
    tables::{
        gpos::{
            CursivePos, CursivePosFormat1, CursivePosFormat2, EntryExitRecord, EntryExitRecord2,
        },
        layout::CoverageTable,
    },
    types::{FixedSize, Offset16, Offset24, Offset32, Uint24},
    FontData, FontRef, MinByteRange,
};

macro_rules! subset_cursive_position {
    ($table:ident, $offset:ident, $count:ident) => {
        impl<'a> SubsetTable<'a> for $table<'_> {
            type ArgsForSubset = (&'a SubsetState, &'a FontRef<'a>, &'a FastHashMap<u16, u16>);
            type Output = ();
            fn subset(
                &self,
                plan: &Plan,
                s: &mut Serializer,
                _args: Self::ArgsForSubset,
            ) -> Result<Self::Output, SerializeErrorFlags> {
                if self.coverage_offset().is_null() {
                    return Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY);
                }
                if self.min_table_bytes().is_empty() {
                    return Err(s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR));
                }
                s.embed(self.pos_format())?;

                //cov offset
                let cov_offset_pos = s.allocate_size($offset::RAW_BYTE_LEN, true)?;

                //entry exit count
                let entryexit_count_pos = s.allocate_size($count::RAW_BYTE_LEN, true)?;

                let coverage = self
                    .coverage()
                    .map_err(|_| SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR)?;
                let exit_records = self.entry_exit_record();
                let font_data = self.offset_data();

                let count = u32::from(self.entry_exit_count());
                let glyph_set = &plan.glyphset_gsub;
                let mut retained_glyphs = Vec::with_capacity(
                    coverage
                        .population()
                        .min(glyph_set.len() as usize)
                        .min(count as usize),
                );
                for_each_intersected_glyph_and_index(
                    &coverage,
                    glyph_set,
                    &plan.glyph_map_gsub,
                    count,
                    |index, gid| {
                        let record = exit_records.get(index as usize).ok_or_else(|| {
                            s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR)
                        })?;
                        if !record.subset(plan, s, font_data).is_empty()? {
                            retained_glyphs.push(gid);
                        }
                        Ok(())
                    },
                )?;

                if retained_glyphs.is_empty() {
                    return Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY);
                }
                let entry_exit_count = $count::try_from(retained_glyphs.len())
                    .map_err(|_| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_INT_OVERFLOW))?;
                s.copy_assign(entryexit_count_pos, entry_exit_count);
                $offset::serialize_serialize::<CoverageTable>(s, &retained_glyphs, cov_offset_pos)
            }
        }
    };
}
subset_cursive_position!(CursivePosFormat1, Offset16, u16);
subset_cursive_position!(CursivePosFormat2, Offset32, Uint24);

macro_rules! subset_entry_exit_record {
    ($record:ident, $offset:ident) => {
        impl<'a> SubsetTable<'a> for $record {
            type ArgsForSubset = FontData<'a>;
            type Output = ();
            fn subset(
                &self,
                plan: &Plan,
                s: &mut Serializer,
                font_data: FontData,
            ) -> Result<Self::Output, SerializeErrorFlags> {
                if self.entry_anchor_offset().is_null() && self.exit_anchor_offset().is_null() {
                    return Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY);
                }
                let entry_offset_pos = s.allocate_size($offset::RAW_BYTE_LEN, true)?;
                if let Some(entry_anchor) = self
                    .entry_anchor(font_data)
                    .transpose()
                    .map_err(|_| SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR)?
                {
                    $offset::serialize_subset(&entry_anchor, s, plan, (), entry_offset_pos)?;
                }

                let exit_offset_pos = s.allocate_size($offset::RAW_BYTE_LEN, true)?;
                if let Some(exit_anchor) = self
                    .exit_anchor(font_data)
                    .transpose()
                    .map_err(|_| SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR)?
                {
                    $offset::serialize_subset(&exit_anchor, s, plan, (), exit_offset_pos)?;
                }
                Ok(())
            }
        }
    };
}
subset_entry_exit_record!(EntryExitRecord, Offset16);
subset_entry_exit_record!(EntryExitRecord2, Offset24);

macro_rules! collect_cursive_variations {
    ($table:ident) => {
        impl CollectVariationIndices for $table<'_> {
            fn collect_variation_indices(&self, plan: &Plan, varidx_set: &mut IntSet<u32>) {
                let Ok(coverage) = self.coverage() else {
                    return;
                };

                let font_data = self.offset_data();
                let glyph_set = &plan.glyphset_gsub;
                let entry_exit_records = self.entry_exit_record();
                let _ = for_each_intersected_coverage_index::<()>(
                    &coverage,
                    glyph_set,
                    self.entry_exit_count(),
                    |index| {
                        let rec = entry_exit_records.get(index as usize).ok_or(())?;
                        if let Some(Ok(anchor)) = rec.entry_anchor(font_data) {
                            anchor.collect_variation_indices(plan, varidx_set);
                        }
                        if let Some(Ok(anchor)) = rec.exit_anchor(font_data) {
                            anchor.collect_variation_indices(plan, varidx_set);
                        }
                        Ok(())
                    },
                );
            }
        }
    };
}
collect_cursive_variations!(CursivePosFormat1);
collect_cursive_variations!(CursivePosFormat2);

impl<'a> SubsetTable<'a> for CursivePos<'_> {
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

impl CollectVariationIndices for CursivePos<'_> {
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
    fn test_subset_cursive_pos() {
        use read_fonts::tables::gpos::PositionSubtables;

        let font = FontRef::new(include_bytes!("../../test-data/fonts/Amiri-Regular.ttf")).unwrap();
        let gpos_lookups = font.gpos().unwrap().lookup_list().unwrap();
        let lookup = gpos_lookups.lookups().get(57).unwrap();

        let PositionSubtables::Cursive(sub_tables) = lookup.subtables().unwrap() else {
            panic!("Wrong type of lookup table!");
        };
        let cursivepos_table = sub_tables.get(0).unwrap();

        let subset_state = SubsetState::default();
        let mut plan = Plan {
            glyph_map_gsub: vec![crate::INVALID_GID; 3099],
            ..Default::default()
        };

        plan.glyph_map_gsub[1803] = GlyphId::from(2_u32);
        plan.glyph_map_gsub[3098] = GlyphId::from(4_u32);
        plan.glyphset_gsub.insert(GlyphId::from(1803_u32));
        plan.glyphset_gsub.insert(GlyphId::from(3098_u32));

        let mut s = Serializer::new(1024);
        assert_eq!(s.start_serialize(), Ok(()));

        cursivepos_table
            .subset(&plan, &mut s, (&subset_state, &font, &plan.gpos_lookups))
            .unwrap();
        assert!(!s.in_error());
        s.end_serialize();

        let subsetted_data = s.copy_bytes();
        let expected_data: [u8; 34] = [
            0x00, 0x01, 0x00, 0x0e, 0x00, 0x02, 0x00, 0x00, 0x00, 0x1c, 0x00, 0x00, 0x00, 0x16,
            0x00, 0x01, 0x00, 0x02, 0x00, 0x02, 0x00, 0x04, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x01, 0x00, 0x00, 0x00, 0xfe,
        ];

        assert_eq!(subsetted_data, expected_data);
    }

    #[test]
    fn cursive_subset_removes_empty_records_from_coverage() {
        use font_test_data::bebuffer::BeBuffer;
        use read_fonts::FontRead;
        let bytes = BeBuffer::new()
            .push(1u16)
            .push(24u16)
            .push(3u16)
            .extend([0u16, 0, 18, 0, 0, 18])
            .push(1u16)
            .push(100i16)
            .push(200i16)
            .push(1u16)
            .push(3u16)
            .extend([10u16, 20, 30])
            .to_vec();
        let table = CursivePosFormat1::read(FontData::new(&bytes)).unwrap();
        let font = FontRef::new(font_test_data::NOTOSERIFHEBREW_AUTOHINT_METRICS).unwrap();
        for keep in [vec![10u32, 20, 30], vec![10]] {
            let mut plan = Plan {
                glyph_map_gsub: vec![crate::INVALID_GID; 31],
                ..Default::default()
            };
            for &gid in &keep {
                plan.glyphset_gsub.insert(GlyphId::new(gid));
                plan.glyph_map_gsub[gid as usize] = GlyphId::new(gid / 10);
            }
            let mut s = Serializer::new(1024);
            s.start_serialize().unwrap();
            let result = table.subset(
                &plan,
                &mut s,
                (&SubsetState::default(), &font, &plan.gpos_lookups),
            );
            if keep.len() == 1 {
                assert_eq!(result, Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY));
                continue;
            }
            result.unwrap();
            s.end_serialize();
            let output = s.copy_bytes();
            let table = CursivePosFormat1::read(FontData::new(&output)).unwrap();
            assert_eq!(table.entry_exit_count(), 2);
            assert_eq!(
                table.coverage().unwrap().iter().collect::<Vec<_>>(),
                [GlyphId::new(2), GlyphId::new(3)]
            );
            let records = table.entry_exit_record();
            assert!(records[0].entry_anchor(table.offset_data()).is_some());
            assert!(records[1].exit_anchor(table.offset_data()).is_some());
        }
    }
}

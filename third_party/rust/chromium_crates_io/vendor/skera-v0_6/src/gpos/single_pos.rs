//! impl subset() for SinglePos subtable

use crate::fnv::FnvHashMap;
use crate::{
    gpos::value_record::{collect_variation_indices, compute_effective_format},
    layout::{intersected_coverage_indices, intersected_glyphs_and_indices},
    offset::SerializeSerialize,
    serialize::{SerializeErrorFlags, Serializer},
    CollectVariationIndices, Plan, Serialize, SubsetFlags, SubsetState, SubsetTable,
};
use write_fonts::{
    read::{
        collections::IntSet,
        tables::{
            gpos::{SinglePos, SinglePosFormat1, SinglePosFormat2, ValueFormat, ValueRecord},
            layout::CoverageTable,
        },
        types::GlyphId,
        FontData, FontRef, TableProvider,
    },
    types::Offset16,
};

impl<'a> SubsetTable<'a> for SinglePos<'_> {
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

impl<'a> SubsetTable<'a> for SinglePosFormat1<'_> {
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
        let coverage = self
            .coverage()
            .map_err(|_| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR))?;
        let retained_glyphs: Vec<GlyphId> = coverage
            .intersect_set(&plan.glyphset_gsub)
            .iter()
            .filter_map(|g| plan.glyph_map_gsub.get(&g))
            .copied()
            .collect();
        if retained_glyphs.is_empty() {
            return Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY);
        }

        let value_record = self.value_record();
        let new_format = if plan
            .subset_flags
            .contains(SubsetFlags::SUBSET_FLAGS_NO_HINTING)
        {
            let (state, font) = args;
            // do not strip hints for VF unless it has no GDEF varstore after subsetting
            let strip_hints = if font.fvar().is_ok() {
                !state.has_gdef_varstore
            } else {
                true
            };
            compute_effective_format(&value_record, strip_hints, true)
        } else {
            self.value_format()
        };

        SinglePosFormat1::serialize(
            s,
            (
                &retained_glyphs,
                &value_record,
                new_format,
                plan,
                self.offset_data(),
            ),
        )
    }
}

impl<'a> Serialize<'a> for SinglePosFormat1<'_> {
    type Args = (
        &'a [GlyphId],
        &'a ValueRecord,
        ValueFormat,
        &'a Plan,
        FontData<'a>,
    );
    fn serialize(s: &mut Serializer, args: Self::Args) -> Result<(), SerializeErrorFlags> {
        // format
        s.embed(1_u16)?;

        // coverage offset
        let cov_offset_pos = s.embed(0_u16)?;

        let (glyphs, value_record, value_format, plan, font_data) = args;
        //value format
        s.embed(value_format)?;
        //value record
        value_record.subset(plan, s, (value_format, font_data))?;

        Offset16::serialize_serialize::<CoverageTable>(s, glyphs, cov_offset_pos)
    }
}

fn compute_new_value_format(
    plan: &Plan,
    has_gdef_varstore: bool,
    font: &FontRef,
    value_records: impl IntoIterator<Item = ValueRecord>,
) -> ValueFormat {
    // TODO: support instancing
    let mut new_format = ValueFormat::empty();
    if plan
        .subset_flags
        .contains(SubsetFlags::SUBSET_FLAGS_NO_HINTING)
    {
        // do not strip hints for VF unless it has no GDEF varstore after subsetting
        let strip_hints = if font.fvar().is_ok() {
            !has_gdef_varstore
        } else {
            true
        };

        for record in value_records {
            new_format |= compute_effective_format(&record, strip_hints, true);
        }
    } else if let Some(rec) = value_records.into_iter().next() {
        new_format = rec.format;
    }

    new_format
}

impl<'a> SubsetTable<'a> for SinglePosFormat2<'_> {
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
        let coverage = self
            .coverage()
            .map_err(|_| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR))?;

        let (retained_glyphs, retained_rec_idxes) =
            intersected_glyphs_and_indices(&coverage, &plan.glyphset_gsub, &plan.glyph_map_gsub);

        if retained_glyphs.is_empty() {
            return Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY);
        }

        let (state, font) = args;
        let value_records = self.value_records();
        let it = value_records
            .iter()
            .enumerate()
            .filter(|&(i, ref _rec)| retained_rec_idxes.contains(i as u16))
            .filter_map(|(_i, rec)| rec.ok());
        let new_format = compute_new_value_format(plan, state.has_gdef_varstore, font, it);

        let Ok(first_retained_rec) =
            value_records.get(retained_rec_idxes.first().unwrap() as usize)
        else {
            return Err(s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR));
        };

        let mut table_format = 1;
        for i in retained_rec_idxes.iter().skip(1) {
            let Ok(rec) = value_records.get(i as usize) else {
                return Err(s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR));
            };

            if rec != first_retained_rec {
                table_format = 2;
                break;
            }
        }

        if table_format == 1 {
            SinglePosFormat1::serialize(
                s,
                (
                    &retained_glyphs,
                    &first_retained_rec,
                    new_format,
                    plan,
                    self.offset_data(),
                ),
            )
        } else {
            SinglePosFormat2::serialize(
                s,
                (
                    &retained_glyphs,
                    new_format,
                    self,
                    &retained_rec_idxes,
                    plan,
                ),
            )
        }
    }
}

impl<'a> Serialize<'a> for SinglePosFormat2<'_> {
    type Args = (
        &'a [GlyphId],
        ValueFormat,
        &'a SinglePosFormat2<'a>,
        &'a IntSet<u16>,
        &'a Plan,
    );
    fn serialize(s: &mut Serializer, args: Self::Args) -> Result<(), SerializeErrorFlags> {
        // format
        s.embed(2_u16)?;

        // coverage offset
        let cov_offset_pos = s.embed(0_u16)?;

        let (glyphs, value_format, table, retained_rec_idxes, plan) = args;
        //value format
        s.embed(value_format)?;

        //value count
        let value_count = glyphs.len();
        s.embed(value_count as u16)?;

        let value_records = table.value_records();
        let font_data = table.offset_data();
        for i in retained_rec_idxes.iter() {
            let value_record = value_records
                .get(i as usize)
                .map_err(|_| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR))?;
            value_record.subset(plan, s, (value_format, font_data))?;
        }

        Offset16::serialize_serialize::<CoverageTable>(s, glyphs, cov_offset_pos)
    }
}

impl CollectVariationIndices for SinglePos<'_> {
    fn collect_variation_indices(&self, plan: &Plan, varidx_set: &mut IntSet<u32>) {
        match self {
            Self::Format1(item) => item.collect_variation_indices(plan, varidx_set),
            Self::Format2(item) => item.collect_variation_indices(plan, varidx_set),
        }
    }
}

impl CollectVariationIndices for SinglePosFormat1<'_> {
    fn collect_variation_indices(&self, plan: &Plan, varidx_set: &mut IntSet<u32>) {
        if !self
            .value_format()
            .intersects(ValueFormat::ANY_DEVICE_OR_VARIDX)
        {
            return;
        }
        collect_variation_indices(&self.value_record(), self.offset_data(), plan, varidx_set);
    }
}

impl CollectVariationIndices for SinglePosFormat2<'_> {
    fn collect_variation_indices(&self, plan: &Plan, varidx_set: &mut IntSet<u32>) {
        if !self
            .value_format()
            .intersects(ValueFormat::ANY_DEVICE_OR_VARIDX)
        {
            return;
        }

        let Ok(coverage) = self.coverage() else {
            return;
        };
        let value_records = self.value_records();
        let glyph_set = &plan.glyphset_gsub;
        let value_record_idxes = intersected_coverage_indices(&coverage, glyph_set);
        for i in value_record_idxes.iter() {
            let Ok(value_record) = value_records.get(i as usize) else {
                return;
            };
            collect_variation_indices(&value_record, self.offset_data(), plan, varidx_set);
        }
    }
}

#[cfg(test)]
mod test {
    use super::*;
    use write_fonts::read::{FontRef, TableProvider};

    #[test]
    fn test_subset_gpos_format1() {
        use write_fonts::read::tables::gpos::PositionSubtables;

        let font = FontRef::new(include_bytes!("../../test-data/fonts/Amiri-Regular.ttf")).unwrap();
        let gpos_lookups = font.gpos().unwrap().lookup_list().unwrap();
        let lookup = gpos_lookups.lookups().get(6).unwrap();

        let PositionSubtables::Single(sub_tables) = lookup.subtables().unwrap() else {
            panic!("Wrong type of lookup table!");
        };
        let singlepos_table = sub_tables.get(0).unwrap();

        let subset_state = SubsetState::default();
        let mut plan = Plan::default();

        plan.glyph_map_gsub
            .insert(GlyphId::from(5987_u32), GlyphId::from(3_u32));
        plan.glyphset_gsub.insert(GlyphId::from(5987_u32));

        let mut s = Serializer::new(1024);
        assert_eq!(s.start_serialize(), Ok(()));

        singlepos_table
            .subset(&plan, &mut s, (&subset_state, &font, &plan.gpos_lookups))
            .unwrap();
        assert!(!s.in_error());
        s.end_serialize();

        let subsetted_data = s.copy_bytes();
        let expected_data: [u8; 16] = [
            0x00, 0x01, 0x00, 0x0a, 0x00, 0x05, 0xfb, 0xc9, 0xfe, 0xdc, 0x00, 0x01, 0x00, 0x01,
            0x00, 0x03,
        ];

        assert_eq!(subsetted_data, expected_data);
    }

    #[test]
    fn test_subset_gpos_format2() {
        use write_fonts::read::tables::gpos::PositionSubtables;

        let font = FontRef::new(include_bytes!("../../test-data/fonts/Amiri-Regular.ttf")).unwrap();
        let gpos_lookups = font.gpos().unwrap().lookup_list().unwrap();
        let lookup = gpos_lookups.lookups().get(36).unwrap();

        let PositionSubtables::Single(sub_tables) = lookup.subtables().unwrap() else {
            panic!("Wrong type of lookup table!");
        };
        let singlepos_table = sub_tables.get(4).unwrap();

        let subset_state = SubsetState::default();
        let mut plan = Plan::default();

        // test case 1: subsetted output is still format 2
        plan.glyph_map_gsub
            .insert(GlyphId::from(2270_u32), GlyphId::from(3_u32));
        plan.glyph_map_gsub
            .insert(GlyphId::from(2349_u32), GlyphId::from(4_u32));
        plan.glyphset_gsub.insert(GlyphId::from(2270_u32));
        plan.glyphset_gsub.insert(GlyphId::from(2349_u32));

        let mut s = Serializer::new(1024);
        assert_eq!(s.start_serialize(), Ok(()));

        singlepos_table
            .subset(&plan, &mut s, (&subset_state, &font, &plan.gpos_lookups))
            .unwrap();
        assert!(!s.in_error());
        s.end_serialize();

        let subsetted_data = s.copy_bytes();
        let expected_data: [u8; 20] = [
            0x00, 0x02, 0x00, 0x0c, 0x00, 0x04, 0x00, 0x02, 0x00, 0xc3, 0x00, 0xfe, 0x00, 0x01,
            0x00, 0x02, 0x00, 0x03, 0x00, 0x04,
        ];

        assert_eq!(subsetted_data, expected_data);

        // test case 2: subsetted output is optimized to format 1
        plan.glyph_map_gsub.clear();
        plan.glyph_map_gsub
            .insert(GlyphId::from(2270_u32), GlyphId::from(3_u32));
        plan.glyph_map_gsub
            .insert(GlyphId::from(6179_u32), GlyphId::from(4_u32));

        plan.glyphset_gsub.clear();
        plan.glyphset_gsub.insert(GlyphId::from(2270_u32));
        plan.glyphset_gsub.insert(GlyphId::from(6179_u32));

        let mut s = Serializer::new(1024);
        assert_eq!(s.start_serialize(), Ok(()));

        singlepos_table
            .subset(&plan, &mut s, (&subset_state, &font, &plan.gpos_lookups))
            .unwrap();
        assert!(!s.in_error());
        s.end_serialize();

        let subsetted_data = s.copy_bytes();
        let expected_data: [u8; 16] = [
            0x00, 0x01, 0x00, 0x08, 0x00, 0x04, 0x00, 0xc3, 0x00, 0x01, 0x00, 0x02, 0x00, 0x03,
            0x00, 0x04,
        ];

        assert_eq!(subsetted_data, expected_data);
    }
}

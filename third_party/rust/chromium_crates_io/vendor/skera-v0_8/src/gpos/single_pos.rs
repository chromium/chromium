//! impl subset() for SinglePos subtable

use crate::fnv::FnvHashMap;
use crate::{
    gpos::value_record::{compute_effective_format, compute_record_len},
    layout::{intersected_glyphs_and_indices, map_gsub_glyph},
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
        FontData, FontRef, ReadError, TableProvider,
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
            .filter_map(|g| map_gsub_glyph(&plan.glyph_map_gsub, g))
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
                .map_err(|_| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR))?
        } else {
            self.value_format()
        };

        SinglePosFormat1::serialize(s, (&retained_glyphs, value_record, new_format, plan))
    }
}

impl<'a> Serialize<'a> for SinglePosFormat1<'_> {
    type Args = (&'a [GlyphId], ValueRecord<'a>, ValueFormat, &'a Plan);
    fn serialize(s: &mut Serializer, args: Self::Args) -> Result<(), SerializeErrorFlags> {
        // format
        s.embed(1_u16)?;

        // coverage offset
        let cov_offset_pos = s.embed(0_u16)?;

        let (glyphs, value_record, value_format, plan) = args;
        //value format
        s.embed(value_format)?;
        //value record
        value_record.subset(plan, s, value_format)?;

        Offset16::serialize_serialize::<CoverageTable>(s, glyphs, cov_offset_pos)
    }
}

pub(crate) struct SinglePosInfo<'a> {
    value_format: ValueFormat,
    records_offset: usize,
    record_size: usize,
    font_data: FontData<'a>,
    new_format: ValueFormat,
}

fn compute_new_value_format(
    singlepos_info: &mut SinglePosInfo,
    plan: &Plan,
    has_gdef_varstore: bool,
    font: &FontRef,
    retained_rec_idxes: &IntSet<u16>,
) -> Result<(), ReadError> {
    // TODO: support instancing
    let (value_format, records_offset, record_size, font_data, new_format) = (
        singlepos_info.value_format,
        singlepos_info.records_offset,
        singlepos_info.record_size,
        singlepos_info.font_data,
        &mut singlepos_info.new_format,
    );
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

        for i in retained_rec_idxes.iter() {
            let offset = records_offset + i as usize * record_size;
            let value_record = ValueRecord::new(font_data, offset, value_format);
            *new_format |= compute_effective_format(&value_record, strip_hints, true)?;
        }
    } else {
        *new_format = value_format;
    }

    Ok(())
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
        let value_format = self.value_format();
        let records_offset = self.value_count_byte_range().end;
        let record_size = compute_record_len(value_format);
        let font_data = self.offset_data();
        let mut singlepos_info = SinglePosInfo {
            value_format,
            records_offset,
            record_size,
            font_data,
            new_format: ValueFormat::empty(),
        };

        compute_new_value_format(
            &mut singlepos_info,
            plan,
            state.has_gdef_varstore,
            font,
            &retained_rec_idxes,
        )
        .map_err(|_| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR))?;

        let Some(first_rec_idx) = retained_rec_idxes.first() else {
            return Err(s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_OTHER));
        };
        let first_retained_rec = ValueRecord::new(
            font_data,
            records_offset + first_rec_idx as usize * record_size,
            value_format,
        );

        let table_format = if retained_rec_idxes
            .iter()
            .skip(1)
            .map(|i| {
                ValueRecord::new(
                    font_data,
                    records_offset + i as usize * record_size,
                    value_format,
                )
            })
            .all(|rec| rec == first_retained_rec)
        {
            1
        } else {
            2
        };

        if table_format == 1 {
            SinglePosFormat1::serialize(
                s,
                (
                    &retained_glyphs,
                    first_retained_rec,
                    singlepos_info.new_format,
                    plan,
                ),
            )
        } else {
            SinglePosFormat2::serialize(
                s,
                (&retained_glyphs, &singlepos_info, &retained_rec_idxes, plan),
            )
        }
    }
}

impl<'a> Serialize<'a> for SinglePosFormat2<'_> {
    type Args = (
        &'a [GlyphId],
        &'a SinglePosInfo<'a>,
        &'a IntSet<u16>,
        &'a Plan,
    );
    fn serialize(s: &mut Serializer, args: Self::Args) -> Result<(), SerializeErrorFlags> {
        // format
        s.embed(2_u16)?;

        // coverage offset
        let cov_offset_pos = s.embed(0_u16)?;

        let (glyphs, singlepos_info, retained_rec_idxes, plan) = args;
        let (value_format, records_offset, record_size, font_data, new_format) = (
            singlepos_info.value_format,
            singlepos_info.records_offset,
            singlepos_info.record_size,
            singlepos_info.font_data,
            singlepos_info.new_format,
        );
        //value format
        s.embed(new_format)?;

        //value count
        let value_count = glyphs.len();
        s.embed(value_count as u16)?;

        for i in retained_rec_idxes.iter() {
            let offset = records_offset + i as usize * record_size;
            let value_record = ValueRecord::new(font_data, offset, value_format);
            value_record.subset(plan, s, new_format)?;
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
        self.value_record()
            .collect_variation_indices(plan, varidx_set);
    }
}

impl CollectVariationIndices for SinglePosFormat2<'_> {
    fn collect_variation_indices(&self, plan: &Plan, varidx_set: &mut IntSet<u32>) {
        let value_format = self.value_format();
        if !value_format.intersects(ValueFormat::ANY_DEVICE_OR_VARIDX) {
            return;
        }

        let Ok(coverage) = self.coverage() else {
            return;
        };
        let glyph_set = &plan.glyphset_gsub;
        let value_count = self.value_count();
        let record_size = compute_record_len(value_format);
        let records_offset = self.value_count_byte_range().end;
        let font_data = self.offset_data();

        let bit_storage = 16 - value_count.leading_zeros() as u64;
        if value_count as u64 > glyph_set.len() * bit_storage {
            for idx in glyph_set.iter().filter_map(|g| coverage.get(g)) {
                let offset = records_offset + idx as usize * record_size;
                let value_record = ValueRecord::new(font_data, offset, value_format);
                value_record.collect_variation_indices(plan, varidx_set);
            }
        } else {
            for i in coverage
                .iter()
                .enumerate()
                .filter_map(|(idx, g)| glyph_set.contains(GlyphId::from(g)).then_some(idx))
            {
                let offset = records_offset + i * record_size;
                let value_record = ValueRecord::new(font_data, offset, value_format);
                value_record.collect_variation_indices(plan, varidx_set);
            }
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
        let mut plan = Plan {
            glyph_map_gsub: vec![crate::INVALID_GID; 5988],
            ..Default::default()
        };

        plan.glyph_map_gsub[5987] = GlyphId::from(3_u32);
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
        let mut plan = Plan {
            glyph_map_gsub: vec![crate::INVALID_GID; 2350],
            ..Default::default()
        };

        // test case 1: subsetted output is still format 2
        plan.glyph_map_gsub[2270] = GlyphId::from(3_u32);
        plan.glyph_map_gsub[2349] = GlyphId::from(4_u32);
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
        plan.glyph_map_gsub = vec![crate::INVALID_GID; 6180];
        plan.glyph_map_gsub[2270] = GlyphId::from(3_u32);
        plan.glyph_map_gsub[6179] = GlyphId::from(4_u32);

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

    #[test]
    fn test_subset_gpos_format2_with_variation_indices() {
        use write_fonts::read::{FontData, FontRead};

        // Construct a SinglePosFormat2 table with VariationIndex devices:
        // - Coverage: 3 glyphs (10, 20, 30)
        // - ValueFormat: X_ADVANCE | X_ADVANCE_DEVICE (0x0044)
        // - 3 ValueRecords with distinct values and variation index pointers:
        //   Record 0 (GID 10): x_advance = 100, varidx = (outer: 1, inner: 2) -> 0x00010002
        //   Record 1 (GID 20): x_advance = 200, varidx = (outer: 3, inner: 4) -> 0x00030004
        //   Record 2 (GID 30): x_advance = 300, varidx = (outer: 5, inner: 6) -> 0x00050006
        #[rustfmt::skip]
        let raw_table: [u8; 48] = [
            0x00, 0x02, 0x00, 0x14, 0x00, 0x44, 0x00, 0x03,
            0x00, 0x64, 0x00, 0x1e, 0x00, 0xc8, 0x00, 0x24,
            0x01, 0x2c, 0x00, 0x2a, 0x00, 0x01, 0x00, 0x03,
            0x00, 0x0a, 0x00, 0x14, 0x00, 0x1e, 0x00, 0x01,
            0x00, 0x02, 0x80, 0x00, 0x00, 0x03, 0x00, 0x04,
            0x80, 0x00, 0x00, 0x05, 0x00, 0x06, 0x80, 0x00,
        ];

        let singlepos = SinglePosFormat2::read(FontData::new(&raw_table)).unwrap();

        // 1. Test CollectVariationIndices (branch 1: value_count > len * bit_storage)
        let mut plan = Plan::default();
        plan.glyphset_gsub.insert(GlyphId::from(20_u32));
        let mut varidx_set = IntSet::empty();
        singlepos.collect_variation_indices(&plan, &mut varidx_set);
        assert_eq!(varidx_set.len(), 1);
        assert!(varidx_set.contains(0x00030004));

        // 2. Test CollectVariationIndices (branch 2: else branch)
        plan.glyphset_gsub.insert(GlyphId::from(10_u32));
        plan.glyphset_gsub.insert(GlyphId::from(30_u32));
        varidx_set.clear();
        singlepos.collect_variation_indices(&plan, &mut varidx_set);
        assert_eq!(varidx_set.len(), 3);
        assert!(varidx_set.contains(0x00010002));
        assert!(varidx_set.contains(0x00030004));
        assert!(varidx_set.contains(0x00050006));

        // 3. Test Subsetting: retaining glyphs 20 and 30 (output remains Format 2)
        let font = FontRef::new(include_bytes!("../../test-data/fonts/Amiri-Regular.ttf")).unwrap();
        let subset_state = SubsetState::default();

        let mut plan = Plan {
            glyph_map_gsub: vec![crate::INVALID_GID; 35],
            ..Default::default()
        };
        plan.glyph_map_gsub[20] = GlyphId::from(1_u32);
        plan.glyph_map_gsub[30] = GlyphId::from(2_u32);
        plan.glyphset_gsub.insert(GlyphId::from(20_u32));
        plan.glyphset_gsub.insert(GlyphId::from(30_u32));

        // Map old varidx -> new varidx
        plan.layout_varidx_delta_map
            .insert(0x00030004, (0x00070008, 0));
        plan.layout_varidx_delta_map
            .insert(0x00050006, (0x0009000a, 0));

        let mut s = Serializer::new(1024);
        assert_eq!(s.start_serialize(), Ok(()));
        singlepos
            .subset(&plan, &mut s, (&subset_state, &font))
            .unwrap();
        assert!(!s.in_error());
        s.end_serialize();

        let subsetted_data = s.copy_bytes();
        #[rustfmt::skip]
        let expected_format2: [u8; 36] = [
            // pos_format=2, cov_offset=16, value_format=0x0044, count=2
            0x00, 0x02, 0x00, 0x10, 0x00, 0x44, 0x00, 0x02,
            // record 0: x_advance=200, dev_offset=30
            0x00, 0xc8, 0x00, 0x1e,
            // record 1: x_advance=300, dev_offset=24
            0x01, 0x2c, 0x00, 0x18,
            // coverage: format 1, count 2, glyphs 1, 2
            0x00, 0x01, 0x00, 0x02, 0x00, 0x01, 0x00, 0x02,
            // VariationIndex 1: remapped outer=9, inner=10, delta_format=0x8000 (offset 24)
            0x00, 0x09, 0x00, 0x0a, 0x80, 0x00,
            // VariationIndex 0: remapped outer=7, inner=8, delta_format=0x8000 (offset 30)
            0x00, 0x07, 0x00, 0x08, 0x80, 0x00,
        ];
        assert_eq!(subsetted_data, expected_format2);

        // 4. Test Subsetting: retaining only glyph 20 (output optimized to Format 1)
        let mut plan = Plan {
            glyph_map_gsub: vec![crate::INVALID_GID; 35],
            ..Default::default()
        };
        plan.glyph_map_gsub[20] = GlyphId::from(1_u32);
        plan.glyphset_gsub.insert(GlyphId::from(20_u32));
        plan.layout_varidx_delta_map
            .insert(0x00030004, (0x00070008, 0));

        let mut s = Serializer::new(1024);
        assert_eq!(s.start_serialize(), Ok(()));
        singlepos
            .subset(&plan, &mut s, (&subset_state, &font))
            .unwrap();
        assert!(!s.in_error());
        s.end_serialize();

        let subsetted_data = s.copy_bytes();
        #[rustfmt::skip]
        let expected_format1: [u8; 22] = [
            // pos_format=1, cov_offset=10, value_format=0x0044
            0x00, 0x01, 0x00, 0x0a, 0x00, 0x44,
            // record: x_advance=200, dev_offset=16
            0x00, 0xc8, 0x00, 0x10,
            // coverage: format 1, count 1, glyph 1
            0x00, 0x01, 0x00, 0x01, 0x00, 0x01,
            // VariationIndex: remapped outer=7, inner=8, delta_format=0x8000
            0x00, 0x07, 0x00, 0x08, 0x80, 0x00,
        ];
        assert_eq!(subsetted_data, expected_format1);
    }
}

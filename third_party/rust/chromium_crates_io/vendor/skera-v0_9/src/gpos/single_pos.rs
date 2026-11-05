//! impl subset() for SinglePos subtable

#[cfg(test)]
mod extended_tests;

use crate::FastHashMap;
use crate::{
    gpos::value_record::{compute_effective_format, compute_record_len},
    layout::{for_each_intersected_glyph_and_index, map_gsub_glyph},
    offset::SerializeSerialize,
    serialize::{SerializeErrorFlags, Serializer},
    CollectVariationIndices, Plan, Serialize, SubsetFlags, SubsetState, SubsetTable,
};
use read_fonts::{
    collections::IntSet,
    tables::{
        gpos::{
            SinglePos, SinglePosFormat1, SinglePosFormat2, SinglePosFormat3, SinglePosFormat4,
            ValueFormat, ValueRecord,
        },
        layout::CoverageTable,
    },
    types::GlyphId,
    types::{FixedSize, Offset16, Offset32, Uint24},
    FontData, FontRef, MinByteRange, ReadError, TableProvider,
};

impl<'a> SubsetTable<'a> for SinglePos<'_> {
    type ArgsForSubset = (&'a SubsetState, &'a FontRef<'a>, &'a FastHashMap<u16, u16>);
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
            Self::Format3(item) => item.subset(plan, s, args),
            Self::Format4(item) => item.subset(plan, s, args),
        }
    }
}

macro_rules! single_value_position {
    ($table:ident, $format:literal, $offset:ident) => {
        impl<'a> SubsetTable<'a> for $table<'_> {
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
                if self.min_table_bytes().is_empty() {
                    return Err(s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR));
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

                $table::serialize(s, (&retained_glyphs, value_record, new_format, plan))
            }
        }

        impl<'a> Serialize<'a> for $table<'_> {
            type Args = (&'a [GlyphId], ValueRecord<'a>, ValueFormat, &'a Plan);
            fn serialize(s: &mut Serializer, args: Self::Args) -> Result<(), SerializeErrorFlags> {
                // format
                s.embed($format)?;

                // coverage offset
                let cov_offset_pos = s.allocate_size($offset::RAW_BYTE_LEN, true)?;

                let (glyphs, value_record, value_format, plan) = args;
                //value format
                s.embed(value_format)?;
                //value record
                value_record.subset(plan, s, value_format)?;

                $offset::serialize_serialize::<CoverageTable>(s, glyphs, cov_offset_pos)
            }
        }
    };
}
single_value_position!(SinglePosFormat1, 1u16, Offset16);
single_value_position!(SinglePosFormat3, 3u16, Offset32);

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
    retained_rec_idxes: &IntSet<u32>,
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

macro_rules! array_value_position {
    ($table:ident, $single:ident, $format:literal, $offset:ident, $count:ident) => {
        impl<'a> SubsetTable<'a> for $table<'_> {
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
                if self.min_table_bytes().is_empty() {
                    return Err(s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR));
                }
                let coverage = self
                    .coverage()
                    .map_err(|_| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR))?;

                let limit = if $format == 2u16 {
                    u32::from(self.value_count())
                } else {
                    u32::MAX
                };
                let mut retained_glyphs = Vec::with_capacity(
                    coverage
                        .population()
                        .min(plan.glyphset_gsub.len() as usize)
                        .min(limit as usize),
                );
                let mut retained_rec_idxes = IntSet::empty();
                for_each_intersected_glyph_and_index::<()>(
                    &coverage,
                    &plan.glyphset_gsub,
                    &plan.glyph_map_gsub,
                    limit,
                    |index, gid| {
                        retained_glyphs.push(gid);
                        retained_rec_idxes.insert(index);
                        Ok(())
                    },
                )
                .unwrap();

                if retained_glyphs.is_empty() {
                    return Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY);
                }

                if retained_rec_idxes
                    .last()
                    .is_some_and(|i| i >= u32::from(self.value_count()))
                {
                    return Err(s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR));
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
                    $single::serialize(
                        s,
                        (
                            &retained_glyphs,
                            first_retained_rec,
                            singlepos_info.new_format,
                            plan,
                        ),
                    )
                } else {
                    $table::serialize(
                        s,
                        (&retained_glyphs, &singlepos_info, &retained_rec_idxes, plan),
                    )
                }
            }
        }

        impl<'a> Serialize<'a> for $table<'_> {
            type Args = (
                &'a [GlyphId],
                &'a SinglePosInfo<'a>,
                &'a IntSet<u32>,
                &'a Plan,
            );
            fn serialize(s: &mut Serializer, args: Self::Args) -> Result<(), SerializeErrorFlags> {
                // format
                s.embed($format)?;

                // coverage offset
                let cov_offset_pos = s.allocate_size($offset::RAW_BYTE_LEN, true)?;

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
                let value_count = $count::try_from(value_count)
                    .map_err(|_| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_INT_OVERFLOW))?;
                s.embed(value_count)?;

                for i in retained_rec_idxes.iter() {
                    let offset = records_offset + i as usize * record_size;
                    let value_record = ValueRecord::new(font_data, offset, value_format);
                    value_record.subset(plan, s, new_format)?;
                }

                $offset::serialize_serialize::<CoverageTable>(s, glyphs, cov_offset_pos)
            }
        }
    };
}
array_value_position!(SinglePosFormat2, SinglePosFormat1, 2u16, Offset16, u16);
array_value_position!(SinglePosFormat4, SinglePosFormat3, 4u16, Offset32, Uint24);

impl CollectVariationIndices for SinglePos<'_> {
    fn collect_variation_indices(&self, plan: &Plan, varidx_set: &mut IntSet<u32>) {
        match self {
            Self::Format1(item) => item.collect_variation_indices(plan, varidx_set),
            Self::Format2(item) => item.collect_variation_indices(plan, varidx_set),
            Self::Format3(item) => item.collect_variation_indices(plan, varidx_set),
            Self::Format4(item) => item.collect_variation_indices(plan, varidx_set),
        }
    }
}

macro_rules! collect_single_value_variations {
    ($table:ident) => {
        impl CollectVariationIndices for $table<'_> {
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
                if !coverage.intersects(&plan.glyphset_gsub) {
                    return;
                }
                self.value_record()
                    .collect_variation_indices(plan, varidx_set);
            }
        }
    };
}
collect_single_value_variations!(SinglePosFormat1);
collect_single_value_variations!(SinglePosFormat3);

macro_rules! collect_array_value_variations {
    ($table:ident) => {
        impl CollectVariationIndices for $table<'_> {
            fn collect_variation_indices(&self, plan: &Plan, varidx_set: &mut IntSet<u32>) {
                let value_format = self.value_format();
                if !value_format.intersects(ValueFormat::ANY_DEVICE_OR_VARIDX) {
                    return;
                }

                let Ok(coverage) = self.coverage() else {
                    return;
                };
                let glyph_set = &plan.glyphset_gsub;
                let value_count = u32::from(self.value_count());
                let record_size = compute_record_len(value_format);
                let records_offset = self.value_count_byte_range().end;
                let font_data = self.offset_data();

                let bit_storage = 32 - value_count.leading_zeros() as u64;
                if value_count as u64 > glyph_set.len() * bit_storage {
                    for idx in glyph_set
                        .iter()
                        .filter_map(|g| coverage.get(g))
                        .filter(|&idx| idx < value_count)
                    {
                        let offset = records_offset + idx as usize * record_size;
                        let value_record = ValueRecord::new(font_data, offset, value_format);
                        value_record.collect_variation_indices(plan, varidx_set);
                    }
                } else {
                    for i in coverage.iter().enumerate().filter_map(|(idx, g)| {
                        (idx < value_count as usize && glyph_set.contains(g)).then_some(idx)
                    }) {
                        let offset = records_offset + i * record_size;
                        let value_record = ValueRecord::new(font_data, offset, value_format);
                        value_record.collect_variation_indices(plan, varidx_set);
                    }
                }
            }
        }
    };
}
collect_array_value_variations!(SinglePosFormat2);
collect_array_value_variations!(SinglePosFormat4);

#[cfg(test)]
mod test {
    use super::*;
    use read_fonts::{FontRef, TableProvider};

    #[test]
    fn test_subset_gpos_format1() {
        use read_fonts::tables::gpos::PositionSubtables;

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
        use read_fonts::tables::gpos::PositionSubtables;

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
        use read_fonts::{FontData, FontRead};

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

    /// Builds a SinglePosFormat2 covering `cov_glyphs`, with one ValueRecord
    /// per entry of `x_advances`.
    ///
    /// Passing fewer advances than coverage glyphs produces the shape these
    /// tests are about: coverage entries that index past the end of the
    /// ValueRecord array. An empty `x_advances` also gives an empty
    /// ValueFormat, which is how Mukta-SemiBold.ttf spells it.
    fn single_pos_format2(cov_glyphs: &[u16], x_advances: &[u16]) -> Vec<u8> {
        let value_format: u16 = if x_advances.is_empty() { 0 } else { 0x0004 };
        let cov_offset = 8 + 2 * x_advances.len();

        let mut out = Vec::new();
        out.extend_from_slice(&2_u16.to_be_bytes()); // posFormat
        out.extend_from_slice(&(cov_offset as u16).to_be_bytes()); // coverageOffset
        out.extend_from_slice(&value_format.to_be_bytes()); // valueFormat
        out.extend_from_slice(&(x_advances.len() as u16).to_be_bytes()); // valueCount
        for advance in x_advances {
            out.extend_from_slice(&advance.to_be_bytes());
        }
        // CoverageFormat1
        out.extend_from_slice(&1_u16.to_be_bytes());
        out.extend_from_slice(&(cov_glyphs.len() as u16).to_be_bytes());
        for g in cov_glyphs {
            out.extend_from_slice(&g.to_be_bytes());
        }
        out
    }

    /// Retains `old_gids`, mapping them to new gids 1..n.
    fn plan_retaining(old_gids: &[u16]) -> Plan {
        let mut plan = Plan {
            glyph_map_gsub: vec![crate::INVALID_GID; 256],
            ..Default::default()
        };
        for (i, old_gid) in old_gids.iter().enumerate() {
            plan.glyph_map_gsub[*old_gid as usize] = GlyphId::from(i as u32 + 1);
            plan.glyphset_gsub.insert(GlyphId::from(*old_gid as u32));
        }
        plan
    }

    fn subset_single_pos_format2(
        raw_table: &[u8],
        plan: &Plan,
    ) -> Result<Vec<u8>, SerializeErrorFlags> {
        use read_fonts::{FontData, FontRead};

        // Dummy font: this path only consults it for fvar.
        let font = FontRef::new(include_bytes!("../../test-data/fonts/Amiri-Regular.ttf")).unwrap();
        let subset_state = SubsetState::default();
        let singlepos = SinglePosFormat2::read(FontData::new(raw_table)).unwrap();

        let mut s = Serializer::new(1024);
        assert_eq!(s.start_serialize(), Ok(()));
        singlepos.subset(plan, &mut s, (&subset_state, &font))?;
        assert!(!s.in_error());
        s.end_serialize();
        Ok(s.copy_bytes())
    }

    /// A subtable whose every coverage entry indexes past the end of the
    /// ValueRecord array carries no positioning at all, and is dropped.
    ///
    /// Regression test for a divergence from hb-subset on Mukta-SemiBold.ttf,
    /// which has a SinglePos subtable with a 3 glyph coverage, an empty
    /// ValueFormat and valueCount 0. Records are only ever addressed as
    /// `records_offset + idx * record_size`, and an empty ValueFormat makes
    /// record_size 0, so every index read back the same empty record instead of
    /// going out of bounds. All three glyphs then held equal values, and the
    /// subtable was emitted as a no-op SinglePosFormat1.
    #[test]
    fn test_subset_gpos_format2_empty_value_array() {
        let raw_table = single_pos_format2(&[10, 20, 30], &[]);
        let plan = plan_retaining(&[10, 20, 30]);

        assert_eq!(
            subset_single_pos_format2(&raw_table, &plan),
            Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY)
        );
    }

    /// Coverage entries past the end of the ValueRecord array are dropped, the
    /// ones before it are kept.
    #[test]
    fn test_subset_gpos_format2_coverage_longer_than_value_array() {
        // 3 glyphs covered but only 1 record, so glyphs 20 and 30 are dropped.
        let raw_table = single_pos_format2(&[10, 20, 30], &[100]);
        let plan = plan_retaining(&[10, 30]);

        let subsetted_data = subset_single_pos_format2(&raw_table, &plan).unwrap();
        #[rustfmt::skip]
        let expected: [u8; 14] = [
            // pos_format=1, cov_offset=8, value_format=0x0004
            0x00, 0x01, 0x00, 0x08, 0x00, 0x04,
            // record: x_advance=100
            0x00, 0x64,
            // coverage: format 1, count 1, glyph 1
            0x00, 0x01, 0x00, 0x01, 0x00, 0x01,
        ];
        assert_eq!(subsetted_data, expected);
    }

    /// Same as above, but large enough that the intersection walks the glyph
    /// set rather than the coverage table. Both branches have to apply the
    /// bound.
    #[test]
    fn test_subset_gpos_format2_coverage_longer_than_value_array_sparse() {
        // The branch is picked by `coverage_population > glyph_set_len *
        // num_bits`; 64 covered glyphs against a 2 glyph subset takes the
        // glyph set side.
        let cov_glyphs: Vec<u16> = (10..74).collect();
        let raw_table = single_pos_format2(&cov_glyphs, &[100, 200]);
        // Coverage index 1 is in range, coverage index 20 is not.
        let plan = plan_retaining(&[11, 30]);

        let subsetted_data = subset_single_pos_format2(&raw_table, &plan).unwrap();
        #[rustfmt::skip]
        let expected: [u8; 14] = [
            // pos_format=1, cov_offset=8, value_format=0x0004
            0x00, 0x01, 0x00, 0x08, 0x00, 0x04,
            // record: x_advance=200, the second record
            0x00, 0xc8,
            // coverage: format 1, count 1, glyph 1
            0x00, 0x01, 0x00, 0x01, 0x00, 0x01,
        ];
        assert_eq!(subsetted_data, expected);
    }

    /// A SinglePosFormat2 with a 6 glyph coverage but only 4 ValueRecords.
    ///
    /// The bytes immediately after the record array happen to read back as two
    /// more well formed records, each pointing at a VariationIndex. Those are
    /// what a coverage entry with no record of its own lands on, so collecting
    /// from index 4 or 5 picks up a variation index the subtable does not
    /// actually reference.
    #[rustfmt::skip]
    const COVERAGE_LONGER_THAN_VALUES: [u8; 84] = [
        // pos_format=2, cov_offset=68, value_format=0x0044, value_count=4
        0x00, 0x02, 0x00, 0x44, 0x00, 0x44, 0x00, 0x04,
        // record 0: x_advance=100, device_offset=32
        0x00, 0x64, 0x00, 0x20,
        // record 1: x_advance=200, device_offset=38
        0x00, 0xc8, 0x00, 0x26,
        // record 2: x_advance=300, device_offset=44
        0x01, 0x2c, 0x00, 0x2c,
        // record 3: x_advance=400, device_offset=50
        0x01, 0x90, 0x00, 0x32,
        // past the end of the array: reads back as a record with device_offset=56
        0x00, 0x01, 0x00, 0x38,
        // past the end of the array: reads back as a record with device_offset=62
        0x00, 0x02, 0x00, 0x3e,
        // VariationIndex tables for records 0..3, at 32, 38, 44 and 50
        0x00, 0x01, 0x00, 0x02, 0x80, 0x00,
        0x00, 0x03, 0x00, 0x04, 0x80, 0x00,
        0x00, 0x05, 0x00, 0x06, 0x80, 0x00,
        0x00, 0x07, 0x00, 0x08, 0x80, 0x00,
        // VariationIndex tables at 56 and 62, only reachable past the array end
        0x00, 0x09, 0x00, 0x0a, 0x80, 0x00,
        0x00, 0x0b, 0x00, 0x0c, 0x80, 0x00,
        // coverage @68: format 1, count 6, glyphs 10, 20, 30, 40, 50, 60
        0x00, 0x01, 0x00, 0x06,
        0x00, 0x0a, 0x00, 0x14, 0x00, 0x1e, 0x00, 0x28, 0x00, 0x32, 0x00, 0x3c,
    ];

    fn collect_varidxes(raw_table: &[u8], glyphs: &[u32]) -> IntSet<u32> {
        use read_fonts::{FontData, FontRead};

        let singlepos = SinglePosFormat2::read(FontData::new(raw_table)).unwrap();
        let mut plan = Plan::default();
        for g in glyphs {
            plan.glyphset_gsub.insert(GlyphId::from(*g));
        }

        let mut varidx_set = IntSet::empty();
        singlepos.collect_variation_indices(&plan, &mut varidx_set);
        varidx_set
    }

    /// Glyphs covered past the end of the ValueRecord array contribute no
    /// variation indices.
    #[test]
    fn test_collect_variation_indices_coverage_longer_than_value_array() {
        // Glyph 10 is at coverage index 0, glyph 60 at index 5. Two glyphs
        // against valueCount 4 walks the coverage table.
        let varidx_set = collect_varidxes(&COVERAGE_LONGER_THAN_VALUES, &[10, 60]);

        // Only glyph 10's record. Glyph 60 would otherwise pull in 0x000b000c.
        assert_eq!(varidx_set.iter().collect::<Vec<_>>(), vec![0x00010002]);
    }

    /// Same, but sparse enough that the collection walks the glyph set instead.
    #[test]
    fn test_collect_variation_indices_coverage_longer_than_value_array_sparse() {
        // One glyph against valueCount 4 walks the glyph set. Glyph 60 is at
        // coverage index 5, which has no record.
        let varidx_set = collect_varidxes(&COVERAGE_LONGER_THAN_VALUES, &[60]);

        assert!(varidx_set.is_empty());
    }

    /// A SinglePosFormat1 whose coverage misses the glyph set is dropped by
    /// subset(), so its ValueRecord's variation index must not be collected.
    #[test]
    fn test_collect_variation_indices_format1_requires_coverage_intersection() {
        use read_fonts::{FontData, FontRead};

        #[rustfmt::skip]
        let raw_table: [u8; 20] = [
            // posFormat=1, coverageOffset=14, valueFormat=X_ADVANCE_DEVICE
            0x00, 0x01, 0x00, 0x0e, 0x00, 0x40,
            // ValueRecord: xAdvDeviceOffset=8
            0x00, 0x08,
            // @8 VariationIndex (1, 2)
            0x00, 0x01, 0x00, 0x02, 0x80, 0x00,
            // @14 coverage: format 1, glyph 10
            0x00, 0x01, 0x00, 0x01, 0x00, 0x0a,
        ];
        let singlepos = SinglePosFormat1::read(FontData::new(&raw_table)).unwrap();

        let collect = |glyphs: &[u32]| {
            let mut plan = Plan::default();
            for g in glyphs {
                plan.glyphset_gsub.insert(GlyphId::from(*g));
            }
            let mut varidx_set = IntSet::empty();
            singlepos.collect_variation_indices(&plan, &mut varidx_set);
            varidx_set.iter().collect::<Vec<_>>()
        };

        // Glyph 20 is not covered: nothing is retained.
        assert!(collect(&[20]).is_empty());
        // Glyph 10 is covered: the shared record's variation index is kept.
        assert_eq!(collect(&[10, 20]), vec![0x00010002]);
    }
}

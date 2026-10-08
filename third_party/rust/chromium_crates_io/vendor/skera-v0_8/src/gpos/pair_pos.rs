//! impl subset() for PairPos subtable

use crate::{
    gpos::value_record::{compute_effective_format, compute_record_len},
    layout::{map_gsub_glyph, ClassDefSubsetStruct},
    offset::{SerializeSerialize, SerializeSubset},
    offset_array::SubsetOffsetArray,
    serialize::{SerializeErrorFlags, SerializeResultEmpty, Serializer},
    CollectVariationIndices, Plan, SubsetFlags, SubsetState, SubsetTable,
};

use crate::fnv::FnvHashMap;
use write_fonts::{
    read::{
        collections::IntSet,
        tables::{
            gpos::{PairPos, PairPosFormat1, PairPosFormat2, PairSet, ValueFormat, ValueRecord},
            layout::CoverageTable,
        },
        types::GlyphId,
        ArrayOfOffsets, FontData, FontRef, ReadError, TableProvider,
    },
    types::Offset16,
};

impl<'a> SubsetTable<'a> for PairPos<'_> {
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

pub(crate) struct PairSetInfo<'a> {
    coverage: &'a CoverageTable<'a>,
    pair_sets: &'a ArrayOfOffsets<'a, PairSet<'a>>,
    pair_set_count: u16,
    value_format1: ValueFormat,
    value_format2: ValueFormat,
    record1_size: usize,
    pair_record_size: usize,
    new_format1: ValueFormat,
    new_format2: ValueFormat,
}

fn compute_pair_set_effective_formats(
    pair_set: &PairSet,
    glyph_set: &IntSet<GlyphId>,
    pair_set_info: &mut PairSetInfo,
    strip_hints: bool,
    strip_empty: bool,
) -> Result<(), ReadError> {
    let (value_format1, value_format2, record1_size, pair_record_size, new_format1, new_format2) = (
        pair_set_info.value_format1,
        pair_set_info.value_format2,
        pair_set_info.record1_size,
        pair_set_info.pair_record_size,
        &mut pair_set_info.new_format1,
        &mut pair_set_info.new_format2,
    );
    for i in 0..pair_set.pair_value_count() as usize {
        let offset = 2 + i * pair_record_size;
        let font_data = pair_set.offset_data();
        let second_glyph = font_data.read_at::<u16>(offset)?;
        if !glyph_set.contains(GlyphId::from(second_glyph)) {
            continue;
        }

        let value_record1 = ValueRecord::new(font_data, offset + 2, value_format1);
        *new_format1 |= compute_effective_format(&value_record1, strip_hints, strip_empty)?;

        let value_record2 = ValueRecord::new(font_data, offset + 2 + record1_size, value_format2);
        *new_format2 |= compute_effective_format(&value_record2, strip_hints, strip_empty)?;
    }
    Ok(())
}

fn compute_effective_pair_formats_1(
    glyph_set: &IntSet<GlyphId>,
    pair_set_info: &mut PairSetInfo,
    strip_hints: bool,
    strip_empty: bool,
) -> Result<(), ReadError> {
    let (coverage, pair_sets, pair_set_count, value_format1, value_format2) = (
        pair_set_info.coverage,
        pair_set_info.pair_sets,
        pair_set_info.pair_set_count,
        pair_set_info.value_format1,
        pair_set_info.value_format2,
    );
    let bit_storage = 16 - pair_set_count.leading_zeros() as u64;

    if pair_set_count as u64 > glyph_set.len() * bit_storage {
        for g in glyph_set.iter() {
            if let Some(idx) = coverage.get(g) {
                let pair_set = match pair_sets.get(idx as usize) {
                    Err(ReadError::NullOffset) => continue,
                    other => other,
                }?;

                compute_pair_set_effective_formats(
                    &pair_set,
                    glyph_set,
                    pair_set_info,
                    strip_hints,
                    strip_empty,
                )?;
                if pair_set_info.new_format1 == value_format1
                    && pair_set_info.new_format2 == value_format2
                {
                    break;
                }
            }
        }
    } else {
        for idx in coverage
            .iter()
            .enumerate()
            .filter_map(|(i, g)| glyph_set.contains(GlyphId::from(g)).then_some(i))
        {
            let pair_set = match pair_sets.get(idx) {
                Err(ReadError::NullOffset) => continue,
                other => other,
            }?;
            compute_pair_set_effective_formats(
                &pair_set,
                glyph_set,
                pair_set_info,
                strip_hints,
                strip_empty,
            )?;
            if pair_set_info.new_format1 == value_format1
                && pair_set_info.new_format2 == value_format2
            {
                break;
            }
        }
    }

    Ok(())
}

impl<'a> SubsetTable<'a> for PairSet<'_> {
    type ArgsForSubset = &'a PairSetInfo<'a>;
    type Output = ();
    fn subset(
        &self,
        plan: &Plan,
        s: &mut Serializer,
        args: Self::ArgsForSubset,
    ) -> Result<(), SerializeErrorFlags> {
        // pairvalue count
        let pairvalue_count_pos = s.embed(0_u16)?;
        let mut count = 0_u16;

        let glyph_map = &plan.glyph_map_gsub;
        let (
            value_format1,
            value_format2,
            new_format1,
            new_format2,
            record1_size,
            pair_record_size,
        ) = (
            args.value_format1,
            args.value_format2,
            args.new_format1,
            args.new_format2,
            args.record1_size,
            args.pair_record_size,
        );

        let pair_value_count = self.pair_value_count();
        let bit_storage = 16 - pair_value_count.leading_zeros() as u16;
        let font_data = self.offset_data();
        if pair_value_count as u64 > plan.glyphset_gsub.len() * bit_storage as u64 {
            for g in plan.glyphset_gsub.iter() {
                let mut hi = pair_value_count as usize;
                let mut lo = 0;
                while lo < hi {
                    // This recommends using usize::midpoint which expands to u128.
                    // We definitely do not want to do that here since the input values
                    // are 16-bit.
                    #[allow(clippy::manual_midpoint)]
                    let mid = (lo + hi) / 2;
                    let pair_record_offset = 2 + mid * pair_record_size;
                    let glyph_id = GlyphId::from(
                        font_data
                            .read_at::<u16>(pair_record_offset)
                            .map_err(|_| SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR)?,
                    );
                    if glyph_id < g {
                        lo = mid + 1;
                    } else if glyph_id > g {
                        hi = mid;
                    } else {
                        let new_gid = map_gsub_glyph(glyph_map, glyph_id)
                            .ok_or_else(|| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_OTHER))?;
                        s.embed(new_gid.to_u32() as u16)?;

                        let offset = pair_record_offset + 2;
                        let value_record1 = ValueRecord::new(font_data, offset, value_format1);
                        value_record1.subset(plan, s, new_format1)?;

                        let value_record2 =
                            ValueRecord::new(font_data, offset + record1_size, value_format2);
                        value_record2.subset(plan, s, new_format2)?;

                        count += 1;
                        break;
                    }
                }
            }
        } else {
            for i in 0..pair_value_count as usize {
                let pair_record_offset = 2 + i * pair_record_size;
                let glyph_id = GlyphId::from(
                    font_data
                        .read_at::<u16>(pair_record_offset)
                        .map_err(|_| SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR)?,
                );

                let Some(new_gid) = map_gsub_glyph(glyph_map, glyph_id) else {
                    continue;
                };

                s.embed(new_gid.to_u32() as u16)?;

                let offset = pair_record_offset + 2;
                let value_record1 = ValueRecord::new(font_data, offset, value_format1);
                value_record1.subset(plan, s, new_format1)?;

                let value_record2 =
                    ValueRecord::new(font_data, offset + record1_size, value_format2);
                value_record2.subset(plan, s, new_format2)?;

                count += 1;
            }
        }

        if count == 0 {
            return Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY);
        }
        s.copy_assign(pairvalue_count_pos, count);
        Ok(())
    }
}

impl<'a> SubsetTable<'a> for PairPosFormat1<'_> {
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
        let (subset_state, font) = args;
        let glyph_map = &plan.glyph_map_gsub;
        let glyph_set = &plan.glyphset_gsub;
        // format
        s.embed(self.pos_format())?;

        let coverage = self
            .coverage()
            .map_err(|_| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR))?;
        let pair_sets = self.pair_sets();
        let pair_set_count = self.pair_set_count();

        // coverage offset
        let cov_offset_pos = s.embed(0_u16)?;

        // value_formats
        let value_format1 = self.value_format1();
        let value_format2 = self.value_format2();
        let record1_size = compute_record_len(value_format1);
        let pair_record_size = 2 + record1_size + compute_record_len(value_format2);
        let mut pair_set_info = PairSetInfo {
            coverage: &coverage,
            pair_sets: &pair_sets,
            pair_set_count,
            value_format1,
            value_format2,
            record1_size,
            pair_record_size,
            new_format1: ValueFormat::empty(),
            new_format2: ValueFormat::empty(),
        };

        if plan
            .subset_flags
            .contains(SubsetFlags::SUBSET_FLAGS_NO_HINTING)
        {
            // do not strip hints for VF unless it has no GDEF varstore after subsetting
            let strip_hints = if font.fvar().is_ok() {
                !subset_state.has_gdef_varstore
            } else {
                true
            };

            compute_effective_pair_formats_1(glyph_set, &mut pair_set_info, strip_hints, true)
                .map_err(|_| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR))?;
        } else {
            pair_set_info.new_format1 = value_format1;
            pair_set_info.new_format2 = value_format2;
        };

        s.embed(pair_set_info.new_format1)?;
        s.embed(pair_set_info.new_format2)?;

        // pairset count
        let pairset_count_pos = s.embed(0_u16)?;
        let mut pairset_count = 0_u16;

        let mut retained_glyphs =
            Vec::with_capacity((pair_set_count as usize).min(glyph_set.len() as usize));

        let bit_storage = 16 - pair_set_count.leading_zeros() as u64;
        if pair_set_count as u64 > glyph_set.len() * bit_storage {
            for g in glyph_set.iter() {
                let Some(pair_set_idx) = coverage.get(g) else {
                    continue;
                };

                if !pair_sets
                    .subset_offset(pair_set_idx as usize, s, plan, &pair_set_info)
                    .is_empty()?
                {
                    pairset_count += 1;
                    let new_g = map_gsub_glyph(glyph_map, g)
                        .ok_or_else(|| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_OTHER))?;
                    retained_glyphs.push(new_g);
                }
            }
        } else {
            for (i, g) in coverage.iter().enumerate().filter_map(|(i, g)| {
                map_gsub_glyph(glyph_map, GlyphId::from(g)).map(|new_g| (i, new_g))
            }) {
                if !pair_sets
                    .subset_offset(i, s, plan, &pair_set_info)
                    .is_empty()?
                {
                    pairset_count += 1;
                    retained_glyphs.push(g);
                }
            }
        }

        if retained_glyphs.is_empty() {
            return Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY);
        }
        s.copy_assign(pairset_count_pos, pairset_count);
        Offset16::serialize_serialize::<CoverageTable>(s, &retained_glyphs, cov_offset_pos)
    }
}

struct PairPosFormat2Info<'a> {
    font_data: FontData<'a>,
    value_format1: ValueFormat,
    value_format2: ValueFormat,
    class1_count: u16,
    class2_count: usize,
    records_offset: usize,
    record1_size: usize,
    record_size: usize,
    new_format1: ValueFormat,
    new_format2: ValueFormat,
}

fn compute_effective_pair_formats_2(
    pairpos2_info: &mut PairPosFormat2Info,
    class1_map: &FnvHashMap<u16, u16>,
    class2_idxes: &[u16],
    strip_hints: bool,
    strip_empty: bool,
) -> Result<(), ReadError> {
    let (
        font_data,
        value_format1,
        value_format2,
        class1_count,
        class2_count,
        records_offset,
        record1_size,
        record_size,
        new_format1,
        new_format2,
    ) = (
        pairpos2_info.font_data,
        pairpos2_info.value_format1,
        pairpos2_info.value_format2,
        pairpos2_info.class1_count,
        pairpos2_info.class2_count,
        pairpos2_info.records_offset,
        pairpos2_info.record1_size,
        pairpos2_info.record_size,
        &mut pairpos2_info.new_format1,
        &mut pairpos2_info.new_format2,
    );

    for i in (0..class1_count).filter(|i| class1_map.contains_key(i)) {
        for j in class2_idxes {
            let offset = records_offset + (i as usize * class2_count + *j as usize) * record_size;
            let record1 = ValueRecord::new(font_data, offset, value_format1);
            let record2 = ValueRecord::new(font_data, offset + record1_size, value_format2);

            *new_format1 |= compute_effective_format(&record1, strip_hints, strip_empty)?;
            *new_format2 |= compute_effective_format(&record2, strip_hints, strip_empty)?;
        }
        if *new_format1 == value_format1 && *new_format2 == value_format2 {
            break;
        }
    }
    Ok(())
}

impl<'a> SubsetTable<'a> for PairPosFormat2<'_> {
    type ArgsForSubset = (&'a SubsetState, &'a FontRef<'a>);
    type Output = ();
    fn subset(
        &self,
        plan: &Plan,
        s: &mut Serializer,
        args: Self::ArgsForSubset,
    ) -> Result<(), SerializeErrorFlags> {
        if self.coverage_offset().is_null()
            || self.class_def1_offset().is_null()
            || self.class_def2_offset().is_null()
        {
            return Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY);
        }
        let Ok(coverage) = self.coverage() else {
            return Err(s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR));
        };

        // format
        s.embed(self.pos_format())?;

        // coverage offset
        let cov_offset_pos = s.embed(0_u16)?;

        // value format
        let value_format1_pos = s.embed(0_u16)?;
        let value_format2_pos = s.embed(0_u16)?;

        // classdef1 offset
        let classdef1_offset_pos = s.embed(0_u16)?;
        let class_def1 = self
            .class_def1()
            .map_err(|_| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR))?;
        let class1_map = match Offset16::serialize_subset(
            &class_def1,
            s,
            plan,
            &ClassDefSubsetStruct {
                remap_class: true,
                keep_empty_table: true,
                use_class_zero: true,
                glyph_filter: Some(&coverage),
            },
            classdef1_offset_pos,
        ) {
            Ok(Some(out)) => out,
            _ => FnvHashMap::default(),
        };

        if class1_map.is_empty() {
            return Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY);
        }
        let class1_count = class1_map.len() as u16;

        // classdef2 offset
        let classdef2_offset_pos = s.embed(0_u16)?;
        let class_def2 = self
            .class_def2()
            .map_err(|_| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR))?;

        let class2_map = match Offset16::serialize_subset(
            &class_def2,
            s,
            plan,
            &ClassDefSubsetStruct {
                remap_class: true,
                keep_empty_table: true,
                use_class_zero: false,
                glyph_filter: None,
            },
            classdef2_offset_pos,
        ) {
            Ok(Some(out)) => out,
            _ => FnvHashMap::default(),
        };

        // If only Class2 0 left, no need to keep anything.
        if class2_map.len() <= 1 {
            return Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY);
        }
        let class2_count = class2_map.len() as u16;

        // class1_count
        s.embed(class1_count)?;
        // class2_count
        s.embed(class2_count)?;

        // value formats
        let (subset_state, font) = args;
        let class2_idxes: Vec<u16> = (0..self.class2_count())
            .filter(|i| class2_map.contains_key(i))
            .collect();

        let value_format1 = self.value_format1();
        let value_format2 = self.value_format2();
        let class2_count = self.class2_count() as usize;
        let records_offset = self.class2_count_byte_range().end;
        let record1_size = compute_record_len(value_format1);
        let record_size = record1_size + compute_record_len(value_format2);
        let font_data = self.offset_data();
        let class1_count = self.class1_count();
        let mut pairpos2_info = PairPosFormat2Info {
            font_data,
            value_format1,
            value_format2,
            class1_count,
            class2_count,
            records_offset,
            record1_size,
            record_size,
            new_format1: ValueFormat::empty(),
            new_format2: ValueFormat::empty(),
        };

        if plan
            .subset_flags
            .contains(SubsetFlags::SUBSET_FLAGS_NO_HINTING)
        {
            // do not strip hints for VF unless it has no GDEF varstore after subsetting
            let strip_hints = if font.fvar().is_ok() {
                !subset_state.has_gdef_varstore
            } else {
                true
            };

            compute_effective_pair_formats_2(
                &mut pairpos2_info,
                &class1_map,
                &class2_idxes,
                strip_hints,
                true,
            )
            .map_err(|_| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR))?;
        } else {
            pairpos2_info.new_format1 = value_format1;
            pairpos2_info.new_format2 = value_format2;
        };

        s.copy_assign(value_format1_pos, pairpos2_info.new_format1);
        s.copy_assign(value_format2_pos, pairpos2_info.new_format2);

        // serialize value records
        for i in (0..self.class1_count()).filter(|i| class1_map.contains_key(i)) {
            for j in &class2_idxes {
                let offset =
                    records_offset + (i as usize * class2_count + *j as usize) * record_size;
                let record1 = ValueRecord::new(self.offset_data(), offset, value_format1);
                let record2 =
                    ValueRecord::new(self.offset_data(), offset + record1_size, value_format2);

                record1.subset(plan, s, pairpos2_info.new_format1)?;
                record2.subset(plan, s, pairpos2_info.new_format2)?;
            }
        }

        // this can be moved, put it at last so we have the same binary data with Harfbuzz subsetter
        Offset16::serialize_subset(&coverage, s, plan, (), cov_offset_pos)
    }
}

fn collect_pairset_variation_indices(
    pair_set: &PairSet,
    pair_set_info: &PairSetInfo,
    plan: &Plan,
    varidx_set: &mut IntSet<u32>,
) {
    let (value_format1, value_format2, record1_size, pair_record_size) = (
        pair_set_info.value_format1,
        pair_set_info.value_format2,
        pair_set_info.record1_size,
        pair_set_info.pair_record_size,
    );

    let pair_value_count = pair_set.pair_value_count();
    let bit_storage = 16 - pair_value_count.leading_zeros() as u16;
    let font_data = pair_set.offset_data();
    if pair_value_count as u64 > plan.glyphset_gsub.len() * bit_storage as u64 {
        for g in plan.glyphset_gsub.iter() {
            let mut hi = pair_value_count as usize;
            let mut lo = 0;
            while lo < hi {
                // This recommends using usize::midpoint which expands to u128.
                // We definitely do not want to do that here since the input values
                // are 16-bit.
                #[allow(clippy::manual_midpoint)]
                let mid = (lo + hi) / 2;
                let pair_record_offset = 2 + mid * pair_record_size;
                let Ok(glyph_id) = font_data.read_at::<u16>(pair_record_offset) else {
                    return;
                };
                let glyph_id = GlyphId::from(glyph_id);
                if glyph_id < g {
                    lo = mid + 1;
                } else if glyph_id > g {
                    hi = mid;
                } else {
                    let offset = pair_record_offset + 2;
                    let value_record1 = ValueRecord::new(font_data, offset, value_format1);
                    value_record1.collect_variation_indices(plan, varidx_set);

                    let value_record2 =
                        ValueRecord::new(font_data, offset + record1_size, value_format2);
                    value_record2.collect_variation_indices(plan, varidx_set);
                    break;
                }
            }
        }
    } else {
        let glyph_set = &plan.glyphset_gsub;
        for i in 0..pair_value_count as usize {
            let pair_record_offset = 2 + i * pair_record_size;
            let Ok(glyph_id) = font_data.read_at::<u16>(pair_record_offset) else {
                return;
            };

            if !glyph_set.contains(GlyphId::from(glyph_id)) {
                continue;
            }

            let offset = pair_record_offset + 2;
            let value_record1 = ValueRecord::new(font_data, offset, value_format1);
            value_record1.collect_variation_indices(plan, varidx_set);

            let value_record2 = ValueRecord::new(font_data, offset + record1_size, value_format2);
            value_record2.collect_variation_indices(plan, varidx_set);
        }
    }
}

impl CollectVariationIndices for PairPos<'_> {
    fn collect_variation_indices(&self, plan: &Plan, varidx_set: &mut IntSet<u32>) {
        match self {
            Self::Format1(item) => item.collect_variation_indices(plan, varidx_set),
            Self::Format2(item) => item.collect_variation_indices(plan, varidx_set),
        }
    }
}

impl CollectVariationIndices for PairPosFormat1<'_> {
    fn collect_variation_indices(&self, plan: &Plan, varidx_set: &mut IntSet<u32>) {
        let value_format1 = self.value_format1();
        let value_format2 = self.value_format2();

        if !value_format1.intersects(ValueFormat::ANY_DEVICE_OR_VARIDX)
            && !value_format2.intersects(ValueFormat::ANY_DEVICE_OR_VARIDX)
        {
            return;
        }

        let Ok(coverage) = self.coverage() else {
            return;
        };

        let glyph_set = &plan.glyphset_gsub;
        let pair_sets = self.pair_sets();
        let pair_set_count = self.pair_set_count();

        let record1_size = compute_record_len(value_format1);
        let pair_record_size = 2 + record1_size + compute_record_len(value_format2);
        let pair_set_info = PairSetInfo {
            coverage: &coverage,
            pair_sets: &pair_sets,
            pair_set_count,
            value_format1,
            value_format2,
            record1_size,
            pair_record_size,
            new_format1: ValueFormat::empty(),
            new_format2: ValueFormat::empty(),
        };

        let bit_storage = 16 - pair_set_count.leading_zeros() as u64;
        if pair_set_count as u64 > glyph_set.len() * bit_storage {
            for g in glyph_set.iter() {
                if let Some(idx) = coverage.get(g) {
                    let pair_set = match pair_sets.get(idx as usize) {
                        Ok(pair_set) => pair_set,
                        Err(ReadError::NullOffset) => continue,
                        Err(_) => return,
                    };
                    collect_pairset_variation_indices(&pair_set, &pair_set_info, plan, varidx_set);
                }
            }
        } else {
            for idx in coverage
                .iter()
                .enumerate()
                .filter_map(|(i, g)| glyph_set.contains(GlyphId::from(g)).then_some(i))
            {
                let pair_set = match pair_sets.get(idx) {
                    Ok(pair_set) => pair_set,
                    Err(ReadError::NullOffset) => continue,
                    Err(_) => return,
                };
                collect_pairset_variation_indices(&pair_set, &pair_set_info, plan, varidx_set);
            }
        }
    }
}

impl CollectVariationIndices for PairPosFormat2<'_> {
    fn collect_variation_indices(&self, plan: &Plan, varidx_set: &mut IntSet<u32>) {
        let value_format1 = self.value_format1();
        let value_format2 = self.value_format2();

        if !value_format1.intersects(ValueFormat::ANY_DEVICE_OR_VARIDX)
            && !value_format2.intersects(ValueFormat::ANY_DEVICE_OR_VARIDX)
        {
            return;
        }
        let Ok(coverage) = self.coverage() else {
            return;
        };

        let glyph_set = &plan.glyphset_gsub;
        let cov_glyphs = coverage.intersect_set(glyph_set);
        if cov_glyphs.is_empty() {
            return;
        };

        let Ok(classdef1) = self.class_def1() else {
            return;
        };

        let Ok(classdef2) = self.class_def2() else {
            return;
        };

        let class1_set = classdef1.intersect_classes(&cov_glyphs);
        if class1_set.is_empty() {
            return;
        }
        let mut class2_set = classdef2.intersect_classes(glyph_set);
        if class2_set.is_empty() {
            return;
        }
        class2_set.insert(0);

        let class2_count = self.class2_count() as usize;
        let records_offset = self.class2_count_byte_range().end;
        let record1_size = compute_record_len(value_format1);
        let record_size = record1_size + compute_record_len(value_format2);
        let font_data = self.offset_data();

        for i in class1_set.iter() {
            for j in class2_set.iter() {
                let offset =
                    records_offset + (i as usize * class2_count + j as usize) * record_size;

                if value_format1.intersects(ValueFormat::ANY_DEVICE_OR_VARIDX) {
                    let record1 = ValueRecord::new(font_data, offset, value_format1);
                    record1.collect_variation_indices(plan, varidx_set);
                }

                if value_format2.intersects(ValueFormat::ANY_DEVICE_OR_VARIDX) {
                    let record2 = ValueRecord::new(font_data, offset + record1_size, value_format2);
                    record2.collect_variation_indices(plan, varidx_set);
                }
            }
        }
    }
}

#[cfg(test)]
mod test {
    use super::*;
    use write_fonts::read::{FontRef, TableProvider};

    #[test]
    fn test_subset_pairpos_format1() {
        use write_fonts::read::tables::gpos::PositionSubtables;

        let font = FontRef::new(include_bytes!("../../test-data/fonts/Amiri-Regular.ttf")).unwrap();
        let gpos_lookups = font.gpos().unwrap().lookup_list().unwrap();
        let lookup = gpos_lookups.lookups().get(56).unwrap();

        let PositionSubtables::Pair(sub_tables) = lookup.subtables().unwrap() else {
            panic!("Wrong type of lookup table!");
        };
        let pairpos_table = sub_tables.get(0).unwrap();

        let subset_state = SubsetState::default();
        let mut plan = Plan {
            glyph_map_gsub: vec![crate::INVALID_GID; 6299],
            ..Default::default()
        };

        plan.glyph_map_gsub[6292] = GlyphId::from(3_u32);
        plan.glyph_map_gsub[6298] = GlyphId::from(4_u32);

        plan.glyphset_gsub.insert(GlyphId::from(6292_u32));
        plan.glyphset_gsub.insert(GlyphId::from(6298_u32));

        let mut s = Serializer::new(1024);
        assert_eq!(s.start_serialize(), Ok(()));

        pairpos_table
            .subset(&plan, &mut s, (&subset_state, &font, &plan.gpos_lookups))
            .unwrap();
        assert!(!s.in_error());
        s.end_serialize();

        let subsetted_data = s.copy_bytes();
        let expected_data: [u8; 34] = [
            0x00, 0x01, 0x00, 0x0e, 0x00, 0x04, 0x00, 0x00, 0x00, 0x02, 0x00, 0x1c, 0x00, 0x16,
            0x00, 0x01, 0x00, 0x02, 0x00, 0x03, 0x00, 0x04, 0x00, 0x01, 0x00, 0x03, 0xff, 0xcf,
            0x00, 0x01, 0x00, 0x04, 0xff, 0xe8,
        ];

        assert_eq!(subsetted_data, expected_data);
    }

    #[test]
    fn test_subset_pairpos_format2() {
        use write_fonts::read::tables::gpos::PositionSubtables;

        let font = FontRef::new(include_bytes!("../../test-data/fonts/Amiri-Regular.ttf")).unwrap();
        let gpos_lookups = font.gpos().unwrap().lookup_list().unwrap();
        let lookup = gpos_lookups.lookups().get(82).unwrap();

        let PositionSubtables::Pair(sub_tables) = lookup.subtables().unwrap() else {
            panic!("Wrong type of lookup table!");
        };
        let pairpos_table = sub_tables.get(0).unwrap();

        let subset_state = SubsetState::default();
        let mut plan = Plan {
            glyph_map_gsub: vec![crate::INVALID_GID; 6737],
            font_num_glyphs: 6782,
            ..Default::default()
        };

        //test case 1: ValueFormat remains the same
        plan.glyph_map_gsub[40] = GlyphId::from(1_u32);
        plan.glyph_map_gsub[72] = GlyphId::from(2_u32);
        plan.glyph_map_gsub[168] = GlyphId::from(3_u32);
        plan.glyph_map_gsub[6736] = GlyphId::from(4_u32);

        plan.glyphset_gsub.insert(GlyphId::from(40_u32));
        plan.glyphset_gsub.insert(GlyphId::from(72_u32));
        plan.glyphset_gsub.insert(GlyphId::from(168_u32));
        plan.glyphset_gsub.insert(GlyphId::from(6736_u32));

        let mut s = Serializer::new(1024);
        assert_eq!(s.start_serialize(), Ok(()));

        pairpos_table
            .subset(&plan, &mut s, (&subset_state, &font, &plan.gpos_lookups))
            .unwrap();
        assert!(!s.in_error());
        s.end_serialize();

        let subsetted_data = s.copy_bytes();
        let expected_data: [u8; 82] = [
            0x00, 0x02, 0x00, 0x2e, 0x00, 0x04, 0x00, 0x00, 0x00, 0x46, 0x00, 0x38, 0x00, 0x03,
            0x00, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0xff, 0xf2, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x01, 0x00, 0x01, 0x00, 0x04, 0x00, 0x00,
            0x00, 0x01, 0x00, 0x01, 0x00, 0x04, 0x00, 0x01, 0x00, 0x02, 0x00, 0x04, 0x00, 0x03,
            0x00, 0x01, 0x00, 0x01, 0x00, 0x03, 0x00, 0x02, 0x00, 0x01, 0x00, 0x01,
        ];

        assert_eq!(subsetted_data, expected_data);

        // test case 2: strip hints is enabled
        plan.subset_flags = SubsetFlags::SUBSET_FLAGS_NO_HINTING;
        plan.glyph_map_gsub.resize(6737, crate::INVALID_GID);
        plan.glyph_map_gsub[72] = GlyphId::from(1_u32);
        plan.glyph_map_gsub[168] = GlyphId::from(2_u32);
        plan.glyph_map_gsub[6736] = GlyphId::from(3_u32);

        plan.glyphset_gsub.clear();
        plan.glyphset_gsub.insert(GlyphId::from(72_u32));
        plan.glyphset_gsub.insert(GlyphId::from(168_u32));
        plan.glyphset_gsub.insert(GlyphId::from(6736_u32));

        let mut s = Serializer::new(1024);
        assert_eq!(s.start_serialize(), Ok(()));

        pairpos_table
            .subset(&plan, &mut s, (&subset_state, &font, &plan.gpos_lookups))
            .unwrap();
        assert!(!s.in_error());
        s.end_serialize();

        let subsetted_data = s.copy_bytes();
        let expected_data: [u8; 48] = [
            0x00, 0x02, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x26, 0x00, 0x1a, 0x00, 0x02,
            0x00, 0x04, 0x00, 0x01, 0x00, 0x03, 0x00, 0x01, 0x00, 0x02, 0x00, 0x03, 0x00, 0x01,
            0x00, 0x01, 0x00, 0x03, 0x00, 0x01, 0x00, 0x03, 0x00, 0x02, 0x00, 0x01, 0x00, 0x01,
            0x00, 0x02, 0x00, 0x01, 0x00, 0x01,
        ];

        assert_eq!(subsetted_data, expected_data);
    }

    #[test]
    fn test_collect_variation_indices_pairpos_format1() {
        use write_fonts::read::tables::gpos::PositionSubtables;

        let font = FontRef::new(include_bytes!(
            "../../test-data/fonts/RobotoFlex-Variable.ttf"
        ))
        .unwrap();
        let gpos_lookups = font.gpos().unwrap().lookup_list().unwrap();
        let lookup = gpos_lookups.lookups().get(0).unwrap();

        let PositionSubtables::Pair(sub_tables) = lookup.subtables().unwrap() else {
            panic!("Wrong type of lookup table!");
        };
        let pairpos_table = sub_tables.get(0).unwrap();
        let mut plan = Plan::default();

        plan.glyphset_gsub.insert(GlyphId::from(3_u32));
        plan.glyphset_gsub.insert(GlyphId::from(11_u32));
        plan.glyphset_gsub.insert(GlyphId::from(55_u32));
        plan.glyphset_gsub.insert(GlyphId::from(57_u32));

        let mut varidx_set = IntSet::empty();
        pairpos_table.collect_variation_indices(&plan, &mut varidx_set);
        assert_eq!(varidx_set.len(), 5);
        assert!(varidx_set.contains(0x6f0013_u32));
        assert!(varidx_set.contains(0x3e0004_u32));
        assert!(varidx_set.contains(0x540010_u32));
        assert!(varidx_set.contains(0x1c0024_u32));
        assert!(varidx_set.contains(0x1c003c_u32));
    }

    #[test]
    fn test_collect_variation_indices_pairpos_format2() {
        use write_fonts::read::tables::gpos::PositionSubtables;

        let font = FontRef::new(include_bytes!(
            "../../test-data/fonts/RobotoFlex-Variable.ttf"
        ))
        .unwrap();
        let gpos_lookups = font.gpos().unwrap().lookup_list().unwrap();
        let lookup = gpos_lookups.lookups().get(0).unwrap();

        let PositionSubtables::Pair(sub_tables) = lookup.subtables().unwrap() else {
            panic!("Wrong type of lookup table!");
        };
        let pairpos_table = sub_tables.get(1).unwrap();
        let mut plan = Plan::default();

        plan.glyphset_gsub.insert(GlyphId::from(38_u32));
        plan.glyphset_gsub.insert(GlyphId::from(39_u32));
        plan.glyphset_gsub.insert(GlyphId::from(68_u32));
        plan.glyphset_gsub.insert(GlyphId::from(127_u32));

        let mut varidx_set = IntSet::empty();
        pairpos_table.collect_variation_indices(&plan, &mut varidx_set);
        assert_eq!(varidx_set.len(), 9);
        assert!(varidx_set.contains(0x12000f_u32));
        assert!(varidx_set.contains(0x3c0000_u32));
        assert!(varidx_set.contains(0x54001e_u32));
        assert!(varidx_set.contains(0x1c0031_u32));
        assert!(varidx_set.contains(0xb000b_u32));
        assert!(varidx_set.contains(0x1c0035_u32));
        assert!(varidx_set.contains(0x1c0022_u32));
        assert!(varidx_set.contains(0x100005_u32));
        assert!(varidx_set.contains(0x1c0036_u32));
    }
}

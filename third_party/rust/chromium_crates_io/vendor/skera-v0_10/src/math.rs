//! MATH closure and subsetting follow HarfBuzz's hb-ot-math-table.hh.
//! Variants use the cmap closure; per-glyph values use the math closure.
//! Closure and variation collection skip malformed data; serialization fails on it.
//! Unlike HarfBuzz's sanitizer, malformed required subtable offsets fail
//! serialization instead of being replaced with null.
//! Child subsetting errors propagate because they also set the serializer error;
//! only unmappable variation devices return an empty result with a null offset.
use crate::{
    layout::variation_index,
    offset::{SerializeSerialize, SerializeSubset},
    serialize::{SerializeErrorFlags, SerializeResultEmpty, Serializer},
    CollectVariationIndices, Plan, Subset, SubsetError, SubsetTable,
};
use read_fonts::{
    collections::IntSet,
    tables::{
        layout::{CoverageTable, DeviceOrVariationIndex},
        math::*,
    },
    types::{GlyphId, GlyphId16, Offset16},
    ArrayOfOffsets, FontData, FontRef, MinByteRange, ReadError, TableProvider, TopLevelTable,
};
use write_fonts::FontBuilder;

const MATH_VALUE_RECORD_COUNT: usize = 51;

pub(crate) fn closure(font: &FontRef, glyphs: &mut IntSet<GlyphId>) {
    let Ok(math) = font.math() else { return };
    if math.version().major != 1 {
        return;
    }
    let Ok(variants) = math.math_variants() else {
        return;
    };
    let mut added = IntSet::empty();
    for (coverage, constructions) in [
        (
            variants.vert_glyph_coverage(),
            variants.vert_glyph_constructions(),
        ),
        (
            variants.horiz_glyph_coverage(),
            variants.horiz_glyph_constructions(),
        ),
    ] {
        let Some(Ok(coverage)) = coverage else {
            continue;
        };
        for (gid, construction) in coverage.iter().zip(constructions.iter()) {
            if !glyphs.contains(gid) {
                continue;
            }
            let Ok(construction) = construction else {
                continue;
            };
            for record in construction.math_glyph_variant_records() {
                added.insert(GlyphId::from(record.variant_glyph()));
            }
            if let Some(Ok(assembly)) = construction.glyph_assembly() {
                for part in assembly.part_records() {
                    added.insert(GlyphId::from(part.glyph_id()));
                }
            }
        }
    }
    // HarfBuzz adds one generation of variants and assembly parts. They are
    // terminal shapes, not additional roots for variant construction.
    glyphs.union(&added);
}

fn read_or_fail<T>(
    s: &mut Serializer,
    result: Result<T, ReadError>,
) -> Result<T, SerializeErrorFlags> {
    result.map_err(|_| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR))
}

fn check_len(
    s: &mut Serializer,
    actual: usize,
    expected: usize,
) -> Result<(), SerializeErrorFlags> {
    if actual != expected {
        return Err(s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR));
    }
    Ok(())
}

fn kern_corners<'a>(
    record: &MathKernInfoRecord,
    data: FontData<'a>,
) -> [Option<Result<MathKern<'a>, ReadError>>; 4] {
    [
        record.top_right_math_kern(data),
        record.top_left_math_kern(data),
        record.bottom_right_math_kern(data),
        record.bottom_left_math_kern(data),
    ]
}

fn map_glyph(
    plan: &Plan,
    s: &mut Serializer,
    gid: impl Into<GlyphId>,
) -> Result<GlyphId16, SerializeErrorFlags> {
    // Report an unmapped glyph as INT_OVERFLOW, matching HarfBuzz's check_assign.
    plan.glyph_map
        .get(&gid.into())
        .and_then(|g| GlyphId16::try_from(*g).ok())
        .ok_or_else(|| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_INT_OVERFLOW))
}

impl<'a> SubsetTable<'a> for MathValueRecord {
    type ArgsForSubset = FontData<'a>;
    type Output = ();

    // MathValueRecord::copy preserves both hint devices and variation indices.
    // Its device offset is relative to the enclosing table, not the record.
    fn subset(
        &self,
        plan: &Plan,
        s: &mut Serializer,
        data: FontData<'a>,
    ) -> Result<(), SerializeErrorFlags> {
        let device = self
            .device(data)
            .transpose()
            .map_err(|_| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR))?;
        let delta = match &device {
            Some(DeviceOrVariationIndex::VariationIndex(index)) => {
                let index = variation_index(index);
                plan.layout_varidx_delta_map
                    .get(&index)
                    .map_or(0, |&(_, delta)| delta)
            }
            _ => 0,
        };
        let value = i32::from(self.value().to_i16())
            .checked_add(delta)
            .and_then(|value| i16::try_from(value).ok())
            .ok_or_else(|| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_INT_OVERFLOW))?;
        s.embed(value)?;
        let pos = s.embed(0u16)?;
        if let Some(device) = device {
            Offset16::serialize_subset(&device, s, plan, &plan.layout_varidx_delta_map, pos)
                .is_empty()?;
        }
        Ok(())
    }
}

fn constant_value_records<'a>(
    constants: &MathConstants<'a>,
) -> Result<&'a [MathValueRecord], ReadError> {
    let start = constants.math_leading_byte_range().start;
    let end = constants
        .radical_degree_bottom_raise_percent_byte_range()
        .start;
    let records = constants.offset_data().read_array(start..end)?;
    if records.len() != MATH_VALUE_RECORD_COUNT {
        return Err(ReadError::MalformedData(
            "unexpected MathConstants record count",
        ));
    }
    Ok(records)
}

impl SubsetTable<'_> for MathConstants<'_> {
    type ArgsForSubset = ();
    type Output = ();
    fn subset(&self, plan: &Plan, s: &mut Serializer, _: ()) -> Result<(), SerializeErrorFlags> {
        // Two signed percentages, two minimum heights, MATH_VALUE_RECORD_COUNT
        // MathValueRecords, and the final percentage. Copy devices with their parent base.
        let start = self.math_leading_byte_range().start;
        s.embed_bytes(&self.min_table_bytes()[..start])?;
        let records = read_or_fail(s, constant_value_records(self))?;
        for record in records {
            record.subset(plan, s, self.offset_data())?;
        }
        s.embed(self.radical_degree_bottom_raise_percent())?;
        Ok(())
    }
}

fn copy_math_record_array<T>(
    coverage: &CoverageTable,
    records: &[T],
    plan: &Plan,
    s: &mut Serializer,
    mut subset_record: impl FnMut(&T, &mut Serializer) -> Result<(), SerializeErrorFlags>,
) -> Result<(), SerializeErrorFlags> {
    let coverage_pos = s.embed(0u16)?;
    let count_pos = s.embed(0u16)?;
    let mut retained = Vec::with_capacity(records.len());
    for (gid, record) in coverage.iter().zip(records) {
        if plan.glyphset_mathed.contains(gid) {
            let gid = map_glyph(plan, s, gid)?;
            subset_record(record, s)?;
            retained.push(gid.into());
        }
    }
    s.check_assign::<u16>(
        count_pos,
        retained.len(),
        SerializeErrorFlags::SERIALIZE_ERROR_INT_OVERFLOW,
    )?;
    Offset16::serialize_serialize::<CoverageTable>(s, &retained, coverage_pos)
}

impl SubsetTable<'_> for MathItalicsCorrectionInfo<'_> {
    type ArgsForSubset = ();
    type Output = ();
    fn subset(&self, plan: &Plan, s: &mut Serializer, _: ()) -> Result<(), SerializeErrorFlags> {
        let coverage = read_or_fail(s, self.coverage())?;
        check_len(
            s,
            self.italics_correction().len(),
            self.italics_correction_count() as usize,
        )?;
        copy_math_record_array(
            &coverage,
            self.italics_correction(),
            plan,
            s,
            |record, s| record.subset(plan, s, self.offset_data()),
        )
    }
}

impl SubsetTable<'_> for MathTopAccentAttachment<'_> {
    type ArgsForSubset = ();
    type Output = ();
    fn subset(&self, plan: &Plan, s: &mut Serializer, _: ()) -> Result<(), SerializeErrorFlags> {
        let coverage = read_or_fail(s, self.top_accent_coverage())?;
        check_len(
            s,
            self.top_accent_attachment().len(),
            self.top_accent_attachment_count() as usize,
        )?;
        copy_math_record_array(
            &coverage,
            self.top_accent_attachment(),
            plan,
            s,
            |record, s| record.subset(plan, s, self.offset_data()),
        )
    }
}

impl SubsetTable<'_> for MathKern<'_> {
    type ArgsForSubset = ();
    type Output = ();
    fn subset(&self, plan: &Plan, s: &mut Serializer, _: ()) -> Result<(), SerializeErrorFlags> {
        let count = self.height_count() as usize;
        check_len(s, self.correction_height().len(), count)?;
        check_len(s, self.kern_values().len(), count + 1)?;
        s.embed(self.height_count())?;
        for record in self.correction_height().iter().chain(self.kern_values()) {
            record.subset(plan, s, self.offset_data())?;
        }
        Ok(())
    }
}

impl SubsetTable<'_> for MathKernInfo<'_> {
    type ArgsForSubset = ();
    type Output = ();
    fn subset(&self, plan: &Plan, s: &mut Serializer, _: ()) -> Result<(), SerializeErrorFlags> {
        let coverage = read_or_fail(s, self.math_kern_coverage())?;
        check_len(
            s,
            self.math_kern_info_records().len(),
            self.math_kern_count() as usize,
        )?;
        copy_math_record_array(
            &coverage,
            self.math_kern_info_records(),
            plan,
            s,
            |record, s| {
                for kern in kern_corners(record, self.offset_data()) {
                    let pos = s.embed(0u16)?;
                    if let Some(kern) = kern {
                        let kern = read_or_fail(s, kern)?;
                        Offset16::serialize_subset(&kern, s, plan, (), pos)?;
                    }
                }
                Ok(())
            },
        )
    }
}

impl SubsetTable<'_> for MathGlyphInfo<'_> {
    type ArgsForSubset = ();
    type Output = ();
    fn subset(&self, plan: &Plan, s: &mut Serializer, _: ()) -> Result<(), SerializeErrorFlags> {
        let italics_pos = s.embed(0u16)?;
        let accent_pos = s.embed(0u16)?;
        let extended_pos = s.embed(0u16)?;
        let kern_pos = s.embed(0u16)?;
        if let Some(table) = self.math_italics_correction_info() {
            let table = read_or_fail(s, table)?;
            Offset16::serialize_subset(&table, s, plan, (), italics_pos)?;
        }
        if let Some(table) = self.math_top_accent_attachment() {
            let table = read_or_fail(s, table)?;
            Offset16::serialize_subset(&table, s, plan, (), accent_pos)?;
        }
        if let Some(coverage) = self.extended_shape_coverage() {
            let coverage = read_or_fail(s, coverage)?;
            let retained = coverage
                .iter()
                .take(plan.font_num_glyphs)
                .filter(|g| plan.glyphset_mathed.contains(*g))
                .map(|g| map_glyph(plan, s, g).map(Into::into))
                .collect::<Result<Vec<_>, _>>()?;
            if !retained.is_empty() {
                Offset16::serialize_serialize::<CoverageTable>(s, &retained, extended_pos)?;
            }
        }
        if let Some(table) = self.math_kern_info() {
            let table = read_or_fail(s, table)?;
            Offset16::serialize_subset(&table, s, plan, (), kern_pos)?;
        }
        Ok(())
    }
}

impl SubsetTable<'_> for GlyphAssembly<'_> {
    type ArgsForSubset = ();
    type Output = ();
    fn subset(&self, plan: &Plan, s: &mut Serializer, _: ()) -> Result<(), SerializeErrorFlags> {
        check_len(s, self.part_records().len(), self.part_count() as usize)?;
        self.italics_correction()
            .subset(plan, s, self.offset_data())?;
        s.embed(self.part_count())?;
        for record in self.part_records() {
            let gid = map_glyph(plan, s, record.glyph_id())?;
            s.embed(gid)?;
            s.embed(record.start_connector_length())?;
            s.embed(record.end_connector_length())?;
            s.embed(record.full_advance())?;
            s.embed(record.part_flags())?;
        }
        Ok(())
    }
}

impl SubsetTable<'_> for MathGlyphConstruction<'_> {
    type ArgsForSubset = ();
    type Output = ();
    fn subset(&self, plan: &Plan, s: &mut Serializer, _: ()) -> Result<(), SerializeErrorFlags> {
        check_len(
            s,
            self.math_glyph_variant_records().len(),
            self.variant_count() as usize,
        )?;
        let assembly_pos = s.embed(0u16)?;
        if let Some(assembly) = self.glyph_assembly() {
            let assembly = read_or_fail(s, assembly)?;
            Offset16::serialize_subset(&assembly, s, plan, (), assembly_pos)?;
        }
        s.embed(self.variant_count())?;
        for record in self.math_glyph_variant_records() {
            let gid = map_glyph(plan, s, record.variant_glyph())?;
            s.embed(gid)?;
            s.embed(record.advance_measurement())?;
        }
        Ok(())
    }
}

fn collect_coverage_and_indices(
    coverage: Option<Result<CoverageTable, ReadError>>,
    count: usize,
    plan: &Plan,
    s: &mut Serializer,
) -> Result<(Vec<GlyphId>, Vec<usize>), SerializeErrorFlags> {
    let mut retained = Vec::with_capacity(count);
    let mut indices = Vec::with_capacity(count);
    if let Some(coverage) = coverage {
        let coverage = read_or_fail(s, coverage)?;
        for (index, gid) in coverage.iter().take(count).enumerate() {
            if plan.glyphset_cmaped.contains(gid) {
                retained.push(map_glyph(plan, s, gid)?.into());
                indices.push(index);
            }
        }
    }
    Ok((retained, indices))
}

fn serialize_constructions<'a>(
    constructions: ArrayOfOffsets<'a, MathGlyphConstruction<'a>, Offset16>,
    indices: &[usize],
    plan: &Plan,
    s: &mut Serializer,
) -> Result<(), SerializeErrorFlags> {
    for &index in indices {
        let pos = s.embed(0u16)?;
        match constructions.get(index) {
            Err(ReadError::NullOffset) => (),
            table => {
                let table = read_or_fail(s, table)?;
                Offset16::serialize_subset(&table, s, plan, (), pos)?;
            }
        }
    }
    Ok(())
}

impl SubsetTable<'_> for MathVariants<'_> {
    type ArgsForSubset = ();
    type Output = ();
    fn subset(&self, plan: &Plan, s: &mut Serializer, _: ()) -> Result<(), SerializeErrorFlags> {
        check_len(
            s,
            self.vert_glyph_construction_offsets().len(),
            self.vert_glyph_count() as usize,
        )?;
        check_len(
            s,
            self.horiz_glyph_construction_offsets().len(),
            self.horiz_glyph_count() as usize,
        )?;
        let (vertical, vert_indices) = collect_coverage_and_indices(
            self.vert_glyph_coverage(),
            self.vert_glyph_count() as usize,
            plan,
            s,
        )?;
        let (horizontal, horiz_indices) = collect_coverage_and_indices(
            self.horiz_glyph_coverage(),
            self.horiz_glyph_count() as usize,
            plan,
            s,
        )?;
        s.embed(self.min_connector_overlap())?;
        let vert_pos = s.embed(0u16)?;
        let horiz_pos = s.embed(0u16)?;
        let vert_count_pos = s.embed(0u16)?;
        let horiz_count_pos = s.embed(0u16)?;
        s.check_assign::<u16>(
            vert_count_pos,
            vertical.len(),
            SerializeErrorFlags::SERIALIZE_ERROR_INT_OVERFLOW,
        )?;
        s.check_assign::<u16>(
            horiz_count_pos,
            horizontal.len(),
            SerializeErrorFlags::SERIALIZE_ERROR_INT_OVERFLOW,
        )?;
        serialize_constructions(self.vert_glyph_constructions(), &vert_indices, plan, s)?;
        serialize_constructions(self.horiz_glyph_constructions(), &horiz_indices, plan, s)?;
        if !vertical.is_empty() {
            Offset16::serialize_serialize::<CoverageTable>(s, &vertical, vert_pos)?;
        }
        if !horizontal.is_empty() {
            Offset16::serialize_serialize::<CoverageTable>(s, &horizontal, horiz_pos)?;
        }
        Ok(())
    }
}

fn collect_value_indices(
    record: &MathValueRecord,
    data: FontData,
    plan: &Plan,
    indices: &mut IntSet<u32>,
) {
    if let Some(Ok(device)) = record.device(data) {
        device.collect_variation_indices(plan, indices);
    }
}

fn collect_covered_value_indices(
    coverage: Result<CoverageTable, ReadError>,
    records: &[MathValueRecord],
    data: FontData,
    plan: &Plan,
    indices: &mut IntSet<u32>,
) {
    let Ok(coverage) = coverage else { return };
    for (gid, record) in coverage.iter().zip(records) {
        if plan.glyphset_mathed.contains(gid) {
            collect_value_indices(record, data, plan, indices);
        }
    }
}

impl CollectVariationIndices for MathConstants<'_> {
    fn collect_variation_indices(&self, plan: &Plan, indices: &mut IntSet<u32>) {
        let Ok(records) = constant_value_records(self) else {
            return;
        };
        for record in records {
            collect_value_indices(record, self.offset_data(), plan, indices);
        }
    }
}

impl CollectVariationIndices for MathItalicsCorrectionInfo<'_> {
    fn collect_variation_indices(&self, plan: &Plan, indices: &mut IntSet<u32>) {
        collect_covered_value_indices(
            self.coverage(),
            self.italics_correction(),
            self.offset_data(),
            plan,
            indices,
        );
    }
}

impl CollectVariationIndices for MathTopAccentAttachment<'_> {
    fn collect_variation_indices(&self, plan: &Plan, indices: &mut IntSet<u32>) {
        collect_covered_value_indices(
            self.top_accent_coverage(),
            self.top_accent_attachment(),
            self.offset_data(),
            plan,
            indices,
        );
    }
}

impl CollectVariationIndices for MathKern<'_> {
    fn collect_variation_indices(&self, plan: &Plan, indices: &mut IntSet<u32>) {
        for record in self.correction_height().iter().chain(self.kern_values()) {
            collect_value_indices(record, self.offset_data(), plan, indices);
        }
    }
}

impl CollectVariationIndices for MathKernInfo<'_> {
    fn collect_variation_indices(&self, plan: &Plan, indices: &mut IntSet<u32>) {
        let Ok(coverage) = self.math_kern_coverage() else {
            return;
        };
        for (gid, record) in coverage.iter().zip(self.math_kern_info_records()) {
            if !plan.glyphset_mathed.contains(gid) {
                continue;
            }
            for kern in kern_corners(record, self.offset_data()) {
                if let Some(Ok(kern)) = kern {
                    kern.collect_variation_indices(plan, indices);
                }
            }
        }
    }
}

impl CollectVariationIndices for MathGlyphInfo<'_> {
    fn collect_variation_indices(&self, plan: &Plan, indices: &mut IntSet<u32>) {
        if let Some(Ok(italics)) = self.math_italics_correction_info() {
            italics.collect_variation_indices(plan, indices);
        }
        if let Some(Ok(accents)) = self.math_top_accent_attachment() {
            accents.collect_variation_indices(plan, indices);
        }
        if let Some(Ok(kern_info)) = self.math_kern_info() {
            kern_info.collect_variation_indices(plan, indices);
        }
    }
}

impl CollectVariationIndices for GlyphAssembly<'_> {
    fn collect_variation_indices(&self, plan: &Plan, indices: &mut IntSet<u32>) {
        collect_value_indices(self.italics_correction(), self.offset_data(), plan, indices);
    }
}

impl CollectVariationIndices for MathGlyphConstruction<'_> {
    fn collect_variation_indices(&self, plan: &Plan, indices: &mut IntSet<u32>) {
        if let Some(Ok(assembly)) = self.glyph_assembly() {
            assembly.collect_variation_indices(plan, indices);
        }
    }
}

impl CollectVariationIndices for MathVariants<'_> {
    fn collect_variation_indices(&self, plan: &Plan, indices: &mut IntSet<u32>) {
        for (coverage, constructions) in [
            (self.vert_glyph_coverage(), self.vert_glyph_constructions()),
            (
                self.horiz_glyph_coverage(),
                self.horiz_glyph_constructions(),
            ),
        ] {
            let Some(Ok(coverage)) = coverage else {
                continue;
            };
            for (gid, construction) in coverage.iter().zip(constructions.iter()) {
                if !plan.glyphset_cmaped.contains(gid) {
                    continue;
                }
                if let Ok(construction) = construction {
                    construction.collect_variation_indices(plan, indices);
                }
            }
        }
    }
}

impl CollectVariationIndices for Math<'_> {
    fn collect_variation_indices(&self, plan: &Plan, indices: &mut IntSet<u32>) {
        if self.version().major != 1 {
            return;
        }
        if let Ok(constants) = self.math_constants() {
            constants.collect_variation_indices(plan, indices);
        }
        if let Ok(info) = self.math_glyph_info() {
            info.collect_variation_indices(plan, indices);
        }
        if let Ok(variants) = self.math_variants() {
            variants.collect_variation_indices(plan, indices);
        }
    }
}

impl Subset for Math<'_> {
    fn subset(
        &self,
        plan: &Plan,
        _font: &FontRef,
        s: &mut Serializer,
        _builder: &mut FontBuilder,
    ) -> Result<(), SubsetError> {
        subset_math(self, plan, s).map_err(|_| SubsetError::SubsetTableError(Math::TAG))
    }
}

fn subset_math(math: &Math, plan: &Plan, s: &mut Serializer) -> Result<(), SerializeErrorFlags> {
    if math.version().major != 1 {
        return Err(s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR));
    }
    s.embed(math.version())?;
    let constants_pos = s.embed(0u16)?;
    let info_pos = s.embed(0u16)?;
    let variants_pos = s.embed(0u16)?;
    if !math.math_constants_offset().is_null() {
        let constants = read_or_fail(s, math.math_constants())?;
        Offset16::serialize_subset(&constants, s, plan, (), constants_pos)?;
    }
    if !math.math_glyph_info_offset().is_null() {
        let info = read_or_fail(s, math.math_glyph_info())?;
        Offset16::serialize_subset(&info, s, plan, (), info_pos)?;
    }
    if !math.math_variants_offset().is_null() {
        let variants = read_or_fail(s, math.math_variants())?;
        Offset16::serialize_subset(&variants, s, plan, (), variants_pos)?;
    }
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;
    use write_fonts::tables::{layout::VariationIndex, math::MathValueRecord as OwnedValue};

    #[test]
    fn unsupported_versions_skip_closure_and_variation_collection() {
        use write_fonts::{
            tables::{layout::CoverageTable as OwnedCoverage, math as owned},
            types::GlyphId16,
        };

        let math = owned::Math::new(
            owned::MathConstants {
                math_leading: OwnedValue::new(10.into(), Some(VariationIndex::new(1, 2).into())),
                ..Default::default()
            },
            owned::MathGlyphInfo::default(),
            owned::MathVariants::new(
                10.into(),
                Some(OwnedCoverage::from_iter([GlyphId16::new(2)])),
                None,
                vec![owned::MathGlyphConstruction::new(
                    None,
                    vec![owned::MathGlyphVariantRecord::new(
                        GlyphId16::new(5),
                        100.into(),
                    )],
                )],
                vec![],
            ),
        );
        let source = write_fonts::dump_table(&math).unwrap();
        for major in [0u16, 1, 2, u16::MAX] {
            let mut source = source.clone();
            source[..2].copy_from_slice(&major.to_be_bytes());
            let mut builder = FontBuilder::new();
            builder.add_raw(Math::TAG, source);
            let bytes = builder.build();
            let font = FontRef::new(&bytes).unwrap();
            let mut glyphs = IntSet::from_iter([GlyphId::new(2)]);
            closure(&font, &mut glyphs);
            let expected = if major == 1 { vec![2, 5] } else { vec![2] };
            assert_eq!(
                glyphs.iter().map(GlyphId::to_u32).collect::<Vec<_>>(),
                expected
            );

            let mut indices = IntSet::from_iter([99]);
            font.math()
                .unwrap()
                .collect_variation_indices(&Plan::default(), &mut indices);
            let expected = if major == 1 {
                vec![99, 0x10002]
            } else {
                vec![99]
            };
            assert_eq!(indices.iter().collect::<Vec<_>>(), expected);
        }
    }

    #[test]
    fn value_records_apply_deltas_and_remap_variation_indices() {
        let source = write_fonts::dump_table(&OwnedValue::new(
            10.into(),
            Some(VariationIndex::new(1, 2).into()),
        ))
        .unwrap();
        let source_data = FontData::new(&source);
        let value = source_data.read_array::<MathValueRecord>(0..4).unwrap()[0];
        let mut plan = Plan::default();
        plan.layout_varidx_delta_map.insert(0x10002, (0x30004, 5));
        let mut serializer = Serializer::new(64).unwrap();
        serializer.start_serialize().unwrap();
        value.subset(&plan, &mut serializer, source_data).unwrap();
        serializer.end_serialize();

        let bytes = serializer.copy_bytes();
        let data = FontData::new(&bytes);
        let value = data.read_array::<MathValueRecord>(0..4).unwrap()[0];
        assert_eq!(value.value().to_i16(), 15);
        let DeviceOrVariationIndex::VariationIndex(index) = value.device(data).unwrap().unwrap()
        else {
            panic!("expected variation index")
        };
        assert_eq!(index.delta_set_outer_index(), 3);
        assert_eq!(index.delta_set_inner_index(), 4);
    }

    #[test]
    fn value_records_drop_unmappable_variation_indices() {
        for (index, mapped, expected_value) in [
            ((1, 2), None, 10),
            ((0xffff, 0xffff), None, 10),
            ((1, 2), Some((0xffffffff, 5)), 15),
        ] {
            let source = write_fonts::dump_table(&OwnedValue::new(
                10.into(),
                Some(VariationIndex::new(index.0, index.1).into()),
            ))
            .unwrap();
            let source_data = FontData::new(&source);
            let value = source_data.read_array::<MathValueRecord>(0..4).unwrap()[0];
            let mut plan = Plan::default();
            if let Some(mapped) = mapped {
                plan.layout_varidx_delta_map.insert(0x10002, mapped);
            }
            let mut serializer = Serializer::new(64).unwrap();
            serializer.start_serialize().unwrap();
            value.subset(&plan, &mut serializer, source_data).unwrap();
            serializer.end_serialize();

            let bytes = serializer.copy_bytes();
            let data = FontData::new(&bytes);
            let value = data.read_array::<MathValueRecord>(0..4).unwrap()[0];
            assert_eq!(value.value().to_i16(), expected_value);
            assert!(value.device(data).is_none());
        }
    }
}

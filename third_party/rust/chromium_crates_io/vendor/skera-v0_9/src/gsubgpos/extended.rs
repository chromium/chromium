//! Subsetting the extended contextual lookup formats shared by GSUB and GPOS.

use super::*;
use read_fonts::{
    tables::layout::{
        ChainedSequenceContextFormat4, ChainedSequenceRule2, ChainedSequenceRuleSet2,
        SequenceContextFormat4, SequenceContextFormat6, SequenceRule2, SequenceRuleSet2,
    },
    types::{GlyphId24, Offset24, Offset32, Uint24},
    MinByteRange,
};

macro_rules! subset_glyph_context {
    ($table:ident, $sets:ident) => {
        impl<'a> SubsetTable<'a> for $table<'_> {
            type ArgsForSubset = &'a FastHashMap<u16, u16>;
            type Output = ();
            fn subset(
                &self,
                plan: &Plan,
                s: &mut Serializer,
                lookup_map: Self::ArgsForSubset,
            ) -> Result<(), SerializeErrorFlags> {
                if self.coverage_offset().is_null() {
                    return Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY);
                }
                let coverage = self
                    .coverage()
                    .map_err(|_| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR))?;
                let glyph_set = &plan.glyphset_gsub;
                let sets = self.$sets();
                let set_count = sets.len() as u32;
                s.embed(self.format())?;
                let coverage_pos = s.embed(0u32)?;
                let count_pos = s.embed(Uint24::new(0))?;
                let mut retained_glyphs = Vec::with_capacity(
                    coverage
                        .population()
                        .min(glyph_set.len() as usize)
                        .min(set_count as usize),
                );
                for_each_intersected_glyph_and_index(
                    &coverage,
                    glyph_set,
                    &plan.glyph_map_gsub,
                    set_count,
                    |index, gid| {
                        if !sets
                            .subset_offset(index as usize, s, plan, lookup_map)
                            .is_empty()?
                        {
                            retained_glyphs.push(gid);
                        }
                        Ok(())
                    },
                )?;
                if retained_glyphs.is_empty() {
                    return Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY);
                }
                let count = u32::try_from(retained_glyphs.len())
                    .ok()
                    .and_then(Uint24::checked_new)
                    .ok_or_else(|| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_INT_OVERFLOW))?;
                s.copy_assign(count_pos, count);
                Offset32::serialize_serialize::<CoverageTable>(s, &retained_glyphs, coverage_pos)
            }
        }
    };
}
subset_glyph_context!(SequenceContextFormat4, seq_rule_sets);
subset_glyph_context!(ChainedSequenceContextFormat4, chained_seq_rule_sets);

macro_rules! subset_glyph_rule_set {
    ($table:ident, $rules:ident, $offset:ident) => {
        impl<'a> SubsetTable<'a> for $table<'_> {
            type ArgsForSubset = &'a FastHashMap<u16, u16>;
            type Output = ();
            fn subset(
                &self,
                plan: &Plan,
                s: &mut Serializer,
                lookup_map: Self::ArgsForSubset,
            ) -> Result<(), SerializeErrorFlags> {
                let count_pos = s.embed(0u16)?;
                let mut count = 0usize;
                for rule in self.$rules().iter_as_nullable() {
                    let Some(rule) = rule
                        .transpose()
                        .map_err(|_| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR))?
                    else {
                        continue;
                    };
                    let snap = s.snapshot();
                    let offset_pos = s.allocate_size($offset::RAW_BYTE_LEN, true)?;
                    if $offset::serialize_subset(&rule, s, plan, lookup_map, offset_pos)
                        .is_empty()?
                    {
                        s.revert_snapshot(snap);
                    } else {
                        count += 1;
                    }
                }
                if count == 0 {
                    return Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY);
                }
                let count = u16::try_from(count)
                    .map_err(|_| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_INT_OVERFLOW))?;
                s.copy_assign(count_pos, count);
                Ok(())
            }
        }
    };
}
// Plain rule offsets remain 16-bit in the ISO extended format.
subset_glyph_rule_set!(SequenceRuleSet2, seq_rules, Offset16);
subset_glyph_rule_set!(ChainedSequenceRuleSet2, chained_seq_rules, Offset24);

fn serialize_glyph_sequence24(
    sequence: &[BigEndian<GlyphId24>],
    plan: &Plan,
    s: &mut Serializer,
) -> Result<(), SerializeErrorFlags> {
    for gid in sequence {
        let mapped = map_gsub_glyph(&plan.glyph_map_gsub, gid.get().into())
            .ok_or(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY)?;
        let mapped = GlyphId24::checked_new(mapped.to_u32())
            .ok_or_else(|| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_INT_OVERFLOW))?;
        s.embed(mapped)?;
    }
    Ok(())
}

impl<'a> SubsetTable<'a> for SequenceRule2<'_> {
    type ArgsForSubset = &'a FastHashMap<u16, u16>;
    type Output = ();
    fn subset(
        &self,
        plan: &Plan,
        s: &mut Serializer,
        lookup_map: Self::ArgsForSubset,
    ) -> Result<(), SerializeErrorFlags> {
        if self.glyph_count() == 0 {
            return Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY);
        }
        if self.min_table_bytes().is_empty() {
            return Err(s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR));
        }
        s.embed(self.glyph_count())?;
        let count_pos = s.embed(0u16)?;
        serialize_glyph_sequence24(self.input_sequence(), plan, s)?;
        let count = serialize_lookup_records(self.seq_lookup_records(), plan, lookup_map, s)?;
        s.copy_assign(count_pos, count);
        Ok(())
    }
}

impl<'a> SubsetTable<'a> for ChainedSequenceRule2<'_> {
    type ArgsForSubset = &'a FastHashMap<u16, u16>;
    type Output = ();
    fn subset(
        &self,
        plan: &Plan,
        s: &mut Serializer,
        lookup_map: Self::ArgsForSubset,
    ) -> Result<(), SerializeErrorFlags> {
        if self.min_table_bytes().is_empty() {
            return Err(s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR));
        }
        if self.input_glyph_count() == 0 {
            return Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY);
        }
        s.embed(self.backtrack_glyph_count())?;
        serialize_glyph_sequence24(self.backtrack_sequence(), plan, s)?;
        s.embed(self.input_glyph_count())?;
        serialize_glyph_sequence24(self.input_sequence(), plan, s)?;
        s.embed(self.lookahead_glyph_count())?;
        serialize_glyph_sequence24(self.lookahead_sequence(), plan, s)?;
        let count_pos = s.embed(0u16)?;
        let count = serialize_lookup_records(self.seq_lookup_records(), plan, lookup_map, s)?;
        s.copy_assign(count_pos, count);
        Ok(())
    }
}

impl<'a> SubsetTable<'a> for SequenceContextFormat6<'_> {
    type ArgsForSubset = &'a FastHashMap<u16, u16>;
    type Output = ();
    fn subset(
        &self,
        plan: &Plan,
        s: &mut Serializer,
        lookup_map: Self::ArgsForSubset,
    ) -> Result<(), SerializeErrorFlags> {
        if self.min_table_bytes().is_empty() {
            return Err(s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR));
        }
        s.embed(self.format())?;
        s.embed(self.glyph_count())?;
        let count_pos = s.embed(0u16)?;
        self.coverages().subset(plan, s, ())?;
        let count = serialize_lookup_records(self.seq_lookup_records(), plan, lookup_map, s)?;
        s.copy_assign(count_pos, count);
        Ok(())
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use font_test_data::bebuffer::BeBuffer;
    use read_fonts::{FontData, FontRead};

    fn rule(chain: bool, input: u32, records: &[(u16, u16)]) -> Vec<u8> {
        let mut bytes = BeBuffer::new();
        if chain {
            bytes = bytes
                .push(1u16)
                .push(GlyphId24::new(65535))
                .push(2u16)
                .push(GlyphId24::new(input))
                .push(1u16)
                .push(GlyphId24::new(70002))
                .push(records.len() as u16);
        } else {
            bytes = bytes
                .push(2u16)
                .push(records.len() as u16)
                .push(GlyphId24::new(input));
        }
        for &(sequence, lookup) in records {
            bytes = bytes.push(sequence).push(lookup);
        }
        bytes.to_vec()
    }

    fn input(chain: bool) -> Vec<u8> {
        let first = rule(chain, 70000, &[(0, 7), (1, 8), (1, 7)]);
        let second = rule(chain, 70001, &[(1, 7)]);
        let mut bytes = BeBuffer::new()
            .push(4u16)
            .push(12u32)
            .push(Uint24::new(1))
            .push(Uint24::new(20))
            .push(3u16)
            .push(Uint24::new(1))
            .push(GlyphId24::new(65536))
            .push(2u16);
        if chain {
            bytes = bytes
                .push(Uint24::new(8))
                .push(Uint24::new(8 + first.len() as u32));
        } else {
            bytes = bytes.push(6u16).push(6u16 + first.len() as u16);
        }
        bytes.extend(first).extend(second).to_vec()
    }

    fn make_plan(mapped: [u32; 5]) -> Plan {
        let mut plan = Plan {
            glyph_map_gsub: vec![crate::INVALID_GID; 70003],
            ..Default::default()
        };
        for (old, new) in [65535, 65536, 70000, 70001, 70002].into_iter().zip(mapped) {
            plan.glyphset_gsub.insert(GlyphId::new(old));
            plan.glyph_map_gsub[old as usize] = GlyphId::new(new);
        }
        plan.gsub_lookups.insert(7, 2);
        plan
    }

    fn subset(chain: bool, input: &[u8], plan: &Plan) -> Result<Vec<u8>, SerializeErrorFlags> {
        let mut s = Serializer::new(2 * 1024 * 1024);
        s.start_serialize().unwrap();
        let font = FontRef::new(font_test_data::NOTOSERIFHEBREW_AUTOHINT_METRICS).unwrap();
        let args = (&SubsetState::default(), &font, &plan.gsub_lookups);
        if chain {
            ChainedSequenceContext::read(FontData::new(input))
                .unwrap()
                .subset(plan, &mut s, args)?;
        } else {
            SequenceContext::read(FontData::new(input))
                .unwrap()
                .subset(plan, &mut s, args)?;
        }
        s.end_serialize();
        assert!(!s.in_error());
        Ok(s.copy_bytes())
    }

    fn assert_output(chain: bool, bytes: &[u8], mapped: [u32; 5], rule_count: u16) {
        if chain {
            let table = ChainedSequenceContextFormat4::read(FontData::new(bytes)).unwrap();
            assert_eq!(table.chained_seq_rule_set_count().to_u32(), 1);
            assert_eq!(
                table.coverage().unwrap().get(GlyphId::new(mapped[1])),
                Some(0)
            );
            let set = table.chained_seq_rule_sets().get(0).unwrap().unwrap();
            assert_eq!(set.chained_seq_rule_count(), rule_count);
            for index in 0..rule_count {
                let rule = set.chained_seq_rules().get(index as usize).unwrap();
                assert_eq!(rule.backtrack_sequence()[0].get().to_u32(), mapped[0]);
                assert_eq!(
                    rule.input_sequence()[0].get().to_u32(),
                    mapped[2 + index as usize]
                );
                assert_eq!(rule.lookahead_sequence()[0].get().to_u32(), mapped[4]);
                let records: Vec<_> = rule
                    .seq_lookup_records()
                    .iter()
                    .map(|r| (r.sequence_index(), r.lookup_list_index()))
                    .collect();
                assert_eq!(
                    records,
                    if index == 0 {
                        vec![(0, 2), (1, 2)]
                    } else {
                        vec![(1, 2)]
                    }
                );
            }
        } else {
            let table = SequenceContextFormat4::read(FontData::new(bytes)).unwrap();
            assert_eq!(table.seq_rule_set_count().to_u32(), 1);
            assert_eq!(
                table.coverage().unwrap().get(GlyphId::new(mapped[1])),
                Some(0)
            );
            let set = table.seq_rule_sets().get(0).unwrap().unwrap();
            assert_eq!(set.seq_rule_count(), rule_count);
            for index in 0..rule_count {
                let rule = set.seq_rules().get(index as usize).unwrap();
                assert_eq!(
                    rule.input_sequence()[0].get().to_u32(),
                    mapped[2 + index as usize]
                );
                let records: Vec<_> = rule
                    .seq_lookup_records()
                    .iter()
                    .map(|r| (r.sequence_index(), r.lookup_list_index()))
                    .collect();
                assert_eq!(
                    records,
                    if index == 0 {
                        vec![(0, 2), (1, 2)]
                    } else {
                        vec![(1, 2)]
                    }
                );
            }
        }
    }

    #[test]
    fn glyph_context_subset_preserves_format_rule_order_and_remaps_lookups() {
        for chain in [false, true] {
            for mapped in [[1, 2, 3, 4, 5], [65535, 65536, 70000, 70001, 0xffffff]] {
                let bytes = subset(chain, &input(chain), &make_plan(mapped)).unwrap();
                assert_output(chain, &bytes, mapped, 2);
            }
        }
    }

    #[test]
    fn glyph_context_subset_prunes_rules_with_missing_glyphs_and_null_offsets() {
        for chain in [false, true] {
            let mapped = [1, 2, 3, 4, 5];
            let mut plan = make_plan(mapped);
            plan.glyph_map_gsub[70001] = crate::INVALID_GID;
            let bytes = subset(chain, &input(chain), &plan).unwrap();
            assert_output(chain, &bytes, mapped, 1);
            plan.glyph_map_gsub[70000] = crate::INVALID_GID;
            assert_eq!(
                subset(chain, &input(chain), &plan),
                Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY)
            );
            let plan = make_plan(mapped);
            for range in [2..6, 9..12] {
                let mut bytes = input(chain);
                bytes[range].fill(0);
                assert_eq!(
                    subset(chain, &bytes, &plan),
                    Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY)
                );
            }
            let mut bytes = input(chain);
            let width = if chain { 3 } else { 2 };
            bytes[22 + width..22 + width * 2].fill(0);
            let bytes = subset(chain, &bytes, &plan).unwrap();
            assert_output(chain, &bytes, mapped, 1);
        }
        for excluded in [65535, 70002] {
            let mut plan = make_plan([1, 2, 3, 4, 5]);
            plan.glyph_map_gsub[excluded] = crate::INVALID_GID;
            assert_eq!(
                subset(true, &input(true), &plan),
                Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY)
            );
        }
    }

    #[test]
    fn glyph_context_subset_keeps_large_source_indices_and_retained_set_counts() {
        let count = 65537u32;
        for chain in [false, true] {
            let base = input(chain);
            let coverage_offset = 9 + count * 3;
            let set_offset = coverage_offset + 14;
            let source = BeBuffer::new()
                .push(4u16)
                .push(coverage_offset)
                .push(Uint24::new(count))
                .extend((0..count).map(|i| Uint24::new(if i == 65536 { set_offset } else { 0 })))
                .push(4u16)
                .push(Uint24::new(1))
                .extend([0, 65536, 0].map(Uint24::new))
                .extend(base[20..].iter().copied());
            let bytes = subset(chain, &source, &make_plan([1, 2, 3, 4, 5])).unwrap();
            assert_output(chain, &bytes, [1, 2, 3, 4, 5], 2);

            let source = BeBuffer::new()
                .push(4u16)
                .push(coverage_offset)
                .push(Uint24::new(count))
                .extend((0..count).map(|_| Uint24::new(set_offset)))
                .push(4u16)
                .push(Uint24::new(1))
                .extend([0, 65536, 0].map(Uint24::new))
                .extend(base[20..].iter().copied());
            let mut plan = make_plan([65535, 65536, 70000, 70001, 70002]);
            plan.glyphset_gsub
                .insert_range(GlyphId::new(0)..=GlyphId::new(65536));
            for gid in 0..count {
                plan.glyph_map_gsub[gid as usize] = GlyphId::new(gid);
            }
            let bytes = subset(chain, &source, &plan).unwrap();
            if chain {
                let table = ChainedSequenceContextFormat4::read(FontData::new(&bytes)).unwrap();
                assert_eq!(table.chained_seq_rule_set_count().to_u32(), count);
                assert_eq!(
                    table.coverage().unwrap().get(GlyphId::new(65536)),
                    Some(65536)
                );
                assert!(table.coverage_offset().to_u32() > 65535);
                assert_eq!(
                    table
                        .chained_seq_rule_sets()
                        .get(65536)
                        .unwrap()
                        .unwrap()
                        .chained_seq_rule_count(),
                    2
                );
            } else {
                let table = SequenceContextFormat4::read(FontData::new(&bytes)).unwrap();
                assert_eq!(table.seq_rule_set_count().to_u32(), count);
                assert_eq!(
                    table.coverage().unwrap().get(GlyphId::new(65536)),
                    Some(65536)
                );
                assert!(table.coverage_offset().to_u32() > 65535);
                assert_eq!(
                    table
                        .seq_rule_sets()
                        .get(65536)
                        .unwrap()
                        .unwrap()
                        .seq_rule_count(),
                    2
                );
            }
        }
    }

    #[test]
    fn glyph_context_subset_rejects_invalid_offsets_truncation_and_overflow() {
        for chain in [false, true] {
            let plan = make_plan([1, 2, 3, 4, 5]);
            for range in [2..6, 9..12, if chain { 22..25 } else { 22..24 }] {
                let mut bytes = input(chain);
                bytes[range].fill(0xff);
                assert_eq!(
                    subset(chain, &bytes, &plan),
                    Err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR)
                );
            }
            let mut bytes = input(chain);
            bytes.pop();
            assert_eq!(
                subset(chain, &bytes, &plan),
                Err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR)
            );
            for index in if chain { vec![0, 1, 2, 4] } else { vec![1, 2] } {
                let mut mapped = [1, 2, 3, 4, 5];
                mapped[index] = 0x1000000;
                assert_eq!(
                    subset(chain, &input(chain), &make_plan(mapped)),
                    Err(SerializeErrorFlags::SERIALIZE_ERROR_INT_OVERFLOW)
                );
            }
        }
    }

    #[test]
    fn chained_glyph_context_subset_reads_rule_offsets_above_64k() {
        let mut bytes = input(true);
        bytes[25..28].copy_from_slice(&Uint24::new(70000).to_raw());
        bytes.resize(70020, 0);
        bytes.extend(rule(true, 70001, &[(1, 7)]));
        let mapped = [1, 2, 3, 4, 5];
        let bytes = subset(true, &bytes, &make_plan(mapped)).unwrap();
        assert_output(true, &bytes, mapped, 2);
    }

    #[test]
    fn legacy_context_glyph_sequence_rejects_wide_remapping() {
        let sequence = [BigEndian::from(GlyphId16::new(1))];
        let mut s = Serializer::new(32);
        s.start_serialize().unwrap();
        assert_eq!(
            serialize_glyph_sequence(&sequence, &[GlyphId::new(0), GlyphId::new(65536)], &mut s),
            Err(SerializeErrorFlags::SERIALIZE_ERROR_INT_OVERFLOW)
        );
    }

    fn coverage_context() -> Vec<u8> {
        BeBuffer::new()
            .push(6u16)
            .push(2u16)
            .push(3u16)
            .push(Uint24::new(24))
            .push(Uint24::new(32))
            .extend([0u16, 7, 1, 8, 1, 7])
            .push(3u16)
            .push(Uint24::new(1))
            .push(GlyphId24::new(65536))
            .push(3u16)
            .push(Uint24::new(1))
            .push(GlyphId24::new(70000))
            .to_vec()
    }

    #[test]
    fn coverage_context_subset_preserves_format_and_remaps_coverages_and_lookups() {
        for mapped in [[1, 2, 3, 4, 5], [65535, 65536, 70000, 70001, 0xffffff]] {
            let bytes = subset(false, &coverage_context(), &make_plan(mapped)).unwrap();
            let table = SequenceContextFormat6::read(FontData::new(&bytes)).unwrap();
            assert_eq!(table.glyph_count(), 2);
            assert_eq!(table.seq_lookup_count(), 2);
            assert_eq!(
                table.coverages().get(0).unwrap().iter().collect::<Vec<_>>(),
                [GlyphId::new(mapped[1])]
            );
            assert_eq!(
                table.coverages().get(1).unwrap().iter().collect::<Vec<_>>(),
                [GlyphId::new(mapped[2])]
            );
            assert_eq!(
                table
                    .seq_lookup_records()
                    .iter()
                    .map(|r| (r.sequence_index(), r.lookup_list_index()))
                    .collect::<Vec<_>>(),
                [(0, 2), (1, 2)]
            );
        }
    }

    #[test]
    fn coverage_context_subset_requires_every_coverage_and_rejects_invalid_data() {
        for excluded in [65536, 70000] {
            let mut plan = make_plan([1, 2, 3, 4, 5]);
            plan.glyph_map_gsub[excluded] = crate::INVALID_GID;
            assert_eq!(
                subset(false, &coverage_context(), &plan),
                Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY)
            );
        }
        let plan = make_plan([1, 2, 3, 4, 5]);
        for range in [6..9, 9..12] {
            let mut bytes = coverage_context();
            bytes[range.clone()].fill(0);
            assert_eq!(
                subset(false, &bytes, &plan),
                Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY)
            );
            bytes[range].fill(0xff);
            assert_eq!(
                subset(false, &bytes, &plan),
                Err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR)
            );
        }
        assert_eq!(
            subset(false, &coverage_context()[..23], &plan),
            Err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR)
        );
        assert_eq!(
            subset(
                false,
                &coverage_context(),
                &make_plan([1, 2, 0x1000000, 4, 5])
            ),
            Err(SerializeErrorFlags::SERIALIZE_ERROR_INT_OVERFLOW)
        );
    }

    #[test]
    fn coverage_context_subset_preserves_maximum_count_and_large_offsets() {
        let count = u16::MAX;
        let coverage_offset = 6 + u32::from(count) * 3 + 4;
        let source = BeBuffer::new()
            .push(6u16)
            .push(count)
            .push(1u16)
            .extend((0..count).map(|_| Uint24::new(coverage_offset)))
            .push(count - 1)
            .push(7u16)
            .push(3u16)
            .push(Uint24::new(1))
            .push(GlyphId24::new(65536));
        let bytes = subset(
            false,
            &source,
            &make_plan([65535, 65536, 70000, 70001, 70002]),
        )
        .unwrap();
        let table = SequenceContextFormat6::read(FontData::new(&bytes)).unwrap();
        assert_eq!(table.glyph_count(), count);
        assert!(table.coverage_offsets()[count as usize - 1].get().to_u32() > 65535);
        assert_eq!(
            table
                .coverages()
                .get(count as usize - 1)
                .unwrap()
                .get(GlyphId::new(65536)),
            Some(0)
        );
        assert_eq!(table.seq_lookup_records()[0].sequence_index(), count - 1);
        assert_eq!(table.seq_lookup_records()[0].lookup_list_index(), 2);
    }
}

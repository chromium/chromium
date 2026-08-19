//! impl subset() for Sequence Context/Chained Sequence Context tables
use crate::fnv::FnvHashMap;
use crate::{
    layout::{intersected_glyphs_and_indices, ClassDefSubsetStruct},
    offset::{SerializeSerialize, SerializeSubset},
    offset_array::{IterNullableHelper, SubsetOffsetArray},
    serialize::{SerializeErrorFlags, Serializer},
    Plan, SubsetState, SubsetTable,
};
use write_fonts::{
    read::{
        tables::layout::{
            ChainedClassSequenceRule, ChainedClassSequenceRuleSet, ChainedSequenceContext,
            ChainedSequenceContextFormat1, ChainedSequenceContextFormat2,
            ChainedSequenceContextFormat3, ChainedSequenceRule, ChainedSequenceRuleSet,
            ClassSequenceRule, ClassSequenceRuleSet, CoverageTable, SequenceContext,
            SequenceContextFormat1, SequenceContextFormat2, SequenceContextFormat3,
            SequenceLookupRecord, SequenceRule, SequenceRuleSet,
        },
        ArrayOfOffsets, FontRef,
    },
    types::{BigEndian, FixedSize, GlyphId, GlyphId16, Offset16},
};

impl<'a> SubsetTable<'a> for SequenceContext<'_> {
    type ArgsForSubset = (&'a SubsetState, &'a FontRef<'a>, &'a FnvHashMap<u16, u16>);
    type Output = ();
    fn subset(
        &self,
        plan: &Plan,
        s: &mut Serializer,
        args: Self::ArgsForSubset,
    ) -> Result<Self::Output, SerializeErrorFlags> {
        let lookup_map = args.2;
        match self {
            Self::Format1(item) => item.subset(plan, s, lookup_map),
            Self::Format2(item) => item.subset(plan, s, lookup_map),
            Self::Format3(item) => item.subset(plan, s, lookup_map),
        }
    }
}

impl<'a> SubsetTable<'a> for SequenceContextFormat1<'_> {
    type ArgsForSubset = &'a FnvHashMap<u16, u16>;
    type Output = ();
    fn subset(
        &self,
        plan: &Plan,
        s: &mut Serializer,
        lookup_map: Self::ArgsForSubset,
    ) -> Result<Self::Output, SerializeErrorFlags> {
        if self.coverage_offset().is_null() {
            return Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY);
        }
        let coverage = self
            .coverage()
            .map_err(|_| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR))?;

        let (cov_glyphs, rule_sets_idxes) =
            intersected_glyphs_and_indices(&coverage, &plan.glyphset_gsub, &plan.glyph_map_gsub);
        if rule_sets_idxes.is_empty() {
            return Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY);
        }

        // format
        s.embed(self.format())?;

        // coverage offset
        let cov_offset_pos = s.embed(0_u16)?;
        // seq ruleset count
        let seq_ruleset_count_pos = s.embed(0_u16)?;
        let mut rule_set_count = 0_u16;

        let mut new_cov_glyphs = Vec::with_capacity(cov_glyphs.len());
        // seq rulesets offsets
        let rule_sets = self.seq_rule_sets();
        for (g, idx) in cov_glyphs.iter().zip(rule_sets_idxes.iter()) {
            match rule_sets.subset_offset(idx as usize, s, plan, lookup_map) {
                Ok(()) => {
                    new_cov_glyphs.push(*g);
                    rule_set_count += 1;
                }
                Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY) => (),
                Err(e) => return Err(e),
            }
        }

        if rule_set_count == 0 {
            return Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY);
        }

        s.copy_assign(seq_ruleset_count_pos, rule_set_count);
        Offset16::serialize_serialize::<CoverageTable>(s, &new_cov_glyphs, cov_offset_pos)
    }
}

impl<'a> SubsetTable<'a> for SequenceRuleSet<'_> {
    type ArgsForSubset = &'a FnvHashMap<u16, u16>;
    type Output = ();
    fn subset(
        &self,
        plan: &Plan,
        s: &mut Serializer,
        lookup_map: Self::ArgsForSubset,
    ) -> Result<Self::Output, SerializeErrorFlags> {
        // seq rule count
        let seq_rule_count_pos = s.embed(0_u16)?;
        let mut seq_rule_count = 0_u16;

        let seq_rules = self.seq_rules();
        for seq_rule in seq_rules.iter_as_nullable() {
            let Some(seq_rule) = seq_rule
                .transpose()
                .map_err(|_| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR))?
            else {
                continue;
            };
            let snap = s.snapshot();
            let offset_pos = s.allocate_size(Offset16::RAW_BYTE_LEN, true)?;
            match Offset16::serialize_subset(&seq_rule, s, plan, lookup_map, offset_pos) {
                Ok(()) => seq_rule_count += 1,
                Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY) => s.revert_snapshot(snap),
                Err(e) => return Err(e),
            }
        }

        if seq_rule_count == 0 {
            return Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY);
        }
        s.copy_assign(seq_rule_count_pos, seq_rule_count);
        Ok(())
    }
}

fn serialize_glyph_sequence(
    sequence: &[BigEndian<GlyphId16>],
    glyph_map: &FnvHashMap<GlyphId, GlyphId>,
    s: &mut Serializer,
) -> Result<(), SerializeErrorFlags> {
    for g in sequence {
        if let Some(new_g) = glyph_map.get(&GlyphId::from(g.get())) {
            s.embed(new_g.to_u32() as u16)?;
        } else {
            return Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY);
        }
    }
    Ok(())
}

fn serialize_class_sequence(
    sequence: &[BigEndian<u16>],
    class_map: &FnvHashMap<u16, u16>,
    s: &mut Serializer,
) -> Result<(), SerializeErrorFlags> {
    for c in sequence {
        let new_c = class_map
            .get(&c.get())
            .ok_or(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY)?;
        s.embed(*new_c)?;
    }
    Ok(())
}

fn serialize_lookup_records(
    lookup_records: &[SequenceLookupRecord],
    plan: &Plan,
    lookup_map: &FnvHashMap<u16, u16>,
    s: &mut Serializer,
) -> Result<u16, SerializeErrorFlags> {
    let mut seq_lookup_count = 0_u16;
    for lookup in lookup_records {
        match lookup.subset(plan, s, lookup_map) {
            Ok(()) => seq_lookup_count += 1,
            Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY) => (),
            Err(e) => return Err(e),
        }
    }
    Ok(seq_lookup_count)
}

impl<'a> SubsetTable<'a> for SequenceRule<'_> {
    type ArgsForSubset = &'a FnvHashMap<u16, u16>;
    type Output = ();
    fn subset(
        &self,
        plan: &Plan,
        s: &mut Serializer,
        lookup_map: Self::ArgsForSubset,
    ) -> Result<(), SerializeErrorFlags> {
        let glyph_count = self.glyph_count();
        if glyph_count == 0 {
            return Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY);
        }

        s.embed(glyph_count)?;
        let glyph_map = &plan.glyph_map_gsub;
        // seq lookup count
        let seq_lookup_count_pos = s.embed(0_u16)?;

        // input sequence
        serialize_glyph_sequence(self.input_sequence(), glyph_map, s)?;

        // seq lookup records
        let seq_lookup_count =
            serialize_lookup_records(self.seq_lookup_records(), plan, lookup_map, s)?;
        s.copy_assign(seq_lookup_count_pos, seq_lookup_count);
        Ok(())
    }
}

impl<'a> SubsetTable<'a> for SequenceLookupRecord {
    type ArgsForSubset = &'a FnvHashMap<u16, u16>;
    type Output = ();
    fn subset(
        &self,
        _plan: &Plan,
        s: &mut Serializer,
        lookup_map: Self::ArgsForSubset,
    ) -> Result<(), SerializeErrorFlags> {
        let lookup_index = self.lookup_list_index();
        let Some(new_index) = lookup_map.get(&lookup_index) else {
            return Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY);
        };

        s.embed(self.sequence_index())?;
        s.embed(*new_index).map(|_| ())
    }
}

impl<'a> SubsetTable<'a> for SequenceContextFormat2<'_> {
    type ArgsForSubset = &'a FnvHashMap<u16, u16>;
    type Output = ();
    fn subset(
        &self,
        plan: &Plan,
        s: &mut Serializer,
        lookup_map: Self::ArgsForSubset,
    ) -> Result<Self::Output, SerializeErrorFlags> {
        if self.coverage_offset().is_null() || self.class_def_offset().is_null() {
            return Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY);
        }
        let coverage = self
            .coverage()
            .map_err(|_| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR))?;

        let cov_glyphs = coverage.intersect_set(&plan.glyphset_gsub);
        if cov_glyphs.is_empty() {
            return Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY);
        }

        let class_def = self
            .class_def()
            .map_err(|_| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR))?;

        // format
        s.embed(self.format())?;

        // cov offset
        let cov_offset_pos = s.embed(0_u16)?;
        Offset16::serialize_subset(&coverage, s, plan, (), cov_offset_pos)?;

        // classdef offset
        let classdef_offset_pos = s.embed(0_u16)?;
        let cov_classes = class_def.intersect_classes(&cov_glyphs);
        if cov_classes.is_empty() {
            return Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY);
        }

        let Some(class_map) = Offset16::serialize_subset(
            &class_def,
            s,
            plan,
            &ClassDefSubsetStruct {
                remap_class: true,
                keep_empty_table: true,
                use_class_zero: true,
                glyph_filter: None,
            },
            classdef_offset_pos,
        )?
        else {
            return Err(SerializeErrorFlags::SERIALIZE_ERROR_OTHER);
        };

        // seq ruleset count
        let seq_ruleset_count_pos = s.embed(0_u16)?;
        let mut rule_set_count = 0_u16;

        // seq rulesets offsets
        let rule_sets = self.class_seq_rule_sets();
        let n = self
            .class_seq_rule_set_count()
            .min(cov_classes.last().unwrap());

        let mut snap = s.snapshot();
        for (i, c) in (0..=n).filter(|c| class_map.contains_key(c)).enumerate() {
            let offset_pos = s.allocate_size(Offset16::RAW_BYTE_LEN, true)?;
            if !cov_classes.contains(c) {
                continue;
            }
            match rule_sets.get(c as usize) {
                Some(Ok(rule_set)) => {
                    match Offset16::serialize_subset(
                        &rule_set,
                        s,
                        plan,
                        (&class_map, lookup_map),
                        offset_pos,
                    ) {
                        Ok(()) => {
                            rule_set_count = i as u16 + 1;
                            snap = s.snapshot();
                        }
                        Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY) => continue,
                        Err(e) => return Err(e),
                    }
                }
                None => continue,
                Some(Err(_)) => return Err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR),
            }
        }

        if rule_set_count == 0 {
            return Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY);
        }

        // prune trailing empty rulesets
        s.revert_snapshot(snap);
        s.copy_assign(seq_ruleset_count_pos, rule_set_count);
        Ok(())
    }
}

impl<'a> SubsetTable<'a> for ClassSequenceRuleSet<'_> {
    type ArgsForSubset = (&'a FnvHashMap<u16, u16>, &'a FnvHashMap<u16, u16>);
    type Output = ();
    fn subset(
        &self,
        plan: &Plan,
        s: &mut Serializer,
        args: Self::ArgsForSubset,
    ) -> Result<Self::Output, SerializeErrorFlags> {
        // seq rule count
        let seq_rule_count_pos = s.embed(0_u16)?;
        let mut seq_rule_count = 0_u16;

        let seq_rules = self.class_seq_rules();
        for seq_rule in seq_rules.iter_as_nullable() {
            let Some(seq_rule) = seq_rule
                .transpose()
                .map_err(|_| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR))?
            else {
                continue;
            };
            let snap = s.snapshot();
            let offset_pos = s.allocate_size(Offset16::RAW_BYTE_LEN, true)?;
            match Offset16::serialize_subset(&seq_rule, s, plan, args, offset_pos) {
                Ok(()) => seq_rule_count += 1,
                Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY) => s.revert_snapshot(snap),
                Err(e) => return Err(e),
            }
        }

        if seq_rule_count == 0 {
            return Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY);
        }
        s.copy_assign(seq_rule_count_pos, seq_rule_count);
        Ok(())
    }
}

impl<'a> SubsetTable<'a> for ClassSequenceRule<'_> {
    type ArgsForSubset = (&'a FnvHashMap<u16, u16>, &'a FnvHashMap<u16, u16>);
    type Output = ();
    fn subset(
        &self,
        plan: &Plan,
        s: &mut Serializer,
        args: Self::ArgsForSubset,
    ) -> Result<(), SerializeErrorFlags> {
        let glyph_count = self.glyph_count();
        if glyph_count == 0 {
            return Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY);
        }

        // glyph count
        s.embed(glyph_count)?;

        let (class_map, lookup_map) = args;
        // seq lookup count
        let seq_lookup_count_pos = s.embed(0_u16)?;

        // input sequence
        serialize_class_sequence(self.input_sequence(), class_map, s)?;

        // seq lookup records
        let seq_lookup_count =
            serialize_lookup_records(self.seq_lookup_records(), plan, lookup_map, s)?;
        s.copy_assign(seq_lookup_count_pos, seq_lookup_count);
        Ok(())
    }
}

impl<'a> SubsetTable<'a> for ArrayOfOffsets<'a, CoverageTable<'a>, Offset16> {
    type ArgsForSubset = ();
    type Output = ();
    fn subset(
        &self,
        plan: &Plan,
        s: &mut Serializer,
        _args: Self::ArgsForSubset,
    ) -> Result<Self::Output, SerializeErrorFlags> {
        let snap = s.snapshot();
        for cov in self.iter_as_nullable() {
            let Some(cov) = cov
                .transpose()
                .map_err(|_| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR))?
            else {
                s.revert_snapshot(snap);
                return Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY);
            };
            let offset_pos = s.allocate_size(Offset16::RAW_BYTE_LEN, true)?;
            Offset16::serialize_subset(&cov, s, plan, (), offset_pos)?;
        }
        Ok(())
    }
}

impl<'a> SubsetTable<'a> for SequenceContextFormat3<'_> {
    type ArgsForSubset = &'a FnvHashMap<u16, u16>;
    type Output = ();
    fn subset(
        &self,
        plan: &Plan,
        s: &mut Serializer,
        lookup_map: Self::ArgsForSubset,
    ) -> Result<Self::Output, SerializeErrorFlags> {
        // format
        s.embed(self.format())?;
        // glyph count
        s.embed(self.glyph_count())?;

        // seq lookup count
        let seq_lookup_count_pos = s.embed(0_u16)?;

        // coverage offsets
        self.coverages().subset(plan, s, ())?;

        // seq lookup records
        let seq_lookup_count =
            serialize_lookup_records(self.seq_lookup_records(), plan, lookup_map, s)?;
        s.copy_assign(seq_lookup_count_pos, seq_lookup_count);
        Ok(())
    }
}

impl<'a> SubsetTable<'a> for ChainedSequenceContext<'_> {
    type ArgsForSubset = (&'a SubsetState, &'a FontRef<'a>, &'a FnvHashMap<u16, u16>);
    type Output = ();
    fn subset(
        &self,
        plan: &Plan,
        s: &mut Serializer,
        args: Self::ArgsForSubset,
    ) -> Result<Self::Output, SerializeErrorFlags> {
        let lookup_map = args.2;
        match self {
            Self::Format1(item) => item.subset(plan, s, lookup_map),
            Self::Format2(item) => item.subset(plan, s, lookup_map),
            Self::Format3(item) => item.subset(plan, s, lookup_map),
        }
    }
}

impl<'a> SubsetTable<'a> for ChainedSequenceContextFormat1<'_> {
    type ArgsForSubset = &'a FnvHashMap<u16, u16>;
    type Output = ();
    fn subset(
        &self,
        plan: &Plan,
        s: &mut Serializer,
        lookup_map: Self::ArgsForSubset,
    ) -> Result<Self::Output, SerializeErrorFlags> {
        if self.coverage_offset().is_null() {
            return Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY);
        }
        let coverage = self
            .coverage()
            .map_err(|_| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR))?;

        let (cov_glyphs, rule_sets_idxes) =
            intersected_glyphs_and_indices(&coverage, &plan.glyphset_gsub, &plan.glyph_map_gsub);
        if rule_sets_idxes.is_empty() {
            return Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY);
        }

        // format
        s.embed(self.format())?;

        // coverage offset
        let cov_offset_pos = s.embed(0_u16)?;
        // chained seq ruleset count
        let seq_ruleset_count_pos = s.embed(0_u16)?;
        let mut rule_set_count = 0_u16;

        let mut new_cov_glyphs = Vec::with_capacity(cov_glyphs.len());
        // chained seq rulesets offsets
        let rule_sets = self.chained_seq_rule_sets();
        for (g, idx) in cov_glyphs.iter().zip(rule_sets_idxes.iter()) {
            match rule_sets.subset_offset(idx as usize, s, plan, lookup_map) {
                Ok(()) => {
                    new_cov_glyphs.push(*g);
                    rule_set_count += 1;
                }
                Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY) => (),
                Err(e) => return Err(e),
            }
        }

        if rule_set_count == 0 {
            return Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY);
        }

        s.copy_assign(seq_ruleset_count_pos, rule_set_count);
        Offset16::serialize_serialize::<CoverageTable>(s, &new_cov_glyphs, cov_offset_pos)
    }
}

impl<'a> SubsetTable<'a> for ChainedSequenceRuleSet<'_> {
    type ArgsForSubset = &'a FnvHashMap<u16, u16>;
    type Output = ();
    fn subset(
        &self,
        plan: &Plan,
        s: &mut Serializer,
        lookup_map: Self::ArgsForSubset,
    ) -> Result<Self::Output, SerializeErrorFlags> {
        // chained seq rule count
        let seq_rule_count_pos = s.embed(0_u16)?;
        let mut seq_rule_count = 0_u16;

        let seq_rules = self.chained_seq_rules();
        for seq_rule in seq_rules.iter_as_nullable() {
            let Some(seq_rule) = seq_rule
                .transpose()
                .map_err(|_| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR))?
            else {
                continue;
            };

            let snap = s.snapshot();
            let offset_pos = s.allocate_size(Offset16::RAW_BYTE_LEN, true)?;
            match Offset16::serialize_subset(&seq_rule, s, plan, lookup_map, offset_pos) {
                Ok(()) => seq_rule_count += 1,
                Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY) => s.revert_snapshot(snap),
                Err(e) => return Err(e),
            }
        }

        if seq_rule_count == 0 {
            return Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY);
        }
        s.copy_assign(seq_rule_count_pos, seq_rule_count);
        Ok(())
    }
}

impl<'a> SubsetTable<'a> for ChainedSequenceRule<'_> {
    type ArgsForSubset = &'a FnvHashMap<u16, u16>;
    type Output = ();
    fn subset(
        &self,
        plan: &Plan,
        s: &mut Serializer,
        lookup_map: Self::ArgsForSubset,
    ) -> Result<(), SerializeErrorFlags> {
        let glyph_map = &plan.glyph_map_gsub;
        // backtrack glyph count
        s.embed(self.backtrack_glyph_count())?;
        // backtrack sequence
        serialize_glyph_sequence(self.backtrack_sequence(), glyph_map, s)?;

        // input glyph count
        s.embed(self.input_glyph_count())?;
        // input sequence
        serialize_glyph_sequence(self.input_sequence(), glyph_map, s)?;

        // lookahead glyph count
        s.embed(self.lookahead_glyph_count())?;
        // lookahead sequence
        serialize_glyph_sequence(self.lookahead_sequence(), glyph_map, s)?;

        // seq lookup count
        let seq_lookup_count_pos = s.embed(0_u16)?;
        // seq lookup records
        let seq_lookup_count =
            serialize_lookup_records(self.seq_lookup_records(), plan, lookup_map, s)?;
        s.copy_assign(seq_lookup_count_pos, seq_lookup_count);
        Ok(())
    }
}

impl<'a> SubsetTable<'a> for ChainedSequenceContextFormat2<'_> {
    type ArgsForSubset = &'a FnvHashMap<u16, u16>;
    type Output = ();
    fn subset(
        &self,
        plan: &Plan,
        s: &mut Serializer,
        lookup_map: Self::ArgsForSubset,
    ) -> Result<Self::Output, SerializeErrorFlags> {
        if self.coverage_offset().is_null() || self.input_class_def_offset().is_null() {
            return Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY);
        }
        let coverage = self
            .coverage()
            .map_err(|_| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR))?;

        let cov_glyphs = coverage.intersect_set(&plan.glyphset_gsub);
        if cov_glyphs.is_empty() {
            return Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY);
        }

        let input_classdef = self
            .input_class_def()
            .map_err(|_| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR))?;

        // format
        s.embed(self.format())?;

        // cov offset
        let cov_offset_pos = s.embed(0_u16)?;
        Offset16::serialize_subset(&coverage, s, plan, (), cov_offset_pos)?;

        // backtrack classdef offset
        let backtrack_classdef_offset_pos = s.embed(0_u16)?;
        let backtrack_class_map = if !self.backtrack_class_def_offset().is_null() {
            let backtrack_classdef = self
                .backtrack_class_def()
                .map_err(|_| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR))?;
            let Some(backtrack_class_map) = Offset16::serialize_subset(
                &backtrack_classdef,
                s,
                plan,
                &ClassDefSubsetStruct {
                    remap_class: true,
                    keep_empty_table: true,
                    use_class_zero: true,
                    glyph_filter: None,
                },
                backtrack_classdef_offset_pos,
            )?
            else {
                return Err(SerializeErrorFlags::SERIALIZE_ERROR_OTHER);
            };
            backtrack_class_map
        } else {
            FnvHashMap::default()
        };

        // input classdef offset
        let input_classdef_offset_pos = s.embed(0_u16)?;
        let cov_classes = input_classdef.intersect_classes(&cov_glyphs);
        if cov_classes.is_empty() {
            return Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY);
        }

        let Some(input_class_map) = Offset16::serialize_subset(
            &input_classdef,
            s,
            plan,
            &ClassDefSubsetStruct {
                remap_class: true,
                keep_empty_table: true,
                use_class_zero: true,
                glyph_filter: None,
            },
            input_classdef_offset_pos,
        )?
        else {
            return Err(SerializeErrorFlags::SERIALIZE_ERROR_OTHER);
        };

        // lookahead classdef offset
        let lookahead_classdef_offset_pos = s.embed(0_u16)?;
        let lookahead_class_map = if !self.lookahead_class_def_offset().is_null() {
            let lookahead_classdef = self
                .lookahead_class_def()
                .map_err(|_| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR))?;

            let Some(lookahead_class_map) = Offset16::serialize_subset(
                &lookahead_classdef,
                s,
                plan,
                &ClassDefSubsetStruct {
                    remap_class: true,
                    keep_empty_table: true,
                    use_class_zero: true,
                    glyph_filter: None,
                },
                lookahead_classdef_offset_pos,
            )?
            else {
                return Err(SerializeErrorFlags::SERIALIZE_ERROR_OTHER);
            };
            lookahead_class_map
        } else {
            FnvHashMap::default()
        };

        // seq ruleset count
        let seq_ruleset_count_pos = s.embed(0_u16)?;
        let mut rule_set_count = 0_u16;

        // seq rulesets offsets
        let rule_sets = self.chained_class_seq_rule_sets();
        let n = self
            .chained_class_seq_rule_set_count()
            .min(cov_classes.last().unwrap());

        let mut snap = s.snapshot();
        let subset_struct = ChainedContextSubsetStruct {
            lookup_map,
            backtrack_class_map: &backtrack_class_map,
            input_class_map: &input_class_map,
            lookahead_class_map: &lookahead_class_map,
        };
        for (i, c) in (0..=n)
            .filter(|c| input_class_map.contains_key(c))
            .enumerate()
        {
            let offset_pos = s.allocate_size(Offset16::RAW_BYTE_LEN, true)?;
            if !cov_classes.contains(c) {
                continue;
            }
            match rule_sets.get(c as usize) {
                Some(Ok(rule_set)) => {
                    match Offset16::serialize_subset(&rule_set, s, plan, &subset_struct, offset_pos)
                    {
                        Ok(()) => {
                            rule_set_count = i as u16 + 1;
                            snap = s.snapshot();
                        }
                        Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY) => continue,
                        Err(e) => return Err(e),
                    }
                }
                None => continue,
                Some(Err(_)) => return Err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR),
            }
        }

        if rule_set_count == 0 {
            return Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY);
        }

        // prune trailing empty rulesets
        s.revert_snapshot(snap);
        s.copy_assign(seq_ruleset_count_pos, rule_set_count);
        Ok(())
    }
}

pub(crate) struct ChainedContextSubsetStruct<'a> {
    lookup_map: &'a FnvHashMap<u16, u16>,
    backtrack_class_map: &'a FnvHashMap<u16, u16>,
    input_class_map: &'a FnvHashMap<u16, u16>,
    lookahead_class_map: &'a FnvHashMap<u16, u16>,
}
impl<'a> SubsetTable<'a> for ChainedClassSequenceRuleSet<'_> {
    type ArgsForSubset = &'a ChainedContextSubsetStruct<'a>;
    type Output = ();
    fn subset(
        &self,
        plan: &Plan,
        s: &mut Serializer,
        args: Self::ArgsForSubset,
    ) -> Result<Self::Output, SerializeErrorFlags> {
        // chained seq rule count
        let seq_rule_count_pos = s.embed(0_u16)?;
        let mut seq_rule_count = 0_u16;

        let seq_rules = self.chained_class_seq_rules();
        for seq_rule in seq_rules.iter_as_nullable() {
            let Some(seq_rule) = seq_rule
                .transpose()
                .map_err(|_| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR))?
            else {
                continue;
            };
            let snap = s.snapshot();
            let offset_pos = s.allocate_size(Offset16::RAW_BYTE_LEN, true)?;
            match Offset16::serialize_subset(&seq_rule, s, plan, args, offset_pos) {
                Ok(()) => seq_rule_count += 1,
                Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY) => s.revert_snapshot(snap),
                Err(e) => return Err(e),
            }
        }

        if seq_rule_count == 0 {
            return Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY);
        }
        s.copy_assign(seq_rule_count_pos, seq_rule_count);
        Ok(())
    }
}

impl<'a> SubsetTable<'a> for ChainedClassSequenceRule<'_> {
    type ArgsForSubset = &'a ChainedContextSubsetStruct<'a>;
    type Output = ();
    fn subset(
        &self,
        plan: &Plan,
        s: &mut Serializer,
        args: Self::ArgsForSubset,
    ) -> Result<(), SerializeErrorFlags> {
        s.embed(self.backtrack_glyph_count())?;
        serialize_class_sequence(self.backtrack_sequence(), args.backtrack_class_map, s)?;

        s.embed(self.input_glyph_count())?;
        serialize_class_sequence(self.input_sequence(), args.input_class_map, s)?;

        s.embed(self.lookahead_glyph_count())?;
        serialize_class_sequence(self.lookahead_sequence(), args.lookahead_class_map, s)?;

        // seq lookup count
        let seq_lookup_count_pos = s.embed(0_u16)?;
        // seq lookup records
        let seq_lookup_count =
            serialize_lookup_records(self.seq_lookup_records(), plan, args.lookup_map, s)?;
        s.copy_assign(seq_lookup_count_pos, seq_lookup_count);
        Ok(())
    }
}

impl<'a> SubsetTable<'a> for ChainedSequenceContextFormat3<'_> {
    type ArgsForSubset = &'a FnvHashMap<u16, u16>;
    type Output = ();
    fn subset(
        &self,
        plan: &Plan,
        s: &mut Serializer,
        lookup_map: Self::ArgsForSubset,
    ) -> Result<Self::Output, SerializeErrorFlags> {
        // format
        s.embed(self.format())?;

        // backtrack glyph count
        s.embed(self.backtrack_glyph_count())?;
        // backtrack coverage offsets
        self.backtrack_coverages().subset(plan, s, ())?;

        // input glyph count
        s.embed(self.input_glyph_count())?;
        // input coverage offsets
        self.input_coverages().subset(plan, s, ())?;

        // lookahead glyph count
        s.embed(self.lookahead_glyph_count())?;
        // lookahead coverage offsets
        self.lookahead_coverages().subset(plan, s, ())?;

        // seq lookup count
        let seq_lookup_count_pos = s.embed(0_u16)?;

        // seq lookup records
        let seq_lookup_count =
            serialize_lookup_records(self.seq_lookup_records(), plan, lookup_map, s)?;
        s.copy_assign(seq_lookup_count_pos, seq_lookup_count);
        Ok(())
    }
}

#[cfg(test)]
mod test {
    use super::*;
    use write_fonts::read::{types::GlyphId, FontRef, TableProvider};

    #[test]
    fn test_subset_context_format1() {
        use write_fonts::read::tables::gsub::SubstitutionSubtables;

        let font = FontRef::new(include_bytes!(
            "../test-data/fonts/NotoNastaliqUrdu-Regular.ttf"
        ))
        .unwrap();
        let gsub_lookups = font.gsub().unwrap().lookup_list().unwrap();
        let lookup = gsub_lookups.lookups().get(105).unwrap();

        let SubstitutionSubtables::Contextual(sub_tables) = lookup.subtables().unwrap() else {
            panic!("Wrong type of lookup table!");
        };
        let contextsubst_table = sub_tables.get(0).unwrap();
        let mut plan = Plan::default();

        plan.glyph_map_gsub
            .insert(GlyphId::from(229_u32), GlyphId::from(1_u32));
        plan.glyph_map_gsub
            .insert(GlyphId::from(235_u32), GlyphId::from(2_u32));
        plan.glyph_map_gsub
            .insert(GlyphId::from(559_u32), GlyphId::from(3_u32));

        plan.glyphset_gsub.insert(GlyphId::from(229_u32));
        plan.glyphset_gsub.insert(GlyphId::from(235_u32));
        plan.glyphset_gsub.insert(GlyphId::from(559_u32));

        let mut lookup_map = FnvHashMap::default();
        lookup_map.insert(58_u16, 0_u16);

        let mut s = Serializer::new(1024);
        assert_eq!(s.start_serialize(), Ok(()));

        let subset_state = SubsetState::default();
        contextsubst_table
            .subset(&plan, &mut s, (&subset_state, &font, &lookup_map))
            .unwrap();
        assert!(!s.in_error());
        s.end_serialize();

        let subsetted_data = s.copy_bytes();
        let expected_data: [u8; 40] = [
            0x00, 0x01, 0x00, 0x08, 0x00, 0x01, 0x00, 0x0e, 0x00, 0x01, 0x00, 0x01, 0x00, 0x03,
            0x00, 0x02, 0x00, 0x10, 0x00, 0x06, 0x00, 0x02, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x02, 0x00, 0x01, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00,
        ];

        assert_eq!(subsetted_data, expected_data);
    }

    #[test]
    fn test_subset_chained_context_format1() {
        use write_fonts::read::tables::gpos::PositionSubtables;

        let font = FontRef::new(include_bytes!(
            "../test-data/fonts/gpos_chaining1_multiple_subrules_f1.otf"
        ))
        .unwrap();
        let gpos_lookups = font.gpos().unwrap().lookup_list().unwrap();
        let lookup = gpos_lookups.lookups().get(4).unwrap();

        let PositionSubtables::ChainContextual(sub_tables) = lookup.subtables().unwrap() else {
            panic!("Wrong type of lookup table!");
        };
        let chainedcontextpos_table = sub_tables.get(0).unwrap();
        let mut plan = Plan::default();

        plan.glyph_map_gsub
            .insert(GlyphId::from(48_u32), GlyphId::from(1_u32));
        plan.glyph_map_gsub
            .insert(GlyphId::from(49_u32), GlyphId::from(2_u32));
        plan.glyph_map_gsub
            .insert(GlyphId::from(50_u32), GlyphId::from(3_u32));
        plan.glyph_map_gsub
            .insert(GlyphId::from(51_u32), GlyphId::from(4_u32));

        plan.glyphset_gsub.insert(GlyphId::from(48_u32));
        plan.glyphset_gsub.insert(GlyphId::from(49_u32));
        plan.glyphset_gsub.insert(GlyphId::from(50_u32));
        plan.glyphset_gsub.insert(GlyphId::from(51_u32));

        let mut lookup_map = FnvHashMap::default();
        lookup_map.insert(1_u16, 0_u16);

        let mut s = Serializer::new(1024);
        assert_eq!(s.start_serialize(), Ok(()));

        let subset_state = SubsetState::default();
        chainedcontextpos_table
            .subset(&plan, &mut s, (&subset_state, &font, &lookup_map))
            .unwrap();
        assert!(!s.in_error());
        s.end_serialize();

        let subsetted_data = s.copy_bytes();
        let expected_data: [u8; 36] = [
            0x00, 0x01, 0x00, 0x08, 0x00, 0x01, 0x00, 0x0e, 0x00, 0x01, 0x00, 0x01, 0x00, 0x02,
            0x00, 0x01, 0x00, 0x04, 0x00, 0x01, 0x00, 0x01, 0x00, 0x02, 0x00, 0x03, 0x00, 0x01,
            0x00, 0x04, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00,
        ];

        assert_eq!(subsetted_data, expected_data);
    }

    #[test]
    fn test_subset_context_format2() {
        use write_fonts::read::tables::gpos::PositionSubtables;

        let font = FontRef::new(include_bytes!(
            "../test-data/fonts/NotoNastaliqUrdu-Regular.ttf"
        ))
        .unwrap();
        let gpos_lookups = font.gpos().unwrap().lookup_list().unwrap();
        let lookup = gpos_lookups.lookups().get(2).unwrap();

        let PositionSubtables::Contextual(sub_tables) = lookup.subtables().unwrap() else {
            panic!("Wrong type of lookup table!");
        };
        let contextpos_table = sub_tables.get(0).unwrap();
        let mut plan = Plan {
            font_num_glyphs: 1398,
            ..Default::default()
        };

        plan.glyph_map_gsub
            .insert(GlyphId::from(0_u32), GlyphId::from(0_u32));
        plan.glyph_map_gsub
            .insert(GlyphId::from(559_u32), GlyphId::from(1_u32));
        plan.glyph_map_gsub
            .insert(GlyphId::from(965_u32), GlyphId::from(2_u32));
        plan.glyph_map_gsub
            .insert(GlyphId::from(966_u32), GlyphId::from(3_u32));
        plan.glyph_map_gsub
            .insert(GlyphId::from(970_u32), GlyphId::from(4_u32));
        plan.glyph_map_gsub
            .insert(GlyphId::from(972_u32), GlyphId::from(5_u32));

        plan.glyphset_gsub.insert(GlyphId::from(0_u32));
        plan.glyphset_gsub.insert(GlyphId::from(559_u32));
        plan.glyphset_gsub.insert(GlyphId::from(965_u32));
        plan.glyphset_gsub.insert(GlyphId::from(966_u32));
        plan.glyphset_gsub.insert(GlyphId::from(970_u32));
        plan.glyphset_gsub.insert(GlyphId::from(972_u32));

        let mut lookup_map = FnvHashMap::default();
        lookup_map.insert(6_u16, 1_u16);
        lookup_map.insert(8_u16, 2_u16);

        let mut s = Serializer::new(1024);
        assert_eq!(s.start_serialize(), Ok(()));

        let subset_state = SubsetState::default();
        contextpos_table
            .subset(&plan, &mut s, (&subset_state, &font, &lookup_map))
            .unwrap();
        assert!(!s.in_error());
        s.end_serialize();

        let subsetted_data = s.copy_bytes();
        let expected_data: [u8; 60] = [
            0x00, 0x02, 0x00, 0x36, 0x00, 0x26, 0x00, 0x02, 0x00, 0x00, 0x00, 0x0c, 0x00, 0x02,
            0x00, 0x10, 0x00, 0x06, 0x00, 0x02, 0x00, 0x01, 0x00, 0x02, 0x00, 0x00, 0x00, 0x02,
            0x00, 0x02, 0x00, 0x01, 0x00, 0x03, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01,
            0x00, 0x05, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x02, 0x00, 0x01,
            0x00, 0x01, 0x00, 0x01,
        ];

        assert_eq!(subsetted_data, expected_data);
    }

    #[test]
    fn test_subset_chain_context_format2() {
        use write_fonts::read::tables::gsub::SubstitutionSubtables;

        let font = FontRef::new(include_bytes!(
            "../test-data/fonts/NotoNastaliqUrdu-Regular.ttf"
        ))
        .unwrap();
        let gsub_lookups = font.gsub().unwrap().lookup_list().unwrap();
        let lookup = gsub_lookups.lookups().get(7).unwrap();

        let SubstitutionSubtables::ChainContextual(sub_tables) = lookup.subtables().unwrap() else {
            panic!("Wrong type of lookup table!");
        };
        let contextsubst_table = sub_tables.get(0).unwrap();
        let mut plan = Plan {
            font_num_glyphs: 1398,
            ..Default::default()
        };

        plan.glyph_map_gsub
            .insert(GlyphId::from(0_u32), GlyphId::from(0_u32));
        plan.glyph_map_gsub
            .insert(GlyphId::from(163_u32), GlyphId::from(1_u32));
        plan.glyph_map_gsub
            .insert(GlyphId::from(934_u32), GlyphId::from(2_u32));
        plan.glyph_map_gsub
            .insert(GlyphId::from(986_u32), GlyphId::from(3_u32));
        plan.glyph_map_gsub
            .insert(GlyphId::from(1000_u32), GlyphId::from(4_u32));
        plan.glyph_map_gsub
            .insert(GlyphId::from(1001_u32), GlyphId::from(5_u32));
        plan.glyph_map_gsub
            .insert(GlyphId::from(1025_u32), GlyphId::from(6_u32));
        plan.glyph_map_gsub
            .insert(GlyphId::from(1119_u32), GlyphId::from(7_u32));

        plan.glyphset_gsub.insert(GlyphId::from(0_u32));
        plan.glyphset_gsub.insert(GlyphId::from(163_u32));
        plan.glyphset_gsub.insert(GlyphId::from(934_u32));
        plan.glyphset_gsub.insert(GlyphId::from(986_u32));
        plan.glyphset_gsub.insert(GlyphId::from(1000_u32));
        plan.glyphset_gsub.insert(GlyphId::from(1001_u32));
        plan.glyphset_gsub.insert(GlyphId::from(1025_u32));
        plan.glyphset_gsub.insert(GlyphId::from(1119_u32));

        let mut lookup_map = FnvHashMap::default();
        lookup_map.insert(4_u16, 0_u16);
        lookup_map.insert(6_u16, 1_u16);

        let mut s = Serializer::new(1024);
        assert_eq!(s.start_serialize(), Ok(()));

        let subset_state = SubsetState::default();
        contextsubst_table
            .subset(&plan, &mut s, (&subset_state, &font, &lookup_map))
            .unwrap();
        assert!(!s.in_error());
        s.end_serialize();

        let subsetted_data = s.copy_bytes();
        let expected_data: [u8; 108] = [
            0x00, 0x02, 0x00, 0x64, 0x00, 0x5a, 0x00, 0x50, 0x00, 0x4c, 0x00, 0x03, 0x00, 0x00,
            0x00, 0x3a, 0x00, 0x12, 0x00, 0x02, 0x00, 0x14, 0x00, 0x06, 0x00, 0x00, 0x00, 0x02,
            0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03,
            0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02,
            0x00, 0x01, 0x00, 0x01, 0x00, 0x04, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00,
            0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01,
            0x00, 0x02, 0x00, 0x01, 0x00, 0x02, 0x00, 0x01, 0x00, 0x06, 0x00, 0x02, 0x00, 0x01,
            0x00, 0x01, 0x00, 0x01, 0x00, 0x02, 0x00, 0x01, 0x00, 0x02,
        ];

        assert_eq!(subsetted_data, expected_data);
    }

    #[test]
    fn test_subset_context_format3() {
        use write_fonts::read::tables::gsub::SubstitutionSubtables;

        let font = FontRef::new(include_bytes!(
            "../test-data/fonts/NotoNastaliqUrdu-Regular.ttf"
        ))
        .unwrap();
        let gsub_lookups = font.gsub().unwrap().lookup_list().unwrap();
        let lookup = gsub_lookups.lookups().get(154).unwrap();

        let SubstitutionSubtables::Contextual(sub_tables) = lookup.subtables().unwrap() else {
            panic!("Wrong type of lookup table!");
        };
        let contextsubst_table = sub_tables.get(1).unwrap();
        let mut plan = Plan {
            font_num_glyphs: 1398,
            ..Default::default()
        };

        plan.glyph_map_gsub
            .insert(GlyphId::from(0_u32), GlyphId::from(0_u32));
        plan.glyph_map_gsub
            .insert(GlyphId::from(277_u32), GlyphId::from(2_u32));
        plan.glyph_map_gsub
            .insert(GlyphId::from(966_u32), GlyphId::from(3_u32));
        plan.glyph_map_gsub
            .insert(GlyphId::from(1383_u32), GlyphId::from(4_u32));

        plan.glyphset_gsub.insert(GlyphId::from(0_u32));
        plan.glyphset_gsub.insert(GlyphId::from(277_u32));
        plan.glyphset_gsub.insert(GlyphId::from(966_u32));
        plan.glyphset_gsub.insert(GlyphId::from(1383_u32));

        let mut lookup_map = FnvHashMap::default();
        lookup_map.insert(152_u16, 0_u16);

        let mut s = Serializer::new(1024);
        assert_eq!(s.start_serialize(), Ok(()));

        let subset_state = SubsetState::default();
        contextsubst_table
            .subset(&plan, &mut s, (&subset_state, &font, &lookup_map))
            .unwrap();
        assert!(!s.in_error());
        s.end_serialize();

        let subsetted_data = s.copy_bytes();
        let expected_data: [u8; 30] = [
            0x00, 0x03, 0x00, 0x02, 0x00, 0x01, 0x00, 0x18, 0x00, 0x0e, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x01, 0x00, 0x03, 0x00, 0x00, 0x00, 0x03, 0x00, 0x04, 0x00, 0x01, 0x00, 0x01,
            0x00, 0x02,
        ];

        assert_eq!(subsetted_data, expected_data);
    }

    #[test]
    fn test_subset_chain_context_format3() {
        use write_fonts::read::tables::gpos::PositionSubtables;

        let font = FontRef::new(include_bytes!(
            "../test-data/fonts/NotoNastaliqUrdu-Regular.ttf"
        ))
        .unwrap();
        let gpos_lookups = font.gpos().unwrap().lookup_list().unwrap();
        let lookup = gpos_lookups.lookups().get(0).unwrap();

        let PositionSubtables::ChainContextual(sub_tables) = lookup.subtables().unwrap() else {
            panic!("Wrong type of lookup table!");
        };
        let contextsubst_table = sub_tables.get(1).unwrap();
        let mut plan = Plan {
            font_num_glyphs: 1398,
            ..Default::default()
        };

        plan.glyph_map_gsub
            .insert(GlyphId::from(0_u32), GlyphId::from(0_u32));
        plan.glyph_map_gsub
            .insert(GlyphId::from(31_u32), GlyphId::from(1_u32));
        plan.glyph_map_gsub
            .insert(GlyphId::from(38_u32), GlyphId::from(2_u32));
        plan.glyph_map_gsub
            .insert(GlyphId::from(218_u32), GlyphId::from(3_u32));
        plan.glyph_map_gsub
            .insert(GlyphId::from(275_u32), GlyphId::from(4_u32));

        plan.glyphset_gsub.insert(GlyphId::from(0_u32));
        plan.glyphset_gsub.insert(GlyphId::from(31_u32));
        plan.glyphset_gsub.insert(GlyphId::from(38_u32));
        plan.glyphset_gsub.insert(GlyphId::from(218_u32));
        plan.glyphset_gsub.insert(GlyphId::from(275_u32));

        let mut lookup_map = FnvHashMap::default();
        lookup_map.insert(1_u16, 1_u16);

        let mut s = Serializer::new(1024);
        assert_eq!(s.start_serialize(), Ok(()));

        let subset_state = SubsetState::default();
        contextsubst_table
            .subset(&plan, &mut s, (&subset_state, &font, &lookup_map))
            .unwrap();
        assert!(!s.in_error());
        s.end_serialize();

        let subsetted_data = s.copy_bytes();
        let expected_data: [u8; 50] = [
            0x00, 0x03, 0x00, 0x03, 0x00, 0x2c, 0x00, 0x26, 0x00, 0x1e, 0x00, 0x01, 0x00, 0x18,
            0x00, 0x01, 0x00, 0x26, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01,
            0x00, 0x03, 0x00, 0x01, 0x00, 0x02, 0x00, 0x03, 0x00, 0x04, 0x00, 0x01, 0x00, 0x01,
            0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x02,
        ];

        assert_eq!(subsetted_data, expected_data);
    }
}

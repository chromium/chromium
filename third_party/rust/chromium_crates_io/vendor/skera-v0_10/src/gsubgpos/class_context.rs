//! Extended class-based contexts, shared by GSUB and GPOS.

#[cfg(test)]
mod tests;

use super::*;
use read_fonts::{
    collections::IntSet,
    tables::layout::{
        ChainedClassSequenceRuleSet2, ChainedSequenceContextFormat5, ClassDef,
        ClassSequenceRuleSet2, SequenceContextFormat5,
    },
    types::{Offset24, Offset32, Uint24},
    ArrayOfNullableOffsets, FontRead, MinByteRange,
};

fn subset_class_def(
    class_def: &ClassDef,
    plan: &Plan,
    s: &mut Serializer,
    offset_pos: usize,
) -> Result<ClassMap, SerializeErrorFlags> {
    Offset32::serialize_subset(
        class_def,
        s,
        plan,
        &ClassDefSubsetStruct {
            remap_class: true,
            keep_empty_table: true,
            use_class_zero: true,
            glyph_filter: None,
        },
        offset_pos,
    )?
    .ok_or_else(|| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_OTHER))
}

fn subset_class_sets<'font, 'args, T>(
    sets: &ArrayOfNullableOffsets<'font, T, Offset24>,
    plan: &Plan,
    s: &mut Serializer,
    input_class_map: &ClassMap,
    coverage_classes: &IntSet<u32>,
    args: T::ArgsForSubset,
) -> Result<u32, SerializeErrorFlags>
where
    T: FontRead<'font> + SubsetTable<'args, Output = ()>,
    T::Args: Copy + 'static,
    T::ArgsForSubset: Copy,
{
    let max_class = coverage_classes.last().unwrap_or_default();
    let mut classes: Vec<_> = input_class_map
        .iter()
        .filter(|&(old, _)| *old <= max_class && (*old as usize) < sets.len())
        .map(|(&old, &new)| (old, new))
        .collect();
    classes.sort_unstable_by_key(|&(_, new)| new);
    let mut count = 0;
    let mut emitted = 0;
    let mut snap = s.snapshot();
    for (old, new) in classes {
        let mut offset_pos = 0;
        while emitted <= new {
            offset_pos = s.allocate_size(Offset24::RAW_BYTE_LEN, true)?;
            emitted += 1;
        }
        if !coverage_classes.contains(old) {
            continue;
        }
        let Some(set) = sets
            .get(old as usize)
            .transpose()
            .map_err(|_| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR))?
        else {
            continue;
        };
        if !Offset24::serialize_subset(&set, s, plan, args, offset_pos).is_empty()? {
            count = new + 1;
            snap = s.snapshot();
        }
    }
    if count == 0 {
        return Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY);
    }
    s.revert_snapshot(snap);
    Ok(count)
}

impl<'a> SubsetTable<'a> for SequenceContextFormat5<'_> {
    type ArgsForSubset = &'a FastHashMap<u16, u16>;
    type Output = ();
    fn subset(
        &self,
        plan: &Plan,
        s: &mut Serializer,
        lookup_map: Self::ArgsForSubset,
    ) -> Result<(), SerializeErrorFlags> {
        if self.coverage_offset().is_null() || self.class_def_offset().is_null() {
            return Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY);
        }
        if self.min_table_bytes().is_empty() {
            return Err(s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR));
        }
        let coverage = self
            .coverage()
            .map_err(|_| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR))?;
        let glyphs = coverage.intersect_set(&plan.glyphset_gsub);
        if glyphs.is_empty() {
            return Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY);
        }
        let class_def = self
            .class_def()
            .map_err(|_| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR))?;
        let classes = class_def.intersect_classes(&glyphs);
        s.embed(self.format())?;
        let coverage_pos = s.embed(0u32)?;
        let class_def_pos = s.embed(0u32)?;
        let count_pos = s.embed(Uint24::new(0))?;
        Offset32::serialize_subset(&coverage, s, plan, (), coverage_pos)?;
        let class_map = subset_class_def(&class_def, plan, s, class_def_pos)?;
        let count = subset_class_sets(
            &self.class_seq_rule_sets(),
            plan,
            s,
            &class_map,
            &classes,
            (&class_map, lookup_map),
        )?;
        let count = Uint24::checked_new(count)
            .ok_or_else(|| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_INT_OVERFLOW))?;
        s.copy_assign(count_pos, count);
        Ok(())
    }
}

impl<'a> SubsetTable<'a> for ChainedSequenceContextFormat5<'_> {
    type ArgsForSubset = &'a FastHashMap<u16, u16>;
    type Output = ();
    fn subset(
        &self,
        plan: &Plan,
        s: &mut Serializer,
        lookup_map: Self::ArgsForSubset,
    ) -> Result<(), SerializeErrorFlags> {
        if self.coverage_offset().is_null() || self.input_class_def_offset().is_null() {
            return Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY);
        }
        if self.min_table_bytes().is_empty() {
            return Err(s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR));
        }
        let coverage = self
            .coverage()
            .map_err(|_| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR))?;
        let glyphs = coverage.intersect_set(&plan.glyphset_gsub);
        if glyphs.is_empty() {
            return Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY);
        }
        let input_class_def = self
            .input_class_def()
            .map_err(|_| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR))?;
        let classes = input_class_def.intersect_classes(&glyphs);
        s.embed(self.format())?;
        let coverage_pos = s.embed(0u32)?;
        let backtrack_pos = s.embed(0u32)?;
        let input_pos = s.embed(0u32)?;
        let lookahead_pos = s.embed(0u32)?;
        // The ISO chained format retains a 16-bit rule-set count.
        let count_pos = s.embed(0u16)?;
        Offset32::serialize_subset(&coverage, s, plan, (), coverage_pos)?;
        let input_class_map = subset_class_def(&input_class_def, plan, s, input_pos)?;
        let backtrack_class_map = if self.backtrack_class_def_offset().is_null() {
            ClassMap::default()
        } else {
            let class_def = self
                .backtrack_class_def()
                .map_err(|_| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR))?;
            subset_class_def(&class_def, plan, s, backtrack_pos)?
        };
        let lookahead_class_map = if self.lookahead_class_def_offset().is_null() {
            ClassMap::default()
        } else {
            let class_def = self
                .lookahead_class_def()
                .map_err(|_| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR))?;
            subset_class_def(&class_def, plan, s, lookahead_pos)?
        };
        let args = ChainedContextSubsetStruct {
            lookup_map,
            backtrack_class_map: &backtrack_class_map,
            input_class_map: &input_class_map,
            lookahead_class_map: &lookahead_class_map,
        };
        let count = subset_class_sets(
            &self.chained_class_seq_rule_sets(),
            plan,
            s,
            &input_class_map,
            &classes,
            &args,
        )?;
        let count = u16::try_from(count)
            .map_err(|_| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_INT_OVERFLOW))?;
        s.copy_assign(count_pos, count);
        Ok(())
    }
}

macro_rules! subset_class_rule_set {
    ($table:ident, $rules:ident, $input_count:ident, $args:ty) => {
        impl<'a> SubsetTable<'a> for $table<'_> {
            type ArgsForSubset = $args;
            type Output = ();
            fn subset(
                &self,
                plan: &Plan,
                s: &mut Serializer,
                args: Self::ArgsForSubset,
            ) -> Result<(), SerializeErrorFlags> {
                if self.min_table_bytes().is_empty() {
                    return Err(s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR));
                }
                let count_pos = s.embed(0u16)?;
                let mut count = 0usize;
                for rule in self.$rules().iter_as_nullable() {
                    let Some(rule) = rule
                        .transpose()
                        .map_err(|_| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR))?
                    else {
                        continue;
                    };
                    if rule.min_table_bytes().is_empty() {
                        return Err(s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR));
                    }
                    if rule.$input_count() == 0 {
                        continue;
                    }
                    let snap = s.snapshot();
                    let offset_pos = s.allocate_size(Offset24::RAW_BYTE_LEN, true)?;
                    if Offset24::serialize_subset(&rule, s, plan, args, offset_pos).is_empty()? {
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
// The widened sets still reference the existing 16-bit class-rule records.
subset_class_rule_set!(
    ClassSequenceRuleSet2,
    class_seq_rules,
    glyph_count,
    (&'a ClassMap, &'a FastHashMap<u16, u16>)
);
subset_class_rule_set!(
    ChainedClassSequenceRuleSet2,
    chained_class_seq_rules,
    input_glyph_count,
    &'a ChainedContextSubsetStruct<'a>
);

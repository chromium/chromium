//! Computing the closure over a set of glyphs
//!
//! This means taking a set of glyphs and updating it to include any other glyphs
//! reachable from those glyphs via substitution, recursively.

use std::collections::HashSet;

use font_types::GlyphId16;

use crate::{
    tables::layout::{
        ChainedSequenceContextFormat1, ChainedSequenceContextFormat2,
        ChainedSequenceContextFormat3, ExtensionLookup, SequenceContextFormat1,
        SequenceContextFormat2, SequenceContextFormat3, Subtables,
    },
    FontRead, ReadError,
};

use super::{
    AlternateSubstFormat1, ChainedSequenceContext, ClassDef, Gsub, LigatureSubstFormat1,
    MultipleSubstFormat1, ReverseChainSingleSubstFormat1, SequenceContext, SingleSubst,
    SingleSubstFormat1, SingleSubstFormat2, SubstitutionSubtables,
};

/// A trait for tables which participate in closure
pub(crate) trait GlyphClosure {
    /// Update the set of glyphs with any glyphs reachable via substitution.
    fn add_reachable_glyphs(&self, glyphs: &mut HashSet<GlyphId16>) -> Result<(), ReadError>;
}

impl<'a> Gsub<'a> {
    /// Return the set of glyphs reachable from the input set via any substitution.
    pub fn closure_glyphs(
        &self,
        mut glyphs: HashSet<GlyphId16>,
    ) -> Result<HashSet<GlyphId16>, ReadError> {
        // we need to do this iteratively, since any glyph found in one pass
        // over the lookups could also be the target of substitutions.

        // we always call this once, and then keep calling if it produces
        // additional glyphs
        let mut prev_glyph_count = glyphs.len();
        self.closure_glyphs_once(&mut glyphs)?;
        let mut new_glyph_count = glyphs.len();

        while prev_glyph_count != new_glyph_count {
            prev_glyph_count = new_glyph_count;
            self.closure_glyphs_once(&mut glyphs)?;
            new_glyph_count = glyphs.len();
        }

        Ok(glyphs)
    }

    fn closure_glyphs_once(&self, glyphs: &mut HashSet<GlyphId16>) -> Result<(), ReadError> {
        let lookups_to_use = self.find_reachable_lookups(glyphs)?;
        let lookup_list = self.lookup_list()?;
        for (i, lookup) in lookup_list.lookups().iter().enumerate() {
            if !lookups_to_use.contains(&(i as u16)) {
                continue;
            }
            let subtables = lookup?.subtables()?;
            subtables.add_reachable_glyphs(glyphs)?;
        }
        Ok(())
    }

    fn find_reachable_lookups(
        &self,
        glyphs: &HashSet<GlyphId16>,
    ) -> Result<HashSet<u16>, ReadError> {
        let feature_list = self.feature_list()?;
        let lookup_list = self.lookup_list()?;
        // first we want to get the lookups that are directly referenced by a feature
        // (including in a feature variation table)
        let mut lookup_ids = HashSet::with_capacity(lookup_list.lookup_count() as _);
        let feature_variations = self
            .feature_variations()
            .transpose()?
            .map(|vars| {
                let data = vars.offset_data();
                vars.feature_variation_records()
                    .iter()
                    .filter_map(move |rec| {
                        rec.feature_table_substitution(data)
                            .transpose()
                            .ok()
                            .flatten()
                    })
                    .flat_map(|subs| {
                        subs.substitutions()
                            .iter()
                            .map(move |sub| sub.alternate_feature(subs.offset_data()))
                    })
            })
            .into_iter()
            .flatten();
        for feature in feature_list
            .feature_records()
            .iter()
            .map(|rec| rec.feature(feature_list.offset_data()))
            .chain(feature_variations)
        {
            lookup_ids.extend(feature?.lookup_list_indices().iter().map(|idx| idx.get()));
        }

        // and now we need to add lookups referenced by contextual lookups,
        // IFF they are reachable via the current set of glyphs:
        for lookup in lookup_list.lookups().iter() {
            let subtables = lookup?.subtables()?;
            match subtables {
                SubstitutionSubtables::Contextual(tables) => tables
                    .iter()
                    .try_for_each(|t| t?.add_reachable_lookups(glyphs, &mut lookup_ids)),
                SubstitutionSubtables::ChainContextual(tables) => tables
                    .iter()
                    .try_for_each(|t| t?.add_reachable_lookups(glyphs, &mut lookup_ids)),
                _ => Ok(()),
            }?;
        }
        Ok(lookup_ids)
    }
}

impl<'a> GlyphClosure for SubstitutionSubtables<'a> {
    fn add_reachable_glyphs(&self, glyphs: &mut HashSet<GlyphId16>) -> Result<(), ReadError> {
        match self {
            SubstitutionSubtables::Single(tables) => tables.add_reachable_glyphs(glyphs),
            SubstitutionSubtables::Multiple(tables) => tables.add_reachable_glyphs(glyphs),
            SubstitutionSubtables::Alternate(tables) => tables.add_reachable_glyphs(glyphs),
            SubstitutionSubtables::Ligature(tables) => tables.add_reachable_glyphs(glyphs),
            SubstitutionSubtables::Reverse(tables) => tables.add_reachable_glyphs(glyphs),
            _ => Ok(()),
        }
    }
}

impl<'a, T: FontRead<'a> + GlyphClosure + 'a, Ext: ExtensionLookup<'a, T> + 'a> GlyphClosure
    for Subtables<'a, T, Ext>
{
    fn add_reachable_glyphs(&self, glyphs: &mut HashSet<GlyphId16>) -> Result<(), ReadError> {
        self.iter()
            .try_for_each(|t| t?.add_reachable_glyphs(glyphs))
    }
}

impl<'a> GlyphClosure for SingleSubst<'a> {
    fn add_reachable_glyphs(&self, glyphs: &mut HashSet<GlyphId16>) -> Result<(), ReadError> {
        for (target, replacement) in self.iter_subs()? {
            if glyphs.contains(&target) {
                glyphs.insert(replacement);
            }
        }
        Ok(())
    }
}

impl<'a> SingleSubst<'a> {
    fn iter_subs(&self) -> Result<impl Iterator<Item = (GlyphId16, GlyphId16)> + '_, ReadError> {
        let (left, right) = match self {
            SingleSubst::Format1(t) => (Some(t.iter_subs()?), None),
            SingleSubst::Format2(t) => (None, Some(t.iter_subs()?)),
        };
        Ok(left
            .into_iter()
            .flatten()
            .chain(right.into_iter().flatten()))
    }
}

impl<'a> SingleSubstFormat1<'a> {
    fn iter_subs(&self) -> Result<impl Iterator<Item = (GlyphId16, GlyphId16)> + '_, ReadError> {
        let delta = self.delta_glyph_id();
        let coverage = self.coverage()?;
        Ok(coverage.iter().filter_map(move |gid| {
            let raw = (gid.to_u16() as i32).checked_add(delta as i32);
            let raw = raw.and_then(|raw| u16::try_from(raw).ok())?;
            Some((gid, GlyphId16::new(raw)))
        }))
    }
}

impl<'a> SingleSubstFormat2<'a> {
    fn iter_subs(&self) -> Result<impl Iterator<Item = (GlyphId16, GlyphId16)> + '_, ReadError> {
        let coverage = self.coverage()?;
        let subs = self.substitute_glyph_ids();
        Ok(coverage.iter().zip(subs.iter().map(|id| id.get())))
    }
}

impl<'a> GlyphClosure for MultipleSubstFormat1<'a> {
    fn add_reachable_glyphs(&self, glyphs: &mut HashSet<GlyphId16>) -> Result<(), ReadError> {
        let coverage = self.coverage()?;
        let sequences = self.sequences();
        for (gid, replacements) in coverage.iter().zip(sequences.iter()) {
            let replacements = replacements?;
            if glyphs.contains(&gid) {
                glyphs.extend(
                    replacements
                        .substitute_glyph_ids()
                        .iter()
                        .map(|gid| gid.get()),
                );
            }
        }
        Ok(())
    }
}

impl<'a> GlyphClosure for AlternateSubstFormat1<'a> {
    fn add_reachable_glyphs(&self, glyphs: &mut HashSet<GlyphId16>) -> Result<(), ReadError> {
        let coverage = self.coverage()?;
        let alts = self.alternate_sets();
        for (gid, alts) in coverage.iter().zip(alts.iter()) {
            let alts = alts?;
            if glyphs.contains(&gid) {
                glyphs.extend(alts.alternate_glyph_ids().iter().map(|gid| gid.get()));
            }
        }
        Ok(())
    }
}

impl<'a> GlyphClosure for LigatureSubstFormat1<'a> {
    fn add_reachable_glyphs(&self, glyphs: &mut HashSet<GlyphId16>) -> Result<(), ReadError> {
        let coverage = self.coverage()?;
        let ligs = self.ligature_sets();
        for (gid, lig_set) in coverage.iter().zip(ligs.iter()) {
            let lig_set = lig_set?;
            if glyphs.contains(&gid) {
                for lig in lig_set.ligatures().iter() {
                    let lig = lig?;
                    if lig
                        .component_glyph_ids()
                        .iter()
                        .all(|gid| glyphs.contains(&gid.get()))
                    {
                        glyphs.insert(lig.ligature_glyph());
                    }
                }
            }
        }
        Ok(())
    }
}

impl GlyphClosure for ReverseChainSingleSubstFormat1<'_> {
    fn add_reachable_glyphs(&self, glyphs: &mut HashSet<GlyphId16>) -> Result<(), ReadError> {
        for coverage in self
            .backtrack_coverages()
            .iter()
            .chain(self.lookahead_coverages().iter())
        {
            if !coverage?.iter().any(|gid| glyphs.contains(&gid)) {
                return Ok(());
            }
        }

        for (gid, sub) in self.coverage()?.iter().zip(self.substitute_glyph_ids()) {
            if glyphs.contains(&gid) {
                glyphs.insert(sub.get());
            }
        }

        Ok(())
    }
}

impl SequenceContext<'_> {
    fn add_reachable_lookups(
        &self,
        glyphs: &HashSet<GlyphId16>,
        lookups: &mut HashSet<u16>,
    ) -> Result<(), ReadError> {
        match self {
            SequenceContext::Format1(table) => table.add_reachable_lookups(glyphs, lookups),
            SequenceContext::Format2(table) => table.add_reachable_lookups(glyphs, lookups),
            SequenceContext::Format3(table) => table.add_reachable_lookups(glyphs, lookups),
        }
    }
}

impl SequenceContextFormat1<'_> {
    fn add_reachable_lookups(
        &self,
        glyphs: &HashSet<GlyphId16>,
        lookups: &mut HashSet<u16>,
    ) -> Result<(), ReadError> {
        let coverage = self.coverage()?;
        for seq in coverage
            .iter()
            .zip(self.seq_rule_sets().iter())
            .filter_map(|(gid, seq)| seq.filter(|_| glyphs.contains(&gid)))
        {
            for rule in seq?.seq_rules().iter() {
                let rule = rule?;
                if rule
                    .input_sequence()
                    .iter()
                    .all(|gid| glyphs.contains(&gid.get()))
                {
                    lookups.extend(
                        rule.seq_lookup_records()
                            .iter()
                            .map(|rec| rec.lookup_list_index()),
                    );
                }
            }
        }
        Ok(())
    }
}

impl SequenceContextFormat2<'_> {
    fn add_reachable_lookups(
        &self,
        glyphs: &HashSet<GlyphId16>,
        lookups: &mut HashSet<u16>,
    ) -> Result<(), ReadError> {
        let classdef = self.class_def()?;
        let our_classes = make_class_set(glyphs, &classdef);
        for seq in self
            .class_seq_rule_sets()
            .iter()
            .enumerate()
            .filter_map(|(i, seq)| seq.filter(|_| our_classes.contains(&(i as u16))))
        {
            for rule in seq?.class_seq_rules().iter() {
                let rule = rule?;
                if rule
                    .input_sequence()
                    .iter()
                    .all(|class_id| our_classes.contains(&class_id.get()))
                {
                    lookups.extend(
                        rule.seq_lookup_records()
                            .iter()
                            .map(|rec| rec.lookup_list_index()),
                    )
                }
            }
        }
        Ok(())
    }
}

impl SequenceContextFormat3<'_> {
    fn add_reachable_lookups(
        &self,
        glyphs: &HashSet<GlyphId16>,
        lookups: &mut HashSet<u16>,
    ) -> Result<(), ReadError> {
        for coverage in self.coverages().iter() {
            if !coverage?.iter().any(|gid| glyphs.contains(&gid)) {
                return Ok(());
            }
        }
        lookups.extend(
            self.seq_lookup_records()
                .iter()
                .map(|rec| rec.lookup_list_index()),
        );
        Ok(())
    }
}

impl ChainedSequenceContext<'_> {
    fn add_reachable_lookups(
        &self,
        glyphs: &HashSet<GlyphId16>,
        lookups: &mut HashSet<u16>,
    ) -> Result<(), ReadError> {
        match self {
            ChainedSequenceContext::Format1(table) => table.add_reachable_lookups(glyphs, lookups),
            ChainedSequenceContext::Format2(table) => table.add_reachable_lookups(glyphs, lookups),
            ChainedSequenceContext::Format3(table) => table.add_reachable_lookups(glyphs, lookups),
        }
    }
}

impl ChainedSequenceContextFormat1<'_> {
    fn add_reachable_lookups(
        &self,
        glyphs: &HashSet<GlyphId16>,
        lookups: &mut HashSet<u16>,
    ) -> Result<(), ReadError> {
        let coverage = self.coverage()?;
        for seq in coverage
            .iter()
            .zip(self.chained_seq_rule_sets().iter())
            .filter_map(|(gid, seq)| seq.filter(|_| glyphs.contains(&gid)))
        {
            for rule in seq?.chained_seq_rules().iter() {
                let rule = rule?;
                if rule
                    .input_sequence()
                    .iter()
                    .chain(rule.backtrack_sequence())
                    .chain(rule.lookahead_sequence())
                    .all(|gid| glyphs.contains(&gid.get()))
                {
                    lookups.extend(
                        rule.seq_lookup_records()
                            .iter()
                            .map(|rec| rec.lookup_list_index()),
                    );
                }
            }
        }
        Ok(())
    }
}

impl ChainedSequenceContextFormat2<'_> {
    fn add_reachable_lookups(
        &self,
        glyphs: &HashSet<GlyphId16>,
        lookups: &mut HashSet<u16>,
    ) -> Result<(), ReadError> {
        let input = self.input_class_def()?;
        let backtrack = self.backtrack_class_def()?;
        let lookahead = self.lookahead_class_def()?;

        let input_classes = make_class_set(glyphs, &input);
        let backtrack_classes = make_class_set(glyphs, &backtrack);
        let lookahead_classes = make_class_set(glyphs, &lookahead);
        for seq in self
            .chained_class_seq_rule_sets()
            .iter()
            .enumerate()
            .filter_map(|(i, seq)| seq.filter(|_| input_classes.contains(&(i as u16))))
        {
            for rule in seq?.chained_class_seq_rules().iter() {
                let rule = rule?;
                if rule
                    .input_sequence()
                    .iter()
                    .all(|cls| input_classes.contains(&cls.get()))
                    && rule
                        .backtrack_sequence()
                        .iter()
                        .all(|cls| backtrack_classes.contains(&cls.get()))
                    && rule
                        .lookahead_sequence()
                        .iter()
                        .all(|cls| lookahead_classes.contains(&cls.get()))
                {
                    lookups.extend(
                        rule.seq_lookup_records()
                            .iter()
                            .map(|rec| rec.lookup_list_index()),
                    )
                }
            }
        }
        Ok(())
    }
}

impl ChainedSequenceContextFormat3<'_> {
    fn add_reachable_lookups(
        &self,
        glyphs: &HashSet<GlyphId16>,
        lookups: &mut HashSet<u16>,
    ) -> Result<(), ReadError> {
        for coverage in self
            .backtrack_coverages()
            .iter()
            .chain(self.input_coverages().iter())
            .chain(self.lookahead_coverages().iter())
        {
            if !coverage?.iter().any(|gid| glyphs.contains(&gid)) {
                return Ok(());
            }
        }
        lookups.extend(
            self.seq_lookup_records()
                .iter()
                .map(|rec| rec.lookup_list_index()),
        );
        Ok(())
    }
}

fn make_class_set(glyphs: &HashSet<GlyphId16>, classdef: &ClassDef) -> HashSet<u16> {
    glyphs.iter().map(|gid| classdef.get(*gid)).collect()
}

#[cfg(test)]
mod tests {
    use std::collections::HashMap;

    use crate::{FontRef, TableProvider};

    use super::*;
    use font_test_data::closure as test_data;

    struct GlyphMap {
        to_gid: HashMap<&'static str, GlyphId16>,
        from_gid: HashMap<GlyphId16, &'static str>,
    }

    impl GlyphMap {
        fn new(raw_order: &'static str) -> GlyphMap {
            let to_gid: HashMap<_, _> = raw_order
                .split('\n')
                .map(|line| line.trim())
                .filter(|line| !(line.starts_with('#') || line.is_empty()))
                .enumerate()
                .map(|(gid, name)| (name, GlyphId16::new(gid.try_into().unwrap())))
                .collect();
            let from_gid = to_gid.iter().map(|(name, gid)| (*gid, *name)).collect();
            GlyphMap { from_gid, to_gid }
        }

        fn get_gid(&self, name: &str) -> Option<GlyphId16> {
            self.to_gid.get(name).copied()
        }

        fn get_name(&self, gid: GlyphId16) -> Option<&str> {
            self.from_gid.get(&gid).copied()
        }
    }

    fn get_gsub(test_data: &'static [u8]) -> Gsub<'_> {
        let font = FontRef::new(test_data).unwrap();
        font.gsub().unwrap()
    }

    fn compute_closure(gsub: &Gsub, glyph_map: &GlyphMap, input: &[&str]) -> HashSet<GlyphId16> {
        let input_glyphs = input
            .iter()
            .map(|name| glyph_map.get_gid(name).unwrap())
            .collect();
        gsub.closure_glyphs(input_glyphs).unwrap()
    }

    /// assert a set of glyph ids matches a slice of names
    macro_rules! assert_closure_result {
        ($glyph_map:expr, $result:expr, $expected:expr) => {
            let result = $result
                .iter()
                .map(|gid| $glyph_map.get_name(*gid).unwrap())
                .collect::<HashSet<_>>();
            let expected = $expected.iter().copied().collect::<HashSet<_>>();
            if expected != result {
                let in_output = result.difference(&expected).collect::<Vec<_>>();
                let in_expected = expected.difference(&result).collect::<Vec<_>>();
                let mut msg = format!("Closure output does not match\n");
                if !in_expected.is_empty() {
                    msg.push_str(format!("missing {in_expected:?}\n").as_str());
                }
                if !in_output.is_empty() {
                    msg.push_str(format!("unexpected {in_output:?}").as_str());
                }
                panic!("{msg}")
            }
        };
    }

    #[test]
    fn smoke_test() {
        // tests various lookup types.
        // test input is font-test-data/test_data/fea/simple_closure.fea
        let gsub = get_gsub(test_data::SIMPLE);
        let glyph_map = GlyphMap::new(test_data::SIMPLE_GLYPHS);
        let result = compute_closure(&gsub, &glyph_map, &["a"]);

        assert_closure_result!(
            glyph_map,
            result,
            &["a", "A", "b", "c", "d", "a_a", "a.1", "a.2", "a.3"]
        );
    }

    #[test]
    fn recursive() {
        // a scenario in which one substitution adds glyphs that trigger additional
        // substitutions.
        //
        // test input is font-test-data/test_data/fea/recursive_closure.fea
        let gsub = get_gsub(test_data::RECURSIVE);
        let glyph_map = GlyphMap::new(test_data::RECURSIVE_GLYPHS);
        let result = compute_closure(&gsub, &glyph_map, &["a"]);
        assert_closure_result!(glyph_map, result, &["a", "b", "c", "d"]);
    }

    #[test]
    fn contextual_lookups() {
        let gsub = get_gsub(test_data::CONTEXTUAL);
        let glyph_map = GlyphMap::new(test_data::CONTEXTUAL_GLYPHS);

        // these match the lookups but not the context
        let nop = compute_closure(&gsub, &glyph_map, &["three", "four", "e", "f"]);
        assert_closure_result!(glyph_map, nop, &["three", "four", "e", "f"]);

        let gsub6f1 = compute_closure(
            &gsub,
            &glyph_map,
            &["one", "two", "three", "four", "five", "six", "seven"],
        );
        assert_closure_result!(
            glyph_map,
            gsub6f1,
            &["one", "two", "three", "four", "five", "six", "seven", "X", "Y"]
        );

        let gsub6f3 = compute_closure(&gsub, &glyph_map, &["space", "e"]);
        assert_closure_result!(glyph_map, gsub6f3, &["space", "e", "e.2"]);

        let gsub5f3 = compute_closure(&gsub, &glyph_map, &["f", "g"]);
        assert_closure_result!(glyph_map, gsub5f3, &["f", "g", "f.2"]);
    }

    #[test]
    fn recursive_context() {
        let gsub = get_gsub(test_data::RECURSIVE_CONTEXTUAL);
        let glyph_map = GlyphMap::new(test_data::RECURSIVE_CONTEXTUAL_GLYPHS);

        let nop = compute_closure(&gsub, &glyph_map, &["b", "B"]);
        assert_closure_result!(glyph_map, nop, &["b", "B"]);

        let full = compute_closure(&gsub, &glyph_map, &["a", "b", "c"]);
        assert_closure_result!(glyph_map, full, &["a", "b", "c", "B", "B.2", "B.3"]);

        let intermediate = compute_closure(&gsub, &glyph_map, &["a", "B.2"]);
        assert_closure_result!(glyph_map, intermediate, &["a", "B.2", "B.3"]);
    }

    #[test]
    fn feature_variations() {
        let gsub = get_gsub(test_data::VARIATIONS_CLOSURE);
        let glyph_map = GlyphMap::new(test_data::VARIATIONS_GLYPHS);

        let input = compute_closure(&gsub, &glyph_map, &["a"]);
        assert_closure_result!(glyph_map, input, &["a", "b", "c"]);
    }
}

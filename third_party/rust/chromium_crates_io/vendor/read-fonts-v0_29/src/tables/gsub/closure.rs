//! Computing the closure over a set of glyphs
//!
//! This means taking a set of glyphs and updating it to include any other glyphs
//! reachable from those glyphs via substitution, recursively.

use font_types::{GlyphId, GlyphId16};

use crate::{
    collections::IntSet,
    tables::layout::{ExtensionLookup, Subtables},
    FontRead, ReadError, Tag,
};

use super::{
    AlternateSubstFormat1, ChainedSequenceContext, ClassDef, CoverageTable, Gsub, Ligature,
    LigatureSet, LigatureSubstFormat1, MultipleSubstFormat1, ReverseChainSingleSubstFormat1,
    SequenceContext, SingleSubst, SingleSubstFormat1, SingleSubstFormat2, SubstitutionLookup,
    SubstitutionLookupList, SubstitutionSubtables,
};

#[cfg(feature = "std")]
use crate::tables::layout::{
    ContextFormat1, ContextFormat2, ContextFormat3, LookupClosure, LookupClosureCtx,
};

// we put ClosureCtx in its own module to enforce visibility rules;
// specifically we don't want cur_glyphs to be reachable directly
mod ctx {
    use std::collections::HashMap;

    use types::GlyphId16;

    use crate::{collections::IntSet, tables::gsub::SubstitutionLookup};

    use super::GlyphClosure as _;

    pub(super) struct ClosureCtx<'a> {
        /// the current closure glyphs. This is updated as we go.
        glyphs: &'a mut IntSet<GlyphId16>,
        // in certain situations (like when recursing into contextual lookups) we
        // consider a smaller subset of glyphs to be 'active'.
        cur_glyphs: Option<IntSet<GlyphId16>>,
        finished_lookups: HashMap<u16, (u64, Option<IntSet<GlyphId16>>)>,
        // when we encounter contextual lookups we want to visit the lookups
        // they reference, but only with the glyphs that would trigger those
        // subtable lookups.
        //
        // here we store tuples of (LookupId, relevant glyphs); these todos can
        // be done at the end of each pass.
        contextual_lookup_todos: Vec<super::ContextualLookupRef>,
    }

    impl<'a> ClosureCtx<'a> {
        pub(super) fn new(glyphs: &'a mut IntSet<GlyphId16>) -> Self {
            Self {
                glyphs,
                cur_glyphs: Default::default(),
                contextual_lookup_todos: Default::default(),
                finished_lookups: Default::default(),
            }
        }

        pub(super) fn current_glyphs(&self) -> &IntSet<GlyphId16> {
            self.cur_glyphs.as_ref().unwrap_or(self.glyphs)
        }

        pub(super) fn glyphs(&self) -> &IntSet<GlyphId16> {
            self.glyphs
        }

        pub(super) fn add_glyph(&mut self, gid: GlyphId16) {
            self.glyphs.insert(gid);
        }

        pub(super) fn extend_glyphs(&mut self, iter: impl IntoIterator<Item = GlyphId16>) {
            self.glyphs.extend(iter)
        }

        pub(super) fn add_todo(
            &mut self,
            lookup_id: u16,
            active_glyphs: Option<IntSet<GlyphId16>>,
        ) {
            self.contextual_lookup_todos
                .push(super::ContextualLookupRef {
                    lookup_id,
                    active_glyphs,
                })
        }

        pub(super) fn pop_a_todo(&mut self) -> Option<super::ContextualLookupRef> {
            self.contextual_lookup_todos.pop()
        }

        pub(super) fn closure_glyphs(
            &mut self,
            lookup: SubstitutionLookup,
            lookup_id: u16,
            current_glyphs: Option<IntSet<GlyphId16>>,
        ) -> Result<(), crate::ReadError> {
            if self.needs_to_do_lookup(lookup_id, current_glyphs.as_ref()) {
                self.cur_glyphs = current_glyphs;
                lookup.add_reachable_glyphs(self)?;
                self.cur_glyphs = None;
            }
            Ok(())
        }

        /// skip lookups if we've already seen them with our current state
        /// <https://github.com/fonttools/fonttools/blob/a6f59a4f87a0111060/Lib/fontTools/subset/__init__.py#L1510>
        fn needs_to_do_lookup(
            &mut self,
            id: u16,
            current_glyphs: Option<&IntSet<GlyphId16>>,
        ) -> bool {
            let (count, covered) = self.finished_lookups.entry(id).or_insert((0, None));
            if *count != self.glyphs.len() {
                *count = self.glyphs.len();
                *covered = Some(IntSet::new());
            }
            //TODO: would be nice to have IntSet::is_subset
            if current_glyphs.unwrap_or(self.glyphs).iter().all(|gid| {
                covered
                    .as_ref()
                    .map(|cov| cov.contains(gid))
                    // only true if self.glyphs is empty, which means it's a noop anyway?
                    .unwrap_or(false)
            }) {
                return false;
            }
            covered
                .get_or_insert_with(Default::default)
                .extend(current_glyphs.unwrap_or(self.glyphs).iter());
            true
        }
    }
}

use ctx::ClosureCtx;

/// a lookup referenced by a contextual lookup
#[derive(Debug)]
struct ContextualLookupRef {
    lookup_id: u16,
    // 'none' means the graph is too complex, assume all glyphs are active
    active_glyphs: Option<IntSet<GlyphId16>>,
}

/// A trait for tables which participate in closure
trait GlyphClosure {
    /// Update the set of glyphs with any glyphs reachable via substitution.
    fn add_reachable_glyphs(&self, ctx: &mut ClosureCtx) -> Result<(), ReadError>;
}

impl Gsub<'_> {
    /// Return the set of glyphs reachable from the input set via any substitution.
    pub fn closure_glyphs(
        &self,
        mut glyphs: IntSet<GlyphId16>,
    ) -> Result<IntSet<GlyphId16>, ReadError> {
        // we need to do this iteratively, since any glyph found in one pass
        // over the lookups could also be the target of substitutions.

        let mut ctx = ClosureCtx::new(&mut glyphs);

        let reachable_lookups = self.find_reachable_lookups()?;
        let mut prev_lookup_count = 0;
        let mut prev_glyph_count = 0;
        let mut new_glyph_count = ctx.glyphs().len();
        let mut new_lookup_count = reachable_lookups.len();

        while (prev_glyph_count, prev_lookup_count) != (new_glyph_count, new_lookup_count) {
            (prev_glyph_count, prev_lookup_count) = (new_glyph_count, new_lookup_count);

            // we always call this once, and then keep calling if it produces
            // additional glyphs
            self.closure_glyphs_once(&mut ctx, &reachable_lookups)?;

            new_lookup_count = reachable_lookups.len();
            new_glyph_count = ctx.glyphs().len();
        }

        Ok(glyphs)
    }

    fn closure_glyphs_once(
        &self,
        ctx: &mut ClosureCtx,
        lookups_to_use: &IntSet<u16>,
    ) -> Result<(), ReadError> {
        let lookup_list = self.lookup_list()?;
        for idx in lookups_to_use.iter() {
            let lookup = lookup_list.lookups().get(idx as usize)?;
            ctx.closure_glyphs(lookup, idx, None)?;
        }
        // then do any lookups referenced by contextual lookups
        while let Some(todo) = ctx.pop_a_todo() {
            let lookup = lookup_list.lookups().get(todo.lookup_id as _)?;
            ctx.closure_glyphs(lookup, todo.lookup_id, todo.active_glyphs)?;
        }
        Ok(())
    }

    fn find_reachable_lookups(&self) -> Result<IntSet<u16>, ReadError> {
        let feature_list = self.feature_list()?;
        let mut lookup_ids = IntSet::new();

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
        Ok(lookup_ids)
    }

    /// Return a set of all feature indices underneath the specified scripts, languages and features
    pub fn collect_features(
        &self,
        scripts: &IntSet<Tag>,
        languages: &IntSet<Tag>,
        features: &IntSet<Tag>,
    ) -> Result<IntSet<u16>, ReadError> {
        let feature_list = self.feature_list()?;
        let script_list = self.script_list()?;
        let head_ptr = self.offset_data().as_bytes().as_ptr() as usize;
        script_list.collect_features(head_ptr, &feature_list, scripts, languages, features)
    }
}

impl GlyphClosure for SubstitutionLookup<'_> {
    fn add_reachable_glyphs(&self, ctx: &mut ClosureCtx) -> Result<(), ReadError> {
        self.subtables()?.add_reachable_glyphs(ctx)
    }
}

impl GlyphClosure for SubstitutionSubtables<'_> {
    fn add_reachable_glyphs(&self, glyphs: &mut ClosureCtx<'_>) -> Result<(), ReadError> {
        match self {
            SubstitutionSubtables::Single(tables) => tables.add_reachable_glyphs(glyphs),
            SubstitutionSubtables::Multiple(tables) => tables.add_reachable_glyphs(glyphs),
            SubstitutionSubtables::Alternate(tables) => tables.add_reachable_glyphs(glyphs),
            SubstitutionSubtables::Ligature(tables) => tables.add_reachable_glyphs(glyphs),
            SubstitutionSubtables::Reverse(tables) => tables.add_reachable_glyphs(glyphs),
            SubstitutionSubtables::Contextual(tables) => tables.add_reachable_glyphs(glyphs),
            SubstitutionSubtables::ChainContextual(tables) => tables.add_reachable_glyphs(glyphs),
        }
    }
}

impl<'a, T: FontRead<'a> + GlyphClosure + 'a, Ext: ExtensionLookup<'a, T> + 'a> GlyphClosure
    for Subtables<'a, T, Ext>
{
    fn add_reachable_glyphs(&self, ctx: &mut ClosureCtx<'_>) -> Result<(), ReadError> {
        self.iter().try_for_each(|t| t?.add_reachable_glyphs(ctx))
    }
}

impl GlyphClosure for SingleSubst<'_> {
    fn add_reachable_glyphs(&self, ctx: &mut ClosureCtx<'_>) -> Result<(), ReadError> {
        for (target, replacement) in self.iter_subs()? {
            if ctx.current_glyphs().contains(target) {
                ctx.add_glyph(replacement);
            }
        }
        Ok(())
    }
}

impl SingleSubst<'_> {
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

impl SingleSubstFormat1<'_> {
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

impl SingleSubstFormat2<'_> {
    fn iter_subs(&self) -> Result<impl Iterator<Item = (GlyphId16, GlyphId16)> + '_, ReadError> {
        let coverage = self.coverage()?;
        let subs = self.substitute_glyph_ids();
        Ok(coverage.iter().zip(subs.iter().map(|id| id.get())))
    }
}

impl GlyphClosure for MultipleSubstFormat1<'_> {
    fn add_reachable_glyphs(&self, ctx: &mut ClosureCtx<'_>) -> Result<(), ReadError> {
        let coverage = self.coverage()?;
        let sequences = self.sequences();
        for (gid, replacements) in coverage.iter().zip(sequences.iter()) {
            let replacements = replacements?;
            if ctx.current_glyphs().contains(gid) {
                ctx.extend_glyphs(
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

impl GlyphClosure for AlternateSubstFormat1<'_> {
    fn add_reachable_glyphs(&self, ctx: &mut ClosureCtx<'_>) -> Result<(), ReadError> {
        let coverage = self.coverage()?;
        let alts = self.alternate_sets();
        for (gid, alts) in coverage.iter().zip(alts.iter()) {
            let alts = alts?;
            if ctx.current_glyphs().contains(gid) {
                ctx.extend_glyphs(alts.alternate_glyph_ids().iter().map(|gid| gid.get()));
            }
        }
        Ok(())
    }
}

impl GlyphClosure for LigatureSubstFormat1<'_> {
    fn add_reachable_glyphs(&self, ctx: &mut ClosureCtx<'_>) -> Result<(), ReadError> {
        let coverage = self.coverage()?;
        let ligs = self.ligature_sets();
        for (gid, lig_set) in coverage.iter().zip(ligs.iter()) {
            let lig_set = lig_set?;
            if ctx.current_glyphs().contains(gid) {
                for lig in lig_set.ligatures().iter() {
                    let lig = lig?;
                    if lig
                        .component_glyph_ids()
                        .iter()
                        .all(|gid| ctx.glyphs().contains(gid.get()))
                    {
                        ctx.add_glyph(lig.ligature_glyph());
                    }
                }
            }
        }
        Ok(())
    }
}

impl GlyphClosure for ReverseChainSingleSubstFormat1<'_> {
    fn add_reachable_glyphs(&self, ctx: &mut ClosureCtx<'_>) -> Result<(), ReadError> {
        for coverage in self
            .backtrack_coverages()
            .iter()
            .chain(self.lookahead_coverages().iter())
        {
            if !coverage?.iter().any(|gid| ctx.glyphs().contains(gid)) {
                return Ok(());
            }
        }

        for (gid, sub) in self.coverage()?.iter().zip(self.substitute_glyph_ids()) {
            if ctx.current_glyphs().contains(gid) {
                ctx.add_glyph(sub.get());
            }
        }

        Ok(())
    }
}

impl GlyphClosure for SequenceContext<'_> {
    fn add_reachable_glyphs(&self, ctx: &mut ClosureCtx) -> Result<(), ReadError> {
        match self {
            Self::Format1(table) => ContextFormat1::Plain(table.clone()).add_reachable_glyphs(ctx),
            Self::Format2(table) => ContextFormat2::Plain(table.clone()).add_reachable_glyphs(ctx),
            Self::Format3(table) => ContextFormat3::Plain(table.clone()).add_reachable_glyphs(ctx),
        }
    }
}

impl GlyphClosure for ChainedSequenceContext<'_> {
    fn add_reachable_glyphs(&self, ctx: &mut ClosureCtx) -> Result<(), ReadError> {
        match self {
            Self::Format1(table) => ContextFormat1::Chain(table.clone()).add_reachable_glyphs(ctx),
            Self::Format2(table) => ContextFormat2::Chain(table.clone()).add_reachable_glyphs(ctx),
            Self::Format3(table) => ContextFormat3::Chain(table.clone()).add_reachable_glyphs(ctx),
        }
    }
}

//https://github.com/fonttools/fonttools/blob/a6f59a4f8/Lib/fontTools/subset/__init__.py#L1182
impl GlyphClosure for ContextFormat1<'_> {
    fn add_reachable_glyphs(&self, ctx: &mut ClosureCtx<'_>) -> Result<(), ReadError> {
        let coverage = self.coverage()?;
        let Some(cur_glyphs) = intersect_coverage(&coverage, ctx.current_glyphs()) else {
            return Ok(());
        };

        // now for each rule set that applies to a current glyph:
        for (i, seq) in coverage
            .iter()
            .zip(self.rule_sets())
            .enumerate()
            .filter_map(|(i, (gid, seq))| {
                seq.filter(|_| cur_glyphs.contains(gid)).map(|seq| (i, seq))
            })
        {
            for rule in seq?.rules() {
                let rule = rule?;
                // skip rules if the whole input sequence isn't in our glyphset
                if !rule.matches_glyphs(ctx.glyphs()) {
                    continue;
                }
                // python calls this 'chaos'. Basically: if there are multiple
                // lookups applied at a single position they can interact, and
                // we can no longer trivially determine the state of the context
                // at that point. In this case we give up, and assume that the
                // second lookup is reachable by all glyphs.
                let mut seen_sequence_indices = IntSet::new();
                for lookup_record in rule.lookup_records() {
                    let lookup_id = lookup_record.lookup_list_index();
                    let sequence_idx = lookup_record.sequence_index();
                    let active_glyphs = if !seen_sequence_indices.insert(sequence_idx) {
                        // During processing, when we see an empty set we will replace
                        // it with the full current glyph set
                        None
                    } else if sequence_idx == 0 {
                        Some(IntSet::from([coverage.iter().nth(i).unwrap()]))
                    } else {
                        Some(IntSet::from([rule.input_sequence()
                            [sequence_idx as usize - 1]
                            .get()]))
                    };
                    ctx.add_todo(lookup_id, active_glyphs);
                }
            }
        }
        Ok(())
    }
}

//https://github.com/fonttools/fonttools/blob/a6f59a4f87a0111/Lib/fontTools/subset/__init__.py#L1215
impl GlyphClosure for ContextFormat2<'_> {
    fn add_reachable_glyphs(&self, ctx: &mut ClosureCtx) -> Result<(), ReadError> {
        let coverage = self.coverage()?;
        let Some(cur_glyphs) = intersect_coverage(&coverage, ctx.current_glyphs()) else {
            return Ok(());
        };

        let classdef = self.input_class_def()?;
        let our_classes = make_class_set(ctx.glyphs(), &classdef);
        for (class_i, seq) in self
            .rule_sets()
            .enumerate()
            .filter_map(|(i, seq)| seq.map(|seq| (i as u16, seq)))
            .filter(|x| our_classes.contains(x.0))
        {
            for rule in seq?.rules() {
                let rule = rule?;
                if !rule.matches_classes(&our_classes) {
                    continue;
                }

                let mut seen_sequence_indices = IntSet::new();
                for lookup_record in rule.lookup_records() {
                    let lookup_id = lookup_record.lookup_list_index();
                    let seq_idx = lookup_record.sequence_index();
                    let active_glyphs = if !seen_sequence_indices.insert(seq_idx) {
                        None
                    } else if seq_idx == 0 {
                        Some(intersect_class(&classdef, &cur_glyphs, class_i))
                    } else {
                        Some(intersect_class(
                            &classdef,
                            ctx.glyphs(),
                            rule.input_sequence()[seq_idx as usize - 1].get(),
                        ))
                    };

                    ctx.add_todo(lookup_id, active_glyphs);
                }
            }
        }
        Ok(())
    }
}

impl GlyphClosure for ContextFormat3<'_> {
    fn add_reachable_glyphs(&self, ctx: &mut ClosureCtx) -> Result<(), ReadError> {
        let cov0 = self.coverages().get(0)?;
        let Some(cur_glyphs) = intersect_coverage(&cov0, ctx.current_glyphs()) else {
            return Ok(());
        };

        let glyphs = ctx.glyphs().iter().map(GlyphId::from).collect();
        if !self.matches_glyphs(&glyphs)? {
            return Ok(());
        }
        for record in self.lookup_records() {
            let mut seen_sequence_indices = IntSet::new();
            let seq_idx = record.sequence_index();
            let lookup_id = record.lookup_list_index();
            let active_glyphs = if !seen_sequence_indices.insert(seq_idx) {
                None
            } else if seq_idx == 0 {
                Some(cur_glyphs.clone())
            } else {
                Some(
                    self.coverages()
                        .get(seq_idx as _)?
                        .iter()
                        .filter(|gid| ctx.glyphs().contains(*gid))
                        .collect(),
                )
            };

            ctx.add_todo(lookup_id, active_glyphs);
        }
        Ok(())
    }
}

/// The set of classes for this set of glyphs
fn make_class_set(glyphs: &IntSet<GlyphId16>, classdef: &ClassDef) -> IntSet<u16> {
    glyphs.iter().map(|gid| classdef.get(gid)).collect()
}

/// Return the subset of `glyphs` that has the given class in this classdef
// https://github.com/fonttools/fonttools/blob/a6f59a4f87a01110/Lib/fontTools/subset/__init__.py#L516
fn intersect_class(
    classdef: &ClassDef,
    glyphs: &IntSet<GlyphId16>,
    class: u16,
) -> IntSet<GlyphId16> {
    glyphs
        .iter()
        .filter(|gid| classdef.get(*gid) == class)
        .collect()
}

fn intersect_coverage(
    coverage: &CoverageTable,
    glyphs: &IntSet<GlyphId16>,
) -> Option<IntSet<GlyphId16>> {
    let r = coverage
        .iter()
        .filter(|gid| glyphs.contains(*gid))
        .collect::<IntSet<_>>();
    Some(r).filter(|set| !set.is_empty())
}

impl SubstitutionLookupList<'_> {
    pub fn closure_lookups(
        &self,
        glyph_set: &IntSet<GlyphId>,
        lookup_indices: &mut IntSet<u16>,
    ) -> Result<(), ReadError> {
        let mut c = LookupClosureCtx::new(glyph_set);

        let lookups = self.lookups();
        for idx in lookup_indices.iter() {
            let lookup = lookups.get(idx as usize)?;
            lookup.closure_lookups(&mut c, idx)?;
        }

        lookup_indices.union(c.visited_lookups());
        lookup_indices.subtract(c.inactive_lookups());
        Ok(())
    }
}

impl LookupClosure for SubstitutionLookup<'_> {
    fn closure_lookups(
        &self,
        c: &mut LookupClosureCtx,
        lookup_index: u16,
    ) -> Result<(), ReadError> {
        if !c.should_visit_lookup(lookup_index) {
            return Ok(());
        }

        if !self.intersects(c.glyphs())? {
            c.set_lookup_inactive(lookup_index);
            return Ok(());
        }

        let lookup_type = self.lookup_type();
        self.subtables()?.closure_lookups(c, lookup_type)
    }

    fn intersects(&self, glyph_set: &IntSet<GlyphId>) -> Result<bool, ReadError> {
        self.subtables()?.intersects(glyph_set)
    }
}

impl LookupClosure for SubstitutionSubtables<'_> {
    fn closure_lookups(&self, _c: &mut LookupClosureCtx, _arg: u16) -> Result<(), ReadError> {
        Ok(())
    }

    fn intersects(&self, glyph_set: &IntSet<GlyphId>) -> Result<bool, ReadError> {
        match self {
            SubstitutionSubtables::Single(subtables) => subtables.intersects(glyph_set),
            SubstitutionSubtables::Multiple(subtables) => subtables.intersects(glyph_set),
            SubstitutionSubtables::Alternate(subtables) => subtables.intersects(glyph_set),
            SubstitutionSubtables::Ligature(subtables) => subtables.intersects(glyph_set),
            SubstitutionSubtables::Contextual(subtables) => subtables.intersects(glyph_set),
            SubstitutionSubtables::ChainContextual(subtables) => subtables.intersects(glyph_set),
            SubstitutionSubtables::Reverse(subtables) => subtables.intersects(glyph_set),
        }
    }
}

impl LookupClosure for SingleSubst<'_> {
    fn intersects(&self, glyph_set: &IntSet<GlyphId>) -> Result<bool, ReadError> {
        match self {
            Self::Format1(item) => item.intersects(glyph_set),
            Self::Format2(item) => item.intersects(glyph_set),
        }
    }
}

impl LookupClosure for SingleSubstFormat1<'_> {
    fn intersects(&self, glyph_set: &IntSet<GlyphId>) -> Result<bool, ReadError> {
        Ok(self.coverage()?.intersects(glyph_set))
    }
}

impl LookupClosure for SingleSubstFormat2<'_> {
    fn intersects(&self, glyph_set: &IntSet<GlyphId>) -> Result<bool, ReadError> {
        Ok(self.coverage()?.intersects(glyph_set))
    }
}

impl LookupClosure for MultipleSubstFormat1<'_> {
    fn intersects(&self, glyph_set: &IntSet<GlyphId>) -> Result<bool, ReadError> {
        Ok(self.coverage()?.intersects(glyph_set))
    }
}

impl LookupClosure for AlternateSubstFormat1<'_> {
    fn intersects(&self, glyph_set: &IntSet<GlyphId>) -> Result<bool, ReadError> {
        Ok(self.coverage()?.intersects(glyph_set))
    }
}

impl LookupClosure for LigatureSubstFormat1<'_> {
    fn intersects(&self, glyph_set: &IntSet<GlyphId>) -> Result<bool, ReadError> {
        let coverage = self.coverage()?;
        let lig_sets = self.ligature_sets();
        for lig_set in coverage
            .iter()
            .zip(lig_sets.iter())
            .filter_map(|(g, lig_set)| glyph_set.contains(GlyphId::from(g)).then_some(lig_set))
        {
            if lig_set?.intersects(glyph_set)? {
                return Ok(true);
            }
        }
        Ok(false)
    }
}

impl LookupClosure for LigatureSet<'_> {
    fn intersects(&self, glyph_set: &IntSet<GlyphId>) -> Result<bool, ReadError> {
        let ligs = self.ligatures();
        for lig in ligs.iter() {
            if lig?.intersects(glyph_set)? {
                return Ok(true);
            }
        }
        Ok(false)
    }
}

impl LookupClosure for Ligature<'_> {
    fn intersects(&self, glyph_set: &IntSet<GlyphId>) -> Result<bool, ReadError> {
        let ret = self
            .component_glyph_ids()
            .iter()
            .all(|g| glyph_set.contains(GlyphId::from(g.get())));
        Ok(ret)
    }
}

impl LookupClosure for ReverseChainSingleSubstFormat1<'_> {
    fn intersects(&self, glyph_set: &IntSet<GlyphId>) -> Result<bool, ReadError> {
        if !self.coverage()?.intersects(glyph_set) {
            return Ok(false);
        }

        for coverage in self.backtrack_coverages().iter() {
            if !coverage?.intersects(glyph_set) {
                return Ok(false);
            }
        }

        for coverage in self.lookahead_coverages().iter() {
            if !coverage?.intersects(glyph_set) {
                return Ok(false);
            }
        }
        Ok(true)
    }
}

#[cfg(test)]
mod tests {
    use std::collections::{HashMap, HashSet};

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

    fn get_gsub(test_data: &'static [u8]) -> Gsub<'static> {
        let font = FontRef::new(test_data).unwrap();
        font.gsub().unwrap()
    }

    fn compute_closure(gsub: &Gsub, glyph_map: &GlyphMap, input: &[&str]) -> IntSet<GlyphId16> {
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
                .map(|gid| $glyph_map.get_name(gid).unwrap())
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
    fn contextual_lookups_nop() {
        let gsub = get_gsub(test_data::CONTEXTUAL);
        let glyph_map = GlyphMap::new(test_data::CONTEXTUAL_GLYPHS);

        // these match the lookups but not the context
        let nop = compute_closure(&gsub, &glyph_map, &["three", "four", "e", "f"]);
        assert_closure_result!(glyph_map, nop, &["three", "four", "e", "f"]);
    }

    #[test]
    fn contextual_lookups_chained_f1() {
        let gsub = get_gsub(test_data::CONTEXTUAL);
        let glyph_map = GlyphMap::new(test_data::CONTEXTUAL_GLYPHS);
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
    }

    #[test]
    fn contextual_lookups_chained_f3() {
        let gsub = get_gsub(test_data::CONTEXTUAL);
        let glyph_map = GlyphMap::new(test_data::CONTEXTUAL_GLYPHS);
        let gsub6f3 = compute_closure(&gsub, &glyph_map, &["space", "e"]);
        assert_closure_result!(glyph_map, gsub6f3, &["space", "e", "e.2"]);

        let gsub5f3 = compute_closure(&gsub, &glyph_map, &["f", "g"]);
        assert_closure_result!(glyph_map, gsub5f3, &["f", "g", "f.2"]);
    }

    #[test]
    fn contextual_plain_f1() {
        let gsub = get_gsub(test_data::CONTEXTUAL);
        let glyph_map = GlyphMap::new(test_data::CONTEXTUAL_GLYPHS);
        let gsub5f1 = compute_closure(&gsub, &glyph_map, &["a", "b"]);
        assert_closure_result!(glyph_map, gsub5f1, &["a", "b", "a_b"]);
    }

    #[test]
    fn contextual_plain_f3() {
        let gsub = get_gsub(test_data::CONTEXTUAL);
        let glyph_map = GlyphMap::new(test_data::CONTEXTUAL_GLYPHS);
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

    #[test]
    fn context_with_unreachable_rules() {
        let gsub = get_gsub(test_data::CONTEXT_WITH_UNREACHABLE_BITS);
        let glyph_map = GlyphMap::new(test_data::CONTEXT_WITH_UNREACHABLE_BITS_GLYPHS);

        let nop = compute_closure(&gsub, &glyph_map, &["c", "z"]);
        assert_closure_result!(glyph_map, nop, &["c", "z"]);

        let full = compute_closure(&gsub, &glyph_map, &["a", "b", "c", "z"]);
        assert_closure_result!(glyph_map, full, &["a", "b", "c", "z", "A", "B"]);
    }

    #[test]
    fn cyclical_context() {
        let gsub = get_gsub(test_data::CYCLIC_CONTEXTUAL);
        let glyph_map = GlyphMap::new(test_data::RECURSIVE_CONTEXTUAL_GLYPHS);
        // we mostly care that this terminates
        let nop = compute_closure(&gsub, &glyph_map, &["a", "b", "c"]);
        assert_closure_result!(glyph_map, nop, &["a", "b", "c"]);
    }

    #[test]
    fn collect_all_features() {
        let font = FontRef::new(font_test_data::closure::CONTEXTUAL).unwrap();
        let gsub = font.gsub().unwrap();
        let ret = gsub
            .collect_features(&IntSet::all(), &IntSet::all(), &IntSet::all())
            .unwrap();
        assert_eq!(ret.len(), 2);
        assert!(ret.contains(0));
        assert!(ret.contains(1));
    }

    #[test]
    fn collect_all_features_with_feature_filter() {
        let font = FontRef::new(font_test_data::closure::CONTEXTUAL).unwrap();
        let gsub = font.gsub().unwrap();

        let mut feature_tags = IntSet::empty();
        feature_tags.insert(Tag::new(b"SUB5"));

        let ret = gsub
            .collect_features(&IntSet::all(), &IntSet::all(), &feature_tags)
            .unwrap();
        assert_eq!(ret.len(), 1);
        assert!(ret.contains(0));
    }

    #[test]
    fn collect_all_features_with_script_filter() {
        let font = FontRef::new(font_test_data::closure::CONTEXTUAL).unwrap();
        let gsub = font.gsub().unwrap();

        let mut script_tags = IntSet::empty();
        script_tags.insert(Tag::new(b"LATN"));

        let ret = gsub
            .collect_features(&script_tags, &IntSet::all(), &IntSet::all())
            .unwrap();
        assert!(ret.is_empty());
    }
}

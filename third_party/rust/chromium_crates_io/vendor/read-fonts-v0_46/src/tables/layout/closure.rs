//! Support Layout Closure

use types::{BigEndian, Compatible, GlyphId16, GlyphId24, Offset16};

use super::{
    ArrayOfOffsets, ChainedClassSequenceRule, ChainedClassSequenceRuleSet,
    ChainedClassSequenceRuleSet2, ChainedSequenceContext, ChainedSequenceContextFormat1,
    ChainedSequenceContextFormat2, ChainedSequenceContextFormat3, ChainedSequenceContextFormat4,
    ChainedSequenceContextFormat5, ChainedSequenceRule, ChainedSequenceRule2,
    ChainedSequenceRuleSet, ChainedSequenceRuleSet2, ClassDef, ClassDefFormat1, ClassDefFormat2,
    ClassSequenceRule, ClassSequenceRuleSet, ClassSequenceRuleSet2, CoverageTable, ExtensionLookup,
    Feature, FeatureList, FeatureVariations, GlyphId, LangSys, ReadError, Script, ScriptList,
    SequenceContext, SequenceContextFormat1, SequenceContextFormat2, SequenceContextFormat3,
    SequenceContextFormat4, SequenceContextFormat5, SequenceContextFormat6, SequenceLookupRecord,
    SequenceRule, SequenceRule2, SequenceRuleSet, SequenceRuleSet2, Subtables, Tag,
};
use crate::{
    collections::{FastHashMap, IntSet},
    tables::{gpos::PositionLookupList, gsub::SubstitutionLookupList},
    FontRead, MinByteRange,
};

use crate::limits::{
    MAX_FEATURE_INDICES, MAX_LANGSYS, MAX_LOOKUP_VISIT_COUNT, MAX_RECURSION_DEPTH, MAX_SCRIPTS,
};

struct CollectFeaturesContext<'a> {
    script_count: u16,
    langsys_count: u16,
    feature_index_count: u16,
    visited_script: IntSet<u32>,
    visited_langsys: IntSet<u32>,
    feature_indices: &'a mut IntSet<u16>,
    feature_indices_filter: IntSet<u16>,
    table_head: usize,
}

impl<'a> CollectFeaturesContext<'a> {
    pub(crate) fn new(
        features: &IntSet<Tag>,
        table_head: usize,
        feature_list: &'a FeatureList<'a>,
        feature_indices: &'a mut IntSet<u16>,
    ) -> Self {
        Self {
            script_count: 0,
            langsys_count: 0,
            feature_index_count: 0,
            visited_script: IntSet::empty(),
            visited_langsys: IntSet::empty(),
            feature_indices,
            feature_indices_filter: feature_list
                .feature_records()
                .iter()
                .enumerate()
                .filter(|(_i, record)| features.contains(record.feature_tag()))
                .map(|(idx, _)| idx as u16)
                .collect(),
            table_head,
        }
    }

    /// Return true if the script limit has been exceeded or the script is visited before
    pub(crate) fn script_visited(&mut self, s: &Script) -> bool {
        if self.script_count > MAX_SCRIPTS {
            return true;
        }

        self.script_count += 1;

        let delta = (s.offset_data().as_bytes().as_ptr() as usize - self.table_head) as u32;
        !self.visited_script.insert(delta)
    }

    /// Return true if the Langsys limit has been exceeded or the Langsys is visited before
    pub(crate) fn langsys_visited(&mut self, langsys: &LangSys) -> bool {
        if self.langsys_count > MAX_LANGSYS {
            return true;
        }

        self.langsys_count += 1;

        let delta = (langsys.offset_data().as_bytes().as_ptr() as usize - self.table_head) as u32;
        !self.visited_langsys.insert(delta)
    }

    /// Returns true if the feature limit has been exceeded
    pub(crate) fn feature_indices_limit_exceeded(&mut self, count: u16) -> bool {
        let (new_count, overflow) = self.feature_index_count.overflowing_add(count);
        if overflow {
            self.feature_index_count = MAX_FEATURE_INDICES;
            return true;
        }
        self.feature_index_count = new_count;
        new_count > MAX_FEATURE_INDICES
    }
}

impl ScriptList<'_> {
    /// Return a set of all feature indices underneath the specified scripts, languages and features
    pub(crate) fn collect_features(
        &self,
        layout_table_head: usize,
        feature_list: &FeatureList,
        scripts: &IntSet<Tag>,
        languages: &IntSet<Tag>,
        features: &IntSet<Tag>,
    ) -> Result<IntSet<u16>, ReadError> {
        let mut out = IntSet::empty();
        let mut c =
            CollectFeaturesContext::new(features, layout_table_head, feature_list, &mut out);
        let script_records = self.script_records();
        let font_data = self.offset_data();
        if scripts.is_inverted() {
            for record in script_records {
                let tag = record.script_tag();
                if !scripts.contains(tag) || record.script_offset().is_null() {
                    continue;
                }
                let script = record.script(font_data)?;
                script.collect_features(&mut c, languages)?;
            }
        } else {
            for idx in scripts.iter().filter_map(|tag| self.index_for_tag(tag)) {
                let record = script_records[idx as usize];
                if record.script_offset().is_null() {
                    continue;
                }
                let script = record.script(font_data)?;
                script.collect_features(&mut c, languages)?;
            }
        }
        Ok(out)
    }
}

impl Script<'_> {
    fn collect_features(
        &self,
        c: &mut CollectFeaturesContext,
        languages: &IntSet<Tag>,
    ) -> Result<(), ReadError> {
        if c.script_visited(self) {
            return Ok(());
        }

        let lang_sys_records = self.lang_sys_records();
        let font_data = self.offset_data();

        if let Some(default_lang_sys) = self.default_lang_sys().transpose()? {
            default_lang_sys.collect_features(c);
        }

        if languages.is_inverted() {
            for record in lang_sys_records {
                let tag = record.lang_sys_tag();
                if !languages.contains(tag) || record.lang_sys_offset().is_null() {
                    continue;
                }
                let lang_sys = record.lang_sys(font_data)?;
                lang_sys.collect_features(c);
            }
        } else {
            for idx in languages
                .iter()
                .filter_map(|tag| self.lang_sys_index_for_tag(tag))
            {
                let record = lang_sys_records[idx as usize];
                if record.lang_sys_offset().is_null() {
                    continue;
                }
                let lang_sys = record.lang_sys(font_data)?;
                lang_sys.collect_features(c);
            }
        }
        Ok(())
    }
}

impl LangSys<'_> {
    fn collect_features(&self, c: &mut CollectFeaturesContext) {
        if c.langsys_visited(self) {
            return;
        }

        if c.feature_indices_filter.is_empty() {
            return;
        }

        let required_feature_idx = self.required_feature_index();
        if required_feature_idx != 0xFFFF
            && !c.feature_indices_limit_exceeded(1)
            && c.feature_indices_filter.contains(required_feature_idx)
        {
            c.feature_indices.insert(required_feature_idx);
        }

        if c.feature_indices_limit_exceeded(self.feature_index_count()) {
            return;
        }

        for feature_index in self.feature_indices() {
            let idx = feature_index.get();
            if !c.feature_indices_filter.contains(idx) {
                continue;
            }
            c.feature_indices.insert(idx);
            c.feature_indices_filter.remove(idx);
        }
    }
}

impl Feature<'_> {
    pub(crate) fn collect_lookups(&self) -> Vec<u16> {
        self.lookup_list_indices()
            .iter()
            .map(|idx| idx.get())
            .collect()
    }
}

impl FeatureList<'_> {
    pub(crate) fn collect_lookups(
        &self,
        feature_indices: &IntSet<u16>,
    ) -> Result<IntSet<u16>, ReadError> {
        let features_records = self.feature_records();
        let num_features = self.feature_count();
        let font_data = self.offset_data();
        let mut lookup_idxes = IntSet::empty();

        if feature_indices.is_inverted() {
            for feature_rec in (0..num_features).filter_map(|i| {
                feature_indices
                    .contains(i)
                    .then(|| features_records.get(i as usize))
                    .flatten()
            }) {
                if feature_rec.feature_offset().is_null() {
                    continue;
                }
                lookup_idxes.extend_unsorted(feature_rec.feature(font_data)?.collect_lookups());
            }
        } else {
            for feature_rec in feature_indices
                .iter()
                .filter_map(|i| features_records.get(i as usize))
            {
                if feature_rec.feature_offset().is_null() {
                    continue;
                }
                lookup_idxes.extend_unsorted(feature_rec.feature(font_data)?.collect_lookups());
            }
        }
        Ok(lookup_idxes)
    }
}

impl FeatureVariations<'_> {
    pub(crate) fn collect_lookups(
        &self,
        feature_indices: &IntSet<u16>,
    ) -> Result<IntSet<u16>, ReadError> {
        let mut out = IntSet::empty();
        if u64::from(self.feature_variation_record_count()) * 8
            > self.offset_data().len().saturating_sub(8) as u64
        {
            return Err(ReadError::OutOfBounds);
        }

        for variation_rec in self.feature_variation_records() {
            let Some(subs) = variation_rec
                .feature_table_substitution(self.offset_data())
                .transpose()?
            else {
                continue;
            };

            for sub_record in subs
                .substitutions()
                .iter()
                .filter(|sub_rec| feature_indices.contains(sub_rec.feature_index()))
            {
                if sub_record.alternate_feature_offset().is_null() {
                    continue;
                }
                let sub_f = sub_record.alternate_feature(subs.offset_data())?;
                out.extend_unsorted(sub_f.lookup_list_indices().iter().map(|i| i.get()));
            }
        }
        if self.version().compatible((1, 1)) {
            let records = self
                .lookup_variation_records()
                .ok_or(ReadError::OutOfBounds)?;
            for record in records
                .iter()
                .filter(|rec| feature_indices.contains(rec.feature_index()))
            {
                if record.feature_lookups_offset().is_null() {
                    continue;
                }
                let lookups = record.feature_lookups(self.offset_data())?;
                if u64::from(lookups.lookup_condition_count()) * 8
                    > lookups.offset_data().len().saturating_sub(10) as u64
                {
                    return Err(ReadError::OutOfBounds);
                }
                for record in lookups.lookup_condition_records() {
                    if record.lookup_index_list_offset().is_null() {
                        continue;
                    }
                    let indices = record.lookup_index_list(lookups.offset_data())?;
                    if indices.min_table_bytes().is_empty() {
                        return Err(ReadError::OutOfBounds);
                    }
                    // Closure includes every design-space branch; do not evaluate conditions.
                    out.extend_unsorted(indices.lookup_indices().iter().map(|i| i.get()));
                }
            }
        }
        Ok(out)
    }
}

pub(crate) enum LayoutLookupList<'a> {
    Gsub(&'a SubstitutionLookupList<'a>),
    Gpos(&'a PositionLookupList<'a>),
}

pub(crate) struct LookupClosureCtx<'a> {
    visited_lookups: IntSet<u16>,
    inactive_lookups: IntSet<u16>,
    glyph_set: &'a IntSet<GlyphId>,
    lookup_count: u16,
    nesting_level_left: usize,
    lookup_list: &'a LayoutLookupList<'a>,
}

impl<'a> LookupClosureCtx<'a> {
    pub(crate) fn new(glyph_set: &'a IntSet<GlyphId>, lookup_list: &'a LayoutLookupList) -> Self {
        Self {
            visited_lookups: IntSet::empty(),
            inactive_lookups: IntSet::empty(),
            glyph_set,
            lookup_count: 0,
            nesting_level_left: MAX_RECURSION_DEPTH,
            lookup_list,
        }
    }

    pub(crate) fn visited_lookups(&self) -> &IntSet<u16> {
        &self.visited_lookups
    }

    pub(crate) fn inactive_lookups(&self) -> &IntSet<u16> {
        &self.inactive_lookups
    }

    pub(crate) fn glyphs(&self) -> &IntSet<GlyphId> {
        self.glyph_set
    }

    pub(crate) fn set_lookup_inactive(&mut self, lookup_index: u16) {
        self.inactive_lookups.insert(lookup_index);
    }

    pub(crate) fn lookup_limit_exceed(&self) -> bool {
        self.lookup_count > MAX_LOOKUP_VISIT_COUNT
    }

    // return false if lookup limit exceeded or lookup visited,and visited set is not modified
    // Otherwise return true and insert lookup index into the visited set
    pub(crate) fn should_visit_lookup(&mut self, lookup_index: u16) -> bool {
        if self.lookup_count > MAX_LOOKUP_VISIT_COUNT {
            return false;
        }
        self.lookup_count += 1;
        self.visited_lookups.insert(lookup_index)
    }

    pub(crate) fn recurse(&mut self, lookup_index: u16) -> Result<(), ReadError> {
        if self.nesting_level_left == 0 {
            return Ok(());
        }

        if self.lookup_limit_exceed() || self.visited_lookups.contains(lookup_index) {
            return Ok(());
        }

        self.nesting_level_left -= 1;
        match self.lookup_list {
            LayoutLookupList::Gpos(lookuplist) => {
                match lookuplist.lookups().get(lookup_index as usize) {
                    Err(ReadError::NullOffset) | Err(ReadError::InvalidCollectionIndex(_)) => (),
                    lookup => lookup?.closure_lookups(self, lookup_index)?,
                }
            }
            LayoutLookupList::Gsub(lookuplist) => {
                match lookuplist.lookups().get(lookup_index as usize) {
                    Err(ReadError::NullOffset) | Err(ReadError::InvalidCollectionIndex(_)) => (),
                    lookup => lookup?.closure_lookups(self, lookup_index)?,
                }
            }
        }
        self.nesting_level_left += 1;
        Ok(())
    }
}

/// Compute the transitive closure of lookups
pub(crate) trait LookupClosure {
    fn closure_lookups(&self, _c: &mut LookupClosureCtx, _arg: u16) -> Result<(), ReadError> {
        Ok(())
    }
}

pub trait Intersect {
    fn intersects(&self, glyph_set: &IntSet<GlyphId>) -> Result<bool, ReadError>;
}

impl Intersect for ClassDef<'_> {
    fn intersects(&self, glyph_set: &IntSet<GlyphId>) -> Result<bool, ReadError> {
        match self {
            ClassDef::Format1(table) => table.intersects(glyph_set),
            ClassDef::Format2(table) => table.intersects(glyph_set),
            _ => Ok(self
                .intersect_classes(glyph_set)
                .iter()
                .any(|class| class != 0)),
        }
    }
}

impl Intersect for ClassDefFormat1<'_> {
    fn intersects(&self, glyph_set: &IntSet<GlyphId>) -> Result<bool, ReadError> {
        let class_values = self.class_value_array();
        if class_values.is_empty() {
            return Ok(false);
        }

        let start = self.start_glyph_id().to_u32();
        let end = start + self.glyph_count() as u32;

        let mut start_glyph = GlyphId::from(start);
        if glyph_set.contains(start_glyph) && class_values[0] != 0 {
            return Ok(true);
        }

        while let Some(g) = glyph_set.iter_after(start_glyph).next() {
            let g = g.to_u32();
            if g >= end {
                break;
            }
            let Some(class) = class_values.get((g - start) as usize) else {
                break;
            };
            if class.get() != 0 {
                return Ok(true);
            }
            start_glyph = GlyphId::from(g);
        }
        Ok(false)
    }
}

impl Intersect for ClassDefFormat2<'_> {
    fn intersects(&self, glyph_set: &IntSet<GlyphId>) -> Result<bool, ReadError> {
        let num_ranges = self.class_range_count();
        let num_bits = 16 - num_ranges.leading_zeros();
        if num_ranges as u64 > glyph_set.len() * num_bits as u64 {
            for g in glyph_set.iter() {
                if self.get(g) != 0 {
                    return Ok(true);
                }
            }
        } else {
            for record in self.class_range_records() {
                let first = GlyphId::from(record.start_glyph_id());
                let last = GlyphId::from(record.end_glyph_id());
                if glyph_set.intersects_range(first..=last) && record.class() != 0 {
                    return Ok(true);
                }
            }
        }
        Ok(false)
    }
}

impl<'a, T, Ext> LookupClosure for Subtables<'a, T, Ext>
where
    T: LookupClosure + Intersect + FontRead<'a, Args = ()> + 'a,
    Ext: ExtensionLookup<'a, T> + 'a,
{
    fn closure_lookups(&self, c: &mut LookupClosureCtx, arg: u16) -> Result<(), ReadError> {
        for t in self.iter().filter_map(|table| match table {
            Err(ReadError::NullOffset) => None,
            other => Some(other),
        }) {
            t?.closure_lookups(c, arg)?;
        }
        Ok(())
    }
}

impl<'a, T> Intersect for ArrayOfOffsets<'a, T, Offset16>
where
    T: Intersect + FontRead<'a, Args = ()> + 'a,
{
    fn intersects(&self, glyph_set: &IntSet<GlyphId>) -> Result<bool, ReadError> {
        for t in self.iter().filter_map(|table| match table {
            Err(ReadError::NullOffset) => None,
            other => Some(other),
        }) {
            if t?.intersects(glyph_set)? {
                return Ok(true);
            }
        }
        Ok(false)
    }
}

// these are basically the same; but we need to jump through some hoops
// to get the fields to line up
pub(crate) enum ContextFormat1<'a> {
    Plain(SequenceContextFormat1<'a>),
    Chain(ChainedSequenceContextFormat1<'a>),
    PlainWide(SequenceContextFormat4<'a>),
    ChainWide(ChainedSequenceContextFormat4<'a>),
}

pub(crate) enum Format1RuleSet<'a> {
    Plain(SequenceRuleSet<'a>),
    Chain(ChainedSequenceRuleSet<'a>),
    PlainWide(SequenceRuleSet2<'a>),
    ChainWide(ChainedSequenceRuleSet2<'a>),
}

pub(crate) enum Format1Rule<'a> {
    Plain(SequenceRule<'a>),
    Chain(ChainedSequenceRule<'a>),
    PlainWide(SequenceRule2<'a>),
    ChainWide(ChainedSequenceRule2<'a>),
}

impl<'a> ContextFormat1<'a> {
    pub(crate) fn coverage(&self) -> Option<Result<CoverageTable<'_>, ReadError>> {
        match self {
            ContextFormat1::Plain(table) if !table.coverage_offset().is_null() => {
                Some(table.coverage())
            }
            ContextFormat1::Chain(table) if !table.coverage_offset().is_null() => {
                Some(table.coverage())
            }
            ContextFormat1::PlainWide(table) if !table.coverage_offset().is_null() => {
                Some(table.coverage())
            }
            ContextFormat1::ChainWide(table) if !table.coverage_offset().is_null() => {
                Some(table.coverage())
            }
            _ => None,
        }
    }

    pub(crate) fn rule_sets(
        &self,
    ) -> impl Iterator<Item = Option<Result<Format1RuleSet<'a>, ReadError>>> + '_ {
        let count = match self {
            Self::Plain(table) => table.seq_rule_sets().len(),
            Self::Chain(table) => table.chained_seq_rule_sets().len(),
            Self::PlainWide(table) => table.seq_rule_sets().len(),
            Self::ChainWide(table) => table.chained_seq_rule_sets().len(),
        };
        (0..count).map(move |index| match self {
            Self::Plain(table) => table
                .seq_rule_sets()
                .get(index)
                .map(|rs| rs.map(Format1RuleSet::Plain)),
            Self::Chain(table) => table
                .chained_seq_rule_sets()
                .get(index)
                .map(|rs| rs.map(Format1RuleSet::Chain)),
            Self::PlainWide(table) => table
                .seq_rule_sets()
                .get(index)
                .map(|rs| rs.map(Format1RuleSet::PlainWide)),
            Self::ChainWide(table) => table
                .chained_seq_rule_sets()
                .get(index)
                .map(|rs| rs.map(Format1RuleSet::ChainWide)),
        })
    }
}

impl<'a> Format1RuleSet<'a> {
    pub(crate) fn rules(
        &self,
    ) -> impl Iterator<Item = Option<Result<Format1Rule<'a>, ReadError>>> + '_ {
        let count = match self {
            Self::Plain(table) => table.seq_rules().len(),
            Self::Chain(table) => table.chained_seq_rules().len(),
            Self::PlainWide(table) => table.seq_rules().len(),
            Self::ChainWide(table) => table.chained_seq_rules().len(),
        };
        (0..count).map(move |index| {
            let rule = match self {
                Self::Plain(table) => table.seq_rules().get(index).map(Format1Rule::Plain),
                Self::Chain(table) => table.chained_seq_rules().get(index).map(Format1Rule::Chain),
                Self::PlainWide(table) => table.seq_rules().get(index).map(Format1Rule::PlainWide),
                Self::ChainWide(table) => table
                    .chained_seq_rules()
                    .get(index)
                    .map(Format1Rule::ChainWide),
            };
            match rule {
                Err(ReadError::NullOffset) => None,
                other => Some(other),
            }
        })
    }
}

impl Format1Rule<'_> {
    pub(crate) fn input_count(&self) -> usize {
        match self {
            Self::Plain(table) => table.input_sequence().len() + 1,
            Self::Chain(table) => table.input_sequence().len() + 1,
            Self::PlainWide(table) => table.input_sequence().len() + 1,
            Self::ChainWide(table) => table.input_sequence().len() + 1,
        }
    }

    pub(crate) fn input_glyph(&self, index: usize) -> Option<GlyphId> {
        match self {
            Self::Plain(table) => table.input_sequence().get(index).map(|g| g.get().into()),
            Self::Chain(table) => table.input_sequence().get(index).map(|g| g.get().into()),
            Self::PlainWide(table) => table.input_sequence().get(index).map(|g| g.get().into()),
            Self::ChainWide(table) => table.input_sequence().get(index).map(|g| g.get().into()),
        }
    }

    pub(crate) fn lookup_records(&self) -> &[SequenceLookupRecord] {
        match self {
            Self::Plain(table) => table.seq_lookup_records(),
            Self::Chain(table) => table.seq_lookup_records(),
            Self::PlainWide(table) => table.seq_lookup_records(),
            Self::ChainWide(table) => table.seq_lookup_records(),
        }
    }
}

impl Intersect for &[BigEndian<GlyphId16>] {
    fn intersects(&self, glyph_set: &IntSet<GlyphId>) -> Result<bool, ReadError> {
        Ok(self
            .iter()
            .all(|g| glyph_set.contains(GlyphId::from(g.get()))))
    }
}

impl Intersect for &[BigEndian<GlyphId24>] {
    fn intersects(&self, glyph_set: &IntSet<GlyphId>) -> Result<bool, ReadError> {
        Ok(self
            .iter()
            .all(|g| glyph_set.contains(GlyphId::from(g.get()))))
    }
}

impl Intersect for Format1Rule<'_> {
    fn intersects(&self, glyph_set: &IntSet<GlyphId>) -> Result<bool, ReadError> {
        match self {
            Self::Plain(table) => table.input_sequence().intersects(glyph_set),
            Self::Chain(table) => Ok(table.backtrack_sequence().intersects(glyph_set)?
                && table.input_sequence().intersects(glyph_set)?
                && table.lookahead_sequence().intersects(glyph_set)?),
            Self::PlainWide(table) => table.input_sequence().intersects(glyph_set),
            Self::ChainWide(table) => Ok(table.backtrack_sequence().intersects(glyph_set)?
                && table.input_sequence().intersects(glyph_set)?
                && table.lookahead_sequence().intersects(glyph_set)?),
        }
    }
}

impl LookupClosure for Format1Rule<'_> {
    fn closure_lookups(&self, c: &mut LookupClosureCtx, _arg: u16) -> Result<(), ReadError> {
        if c.lookup_limit_exceed() || !self.intersects(c.glyphs())? {
            return Ok(());
        }

        for lookup_record in self.lookup_records() {
            let index = lookup_record.lookup_list_index();
            c.recurse(index)?;
        }
        Ok(())
    }
}

pub(crate) enum ContextFormat2<'a> {
    Plain(SequenceContextFormat2<'a>),
    Chain(ChainedSequenceContextFormat2<'a>),
    PlainWide(SequenceContextFormat5<'a>),
    ChainWide(ChainedSequenceContextFormat5<'a>),
}

pub(crate) enum Format2RuleSet<'a> {
    Plain(ClassSequenceRuleSet<'a>),
    Chain(ChainedClassSequenceRuleSet<'a>),
    PlainWide(ClassSequenceRuleSet2<'a>),
    ChainWide(ChainedClassSequenceRuleSet2<'a>),
}

pub(crate) enum Format2Rule<'a> {
    Plain(ClassSequenceRule<'a>),
    Chain(ChainedClassSequenceRule<'a>),
}

#[derive(Default)]
pub(crate) struct SeqCache {
    input: FastHashMap<u16, bool>,
    backtrack: FastHashMap<u16, bool>,
    lookahead: FastHashMap<u16, bool>,
}

impl<'a> ContextFormat2<'a> {
    pub(crate) fn coverage(&self) -> Option<Result<CoverageTable<'_>, ReadError>> {
        match self {
            ContextFormat2::Plain(table) if !table.coverage_offset().is_null() => {
                Some(table.coverage())
            }
            ContextFormat2::Chain(table) if !table.coverage_offset().is_null() => {
                Some(table.coverage())
            }
            Self::PlainWide(table) if !table.coverage_offset().is_null() => Some(table.coverage()),
            Self::ChainWide(table) if !table.coverage_offset().is_null() => Some(table.coverage()),
            _ => None,
        }
    }

    pub(crate) fn input_class_def(&self) -> Option<Result<ClassDef<'_>, ReadError>> {
        match self {
            ContextFormat2::Plain(table_ref) if !table_ref.class_def_offset().is_null() => {
                Some(table_ref.class_def())
            }
            ContextFormat2::Chain(table_ref) if !table_ref.input_class_def_offset().is_null() => {
                Some(table_ref.input_class_def())
            }
            Self::PlainWide(table) if !table.class_def_offset().is_null() => {
                Some(table.class_def())
            }
            Self::ChainWide(table) if !table.input_class_def_offset().is_null() => {
                Some(table.input_class_def())
            }
            _ => None,
        }
    }

    pub(crate) fn backtrack_class_def(&self) -> Option<Result<ClassDef<'_>, ReadError>> {
        match self {
            Self::Chain(table) if !table.backtrack_class_def_offset().is_null() => {
                Some(table.backtrack_class_def())
            }
            Self::ChainWide(table) if !table.backtrack_class_def_offset().is_null() => {
                Some(table.backtrack_class_def())
            }
            _ => None,
        }
    }

    pub(crate) fn lookahead_class_def(&self) -> Option<Result<ClassDef<'_>, ReadError>> {
        match self {
            Self::Chain(table) if !table.lookahead_class_def_offset().is_null() => {
                Some(table.lookahead_class_def())
            }
            Self::ChainWide(table) if !table.lookahead_class_def_offset().is_null() => {
                Some(table.lookahead_class_def())
            }
            _ => None,
        }
    }

    pub(crate) fn rule_sets(
        &self,
    ) -> impl Iterator<Item = Option<Result<Format2RuleSet<'a>, ReadError>>> + '_ {
        let count = match self {
            Self::Plain(table) => table.class_seq_rule_sets().len(),
            Self::Chain(table) => table.chained_class_seq_rule_sets().len(),
            Self::PlainWide(table) => table.class_seq_rule_sets().len(),
            Self::ChainWide(table) => table.chained_class_seq_rule_sets().len(),
        };
        (0..count).map(move |index| match self {
            Self::Plain(table) => table
                .class_seq_rule_sets()
                .get(index)
                .map(|rs| rs.map(Format2RuleSet::Plain)),
            Self::Chain(table) => table
                .chained_class_seq_rule_sets()
                .get(index)
                .map(|rs| rs.map(Format2RuleSet::Chain)),
            Self::PlainWide(table) => table
                .class_seq_rule_sets()
                .get(index)
                .map(|rs| rs.map(Format2RuleSet::PlainWide)),
            Self::ChainWide(table) => table
                .chained_class_seq_rule_sets()
                .get(index)
                .map(|rs| rs.map(Format2RuleSet::ChainWide)),
        })
    }
}

impl<'a> Format2RuleSet<'a> {
    pub(crate) fn rules(
        &self,
    ) -> impl Iterator<Item = Option<Result<Format2Rule<'a>, ReadError>>> + '_ {
        let count = match self {
            Self::Plain(table) => table.class_seq_rules().len(),
            Self::Chain(table) => table.chained_class_seq_rules().len(),
            Self::PlainWide(table) => table.class_seq_rules().len(),
            Self::ChainWide(table) => table.chained_class_seq_rules().len(),
        };
        (0..count).map(move |index| {
            let rule = match self {
                Self::Plain(table) => table.class_seq_rules().get(index).map(Format2Rule::Plain),
                Self::Chain(table) => table
                    .chained_class_seq_rules()
                    .get(index)
                    .map(Format2Rule::Chain),
                Self::PlainWide(table) => {
                    table.class_seq_rules().get(index).map(Format2Rule::Plain)
                }
                Self::ChainWide(table) => table
                    .chained_class_seq_rules()
                    .get(index)
                    .map(Format2Rule::Chain),
            };
            match rule {
                Err(ReadError::NullOffset) => None,
                other => Some(other),
            }
        })
    }
}

impl Format2Rule<'_> {
    pub(crate) fn input_sequence(&self) -> &[BigEndian<u16>] {
        match self {
            Self::Plain(table) => table.input_sequence(),
            Self::Chain(table) => table.input_sequence(),
        }
    }

    pub(crate) fn lookup_records(&self) -> &[SequenceLookupRecord] {
        match self {
            Self::Plain(table) => table.seq_lookup_records(),
            Self::Chain(table) => table.seq_lookup_records(),
        }
    }

    #[allow(clippy::too_many_arguments)]
    pub(crate) fn intersects(
        &self,
        glyphs: &IntSet<GlyphId>,
        input_class_def: &ClassDef,
        backtrack_class_def: Option<&ClassDef>,
        lookahead_class_def: Option<&ClassDef>,
        seq_cache: &mut SeqCache,
    ) -> bool {
        match self {
            Self::Plain(table) => table.intersects(glyphs, input_class_def, &mut seq_cache.input),
            Self::Chain(table) => table.intersects(
                glyphs,
                input_class_def,
                backtrack_class_def,
                lookahead_class_def,
                seq_cache,
            ),
        }
    }
}

fn intersects_class(
    class_def: &ClassDef,
    glyphs: &IntSet<GlyphId>,
    class: u16,
    cache: &mut FastHashMap<u16, bool>,
) -> bool {
    *cache
        .entry(class)
        .or_insert_with(|| class_def.intersects_class_glyphs(glyphs, u32::from(class)))
}
impl ClassSequenceRule<'_> {
    fn intersects(
        &self,
        glyphs: &IntSet<GlyphId>,
        input_class_def: &ClassDef,
        cache: &mut FastHashMap<u16, bool>,
    ) -> bool {
        self.input_sequence()
            .iter()
            .all(|c| intersects_class(input_class_def, glyphs, c.get(), cache))
    }
}

impl ChainedClassSequenceRule<'_> {
    #[allow(clippy::too_many_arguments)]
    fn intersects(
        &self,
        glyphs: &IntSet<GlyphId>,
        input_class_def: &ClassDef,
        backtrack_class_def: Option<&ClassDef>,
        lookahead_class_def: Option<&ClassDef>,
        seq_cache: &mut SeqCache,
    ) -> bool {
        if !self
            .input_sequence()
            .iter()
            .all(|c| intersects_class(input_class_def, glyphs, c.get(), &mut seq_cache.input))
        {
            return false;
        }

        if let Some(backtrack_class_def) = backtrack_class_def {
            if !self.backtrack_sequence().iter().all(|c| {
                intersects_class(
                    backtrack_class_def,
                    glyphs,
                    c.get(),
                    &mut seq_cache.backtrack,
                )
            }) {
                return false;
            }
        } else if self.backtrack_glyph_count() != 0 {
            return false;
        }

        if let Some(lookahead_class_def) = lookahead_class_def {
            if !self.lookahead_sequence().iter().all(|c| {
                intersects_class(
                    lookahead_class_def,
                    glyphs,
                    c.get(),
                    &mut seq_cache.lookahead,
                )
            }) {
                return false;
            }
        } else if self.lookahead_glyph_count() != 0 {
            return false;
        }
        true
    }
}

pub(crate) enum ContextFormat3<'a> {
    Plain(SequenceContextFormat3<'a>),
    Chain(ChainedSequenceContextFormat3<'a>),
    PlainWide(SequenceContextFormat6<'a>),
}

impl ContextFormat3<'_> {
    pub(crate) fn input_count(&self) -> usize {
        match self {
            Self::Plain(table) => table.coverages().len(),
            Self::Chain(table) => table.input_coverages().len(),
            Self::PlainWide(table) => table.coverages().len(),
        }
    }

    pub(crate) fn coverage(&self, index: usize) -> Result<CoverageTable<'_>, ReadError> {
        match self {
            Self::Plain(table) => table.coverages().get(index),
            Self::Chain(table) => table.input_coverages().get(index),
            Self::PlainWide(table) => table.coverages().get(index),
        }
    }

    pub(crate) fn lookup_records(&self) -> &[SequenceLookupRecord] {
        match self {
            ContextFormat3::Plain(table) => table.seq_lookup_records(),
            ContextFormat3::Chain(table) => table.seq_lookup_records(),
            ContextFormat3::PlainWide(table) => table.seq_lookup_records(),
        }
    }

    pub(crate) fn matches_glyphs(&self, glyphs: &IntSet<GlyphId>) -> Result<bool, ReadError> {
        for index in 0..self.input_count() {
            let coverage = match self.coverage(index) {
                Err(ReadError::NullOffset) => return Ok(false),
                other => other?,
            };
            if !coverage.intersects(glyphs) {
                return Ok(false);
            }
        }
        let (backtrack, lookahead) = match self {
            Self::Plain(_) | Self::PlainWide(_) => (None, None),
            Self::Chain(table) => (
                Some(table.backtrack_coverages()),
                Some(table.lookahead_coverages()),
            ),
        };

        for coverage in backtrack
            .into_iter()
            .flat_map(|x| x.iter_as_nullable())
            .chain(lookahead.into_iter().flat_map(|x| x.iter_as_nullable()))
        {
            let Some(coverage) = coverage.transpose()? else {
                return Ok(false);
            };
            if !coverage.intersects(glyphs) {
                return Ok(false);
            }
        }
        Ok(true)
    }
}

impl Intersect for ContextFormat1<'_> {
    fn intersects(&self, glyph_set: &IntSet<GlyphId>) -> Result<bool, ReadError> {
        let Some(coverage) = self.coverage().transpose()? else {
            return Ok(false);
        };
        for rule_set in coverage
            .iter()
            .zip(self.rule_sets())
            .filter_map(|(g, rule_set)| rule_set.filter(|_| glyph_set.contains(g)))
        {
            for rule in rule_set?.rules() {
                let Some(rule) = rule.transpose()? else {
                    continue;
                };
                if rule.intersects(glyph_set)? {
                    return Ok(true);
                }
            }
        }
        Ok(false)
    }
}

impl LookupClosure for ContextFormat1<'_> {
    fn closure_lookups(&self, c: &mut LookupClosureCtx, arg: u16) -> Result<(), ReadError> {
        let Some(coverage) = self.coverage().transpose()? else {
            return Ok(());
        };

        for (g, rule_set) in coverage
            .iter()
            .zip(self.rule_sets())
            .filter_map(|(g, rule_set)| rule_set.map(|rs| (g, rs)))
        {
            if !c.glyphs().contains(g) {
                continue;
            }
            if c.lookup_limit_exceed() {
                return Ok(());
            }
            for rule in rule_set?.rules() {
                let Some(rule) = rule.transpose()? else {
                    continue;
                };
                rule.closure_lookups(c, arg)?;
            }
        }

        Ok(())
    }
}

impl Intersect for ContextFormat2<'_> {
    fn intersects(&self, glyph_set: &IntSet<GlyphId>) -> Result<bool, ReadError> {
        let Some(coverage) = self.coverage().transpose()? else {
            return Ok(false);
        };
        if !coverage.intersects(glyph_set) {
            return Ok(false);
        }
        let retained_coverage_glyphs = coverage.intersect_set(glyph_set);

        let Some(input_class_def) = self.input_class_def().transpose()? else {
            return Ok(false);
        };

        let backtrack_class_def = self.backtrack_class_def().transpose()?;
        let lookahead_class_def = self.lookahead_class_def().transpose()?;

        let primary_classes = input_class_def.intersect_classes(&retained_coverage_glyphs);
        let mut seq_cache = SeqCache::default();
        for rule_set in self.rule_sets().enumerate().filter_map(|(c, rule_set)| {
            primary_classes
                .contains(c as u32)
                .then_some(rule_set)
                .flatten()
        }) {
            for rule in rule_set?.rules() {
                let Some(rule) = rule.transpose()? else {
                    continue;
                };
                if rule.intersects(
                    glyph_set,
                    &input_class_def,
                    backtrack_class_def.as_ref(),
                    lookahead_class_def.as_ref(),
                    &mut seq_cache,
                ) {
                    return Ok(true);
                }
            }
        }
        Ok(false)
    }
}

impl LookupClosure for ContextFormat2<'_> {
    fn closure_lookups(&self, c: &mut LookupClosureCtx, _arg: u16) -> Result<(), ReadError> {
        let Some(coverage) = self.coverage().transpose()? else {
            return Ok(());
        };
        if !coverage.intersects(c.glyphs()) {
            return Ok(());
        }
        let retained_coverage_glyphs = coverage.intersect_set(c.glyphs());
        let Some(input_class_def) = self.input_class_def().transpose()? else {
            return Ok(());
        };

        let backtrack_class_def = self.backtrack_class_def().transpose()?;
        let lookahead_class_def = self.lookahead_class_def().transpose()?;

        let primary_classes = input_class_def.intersect_classes(&retained_coverage_glyphs);
        let mut seq_cache = SeqCache::default();
        for rule_set in self.rule_sets().enumerate().filter_map(|(c, rule_set)| {
            primary_classes
                .contains(c as u32)
                .then_some(rule_set)
                .flatten()
        }) {
            if c.lookup_limit_exceed() {
                return Ok(());
            }

            for rule in rule_set?.rules() {
                if c.lookup_limit_exceed() {
                    return Ok(());
                }
                let Some(rule) = rule.transpose()? else {
                    continue;
                };

                if !rule.intersects(
                    c.glyphs(),
                    &input_class_def,
                    backtrack_class_def.as_ref(),
                    lookahead_class_def.as_ref(),
                    &mut seq_cache,
                ) {
                    continue;
                }

                for lookup_record in rule.lookup_records() {
                    let index = lookup_record.lookup_list_index();
                    c.recurse(index)?;
                }
            }
        }
        Ok(())
    }
}

impl Intersect for ContextFormat3<'_> {
    fn intersects(&self, glyph_set: &IntSet<GlyphId>) -> Result<bool, ReadError> {
        self.matches_glyphs(glyph_set)
    }
}

impl LookupClosure for ContextFormat3<'_> {
    fn closure_lookups(&self, c: &mut LookupClosureCtx, _arg: u16) -> Result<(), ReadError> {
        if !self.intersects(c.glyphs())? {
            return Ok(());
        }

        for lookup_record in self.lookup_records() {
            let index = lookup_record.lookup_list_index();
            c.recurse(index)?;
        }

        Ok(())
    }
}

impl Intersect for SequenceContext<'_> {
    fn intersects(&self, glyph_set: &IntSet<GlyphId>) -> Result<bool, ReadError> {
        match self {
            Self::Format1(table) => ContextFormat1::Plain(table.clone()).intersects(glyph_set),
            Self::Format2(table) => ContextFormat2::Plain(table.clone()).intersects(glyph_set),
            Self::Format5(table) => ContextFormat2::PlainWide(table.clone()).intersects(glyph_set),
            Self::Format3(table) => ContextFormat3::Plain(table.clone()).intersects(glyph_set),
            Self::Format4(table) => ContextFormat1::PlainWide(table.clone()).intersects(glyph_set),
            Self::Format6(table) => ContextFormat3::PlainWide(table.clone()).intersects(glyph_set),
        }
    }
}

impl LookupClosure for SequenceContext<'_> {
    fn closure_lookups(&self, c: &mut LookupClosureCtx, arg: u16) -> Result<(), ReadError> {
        match self {
            Self::Format1(table) => ContextFormat1::Plain(table.clone()).closure_lookups(c, arg),
            Self::Format2(table) => ContextFormat2::Plain(table.clone()).closure_lookups(c, arg),
            Self::Format5(table) => {
                ContextFormat2::PlainWide(table.clone()).closure_lookups(c, arg)
            }
            Self::Format3(table) => ContextFormat3::Plain(table.clone()).closure_lookups(c, arg),
            Self::Format4(table) => {
                ContextFormat1::PlainWide(table.clone()).closure_lookups(c, arg)
            }
            Self::Format6(table) => {
                ContextFormat3::PlainWide(table.clone()).closure_lookups(c, arg)
            }
        }
    }
}

impl Intersect for ChainedSequenceContext<'_> {
    fn intersects(&self, glyph_set: &IntSet<GlyphId>) -> Result<bool, ReadError> {
        match self {
            Self::Format1(table) => ContextFormat1::Chain(table.clone()).intersects(glyph_set),
            Self::Format2(table) => ContextFormat2::Chain(table.clone()).intersects(glyph_set),
            Self::Format5(table) => ContextFormat2::ChainWide(table.clone()).intersects(glyph_set),
            Self::Format3(table) => ContextFormat3::Chain(table.clone()).intersects(glyph_set),
            Self::Format4(table) => ContextFormat1::ChainWide(table.clone()).intersects(glyph_set),
        }
    }
}

impl LookupClosure for ChainedSequenceContext<'_> {
    fn closure_lookups(&self, c: &mut LookupClosureCtx, arg: u16) -> Result<(), ReadError> {
        match self {
            Self::Format1(table) => ContextFormat1::Chain(table.clone()).closure_lookups(c, arg),
            Self::Format2(table) => ContextFormat2::Chain(table.clone()).closure_lookups(c, arg),
            Self::Format5(table) => {
                ContextFormat2::ChainWide(table.clone()).closure_lookups(c, arg)
            }
            Self::Format3(table) => ContextFormat3::Chain(table.clone()).closure_lookups(c, arg),
            Self::Format4(table) => {
                ContextFormat1::ChainWide(table.clone()).closure_lookups(c, arg)
            }
        }
    }
}

#[cfg(test)]
mod lookup_variations_tests;

#[cfg(test)]
mod tests {
    use super::*;
    use crate::FontData;

    #[test]
    fn classdef_format1_short_read_no_panic() {
        // glyph_count is 5, but only one class value is present.
        let classdef = ClassDefFormat1::read(FontData::new(&[0, 1, 0, 10, 0, 5, 0, 1])).unwrap();
        let glyphs: IntSet<GlyphId> = [GlyphId::new(14)].into_iter().collect();

        assert!(!classdef.intersects(&glyphs).unwrap());
    }

    #[test]
    fn classdef_format2_does_not_alias_wide_glyphs() {
        let bytes = [
            0, 2, 0, 3, 0, 1, 0, 1, 0, 1, 0, 2, 0, 2, 0, 1, 0, 3, 0, 3, 0, 1,
        ];
        let classdef = ClassDefFormat2::read(FontData::new(&bytes)).unwrap();
        let glyphs: IntSet<GlyphId> = [GlyphId::new(65537)].into_iter().collect();
        assert!(!classdef.intersects(&glyphs).unwrap());
        assert!(!ClassDef::Format2(classdef).intersects(&glyphs).unwrap());
    }
}

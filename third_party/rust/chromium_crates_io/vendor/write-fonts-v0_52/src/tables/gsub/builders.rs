//! GSUB lookup builders

use std::{collections::BTreeMap, convert::TryFrom};

use types::{FixedSize, GlyphId16, Offset16};

use crate::{
    tables::{
        layout::{builders::Builder, CoverageTable},
        variations::ivs_builder::VariationStoreBuilder,
    },
    FontWrite,
};

/// Helper type for splitting layout subtables
// NOTE: currently just used for ligature substitution, but hopefully can be
// reused for other lookups as needed?
#[derive(Clone, Debug)]
struct TableSplitter<T: SplitTable> {
    finished: Vec<T>,
    current_coverage: Vec<GlyphId16>,
    current_items: Vec<T::Component>,
    current_size: usize,
}

/// A trait for splitting layout subtables.
trait SplitTable {
    /// The component item of this table.
    type Component;

    /// The (maximum) number of bytes required to store this item.
    ///
    /// This should include only the costs of the item, not the cost of adding
    /// to the coverage table.
    ///
    /// 'Maximum' because this does not account for possible size savings from
    /// deduplication of identical objects, for instance.
    fn size_for_item(item: &Self::Component) -> usize;
    /// The starting size of a new table that will contain this item.
    ///
    /// This does not include the size of the item itself! The item is provided
    /// because sometimes the table contains fields derived from its members.
    fn initial_size_for_item(item: &Self::Component) -> usize;
    fn instantiate(coverage: CoverageTable, items: Vec<Self::Component>) -> Self;
}

impl<T: SplitTable + FontWrite> TableSplitter<T> {
    const MAX_TABLE_SIZE: usize = u16::MAX as usize;

    fn new() -> Self {
        Self {
            finished: Vec::new(),
            current_coverage: Vec::new(),
            current_items: Vec::new(),
            current_size: 0,
        }
    }

    fn add(&mut self, gid: GlyphId16, item: T::Component) {
        let item_size = T::size_for_item(&item);
        if item_size + self.current_size > Self::MAX_TABLE_SIZE {
            let current_len = self.current_coverage.len();
            self.finish_current();
            let type_ = self.finished.last().unwrap().table_type();
            log::info!("adding split in {type_} at {current_len}");
        }

        if self.current_size == 0 {
            self.current_size = T::initial_size_for_item(&item);
        }
        self.current_coverage.push(gid);
        self.current_items.push(item);
        // item size + a glyph in the coverage table (worst case)
        self.current_size += item_size + GlyphId16::RAW_BYTE_LEN;
    }

    fn finish_current(&mut self) {
        if !self.current_coverage.is_empty() {
            let coverage = std::mem::take(&mut self.current_coverage).into();
            self.finished.push(T::instantiate(
                coverage,
                std::mem::take(&mut self.current_items),
            ));
            self.current_size = 0;
        }
    }

    fn finish(mut self) -> Vec<T> {
        self.finish_current();
        self.finished
    }
}
/// A builder for [`SingleSubst`](super::SingleSubst) subtables.
#[derive(Clone, Debug, Default)]
pub struct SingleSubBuilder {
    items: BTreeMap<GlyphId16, GlyphId16>,
}

impl SingleSubBuilder {
    /// Add this replacement to the builder.
    ///
    /// If there is an existing substitution for the provided target, it will
    /// be overwritten.
    pub fn insert(&mut self, target: GlyphId16, replacement: GlyphId16) {
        self.items.insert(target, replacement);
    }

    /// Returns `true` if all the pairs of items in the two iterators can be
    /// added to this lookup.
    ///
    /// The iterators are expected to be equal length.
    pub fn can_add(&self, target: GlyphId16, replacement: GlyphId16) -> bool {
        // only false if target exists with a different replacement
        !matches!(self.items.get(&target), Some(x) if *x != replacement)
    }

    /// Returns the number of rules in the builder.
    pub fn len(&self) -> usize {
        self.items.len()
    }

    /// Returns `true` if there are no rules in this builder.
    pub fn is_empty(&self) -> bool {
        self.items.is_empty()
    }

    /// Iterate all the substitution pairs in this builder.
    ///
    /// used when compiling the `aalt` feature.
    #[deprecated(since = "0.38.2", note = "use ::iter instead")]
    pub fn iter_pairs(&self) -> impl Iterator<Item = (GlyphId16, GlyphId16)> + '_ {
        self.iter()
    }

    /// Iterate all the substitution pairs in this builder.
    pub fn iter(&self) -> impl Iterator<Item = (GlyphId16, GlyphId16)> + '_ {
        self.items.iter().map(|(target, alt)| (*target, *alt))
    }

    /// Convert this `SingleSubBuilder` into a `MultipleSubBuilder`.
    ///
    /// This is used by the fea compiler in some cases to reduce the number
    /// of generated lookups.
    pub fn promote_to_multi_sub(self) -> MultipleSubBuilder {
        MultipleSubBuilder {
            items: self
                .items
                .into_iter()
                .map(|(key, gid)| (key, vec![gid]))
                .collect(),
        }
    }

    /// Convert this `SingleSubBuilder` into a `LigatureSubBuilder`.
    ///
    /// This is used by the fea compiler in some cases to reduce the number
    /// of generated lookups.
    pub fn promote_to_ligature_sub(self) -> LigatureSubBuilder {
        let mut items = BTreeMap::new();
        for (from, to) in self.items.into_iter() {
            items
                .entry(from)
                .or_insert(Vec::new())
                .push((Vec::new(), to));
        }
        LigatureSubBuilder { items }
    }
}

impl Builder for SingleSubBuilder {
    type Output = Vec<super::SingleSubst>;

    fn build(self, _: &mut VariationStoreBuilder) -> Self::Output {
        if self.items.is_empty() {
            return Default::default();
        }
        // if all pairs are equidistant and within the i16 range, find the
        // common delta
        let delta = self
            .items
            .iter()
            .map(|(k, v)| v.to_u16() as i32 - k.to_u16() as i32)
            .reduce(|acc, val| if acc == val { acc } else { i32::MAX })
            .and_then(|delta| i16::try_from(delta).ok());

        let coverage = self.items.keys().copied().collect();
        if let Some(delta) = delta {
            vec![super::SingleSubst::format_1(coverage, delta)]
        } else {
            let replacements = self.items.values().copied().collect();
            vec![super::SingleSubst::format_2(coverage, replacements)]
        }
    }
}

/// A builder for [`MultipleSubstFormat1`](super::MultipleSubstFormat1) subtables.
#[derive(Clone, Debug, Default)]
pub struct MultipleSubBuilder {
    items: BTreeMap<GlyphId16, Vec<GlyphId16>>,
}

impl Builder for MultipleSubBuilder {
    type Output = Vec<super::MultipleSubstFormat1>;

    fn build(self, _: &mut VariationStoreBuilder) -> Self::Output {
        let coverage = self.items.keys().copied().collect();
        let seq_tables = self.items.into_values().map(super::Sequence::new).collect();
        vec![super::MultipleSubstFormat1::new(coverage, seq_tables)]
    }
}

impl MultipleSubBuilder {
    /// Returns the number of rules in the builder.
    pub fn len(&self) -> usize {
        self.items.len()
    }

    /// Returns `true` if there are no rules in this builder.
    pub fn is_empty(&self) -> bool {
        self.items.is_empty()
    }

    /// Add a new substitution to this builder.
    ///
    /// If the target already exists with a different replacement, it will be
    /// overwritten.
    pub fn insert(&mut self, target: GlyphId16, replacement: Vec<GlyphId16>) {
        self.items.insert(target, replacement);
    }

    /// Returns `true` if no other replacement already exists for this target.
    pub fn can_add(&self, target: GlyphId16, replacement: &[GlyphId16]) -> bool {
        match self.items.get(&target) {
            None => true,
            Some(thing) => thing == replacement,
        }
    }

    /// Iterate over the rules in this builder.
    pub fn iter(&self) -> impl Iterator<Item = (&GlyphId16, &Vec<GlyphId16>)> {
        self.items.iter()
    }
}

/// A builder for [`AlternateSubstFormat1`](super::AlternateSubstFormat1) subtables
#[derive(Clone, Debug, Default)]
pub struct AlternateSubBuilder {
    items: BTreeMap<GlyphId16, Vec<GlyphId16>>,
}

impl AlternateSubBuilder {
    /// Returns the number of rules in the builder.
    pub fn len(&self) -> usize {
        self.items.len()
    }

    /// Returns `true` if there are no rules in this builder.
    pub fn is_empty(&self) -> bool {
        self.items.is_empty()
    }

    /// Add a new alternate sub rule to this lookup.
    pub fn insert(&mut self, target: GlyphId16, replacement: Vec<GlyphId16>) {
        self.items.insert(target, replacement);
    }

    /// Iterate over the rules in this builder.
    pub fn iter(&self) -> impl Iterator<Item = (&GlyphId16, &Vec<GlyphId16>)> {
        self.items.iter()
    }

    /// Iterate all alternates in this lookup.
    ///
    /// used when compiling aalt
    pub fn iter_pairs(&self) -> impl Iterator<Item = (GlyphId16, GlyphId16)> + '_ {
        self.items
            .iter()
            .flat_map(|(target, alt)| alt.iter().map(|alt| (*target, *alt)))
    }
}

impl Builder for AlternateSubBuilder {
    type Output = Vec<super::AlternateSubstFormat1>;

    fn build(self, _: &mut VariationStoreBuilder) -> Self::Output {
        let coverage = self.items.keys().copied().collect();
        let seq_tables = self
            .items
            .into_values()
            .map(super::AlternateSet::new)
            .collect();
        vec![super::AlternateSubstFormat1::new(coverage, seq_tables)]
    }
}

/// A builder for [`LigatureSubstFormat1`](super::LigatureSubstFormat1) subtables.
#[derive(Clone, Debug, Default)]
pub struct LigatureSubBuilder {
    items: BTreeMap<GlyphId16, Vec<(Vec<GlyphId16>, GlyphId16)>>,
}

impl LigatureSubBuilder {
    /// Returns the number of rules in the builder.
    pub fn len(&self) -> usize {
        self.items.len()
    }

    /// Returns `true` if there are no rules in this builder.
    pub fn is_empty(&self) -> bool {
        self.items.is_empty()
    }

    /// Add a new ligature substitution rule to the builder.
    pub fn insert(&mut self, target: Vec<GlyphId16>, replacement: GlyphId16) {
        let (first, rest) = target.split_first().unwrap();
        let entry = self.items.entry(*first).or_default();
        // skip duplicates
        if !entry
            .iter()
            .any(|existing| existing.0 == rest && existing.1 == replacement)
        {
            entry.push((rest.to_owned(), replacement))
        }
    }

    /// Check if this target sequence already has a replacement in this lookup.
    pub fn can_add(&self, target: &[GlyphId16], replacement: GlyphId16) -> bool {
        let Some((first, rest)) = target.split_first() else {
            return false;
        };
        match self.items.get(first) {
            Some(ligs) => !ligs
                .iter()
                .any(|(seq, target)| seq == rest && *target != replacement),
            None => true,
        }
    }

    /// Iterate over the current rules in the builder.
    ///
    /// The result is a tuple where the first item is the target glyph and the
    /// second item is a tuple of (components, replacement).
    pub fn iter(&self) -> impl Iterator<Item = (&GlyphId16, &Vec<(Vec<GlyphId16>, GlyphId16)>)> {
        self.items.iter()
    }
}

impl Builder for LigatureSubBuilder {
    type Output = Vec<super::LigatureSubstFormat1>;

    fn build(self, _: &mut VariationStoreBuilder) -> Self::Output {
        let mut splitter = TableSplitter::<super::LigatureSubstFormat1>::new();
        for (gid, mut ligs) in self.items.into_iter() {
            // we want to sort longer items first, but otherwise preserve
            // the order provided by the user.
            ligs.sort_by_key(|(lig, _)| std::cmp::Reverse(lig.len()));
            let lig_set = super::LigatureSet::new(
                ligs.into_iter()
                    .map(|(components, replacement)| super::Ligature::new(replacement, components))
                    .collect(),
            );
            splitter.add(gid, lig_set);
        }
        splitter.finish()
    }
}

impl SplitTable for super::LigatureSubstFormat1 {
    type Component = super::LigatureSet;

    fn size_for_item(item: &Self::Component) -> usize {
        item.compute_size()
    }

    fn initial_size_for_item(_item: &Self::Component) -> usize {
        // format, coverage offset, set count, sets offset
        u16::RAW_BYTE_LEN * 4
    }

    fn instantiate(coverage: CoverageTable, items: Vec<Self::Component>) -> Self {
        Self::new(coverage, items)
    }
}

impl super::LigatureSet {
    fn compute_size(&self) -> usize {
        // ligatureCount
        u16::RAW_BYTE_LEN
            // ligatureOffsets
            + Offset16::RAW_BYTE_LEN * self.ligatures.len()
            // size of each referenced ligature table
            + self
                .ligatures
                .iter()
                .map(|lig| lig.compute_size())
                .sum::<usize>()
    }
}

impl super::Ligature {
    fn compute_size(&self) -> usize {
        // ligatureGlyph
        u16::RAW_BYTE_LEN
            // componentCount
            + u16::RAW_BYTE_LEN
            // componentGlyphIDs
            + u16::RAW_BYTE_LEN * self.component_glyph_ids.len()
    }
}

#[cfg(test)]
mod tests {
    use crate::tables::gsub::LigatureSubstFormat1;

    use super::*;

    fn make_lig_table(n_bytes: u16, first: u16) -> super::super::Ligature {
        assert!(n_bytes >= 6, "minimum table size");
        assert!(n_bytes % 2 == 0, "can only generate even sizes: {n_bytes}");
        // 6 bytes per Ligature (header, including offset)
        let n_glyphs = (n_bytes - 6) / 2;
        let components = (first..=first + n_glyphs).map(GlyphId16::new).collect();
        super::super::Ligature::new(GlyphId16::new(first), components)
    }
    fn make_2048_bytes_of_ligature() -> super::super::LigatureSet {
        // 4 bytes for header
        // 2048 - 4 = 2044
        // 2044 / 2 = 1022
        let lig1 = make_lig_table(1022, 1);
        let lig2 = make_lig_table(1022, 3);
        super::super::LigatureSet::new(vec![lig1, lig2])
    }

    #[test]
    fn who_tests_the_testers1() {
        for size in [6, 12, 144, 2046, u16::MAX - 1] {
            let table = make_lig_table(size, 1);
            let bytes = crate::dump_table(&table).unwrap();
            assert_eq!(bytes.len(), size as usize);
        }
    }

    #[test]
    fn splitting_ligature_subs() {
        let mut splitter = TableSplitter::<LigatureSubstFormat1>::new();
        let ligset = make_2048_bytes_of_ligature();
        for gid in 0u16..31 {
            // in real packing these would be deduplicated but we can't now that here
            splitter.add(GlyphId16::new(gid), ligset.clone());
        }

        // 31 * 2048 < u16::MAX
        assert_eq!(splitter.clone().finish().len(), 1);
        splitter.add(GlyphId16::new(32), ligset);
        // 32 * 2048 < u16::MAX
        assert_eq!(splitter.finish().len(), 2)
    }
}

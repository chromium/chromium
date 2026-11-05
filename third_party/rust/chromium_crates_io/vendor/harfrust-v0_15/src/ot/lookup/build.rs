//! Reading a lookup and deriving the data its subtables apply with.

use super::coverage::Coverage;
use super::{context, gpos, gsub, CachedLookup, CachedSubtable, IntoKind, SubtableDigests};
use crate::budget::{Budget, Exhausted};
use crate::ot::apply::MappingCache;
use crate::set_digest::SetDigest;
use alloc::{boxed::Box, vec::Vec};
use read_fonts::model::Tables;
use read_fonts::tables::{
    gpos::{CursivePos, MarkBasePos, MarkLigPos, MarkMarkPos, PairPos, SinglePos},
    gsub::{
        AlternateSubst, ExtensionSubstFormat1, LigatureSubst, MultipleSubst,
        ReverseChainSingleSubst, SingleSubst,
    },
    layout::{
        ChainedSequenceContext, CoverageTable, Lookup, LookupFlag, SelectedLookupList,
        SequenceContext,
    },
};
use read_fonts::{FontData, FontRead, ReadError, TableProvider};

/// Subtables past this many in a lookup get no glyph lookup caches.
const MAX_CACHED_SUBTABLES: usize = 8;

/// Coverages of fewer than 8 glyphs or ranges get no cache: searching them
/// costs about as much as a cache hit, and a cache takes 512 bytes.
const MIN_CACHED_COVERAGE_COST: u32 = 4;

/// A malformed subtable is skipped; exhausted preprocessing aborts its lookup.
#[derive(Debug, PartialEq, Eq)]
pub(super) enum BuildError {
    Invalid,
    Exhausted,
}

impl From<ReadError> for BuildError {
    fn from(_: ReadError) -> Self {
        Self::Invalid
    }
}

impl From<Exhausted> for BuildError {
    fn from(_: Exhausted) -> Self {
        Self::Exhausted
    }
}

/// The number of lookups in GSUB, or GPOS if `is_subst` is false.
pub(super) fn lookup_count(tables: &Tables, is_subst: bool) -> u16 {
    let count = if is_subst {
        tables
            .gsub()
            .ok()
            .and_then(|gsub| gsub.lookup_list().ok())
            .map(|list| list.lookup_count())
    } else {
        tables
            .gpos()
            .ok()
            .and_then(|gpos| gpos.lookup_list().ok())
            .map(|list| list.lookup_count())
    };
    count.unwrap_or_default()
}

/// Reads lookup `index` of GSUB, or GPOS if `is_subst` is false.
pub(super) fn build_lookup<'a>(
    tables: &'a Tables,
    is_subst: bool,
    index: u16,
    budget: &Budget,
) -> Result<CachedLookup<'a>, BuildError> {
    let (list_data, offset) = if is_subst {
        lookup_location(&tables.gsub()?.lookup_list()?, index)
    } else {
        lookup_location(&tables.gpos()?.lookup_list()?, index)
    }
    .ok_or(BuildError::Invalid)?;
    build_lookup_at(list_data, offset, is_subst, budget)
}

/// The data of a lookup list and the offset of lookup `index` in it.
fn lookup_location<'a, T: FontRead<'a, Args = ()> + 'a>(
    list: &SelectedLookupList<'a, T>,
    index: u16,
) -> Option<(FontData<'a>, usize)> {
    let index = usize::from(index);
    let offset = match list {
        SelectedLookupList::Original(list) => list.lookup_offsets().get(index)?.get().to_u32(),
        SelectedLookupList::Extended(list) => list.lookup_offsets().get(index)?.get().to_u32(),
    };
    Some((list.offset_data(), offset as usize))
}

/// Reads the lookup at `offset` in a GSUB (`is_subst`) or GPOS table.
pub(super) fn build_lookup_at<'a>(
    table_data: FontData<'a>,
    offset: usize,
    is_subst: bool,
    budget: &Budget,
) -> Result<CachedLookup<'a>, BuildError> {
    budget.work(1)?;
    let lookup: Lookup<()> =
        Lookup::read(table_data.split_off(offset).ok_or(BuildError::Invalid)?)?;
    let count = usize::from(lookup.sub_table_count());
    budget.work(count)?;
    budget.alloc::<CachedSubtable>(count)?;
    let lookup_type = lookup.lookup_type();
    let flag = lookup.lookup_flag();
    let mut props = u32::from(flag.to_bits());
    if flag.to_bits() & LookupFlag::USE_MARK_FILTERING_SET.to_bits() != 0 {
        props |= u32::from(lookup.mark_filtering_set().unwrap_or_default()) << 16;
    }
    let mut cached = CachedLookup {
        props,
        is_subst,
        is_reversed: is_subst && is_reversed(table_data, &lookup, offset).unwrap_or_default(),
        digest: SetDigest::new(),
        digest_second: SetDigest::new(),
        hot_subtable: None,
        uses_class_cache: false,
        walk: super::walk_each,
        subtables: Vec::with_capacity(count),
    };
    let mut hot_cost = 0;
    let mut all_share_second_glyph = true;
    for (index, subtable_offset) in lookup.subtable_offsets().iter().enumerate() {
        let source = Source {
            table_data,
            offset: offset + subtable_offset.get().to_u32() as usize,
            is_subst,
            with_caches: index < MAX_CACHED_SUBTABLES,
            budget,
        };
        let built = match source.build(lookup_type) {
            Ok(built) => built,
            Err(BuildError::Invalid) => continue,
            Err(BuildError::Exhausted) => return Err(BuildError::Exhausted),
        };
        all_share_second_glyph &= built.shares_second_glyph;
        cached.digest.union(&built.subtable.digest);
        cached.digest_second.union(&built.digest_second);
        if built.cache_cost > hot_cost {
            hot_cost = built.cache_cost;
            cached.hot_subtable = Some(cached.subtables.len());
        }
        cached.subtables.push(built.subtable);
    }
    // Only the class-based contexts report a cost, and they cache classes.
    cached.uses_class_cache = cached.hot_subtable.is_some();
    cached.walk = match cached.subtables.len() {
        1 => super::walk_single,
        _ if all_share_second_glyph => super::walk_sharing_second_glyph,
        _ => super::walk_each,
    };
    Ok(cached)
}

fn is_extension(is_subst: bool, lookup_type: u16) -> bool {
    lookup_type == if is_subst { 7 } else { 9 }
}

/// Whether a GSUB lookup is a reverse chain, looking through an extension.
fn is_reversed(table_data: FontData, lookup: &Lookup<()>, lookup_offset: usize) -> Option<bool> {
    match lookup.lookup_type() {
        8 => Some(true),
        7 => {
            let offset = lookup_offset + lookup.subtable_offsets().first()?.get().to_u32() as usize;
            let ext = ExtensionSubstFormat1::<()>::read(table_data.split_off(offset)?).ok()?;
            let ext_type = ext.extension_lookup_type();
            (!is_extension(true, ext_type)).then_some(ext_type == 8)
        }
        _ => Some(false),
    }
}

/// A subtable together with what its lookup needs to know about it.
struct BuiltSubtable<'a> {
    subtable: CachedSubtable<'a>,
    digest_second: SetDigest,
    cache_cost: u32,
    /// Whether the subtable adjusts pairs and shares the second glyph.
    shares_second_glyph: bool,
}

/// Where a subtable is and how to cache it.
#[derive(Clone, Copy)]
struct Source<'a, 'b> {
    table_data: FontData<'a>,
    offset: usize,
    is_subst: bool,
    with_caches: bool,
    budget: &'b Budget,
}

impl<'a> Source<'a, '_> {
    fn build(self, lookup_type: u16) -> Result<BuiltSubtable<'a>, BuildError> {
        let data = self
            .table_data
            .split_off(self.offset)
            .ok_or(BuildError::Invalid)?;
        self.budget.work(1)?;
        let budget = self.budget;
        let caches = self.with_caches;
        match (self.is_subst, lookup_type) {
            (true, 1) => match SingleSubst::read(data)? {
                SingleSubst::Format1(t) => self.finish(t.coverage(), gsub::SingleSubst1::new(&t)),
                SingleSubst::Format2(t) => self.finish(t.coverage(), gsub::SingleSubst2::new(&t)),
                SingleSubst::Format3(t) => {
                    self.finish(t.coverage(), gsub::SingleSubst1::new_wide(&t))
                }
                SingleSubst::Format4(t) => {
                    self.finish(t.coverage(), gsub::SingleSubst2::new_wide(&t))
                }
            },
            (true, 2) => match MultipleSubst::read(data)? {
                MultipleSubst::Format1(t) => {
                    self.finish(t.coverage(), gsub::MultipleSubst1::new(&t))
                }
                MultipleSubst::Format2(t) => {
                    self.finish(t.coverage(), gsub::MultipleSubst1::new_wide(&t))
                }
            },
            (true, 3) => match AlternateSubst::read(data)? {
                AlternateSubst::Format1(t) => {
                    self.finish(t.coverage(), gsub::AlternateSubst1::new(&t))
                }
                AlternateSubst::Format2(t) => {
                    self.finish(t.coverage(), gsub::AlternateSubst1::new_wide(&t))
                }
            },
            (true, 4) => match LigatureSubst::read(data)? {
                LigatureSubst::Format1(t) => {
                    self.finish(t.coverage(), Some(gsub::LigatureSubst1::new(&t, budget)?))
                }
                LigatureSubst::Format2(t) => self.finish(
                    t.coverage(),
                    Some(gsub::LigatureSubst1::new_wide(&t, budget)?),
                ),
            },
            (true, 8) => match ReverseChainSingleSubst::read(data)? {
                ReverseChainSingleSubst::Format1(t) => {
                    self.finish(t.coverage(), gsub::ReverseChainSubst1::new(&t, budget)?)
                }
                ReverseChainSingleSubst::Format2(t) => self.finish(
                    t.coverage(),
                    gsub::ReverseChainSubst1::new_wide(&t, budget)?,
                ),
            },
            (false, 1) => match SinglePos::read(data)? {
                SinglePos::Format1(t) => self.finish(t.coverage(), gpos::SinglePos1::new(&t)),
                SinglePos::Format2(t) => self.finish(t.coverage(), gpos::SinglePos2::new(&t)),
                SinglePos::Format3(t) => self.finish(t.coverage(), gpos::SinglePos1::new_wide(&t)),
                SinglePos::Format4(t) => self.finish(t.coverage(), gpos::SinglePos2::new_wide(&t)),
            },
            (false, 2) => match PairPos::read(data)? {
                PairPos::Format1(t) => match gpos::PairKern1::new(&t, budget)? {
                    Some(kern) => self.finish(t.coverage(), Some(kern)),
                    None => self.finish(t.coverage(), Some(gpos::PairPos1::new(&t, budget)?)),
                },
                PairPos::Format2(t) => self.class_pairs(t.coverage(), gpos::ClassPairs::new(&t)),
                PairPos::Format3(t) => {
                    self.finish(t.coverage(), Some(gpos::PairPos1::new_wide(&t, budget)?))
                }
                PairPos::Format4(t) => {
                    self.class_pairs(t.coverage(), gpos::ClassPairs::new_wide(&t))
                }
            },
            (false, 3) => match CursivePos::read(data)? {
                CursivePos::Format1(t) => self.finish(t.coverage(), gpos::CursivePos1::new(&t)),
                CursivePos::Format2(t) => {
                    self.finish(t.coverage(), gpos::CursivePos1::new_wide(&t))
                }
            },
            (false, 4) => match MarkBasePos::read(data)? {
                MarkBasePos::Format1(t) => {
                    self.finish(t.mark_coverage(), gpos::MarkBasePos1::new(&t))
                }
                MarkBasePos::Format2(t) => {
                    self.finish(t.mark_coverage(), gpos::MarkBasePos1::new_wide(&t))
                }
            },
            (false, 5) => match MarkLigPos::read(data)? {
                MarkLigPos::Format1(t) => {
                    self.finish(t.mark_coverage(), gpos::MarkLigPos1::new(&t))
                }
                MarkLigPos::Format2(t) => {
                    self.finish(t.mark_coverage(), gpos::MarkLigPos1::new_wide(&t))
                }
            },
            (false, 6) => match MarkMarkPos::read(data)? {
                MarkMarkPos::Format1(t) => {
                    self.finish(t.mark1_coverage(), gpos::MarkMarkPos1::new(&t))
                }
                MarkMarkPos::Format2(t) => {
                    self.finish(t.mark1_coverage(), gpos::MarkMarkPos1::new_wide(&t))
                }
            },
            (true, 5) | (false, 7) => match SequenceContext::read(data)? {
                SequenceContext::Format1(t) => {
                    self.finish(t.coverage(), context::ChainContext1::from_plain(&t))
                }
                SequenceContext::Format2(t) => self.finish(
                    t.coverage(),
                    Some(context::ChainContext2::from_plain(&t, caches, budget)?),
                ),
                SequenceContext::Format3(t) => self.finish(
                    t.coverages().get(0),
                    Some(context::ChainContext3::from_plain(&t, budget)?),
                ),
                SequenceContext::Format4(t) => {
                    self.finish(t.coverage(), context::ChainContext1::from_plain_wide(&t))
                }
                SequenceContext::Format5(t) => self.finish(
                    t.coverage(),
                    Some(context::ChainContext2::from_plain_wide(&t, caches, budget)?),
                ),
                SequenceContext::Format6(t) => self.finish(
                    t.coverages().get(0),
                    Some(context::ChainContext3::from_plain_wide(&t, budget)?),
                ),
            },
            (true, 6) | (false, 8) => match ChainedSequenceContext::read(data)? {
                ChainedSequenceContext::Format1(t) => {
                    self.finish(t.coverage(), context::ChainContext1::new(&t))
                }
                ChainedSequenceContext::Format2(t) => self.finish(
                    t.coverage(),
                    Some(context::ChainContext2::new(&t, caches, budget)?),
                ),
                ChainedSequenceContext::Format3(t) => self.finish(
                    t.input_coverages().get(0),
                    Some(context::ChainContext3::new(&t, budget)?),
                ),
                ChainedSequenceContext::Format4(t) => {
                    self.finish(t.coverage(), context::ChainContext1::new_wide(&t))
                }
                ChainedSequenceContext::Format5(t) => self.finish(
                    t.coverage(),
                    Some(context::ChainContext2::new_wide(&t, caches, budget)?),
                ),
            },
            (true, 7) | (false, 9) => {
                let ext = ExtensionSubstFormat1::<()>::read(data)?;
                let ext_type = ext.extension_lookup_type();
                if is_extension(self.is_subst, ext_type) {
                    return Err(BuildError::Invalid);
                }
                let offset = self
                    .offset
                    .checked_add(ext.extension_offset().to_u32() as usize)
                    .ok_or(BuildError::Invalid)?;
                Source { offset, ..self }.build(ext_type)
            }
            _ => Err(BuildError::Invalid),
        }
    }

    /// Completes a class pair subtable, as kerning if it holds only that.
    fn class_pairs(
        self,
        coverage: Result<CoverageTable<'a>, ReadError>,
        pairs: gpos::ClassPairs<'a>,
    ) -> Result<BuiltSubtable<'a>, BuildError> {
        let (caches, budget) = (self.with_caches, self.budget);
        match gpos::PairKern2::new(&pairs, caches, budget)? {
            Some(kern) => self.finish(coverage, Some(kern)),
            None => self.finish(coverage, gpos::PairPos2::new(&pairs, caches, budget)?),
        }
    }

    /// Completes a subtable from its first glyph's coverage and its data.
    fn finish<T: IntoKind<'a> + SubtableDigests<'a>>(
        self,
        coverage: Result<CoverageTable<'a>, ReadError>,
        data: Option<T>,
    ) -> Result<BuiltSubtable<'a>, BuildError> {
        let coverage = Coverage::required(coverage).ok_or(BuildError::Invalid)?;
        let data = data.ok_or(BuildError::Invalid)?;
        let digest_second = data.digest_second(self.budget)?;
        let cache_cost = data.cache_cost();
        let coverage_cache = if self.with_caches
            && T::CACHES_COVERAGE
            && coverage.cost() >= MIN_CACHED_COVERAGE_COST
        {
            self.budget.alloc::<MappingCache>(1)?;
            Some(Box::new(MappingCache::new()))
        } else {
            None
        };
        Ok(BuiltSubtable {
            subtable: CachedSubtable {
                kind: data.into_kind(self.budget)?,
                digest: coverage.digest(self.budget)?,
                coverage,
                coverage_cache,
                fns: T::FNS,
                apply_sharing: T::FNS.apply_sharing,
            },
            shares_second_glyph: T::FNS.shares_second_glyph,
            digest_second,
            cache_cost,
        })
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn repeated_single_subst(count: u16) -> Vec<u8> {
        let offset = 6 + count * 2;
        let mut words = alloc::vec![1, 0, count];
        words.resize(3 + usize::from(count), offset);
        // SingleSubst format 1, followed by coverage of glyph 42.
        words.extend_from_slice(&[1, 6, 1, 1, 1, 42]);
        words.into_iter().flat_map(u16::to_be_bytes).collect()
    }

    #[test]
    fn exhaustion_discards_previously_built_subtables() {
        let data = repeated_single_subst(2);
        // Header, two offsets, and the first subtable and coverage fit.
        // Reading the second subtable must abort the whole lookup.
        let budget = Budget::new(5, 1 << 20);
        assert!(matches!(
            build_lookup_at(FontData::new(&data), 0, true, &budget),
            Err(BuildError::Exhausted)
        ));
        let budget = Budget::new(7, 1 << 20);
        let lookup = build_lookup_at(FontData::new(&data), 0, true, &budget).unwrap();
        assert_eq!(lookup.subtables.len(), 2);
    }

    #[test]
    fn subtable_storage_is_reserved_before_building() {
        let data = repeated_single_subst(2);
        let budget = Budget::new(100, size_of::<CachedSubtable>() * 2 - 1);
        assert!(matches!(
            build_lookup_at(FontData::new(&data), 0, true, &budget),
            Err(BuildError::Exhausted)
        ));
        assert_eq!(budget.remaining().0, 97);
    }

    #[test]
    fn thousands_of_subtables_are_allowed() {
        // Gulzar has a lookup with 3625 subtables. Counts alone must not
        // reject it; this synthetic lookup exercises an even larger count.
        let data = repeated_single_subst(3800);
        let budget = Budget::new(1 << 20, 16 << 20);
        let lookup = build_lookup_at(FontData::new(&data), 0, true, &budget).unwrap();
        assert_eq!(lookup.subtables.len(), 3800);
    }

    #[test]
    fn malformed_subtables_do_not_abort_a_lookup() {
        let mut data = repeated_single_subst(2);
        // Only the first offset points outside the data.
        data[6..8].copy_from_slice(&u16::MAX.to_be_bytes());
        let budget = Budget::for_table_bytes(data.len());
        let lookup = build_lookup_at(FontData::new(&data), 0, true, &budget).unwrap();
        assert_eq!(lookup.subtables.len(), 1);
    }

    fn lookup_with_recursive_extension(lookup_type: u16) -> [u8; 16] {
        let mut data = [0; 16];
        // Lookup table.
        data[0..2].copy_from_slice(&lookup_type.to_be_bytes());
        data[4..6].copy_from_slice(&1u16.to_be_bytes());
        data[6..8].copy_from_slice(&8u16.to_be_bytes());
        // ExtensionSubstFormat1/ExtensionPosFormat1 subtable.
        data[8..10].copy_from_slice(&1u16.to_be_bytes());
        data[10..12].copy_from_slice(&lookup_type.to_be_bytes());
        data
    }

    #[test]
    fn gsub_extension_lookup_cannot_target_extension_lookup() {
        let data = lookup_with_recursive_extension(7);
        let lookup = build_lookup_at(
            FontData::new(&data),
            0,
            true,
            &Budget::for_table_bytes(data.len()),
        )
        .unwrap();
        assert!(lookup.subtables.is_empty());
    }

    #[test]
    fn gpos_extension_lookup_cannot_target_extension_lookup() {
        let data = lookup_with_recursive_extension(9);
        let lookup = build_lookup_at(
            FontData::new(&data),
            0,
            false,
            &Budget::for_table_bytes(data.len()),
        )
        .unwrap();
        assert!(lookup.subtables.is_empty());
    }
}

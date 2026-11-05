//! Coverage and class definition tables, reduced to the arrays they search.

use super::Offsets;
use crate::budget::{Budget, Exhausted};
use crate::ot::layout::MAX_CONTEXT_LENGTH;
use crate::set_digest::SetDigest;
use alloc::boxed::Box;
use core::cmp::Ordering;
use read_fonts::tables::layout::{
    self, ClassRangeRecord, ClassRangeRecord2, CoverageTable, RangeRecord, RangeRecord2,
};
use read_fonts::types::{BigEndian, GlyphId, GlyphId16, GlyphId24, Uint24};
use read_fonts::{FontRead, ReadError};

/// A coverage per context glyph, parsed eagerly only for short contexts.
///
/// Arrays longer than [`MAX_CONTEXT_LENGTH`] are read on demand. Overlapping
/// subtables can otherwise multiply a small font into large allocations and
/// parsing work.
pub(crate) enum CoverageArray<'a> {
    Cached(Box<[Coverage<'a>]>),
    Uncached(Offsets<'a>),
}

impl Default for CoverageArray<'_> {
    fn default() -> Self {
        Self::Cached(Box::default())
    }
}

impl<'a> CoverageArray<'a> {
    pub fn new(offsets: Offsets<'a>, budget: &Budget) -> Result<Self, Exhausted> {
        if offsets.len() > MAX_CONTEXT_LENGTH {
            return Ok(Self::Uncached(offsets));
        }
        budget.work(offsets.len())?;
        budget.alloc::<Coverage>(offsets.len())?;
        Ok(Self::Cached(
            (0..offsets.len())
                .map(|index| read_coverage(&offsets, index))
                .collect(),
        ))
    }

    pub fn len(&self) -> usize {
        match self {
            Self::Cached(coverages) => coverages.len(),
            Self::Uncached(offsets) => offsets.len(),
        }
    }

    pub fn is_empty(&self) -> bool {
        self.len() == 0
    }

    /// Missing or unreadable coverages cover no glyphs, in either form.
    #[inline]
    pub fn get(&self, index: usize) -> Coverage<'a> {
        match self {
            Self::Cached(coverages) => coverages.get(index).copied().unwrap_or_default(),
            Self::Uncached(offsets) => read_coverage(offsets, index),
        }
    }
}

/// The coverage at `index` of `offsets`, which is empty if it is null or
/// can't be read. Kept out of line so that the cached arrays of short
/// contexts, which are nearly all of them, inline into the matchers.
#[inline(never)]
fn read_coverage<'a>(offsets: &Offsets<'a>, index: usize) -> Coverage<'a> {
    offsets.data(index).map_or(Coverage::None, |data| {
        Coverage::new(CoverageTable::read(data))
    })
}

/// The position of `gid` among sorted ranges of glyphs.
#[inline]
fn find_range<T, G: PartialOrd>(ranges: &[T], gid: G, bounds: impl Fn(&T) -> (G, G)) -> Option<&T> {
    ranges
        .binary_search_by(|range| {
            let (start, end) = bounds(range);
            if end < gid {
                Ordering::Less
            } else if start > gid {
                Ordering::Greater
            } else {
                Ordering::Equal
            }
        })
        .ok()
        .map(|index| &ranges[index])
}

/// A coverage table: the glyphs it covers, each with a coverage index.
#[derive(Clone, Copy, Default)]
pub(crate) enum Coverage<'a> {
    /// A null or unreadable table, which covers nothing.
    #[default]
    None,
    /// Sorted glyphs, each covered at its position.
    Glyphs(&'a [BigEndian<GlyphId16>]),
    /// Sorted glyph ranges, each with the index of its first glyph.
    Ranges(&'a [RangeRecord]),
    /// Sorted 24-bit glyphs.
    Glyphs24(&'a [BigEndian<GlyphId24>]),
    /// Sorted 24-bit glyph ranges.
    Ranges24(&'a [RangeRecord2]),
}

impl<'a> Coverage<'a> {
    pub fn new(table: Result<CoverageTable<'a>, ReadError>) -> Self {
        match table {
            Ok(CoverageTable::Format1(table)) => Self::Glyphs(table.glyph_array()),
            Ok(CoverageTable::Format2(table)) => Self::Ranges(table.range_records()),
            Ok(CoverageTable::Format3(table)) => Self::Glyphs24(table.glyph_array()),
            Ok(CoverageTable::Format4(table)) => Self::Ranges24(table.range_records()),
            Err(_) => Self::None,
        }
    }

    /// A coverage that must be present, or `None` if it can't be read.
    pub fn required(table: Result<CoverageTable<'a>, ReadError>) -> Option<Self> {
        Some(Self::new(table)).filter(|coverage| !coverage.is_none())
    }

    pub fn is_none(&self) -> bool {
        matches!(self, Self::None)
    }

    /// The coverage, or `None` if the table is missing.
    pub fn present(&self) -> Option<&Self> {
        (!self.is_none()).then_some(self)
    }

    /// The coverage index of `gid`, if it is covered.
    #[inline(always)]
    pub fn get(&self, gid: impl Into<GlyphId>) -> Option<u32> {
        let gid = gid.into();
        match self {
            Self::None => None,
            Self::Glyphs(glyphs) => {
                let gid = GlyphId16::try_from(gid).ok()?;
                glyphs
                    .binary_search(&gid.into())
                    .ok()
                    .map(|index| index as u32)
            }
            Self::Ranges(ranges) => {
                let gid = GlyphId16::try_from(gid).ok()?;
                let range = find_range(ranges, gid, |r| (r.start_glyph_id(), r.end_glyph_id()))?;
                // Subtract first so the index can't overflow on the way.
                range
                    .start_coverage_index()
                    .checked_add(gid.to_u16() - range.start_glyph_id().to_u16())
                    .map(u32::from)
            }
            Self::Glyphs24(_) | Self::Ranges24(_) => self.get_wide(gid),
        }
    }

    // The 24-bit formats share one call, which keeps the inlined lookups
    // of the original formats as small as they were.
    #[inline(never)]
    fn get_wide(&self, gid: GlyphId) -> Option<u32> {
        match self {
            Self::Glyphs24(glyphs) => glyphs24_index(glyphs, gid),
            Self::Ranges24(ranges) => ranges24_index(ranges, gid),
            _ => None,
        }
    }

    /// The cost of looking up a glyph: the number of bits in the length of
    /// the array searched.
    pub fn cost(&self) -> u32 {
        let bits = |len: usize| u32::BITS - (len as u32).leading_zeros();
        match self {
            Self::None => 0,
            Self::Glyphs(glyphs) => bits(glyphs.len()),
            Self::Ranges(ranges) => bits(ranges.len()),
            Self::Glyphs24(glyphs) => bits(glyphs.len()),
            Self::Ranges24(ranges) => bits(ranges.len()),
        }
    }

    pub fn covers(&self, gid: impl Into<GlyphId>) -> bool {
        self.get(gid).is_some()
    }

    /// The glyphs covered, or every glyph if the table is missing.
    pub fn digest(&self, budget: &Budget) -> Result<SetDigest, Exhausted> {
        let mut digest = SetDigest::new();
        match self {
            Self::None => return Ok(SetDigest::full()),
            Self::Glyphs(glyphs) => {
                budget.work(glyphs.len())?;
                for glyph in *glyphs {
                    digest.add(glyph.get().into());
                }
            }
            Self::Ranges(ranges) => {
                budget.work(ranges.len())?;
                for range in *ranges {
                    digest.add_range(range.start_glyph_id().into(), range.end_glyph_id().into());
                }
            }
            Self::Glyphs24(glyphs) => {
                budget.work(glyphs.len())?;
                for glyph in *glyphs {
                    digest.add(glyph.get().to_u32());
                }
            }
            Self::Ranges24(ranges) => {
                budget.work(ranges.len())?;
                for range in *ranges {
                    digest.add_range(
                        range.start_glyph_id().to_u32(),
                        range.end_glyph_id().to_u32(),
                    );
                }
            }
        }
        Ok(digest)
    }
}

fn glyphs24_index(glyphs: &[BigEndian<GlyphId24>], gid: GlyphId) -> Option<u32> {
    let gid = GlyphId24::checked_new(gid.to_u32())?;
    glyphs
        .binary_search(&gid.into())
        .ok()
        .map(|index| index as u32)
}

fn ranges24_index(ranges: &[RangeRecord2], gid: GlyphId) -> Option<u32> {
    let gid = gid.to_u32();
    let range = find_range(ranges, gid, |r| {
        (r.start_glyph_id().to_u32(), r.end_glyph_id().to_u32())
    })?;
    let index = range
        .start_coverage_index()
        .to_u32()
        .checked_add(gid - range.start_glyph_id().to_u32())?;
    (index <= Uint24::MAX.to_u32()).then_some(index)
}

/// A class definition table: the class of each glyph, 0 if unlisted.
#[derive(Clone, Copy, Default)]
pub(crate) enum ClassDef<'a> {
    /// A null or unreadable table, which puts every glyph in class 0.
    #[default]
    None,
    /// The classes of consecutive glyphs from `start`.
    Array {
        start: u16,
        classes: &'a [BigEndian<u16>],
    },
    /// Sorted glyph ranges, each with a class.
    Ranges(&'a [ClassRangeRecord]),
    /// The 24-bit classes of consecutive glyphs from a 24-bit `start`.
    Array24 {
        start: u32,
        classes: &'a [BigEndian<Uint24>],
    },
    /// Sorted 24-bit glyph ranges.
    Ranges24(&'a [ClassRangeRecord2]),
}

impl<'a> ClassDef<'a> {
    pub fn new(table: Result<layout::ClassDef<'a>, ReadError>) -> Self {
        match table {
            Ok(layout::ClassDef::Format1(table)) => Self::Array {
                start: table.start_glyph_id().to_u16(),
                classes: table.class_value_array(),
            },
            Ok(layout::ClassDef::Format2(table)) => Self::Ranges(table.class_range_records()),
            Ok(layout::ClassDef::Format3(table)) => Self::Array24 {
                start: table.start_glyph_id().to_u32(),
                classes: table.class_value_array(),
            },
            Ok(layout::ClassDef::Format4(table)) => Self::Ranges24(table.class_range_records()),
            Err(_) => Self::None,
        }
    }

    pub fn is_none(&self) -> bool {
        matches!(self, Self::None)
    }

    /// The class of `gid`.
    #[inline(always)]
    pub fn get(&self, gid: impl Into<GlyphId>) -> u32 {
        let gid = gid.into();
        match self {
            Self::None => 0,
            Self::Array { start, classes } => gid
                .to_u32()
                .checked_sub(u32::from(*start))
                .and_then(|index| classes.get(index as usize))
                .map_or(0, |class| class.get().into()),
            Self::Ranges(ranges) => GlyphId16::try_from(gid)
                .ok()
                .and_then(|gid| find_range(ranges, gid, |r| (r.start_glyph_id(), r.end_glyph_id())))
                .map_or(0, |range| range.class().into()),
            Self::Array24 { .. } | Self::Ranges24(_) => self.get_wide(gid),
        }
    }

    #[inline(never)]
    fn get_wide(&self, gid: GlyphId) -> u32 {
        match self {
            Self::Array24 { start, classes } => array24_class(*start, classes, gid),
            Self::Ranges24(ranges) => ranges24_class(ranges, gid),
            _ => 0,
        }
    }

    /// The cost of looking up a glyph, which decides which subtable gets
    /// the glyph-info class cache.
    pub fn cost(&self) -> u32 {
        let bits = |len: usize| u32::BITS - (len as u32).leading_zeros();
        match self {
            Self::None => 0,
            Self::Array { .. } | Self::Array24 { .. } => 1,
            Self::Ranges(ranges) => bits(ranges.len()),
            Self::Ranges24(ranges) => bits(ranges.len()),
        }
    }

    /// Adds the glyphs of `class` to `digest`. Class 0 holds every glyph not
    /// listed, so it adds all of them.
    pub fn add_class_to(
        &self,
        digest: &mut SetDigest,
        class: u32,
        budget: &Budget,
    ) -> Result<(), Exhausted> {
        if class == 0 {
            *digest = SetDigest::full();
            return Ok(());
        }
        match self {
            Self::None => {}
            Self::Array { start, classes } => {
                budget.work(classes.len())?;
                for (index, value) in classes.iter().enumerate() {
                    if u32::from(value.get()) == class {
                        digest.add(u32::from(start.saturating_add(index as u16)));
                    }
                }
            }
            Self::Ranges(ranges) => {
                budget.work(ranges.len())?;
                for range in *ranges {
                    if u32::from(range.class()) == class {
                        budget.work(
                            usize::from(range.end_glyph_id().to_u16())
                                .saturating_sub(usize::from(range.start_glyph_id().to_u16()))
                                + 1,
                        )?;
                        for glyph in range.start_glyph_id().to_u16()..=range.end_glyph_id().to_u16()
                        {
                            digest.add(u32::from(glyph));
                        }
                    }
                }
            }
            Self::Array24 { start, classes } => {
                budget.work(classes.len())?;
                for (index, value) in classes.iter().enumerate() {
                    if value.get().to_u32() == class {
                        digest.add(start.saturating_add(index as u32));
                    }
                }
            }
            Self::Ranges24(ranges) => {
                budget.work(ranges.len())?;
                for range in *ranges {
                    if u32::from(range.class()) == class {
                        budget.work(
                            (range.end_glyph_id().to_u32())
                                .saturating_sub(range.start_glyph_id().to_u32())
                                as usize
                                + 1,
                        )?;
                        for glyph in range.start_glyph_id().to_u32()..=range.end_glyph_id().to_u32()
                        {
                            digest.add(glyph);
                        }
                    }
                }
            }
        }
        Ok(())
    }
}

fn array24_class(start: u32, classes: &[BigEndian<Uint24>], gid: GlyphId) -> u32 {
    gid.to_u32()
        .checked_sub(start)
        .and_then(|index| classes.get(index as usize))
        .map_or(0, |class| class.get().to_u32())
}

fn ranges24_class(ranges: &[ClassRangeRecord2], gid: GlyphId) -> u32 {
    let gid = gid.to_u32();
    find_range(ranges, gid, |r| {
        (r.start_glyph_id().to_u32(), r.end_glyph_id().to_u32())
    })
    .map_or(0, |range| range.class().into())
}

#[cfg(test)]
mod tests {
    use super::*;
    use alloc::vec::Vec;
    use read_fonts::FontData;

    /// Data holding 16-bit offsets followed by `tables`. `None` is a null
    /// offset and `Some(at)` points `at` bytes into `tables`.
    fn offsets_then(offsets: &[Option<u16>], tables: &[u8]) -> Vec<u8> {
        let base = (offsets.len() * 2) as u16;
        let mut bytes: Vec<u8> = offsets
            .iter()
            .flat_map(|offset| offset.map_or(0, |at| base + at).to_be_bytes())
            .collect();
        bytes.extend_from_slice(tables);
        bytes
    }

    fn array<'a>(bytes: &'a [u8], count: usize, budget: &Budget) -> CoverageArray<'a> {
        let offsets = Offsets::new(FontData::new(bytes), 0..count * 2, 2);
        CoverageArray::new(offsets, budget).unwrap()
    }

    #[test]
    fn class_range_expansion_is_charged() {
        let words: [u16; 5] = [2, 1, 0, u16::MAX, 1];
        let bytes: Vec<_> = words.into_iter().flat_map(u16::to_be_bytes).collect();
        let class = ClassDef::new(layout::ClassDef::read(FontData::new(&bytes)));
        let mut digest = SetDigest::new();
        assert_eq!(
            class.add_class_to(&mut digest, 1, &Budget::new(10, 0)),
            Err(Exhausted)
        );
        assert!(!digest.may_have(0));
        class
            .add_class_to(&mut digest, 1, &Budget::new(65_537, 0))
            .unwrap();
        assert!(digest.may_have(u32::from(u16::MAX)));
    }

    #[test]
    fn coverage_arrays_keep_long_contexts_without_parsing_every_offset() {
        // Two format 1 coverages, of glyphs 42 and 43.
        let tables = [0, 1, 0, 1, 0, 42, 0, 1, 0, 1, 0, 43];
        for count in [MAX_CONTEXT_LENGTH, MAX_CONTEXT_LENGTH + 1, 20_000] {
            // Many entries can alias the same coverage in a tiny font.
            let mut offsets = alloc::vec![Some(0); count];
            offsets[count - 1] = Some(6);
            let bytes = offsets_then(&offsets, &tables);
            let budget = Budget::for_table_bytes(bytes.len());
            let coverages = array(&bytes, count, &budget);
            assert_eq!(coverages.len(), count);
            assert!(!coverages.is_empty());
            assert_eq!(coverages.get(0).get(42u32), Some(0));
            assert_eq!(coverages.get(count - 1).get(43u32), Some(0));
            assert!(!coverages.get(count - 1).covers(42u32));
            assert!(coverages.get(count).is_none());
            assert_eq!(
                matches!(coverages, CoverageArray::Cached(_)),
                count <= MAX_CONTEXT_LENGTH,
            );
        }
    }

    #[test]
    fn cached_and_uncached_arrays_handle_unreadable_coverages_the_same_way() {
        let tables = [0, 1, 0, 1, 0, 42];
        for count in [MAX_CONTEXT_LENGTH, MAX_CONTEXT_LENGTH + 1] {
            let mut offsets = alloc::vec![Some(0); count];
            // Null, out of bounds, and an unsupported coverage format.
            offsets[0] = None;
            offsets[1] = Some(u16::MAX - (count * 2) as u16);
            offsets[2] = Some(1);
            let bytes = offsets_then(&offsets, &tables);
            let budget = Budget::for_table_bytes(bytes.len());
            let coverages = array(&bytes, count, &budget);
            for index in [0, 1, 2, count] {
                let coverage = coverages.get(index);
                assert!(coverage.is_none());
                assert!(!coverage.covers(42u32));
                assert!(coverage.digest(&Budget::new(0, 0)).unwrap().may_have(42));
            }
            assert_eq!(coverages.get(3).get(42u32), Some(0));
        }
    }

    #[test]
    fn empty_coverage_arrays_cover_nothing() {
        for coverages in [CoverageArray::default(), array(&[], 0, &Budget::new(0, 0))] {
            assert!(coverages.is_empty());
            assert_eq!(coverages.len(), 0);
            assert!(coverages.get(0).is_none());
        }
    }
}

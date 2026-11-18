use super::layout::DELETED_GLYPH;
use super::map::RangeFlags;
use crate::buffer::{Buffer, HB_BUFFER_SCRATCH_FLAG_SHAPER0};
use crate::ot::apply::MappingCache;
use crate::plan::ShapePlan;
use crate::set_digest::SetDigest;
use crate::LayoutData;
use crate::Mask;
use crate::Scale;
use crate::U32Set;
use alloc::vec::Vec;
use core::mem::size_of;
use read_fonts::tables::aat::*;
use read_fonts::types::{FixedSize, GlyphId};
use read_fonts::FontData;

pub const HB_BUFFER_SCRATCH_FLAG_AAT_HAS_DELETED: u32 = HB_BUFFER_SCRATCH_FLAG_SHAPER0;

pub(crate) const START_OF_TEXT: i32 = 0;

pub(crate) type ClassCache = MappingCache;

pub(crate) fn get_class<T: bytemuck::AnyBitPattern + FixedSize>(
    machine: &ExtendedStateTable<'_, T>,
    glyph_id: GlyphId,
    cache: &ClassCache,
) -> u16 {
    if let Some(klass) = cache.get(glyph_id.to_u32()) {
        return klass as u16;
    }
    let klass = machine
        .class(glyph_id)
        .unwrap_or(class::OUT_OF_BOUNDS as u16);
    cache.set(glyph_id.to_u32(), klass as u32);
    klass
}

/// HB: hb_aat_apply_context_t
///
/// See <https://github.com/harfbuzz/harfbuzz/blob/2c22a65f0cb99544c36580b9703a43b5dc97a9e1/src/hb-aat-layout-common.hh#L108>
#[doc(alias = "hb_aat_apply_context_t")]
pub struct AatApplyContext<'a, 'f, 'c> {
    pub plan: &'a ShapePlan,
    pub layout: LayoutData<'a>,
    pub scale: Scale,
    #[cfg(feature = "tracing")]
    pub font: &'a crate::ShaperFont<'f, 'c>,
    #[cfg(not(feature = "tracing"))]
    font_lifetimes: core::marker::PhantomData<(&'f (), &'c ())>,
    pub buffer: &'a mut Buffer,
    pub has_glyph_classes: bool,
    pub range_flags: Option<&'a [RangeFlags]>,
    pub subtable_flags: Mask,
    pub(crate) buffer_is_reversed: bool,
    // Caches
    using_buffer_glyph_set: bool,
    /// Whether the buffer's digest is current, to rule out subtables cheaply.
    using_buffer_digest: bool,
    pub(crate) first_set: Option<&'a U32Set>,
    /// A digest of `first_set`.
    pub(crate) first_digest: SetDigest,
    pub(crate) second_set: Option<&'a U32Set>,
    pub(crate) machine_class_cache: Option<&'a ClassCache>,
    pub(crate) start_end_safe_to_break: u64,
    pub(crate) safe_to_break: SafeToBreak<'a>,
}

// The font's lifetimes are named for the font that tracing takes.
#[cfg_attr(not(feature = "tracing"), allow(clippy::elidable_lifetime_names))]
impl<'a, 'f, 'c> AatApplyContext<'a, 'f, 'c> {
    pub fn new(
        plan: &'a ShapePlan,
        layout: LayoutData<'a>,
        scale: Scale,
        buffer: &'a mut Buffer,
        #[cfg(feature = "tracing")] font: &'a crate::ShaperFont<'f, 'c>,
    ) -> Self {
        Self {
            plan,
            layout,
            scale,
            #[cfg(not(feature = "tracing"))]
            font_lifetimes: core::marker::PhantomData,
            buffer,
            #[cfg(feature = "tracing")]
            font,
            has_glyph_classes: layout.ot.has_glyph_classes(),
            range_flags: None,
            subtable_flags: 0,
            buffer_is_reversed: false,
            using_buffer_glyph_set: false,
            using_buffer_digest: false,
            first_set: None,
            first_digest: SetDigest::full(),
            second_set: None,
            machine_class_cache: None,
            start_end_safe_to_break: 0,
            safe_to_break: SafeToBreak::default(),
        }
    }

    #[inline(always)]
    pub(crate) fn scale_x(&self, value: i32) -> i32 {
        self.scale.scale_x(value)
    }

    #[inline(always)]
    pub(crate) fn scale_y(&self, value: i32) -> i32 {
        self.scale.scale_y(value)
    }

    pub(crate) fn reverse_buffer(&mut self) {
        self.buffer.reverse();
        self.buffer_is_reversed = !self.buffer_is_reversed;
    }

    pub(crate) fn setup_buffer_glyph_set(&mut self) {
        self.using_buffer_glyph_set = self.buffer.len >= 4;

        if self.using_buffer_glyph_set {
            self.buffer.update_glyph_set();
        }
    }

    /// Prepares the digest that rules out subtables cheaply, if a table has
    /// enough subtables to repay building it.
    pub(crate) fn setup_buffer_digest(&mut self, subtable_count: usize) {
        self.using_buffer_digest = subtable_count >= MIN_DIGESTED_SUBTABLES;
        if self.using_buffer_digest {
            self.buffer.update_digest();
        }
    }

    pub(crate) fn buffer_intersects_machine(&self) -> bool {
        if let Some(first_set) = &self.first_set {
            if self.using_buffer_digest && !self.buffer.digest.may_intersect(&self.first_digest) {
                return false;
            }
            if self.using_buffer_glyph_set {
                return self.buffer.glyph_set.intersects_set(first_set);
            }
            for info in self.buffer.glyph_infos() {
                if first_set.contains(info.glyph_id) {
                    return true;
                }
            }
            false
        } else {
            true
        }
    }

    pub fn output_glyph(&mut self, glyph: u32) -> Option<()> {
        self.buffer.digest.add(glyph);
        if self.using_buffer_glyph_set {
            self.buffer.glyph_set.insert(glyph);
        }

        // Insertion state machines can emit glyphs during the end-of-text
        // transition, when there is no current input glyph. output_glyph()
        // supports that by copying the previous output glyph's metadata.
        // Apply the AAT metadata to that new output glyph instead of indexing
        // past the end of the input buffer.
        let at_end = self.buffer.idx == self.buffer.len;
        if at_end {
            let out_len = self.buffer.out_len;
            self.buffer.output_glyph(glyph);
            if self.buffer.out_len == out_len {
                return Some(());
            }
        }

        if glyph == DELETED_GLYPH {
            self.buffer.scratch_flags |= HB_BUFFER_SCRATCH_FLAG_AAT_HAS_DELETED;
            let info = if at_end {
                self.buffer.prev_mut()?
            } else {
                self.buffer.cur_mut(0)?
            };
            info.set_aat_deleted();
        } else {
            if self.has_glyph_classes {
                let glyph_props = self.layout.ot.glyph_props(glyph.into());
                let info = if at_end {
                    self.buffer.prev_mut()?
                } else {
                    self.buffer.cur_mut(0)?
                };
                info.set_glyph_props(glyph_props);
            }
        }
        if !at_end {
            self.buffer.output_glyph(glyph);
        }
        Some(())
    }

    pub fn replace_glyph(&mut self, glyph: u32) -> Option<()> {
        if glyph == DELETED_GLYPH {
            self.buffer.scratch_flags |= HB_BUFFER_SCRATCH_FLAG_AAT_HAS_DELETED;
            self.buffer.cur_mut(0)?.set_aat_deleted();
        }

        self.buffer.digest.add(glyph);
        if self.using_buffer_glyph_set {
            self.buffer.glyph_set.insert(glyph);
        }
        if self.has_glyph_classes {
            self.buffer
                .cur_mut(0)?
                .set_glyph_props(self.layout.ot.glyph_props(glyph.into()));
        }
        self.buffer.replace_glyph(glyph);
        Some(())
    }

    pub fn delete_glyph(&mut self) -> Option<()> {
        self.buffer.scratch_flags |= HB_BUFFER_SCRATCH_FLAG_AAT_HAS_DELETED;
        self.buffer.cur_mut(0)?.set_aat_deleted();
        self.buffer.digest.add(DELETED_GLYPH);
        if self.using_buffer_glyph_set {
            self.buffer.glyph_set.insert(DELETED_GLYPH);
        }
        self.buffer.replace_glyph(DELETED_GLYPH);
        Some(())
    }

    #[inline(always)]
    pub fn replace_glyph_inplace(&mut self, i: usize, glyph: u32) -> Option<()> {
        let info = self.buffer.info.get_mut(i)?;
        info.glyph_id = glyph;
        if glyph == DELETED_GLYPH {
            self.buffer.scratch_flags |= HB_BUFFER_SCRATCH_FLAG_AAT_HAS_DELETED;
            info.set_aat_deleted();
        }
        self.buffer.digest.add(glyph);
        if self.using_buffer_glyph_set {
            self.buffer.glyph_set.insert(glyph);
        }
        if self.has_glyph_classes {
            info.set_glyph_props(self.layout.ot.glyph_props(glyph.into()));
        }
        Some(())
    }
}

/// Tables with fewer subtables than this skip the buffer digest.
const MIN_DIGESTED_SUBTABLES: usize = 4;

/// The heap bytes that `set` keeps: a page of 512 bits with its length and
/// page-map entry for every block of 512 glyph ids it has a member in. A
/// lookup segment spanning the glyph space fills 128 pages from six bytes
/// of font data, so this is charged to the table's preprocessing budget.
pub(crate) fn set_heap_bytes(set: &U32Set) -> usize {
    const PAGE_BYTES: usize = 80;
    let mut pages = 0;
    let mut last_page = None;
    for range in set.iter_ranges() {
        let (first, last) = (*range.start() / 512, *range.end() / 512);
        pages += (last - first) as usize + usize::from(last_page != Some(first));
        last_page = Some(last);
    }
    pages * PAGE_BYTES
}

/// A digest of the glyphs in `set`.
pub(crate) fn set_digest(set: &U32Set) -> SetDigest {
    let mut digest = SetDigest::new();
    for range in set.iter_ranges() {
        digest.add_range(*range.start(), *range.end());
    }
    digest
}

/// Per-table acceleration for the drive loops' safe-to-break computation.
///
/// The data for every state-machine subtable in one AAT table is stored in
/// two packed vectors. Each top-level subtable cache carries a compact
/// descriptor for its ranges. This avoids two `Vec` allocations in every
/// subtable cache while retaining O(classes) + O(states) storage.
#[derive(Default)]
pub(crate) struct SafeToBreakAccel {
    wouldbe: Vec<i32>,
    eot_tail: Vec<u64>,
}

#[derive(Clone, Copy, Default)]
pub(crate) struct SafeToBreakSubtable {
    wouldbe_start: u32,
    wouldbe_end: u32,
    eot_tail_start: u32,
    eot_tail_end: u32,
}

/// The slice of [`SafeToBreakAccel`] belonging to the active subtable.
#[derive(Clone, Copy, Default)]
pub(crate) struct SafeToBreak<'a> {
    n_classes: u32,
    /// Per class: the signed new state of `entry(START_OF_TEXT, class)`
    /// shifted left with the advance bit at the bottom when present and
    /// non-actionable, `WOULDBE_NONE` otherwise — so condition 2c is a
    /// single compare. Legacy tables can transition to negative states.
    wouldbe: &'a [i32],
    /// Condition-3 bits for states 64 and up; states 0 through 63 use
    /// the `start_end_safe_to_break` word. Negative states are probed
    /// directly. Empty for machines with at most 64 non-negative states.
    eot_tail: &'a [u64],
}

pub(crate) const WOULDBE_NONE: i32 = i32::MAX;

#[inline(always)]
pub(crate) fn pack_wouldbe(new_state: i32, advance: bool) -> i32 {
    // State indices are resolved from u16 offsets or indices, so shifting
    // preserves their sign and cannot collide with WOULDBE_NONE.
    (new_state << 1) | i32::from(advance)
}

impl SafeToBreak<'_> {
    /// Condition 2c: would starting from the start state on this class
    /// take a non-actionable transition to the same state, advancing
    /// the same way?
    #[inline(always)]
    pub(crate) fn wouldbe_matches(&self, class: u16, next_state: i32, advance: bool) -> bool {
        let mut class = class as usize;
        if class >= self.n_classes as usize {
            class = class::OUT_OF_BOUNDS as usize;
        }
        self.wouldbe.get(class).copied() == Some(pack_wouldbe(next_state, advance))
    }

    /// Condition 3 for states 64 and up: no end-of-text action can fire
    /// out of this state.
    #[inline(always)]
    pub(crate) fn eot_safe_high(&self, state: i32) -> bool {
        let ix = (state - 64) as usize;
        self.eot_tail
            .get(ix / 64)
            .is_some_and(|word| word & (1 << (ix % 64)) != 0)
    }
}

impl SafeToBreakAccel {
    /// Returns a view of the acceleration data described by a top-level
    /// subtable cache.
    pub(crate) fn subtable(&self, subtable: SafeToBreakSubtable) -> Option<SafeToBreak<'_>> {
        let wouldbe_start = subtable.wouldbe_start as usize;
        let wouldbe_end = subtable.wouldbe_end as usize;
        let eot_tail_start = subtable.eot_tail_start as usize;
        let eot_tail_end = subtable.eot_tail_end as usize;

        Some(SafeToBreak {
            n_classes: subtable.wouldbe_end - subtable.wouldbe_start,
            wouldbe: self.wouldbe.get(wouldbe_start..wouldbe_end)?,
            eot_tail: self.eot_tail.get(eot_tail_start..eot_tail_end)?,
        })
    }

    /// Describes a subtable that does not contain a state machine.
    pub(crate) fn empty_subtable(&self) -> SafeToBreakSubtable {
        SafeToBreakSubtable {
            wouldbe_start: self.wouldbe.len() as u32,
            wouldbe_end: self.wouldbe.len() as u32,
            eot_tail_start: self.eot_tail.len() as u32,
            eot_tail_end: self.eot_tail.len() as u32,
        }
    }

    /// Appends the data for an extended state table whose machine
    /// starts at the beginning of `data`. The predicates mirror the
    /// subtable kind's notion of actionable/advancing entries.
    #[inline(never)]
    pub(crate) fn build_extended<T: bytemuck::AnyBitPattern + FixedSize>(
        &mut self,
        machine: &ExtendedStateTable<T>,
        data: &[u8],
        is_actionable: &dyn Fn(&StateEntry<T>) -> bool,
        can_advance: &dyn Fn(&StateEntry<T>) -> bool,
    ) -> SafeToBreakSubtable {
        let n_classes = machine.n_classes;
        let wouldbe_start = self.wouldbe.len();
        let eot_tail_start = self.eot_tail.len();

        // Cover the OUT_OF_BOUNDS column the runtime clamp can select
        // even on degenerate machines; entry() applies the same clamp
        // internally, so the aliasing matches.
        // Columns that cannot hold an entry need no slots: the drive
        // loops stop when entry() fails, before consulting this table.
        let wouldbe_len =
            extended_class_count(n_classes, data).max(class::OUT_OF_BOUNDS as usize + 1);
        self.wouldbe.reserve(wouldbe_len);
        for class in 0..wouldbe_len {
            self.wouldbe
                .push(match machine.entry(START_OF_TEXT, class as u16) {
                    Ok(entry) if !is_actionable(&entry) => {
                        pack_wouldbe(entry.new_state, can_advance(&entry))
                    }
                    _ => WOULDBE_NONE,
                });
        }

        // The state array runs from its offset to the end of the
        // subtable, matching how the table reader slices it; states
        // beyond it fail entry() and stay unsafe, like the probes they
        // replace.
        if n_classes > 0 {
            if let Ok(parts) = StateTableParts::read(FontData::new(data)) {
                let n_cells = data.len().saturating_sub(parts.state_array_offset as usize)
                    / u16::RAW_BYTE_LEN;
                let n_rows = n_cells.div_ceil(n_classes).min(u16::MAX as usize + 1);
                if n_rows > 64 {
                    let eot_tail_start = self.eot_tail.len();
                    self.eot_tail
                        .resize(eot_tail_start + (n_rows - 64).div_ceil(64), 0);
                    for state in 64..n_rows {
                        if let Ok(entry) =
                            machine.entry(state as i32, u16::from(class::END_OF_TEXT))
                        {
                            if !is_actionable(&entry) {
                                let ix = state - 64;
                                self.eot_tail[eot_tail_start + ix / 64] |= 1 << (ix % 64);
                            }
                        }
                    }
                }
            }
        }

        SafeToBreakSubtable::new(
            wouldbe_len,
            wouldbe_start,
            self.wouldbe.len(),
            eot_tail_start,
            self.eot_tail.len(),
        )
    }

    /// The legacy (`kern` Format1) counterpart of [`Self::build_extended`]:
    /// one-byte state cells and u8 classes.
    #[inline(never)]
    pub(crate) fn build_legacy<T: bytemuck::AnyBitPattern + FixedSize>(
        &mut self,
        machine: &StateTable<T>,
        is_actionable: &dyn Fn(&StateEntry<T>) -> bool,
        can_advance: &dyn Fn(&StateEntry<T>) -> bool,
    ) -> SafeToBreakSubtable {
        let n_classes = machine.header.state_size() as usize;
        let wouldbe_start = self.wouldbe.len();
        let eot_tail_start = self.eot_tail.len();

        let wouldbe_len = n_classes.max(class::OUT_OF_BOUNDS as usize + 1).min(1 << 8);
        self.wouldbe.reserve(wouldbe_len);
        for class in 0..wouldbe_len {
            self.wouldbe
                .push(match machine.entry(START_OF_TEXT, class as u8) {
                    Ok(entry) if !is_actionable(&entry) => {
                        pack_wouldbe(entry.new_state, can_advance(&entry))
                    }
                    _ => WOULDBE_NONE,
                });
        }

        if n_classes > 0 {
            if let Ok(state_array) = machine.header.state_array() {
                let n_cells = state_array.data().len();
                let n_rows = n_cells.div_ceil(n_classes).min(u16::MAX as usize + 1);
                if n_rows > 64 {
                    let eot_tail_start = self.eot_tail.len();
                    self.eot_tail
                        .resize(eot_tail_start + (n_rows - 64).div_ceil(64), 0);
                    for state in 64..n_rows {
                        if let Ok(entry) = machine.entry(state as i32, class::END_OF_TEXT) {
                            if !is_actionable(&entry) {
                                let ix = state - 64;
                                self.eot_tail[eot_tail_start + ix / 64] |= 1 << (ix % 64);
                            }
                        }
                    }
                }
            }
        }

        SafeToBreakSubtable::new(
            wouldbe_len,
            wouldbe_start,
            self.wouldbe.len(),
            eot_tail_start,
            self.eot_tail.len(),
        )
    }
}

impl SafeToBreakSubtable {
    fn new(
        wouldbe_len: usize,
        wouldbe_start: usize,
        wouldbe_end: usize,
        eot_tail_start: usize,
        eot_tail_end: usize,
    ) -> Self {
        debug_assert_eq!(wouldbe_end - wouldbe_start, wouldbe_len);
        Self {
            wouldbe_start: wouldbe_start as u32,
            wouldbe_end: wouldbe_end as u32,
            eot_tail_start: eot_tail_start as u32,
            eot_tail_end: eot_tail_end as u32,
        }
    }

    /// The bytes of [`SafeToBreakAccel`] that this subtable's view covers.
    pub(crate) fn accel_bytes(&self) -> usize {
        (self.wouldbe_end - self.wouldbe_start) as usize * size_of::<i32>()
            + (self.eot_tail_end - self.eot_tail_start) as usize * size_of::<u64>()
    }
}

/// Returns how many columns of the extended state table at the start of
/// `data` can hold an entry. `nClasses` is not checked against the state
/// array, which runs from its offset to the end of the subtable, matching
/// how the table reader slices it; a column past its end fails `entry()`
/// in every state. Classes are u16 values, so columns past 0x10000 are
/// unreachable as well.
pub(crate) fn extended_class_count(n_classes: usize, data: &[u8]) -> usize {
    let n_cells = StateTableParts::read(FontData::new(data)).map_or(0, |parts| {
        data.len().saturating_sub(parts.state_array_offset as usize) / u16::RAW_BYTE_LEN
    });
    n_classes.min(n_cells).min(1 << 16)
}

// Keep one closure type for the morx and kerx class-set filters so that
// their lookup collectors share a monomorphization.
pub(crate) fn class_set_filter(classes: &U32Set) -> impl Fn(u16) -> bool + '_ {
    |class| classes.contains(u32::from(class))
}

pub trait TypedCollectGlyphs<T: LookupValue> {
    /// Add all indices into `set`.
    fn collect_glyphs(&self, set: &mut U32Set, num_glyphs: u32) {
        self.collect_glyphs_filtered::<_>(set, num_glyphs, |_| true);
    }

    /// For each valid index, read the value of type `T`.
    /// If `filter(&value)` returns true, insert the index into `set`.
    fn collect_glyphs_filtered<F>(&self, _set: &mut U32Set, _num_glyphs: u32, _filter: F)
    where
        F: Fn(T) -> bool;
}

impl<T> TypedCollectGlyphs<T> for TypedLookup<'_, T>
where
    T: LookupValue,
{
    fn collect_glyphs(&self, set: &mut U32Set, num_glyphs: u32) {
        self.lookup.collect_glyphs::<T>(set, num_glyphs);
    }
    fn collect_glyphs_filtered<F>(&self, set: &mut U32Set, num_glyphs: u32, filter: F)
    where
        F: Fn(T) -> bool,
    {
        self.lookup
            .collect_glyphs_filtered::<T, F>(set, num_glyphs, filter);
    }
}

pub trait CollectGlyphs {
    /// Add all indices into `set`.
    fn collect_glyphs<T>(&self, set: &mut U32Set, num_glyphs: u32)
    where
        T: LookupValue,
    {
        self.collect_glyphs_filtered::<T, _>(set, num_glyphs, |_| true);
    }

    /// For each valid index, read the value of type `T`.
    /// If `filter(&value)` returns true, insert the index into `set`.
    fn collect_glyphs_filtered<T, F>(&self, _set: &mut U32Set, _num_glyphs: u32, _filter: F)
    where
        T: LookupValue,
        F: Fn(T) -> bool;
}

impl CollectGlyphs for Lookup<'_> {
    fn collect_glyphs<T>(&self, set: &mut U32Set, num_glyphs: u32)
    where
        T: LookupValue,
    {
        match self {
            Lookup::Format0(lookup) => lookup.collect_glyphs::<T>(set, num_glyphs),
            Lookup::Format2(lookup) => lookup.collect_glyphs::<T>(set, num_glyphs),
            Lookup::Format4(lookup) => lookup.collect_glyphs::<T>(set, num_glyphs),
            Lookup::Format6(lookup) => lookup.collect_glyphs::<T>(set, num_glyphs),
            Lookup::Format8(lookup) => lookup.collect_glyphs::<T>(set, num_glyphs),
            Lookup::Format10(lookup) => lookup.collect_glyphs::<T>(set, num_glyphs),
        }
    }
    // Keep the shared collector out of the state-machine cache constructors.
    #[inline(never)]
    fn collect_glyphs_filtered<T, F>(&self, set: &mut U32Set, num_glyphs: u32, filter: F)
    where
        T: LookupValue,
        F: Fn(T) -> bool,
    {
        match self {
            Lookup::Format0(lookup) => {
                lookup.collect_glyphs_filtered::<T, F>(set, num_glyphs, filter);
            }
            Lookup::Format2(lookup) => {
                lookup.collect_glyphs_filtered::<T, F>(set, num_glyphs, filter);
            }
            Lookup::Format4(lookup) => {
                lookup.collect_glyphs_filtered::<T, F>(set, num_glyphs, filter);
            }
            Lookup::Format6(lookup) => {
                lookup.collect_glyphs_filtered::<T, F>(set, num_glyphs, filter);
            }
            Lookup::Format8(lookup) => {
                lookup.collect_glyphs_filtered::<T, F>(set, num_glyphs, filter);
            }
            Lookup::Format10(lookup) => {
                lookup.collect_glyphs_filtered::<T, F>(set, num_glyphs, filter);
            }
        }
    }
}

impl CollectGlyphs for Lookup0<'_> {
    fn collect_glyphs<T>(&self, set: &mut U32Set, num_glyphs: u32)
    where
        T: LookupValue,
    {
        set.insert_range(0..=num_glyphs.saturating_sub(1));
    }
    fn collect_glyphs_filtered<T, F>(&self, set: &mut U32Set, num_glyphs: u32, filter: F)
    where
        T: LookupValue,
        F: Fn(T) -> bool,
    {
        if let Ok(values) = self.values::<T>() {
            for (i, value) in values.iter().take(num_glyphs as usize).enumerate() {
                if filter(value.get()) {
                    set.insert(i as u32);
                }
            }
        }
    }
}
/// Insert a union of segment ranges without repeatedly rewriting the same
/// bitset pages. Malformed lookups may overlap or list ranges out of order.
fn insert_glyph_ranges(set: &mut U32Set, ranges: impl Iterator<Item = (u32, u32)>) {
    let mut ranges: Vec<_> = ranges
        .filter(|&(first, last)| first != DELETED_GLYPH && first <= last)
        .collect();
    ranges.sort_unstable();
    let mut merged: Option<(u32, u32)> = None;
    for (first, last) in ranges {
        if let Some((start, end)) = merged {
            if first <= end + 1 {
                merged = Some((start, end.max(last)));
            } else {
                set.insert_range(start..=end);
                merged = Some((first, last));
            }
        } else {
            merged = Some((first, last));
        }
    }
    if let Some((first, last)) = merged {
        set.insert_range(first..=last);
    }
}

impl CollectGlyphs for Lookup2<'_> {
    fn collect_glyphs_filtered<T, F>(&self, set: &mut U32Set, _num_glyphs: u32, filter: F)
    where
        T: LookupValue,
        F: Fn(T) -> bool,
    {
        if let Ok(segments) = self.segments::<T>() {
            insert_glyph_ranges(
                set,
                segments.iter().filter_map(|segment| {
                    let value = segment.value;
                    filter(value.get()).then_some((
                        u32::from(segment.first_glyph.get()),
                        u32::from(segment.last_glyph.get()),
                    ))
                }),
            );
        }
    }
}
impl CollectGlyphs for Lookup4<'_> {
    fn collect_glyphs<T>(&self, set: &mut U32Set, _num_glyphs: u32)
    where
        T: LookupValue,
    {
        insert_glyph_ranges(
            set,
            self.segments().iter().map(|segment| {
                (
                    u32::from(segment.first_glyph.get()),
                    u32::from(segment.last_glyph.get()),
                )
            }),
        );
    }
    fn collect_glyphs_filtered<T, F>(&self, set: &mut U32Set, _num_glyphs: u32, filter: F)
    where
        T: LookupValue,
        F: Fn(T) -> bool,
    {
        // Walk disjoint segments once. Overlapping or unordered segments
        // contribute unfiltered ranges, keeping the result conservative
        // without repeatedly scanning aliased arrays or rewriting set pages.
        let mut next_glyph = 0u32;
        let mut unwalked = Vec::new();
        for (segment_idx, segment) in self.segments().iter().enumerate() {
            let first_glyph = segment.first_glyph.get() as u32;
            let last_glyph = segment.last_glyph.get() as u32;
            if first_glyph == DELETED_GLYPH || first_glyph > last_glyph {
                continue;
            }
            if first_glyph < next_glyph {
                unwalked.push((first_glyph, last_glyph));
                continue;
            }
            next_glyph = last_glyph + 1;
            if let Ok(segment_values) = self.segment_values(segment_idx) {
                for (i, value) in segment_values.iter().enumerate() {
                    if filter(value.get()) {
                        set.insert(first_glyph + i as u32);
                    }
                }
            }
        }
        insert_glyph_ranges(set, unwalked.into_iter());
    }
}
impl CollectGlyphs for Lookup6<'_> {
    fn collect_glyphs_filtered<T, F>(&self, set: &mut U32Set, _num_glyphs: u32, filter: F)
    where
        T: LookupValue,
        F: Fn(T) -> bool,
    {
        let entries = self.entries();
        if let Ok(entries) = entries {
            for entry in entries {
                let value = entry.value;
                if filter(value.get()) {
                    if entry.glyph.get() as u32 == DELETED_GLYPH {
                        continue;
                    }
                    set.insert(entry.glyph.get() as u32);
                }
            }
        }
    }
}
impl CollectGlyphs for Lookup8<'_> {
    fn collect_glyphs<T>(&self, set: &mut U32Set, _num_glyphs: u32)
    where
        T: LookupValue,
    {
        let n_values = self.value_array().len();
        let first_glyph = self.first_glyph();
        if first_glyph as u32 == DELETED_GLYPH {
            return;
        }
        set.insert_range(
            first_glyph as u32..=first_glyph as u32 + n_values.saturating_sub(1) as u32,
        );
    }
    fn collect_glyphs_filtered<T, F>(&self, set: &mut U32Set, _num_glyphs: u32, filter: F)
    where
        T: LookupValue,
        F: Fn(T) -> bool,
    {
        let values = self.value_array();
        let first_glyph = self.first_glyph();
        if first_glyph as u32 == DELETED_GLYPH {
            return;
        }
        for (i, value) in values.iter().enumerate() {
            if filter(T::from_u16(value.get())) {
                set.insert(first_glyph as u32 + i as u32);
            }
        }
    }
}
impl CollectGlyphs for Lookup10<'_> {
    fn collect_glyphs<T>(&self, set: &mut U32Set, _num_glyphs: u32)
    where
        T: LookupValue,
    {
        let n_values = self.glyph_count();
        let first_glyph = self.first_glyph();
        if first_glyph as u32 == DELETED_GLYPH {
            return;
        }
        set.insert_range(
            first_glyph as u32..=first_glyph as u32 + n_values.saturating_sub(1) as u32,
        );
    }
    fn collect_glyphs_filtered<T, F>(&self, set: &mut U32Set, _num_glyphs: u32, filter: F)
    where
        T: LookupValue,
        F: Fn(T) -> bool,
    {
        let first_glyph = self.first_glyph();
        if first_glyph as u32 == DELETED_GLYPH {
            return;
        }
        for i in 0..self.glyph_count() {
            let idx = first_glyph as u32 + i as u32;
            // TODO: Speed up by accessing the value array directly
            let value = self.value::<T>(idx as u16);
            if let Ok(value) = value {
                if filter(value) {
                    set.insert(idx);
                }
            }
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::{Buffer, Direction, ShapePlan, ShaperFont};
    use core::mem::size_of;

    #[test]
    fn safe_to_break_distinguishes_signed_states_and_advance() {
        for state in [-65535, -1, 0, 32767, 32768, 65535] {
            for advance in [false, true] {
                let wouldbe = [pack_wouldbe(state, advance), WOULDBE_NONE];
                let safe = SafeToBreak {
                    n_classes: 2,
                    wouldbe: &wouldbe,
                    eot_tail: &[],
                };
                assert!(safe.wouldbe_matches(0, state, advance));
                assert!(!safe.wouldbe_matches(0, state, !advance));
                assert!(!safe.wouldbe_matches(0, state ^ 0x8000, advance));
                assert!(!safe.wouldbe_matches(1, state, advance));
            }
        }
    }

    #[test]
    fn safe_to_break_subtable_views_are_packed_and_bounded() {
        assert_eq!(size_of::<SafeToBreakSubtable>(), 16);

        let mut wouldbe = alloc::vec![WOULDBE_NONE; 8];
        wouldbe[class::OUT_OF_BOUNDS as usize] = pack_wouldbe(7, true);
        wouldbe[4 + class::OUT_OF_BOUNDS as usize] = pack_wouldbe(9, false);
        let first_subtable = SafeToBreakSubtable::new(4, 0, 4, 0, 1);
        let empty_subtable = SafeToBreakSubtable {
            wouldbe_start: 4,
            wouldbe_end: 4,
            eot_tail_start: 1,
            eot_tail_end: 1,
        };
        let last_subtable = SafeToBreakSubtable::new(4, 4, 8, 1, 2);
        let accel = SafeToBreakAccel {
            wouldbe,
            eot_tail: alloc::vec![1, 2],
        };

        let first = accel.subtable(first_subtable).unwrap();
        assert!(first.wouldbe_matches(99, 7, true));
        assert!(first.eot_safe_high(64));
        assert!(!first.eot_safe_high(65));
        assert!(!first.eot_safe_high(128));

        let empty = accel.subtable(empty_subtable).unwrap();
        assert_eq!(empty.wouldbe, []);
        assert_eq!(empty.eot_tail, []);

        let last = accel.subtable(last_subtable).unwrap();
        assert!(last.wouldbe_matches(99, 9, false));
        assert!(!last.eot_safe_high(64));
        assert!(last.eot_safe_high(65));

        assert_eq!(first_subtable.accel_bytes(), 4 * 4 + 8);
        assert_eq!(empty_subtable.accel_bytes(), 0);
    }

    #[test]
    fn set_heap_bytes_counts_each_page_once() {
        let mut set = U32Set::default();
        assert_eq!(set_heap_bytes(&set), 0);
        set.extend_unsorted([1, 3, 511, 512, 2000]);
        assert_eq!(set_heap_bytes(&set), 3 * 80);
        set.insert_range(0..=0xFFFE);
        assert_eq!(set_heap_bytes(&set), 128 * 80);
    }

    #[test]
    fn safe_to_break_table_is_bounded_by_the_state_array() {
        use read_fonts::{types::BigEndian, FontRead};

        #[rustfmt::skip]
        let data = [
            0, 1, 0, 0, // nClasses: 65536
            0, 0, 0, 16, // classTableOffset
            0, 0, 0, 24, // stateArrayOffset
            0, 0, 0, 34, // entryTableOffset
            0, 8, 0, 1, 0, 1, 0, 4, // lookup format 8: glyph 1 is class 4
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // five cells, followed by the entry
            0, 0, 0, 0, 0xFF, 0xFF, // entry 0
        ];
        let machine = ExtendedStateTable::<BigEndian<u16>>::read(FontData::new(&data)).unwrap();
        // The state array is read to the end of the data: 8 cells.
        assert_eq!(extended_class_count(machine.n_classes, &data), 8);
        let mut accel = SafeToBreakAccel::default();
        let subtable = accel.build_extended(&machine, &data, &|_| false, &|_| true);
        assert_eq!(accel.wouldbe.len(), 8);
        assert!(accel
            .subtable(subtable)
            .unwrap()
            .wouldbe_matches(4, 0, true));
    }

    #[test]
    fn output_deleted_glyph_at_end_of_text_marks_output() {
        let font_data = include_bytes!("../../tests/fonts/text-rendering-tests/TestMORXOne.ttf");
        let source = crate::font::Font::new(font_data.to_vec(), 0).unwrap();
        let font = source.instance_builder().build();
        let shaper_font = ShaperFont::new(&font);
        let plan = ShapePlan::new(&font, Direction::LeftToRight, None, None, &[]);

        let mut buffer = Buffer::new();
        buffer.push('A' as u32, 0);
        buffer.clear_output();
        buffer.next_glyph();

        let mut context = AatApplyContext::new(
            &plan,
            shaper_font.layout(),
            Scale::default(),
            &mut buffer,
            #[cfg(feature = "tracing")]
            &shaper_font,
        );
        context.output_glyph(DELETED_GLYPH);

        assert_eq!(context.buffer.out_len, 2);
        assert_eq!(context.buffer.prev().unwrap().glyph_id, DELETED_GLYPH);
        assert!(context.buffer.prev().unwrap().is_aat_deleted());
    }

    #[test]
    fn ligature_deletion_enables_later_subtables_in_short_and_cached_buffers() {
        let data = include_bytes!(
            "../../tests/fonts/in-house/007e2c13e6baf28a7f1e8f981c2db0cb850b1f1f.ttf"
        );
        let font = crate::Font::new(data.to_vec(), 0).unwrap();
        let shaper_font = ShaperFont::new(&font);
        // The font ligates 1,2 into 3, then a deleted-glyph transition
        // enables substitution 4 -> 5. Four glyphs enable the glyph set.
        for (text, expected) in [("abc", &[3, 5][..]), ("abcc", &[3, 5, 4][..])] {
            let mut buffer = Buffer::new();
            buffer.push_str(text);
            buffer.set_direction(Direction::LeftToRight);
            crate::shape(&shaper_font, &mut buffer, crate::ShapeOptions::new()).unwrap();
            let glyphs: Vec<_> = buffer
                .glyph_infos()
                .iter()
                .map(|info| info.glyph_id)
                .collect();
            assert_eq!(glyphs, expected, "{text}");
        }
    }

    #[test]
    fn lookup2_merges_accepted_overlapping_and_unordered_ranges() {
        let mut words = alloc::vec![2u16, 6, 5, 0, 0, 0];
        for (first, last, value) in [
            (100, 199, 1),
            (0, 99, 1),
            (50, 149, 1),
            (300, 399, 0),
            (500, 499, 1),
        ] {
            words.extend([last, first, value]);
        }
        let data: Vec<u8> = words.iter().flat_map(|word| word.to_be_bytes()).collect();
        let lookup = <Lookup as read_fonts::FontRead>::read(FontData::new(&data)).unwrap();
        let mut set = U32Set::default();
        lookup.collect_glyphs_filtered::<u16, _>(&mut set, 600, |value| value == 1);
        assert_eq!(set.len(), 200);
        assert!((0..200).all(|glyph| set.contains(glyph)));
        assert!(!set.contains(300));
    }

    #[test]
    fn lookup4_overlapping_segments_are_walked_once() {
        // A format 4 lookup with 100 segments that all cover glyphs 0..=999
        // and share one value array, followed by one well-formed segment.
        let n_segments = 101u16;
        let values_offset = 12 + 6 * n_segments;
        let mut words = alloc::vec![4u16, 6, n_segments, 0, 0, 0];
        for _ in 0..100 {
            words.extend([999, 0, values_offset]);
        }
        words.extend([1999, 1000, values_offset]);
        words.extend([1u16; 1000]);
        let data: Vec<u8> = words.iter().flat_map(|word| word.to_be_bytes()).collect();
        let lookup = <Lookup as read_fonts::FontRead>::read(FontData::new(&data)).unwrap();

        let filter_calls = core::cell::Cell::new(0u32);
        let mut set = U32Set::default();
        lookup.collect_glyphs_filtered::<u16, _>(&mut set, 2000, |class| {
            filter_calls.set(filter_calls.get() + 1);
            class == 1
        });

        // Each glyph is visited once, not once per segment that covers it.
        assert_eq!(filter_calls.get(), 2000);
        assert!((0..2000).all(|glyph| set.contains(glyph)));
        assert!(!set.contains(2000));
    }
}

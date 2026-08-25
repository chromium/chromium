use super::layout::DELETED_GLYPH;
use super::map::RangeFlags;
use crate::hb::buffer::{hb_buffer_t, HB_BUFFER_SCRATCH_FLAG_SHAPER0};
use crate::hb::face::hb_font_t;
use crate::hb::face::Scale;
use crate::hb::hb_mask_t;
use crate::hb::ot_layout_gsubgpos::MappingCache;
use crate::hb::ot_shape_plan::hb_ot_shape_plan_t;
use crate::U32Set;
use alloc::vec::Vec;
use read_fonts::tables::aat::*;
use read_fonts::types::{FixedSize, GlyphId};
use read_fonts::FontData;

pub const HB_BUFFER_SCRATCH_FLAG_AAT_HAS_DELETED: u32 = HB_BUFFER_SCRATCH_FLAG_SHAPER0;

pub(crate) const START_OF_TEXT: u16 = 0;

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
pub struct AatApplyContext<'a> {
    pub plan: &'a hb_ot_shape_plan_t,
    pub face: &'a hb_font_t<'a>,
    pub scale: Scale,
    pub buffer: &'a mut hb_buffer_t,
    pub has_glyph_classes: bool,
    pub range_flags: Option<&'a [RangeFlags]>,
    pub subtable_flags: hb_mask_t,
    pub(crate) buffer_is_reversed: bool,
    // Caches
    using_buffer_glyph_set: bool,
    pub(crate) first_set: Option<&'a U32Set>,
    pub(crate) second_set: Option<&'a U32Set>,
    pub(crate) machine_class_cache: Option<&'a ClassCache>,
    pub(crate) start_end_safe_to_break: u64,
    pub(crate) safe_to_break: SafeToBreak<'a>,
}

impl<'a> AatApplyContext<'a> {
    pub fn new(
        plan: &'a hb_ot_shape_plan_t,
        face: &'a hb_font_t<'a>,
        scale: Scale,
        buffer: &'a mut hb_buffer_t,
    ) -> Self {
        Self {
            plan,
            face,
            scale,
            buffer,
            has_glyph_classes: face.ot_tables.has_glyph_classes(),
            range_flags: None,
            subtable_flags: 0,
            buffer_is_reversed: false,
            using_buffer_glyph_set: false,
            first_set: None,
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

    pub(crate) fn buffer_intersects_machine(&self) -> bool {
        if let Some(first_set) = &self.first_set {
            if self.using_buffer_glyph_set {
                return self.buffer.glyph_set.intersects_set(first_set);
            }
            for info in &self.buffer.info {
                if first_set.contains(info.glyph_id) {
                    return true;
                }
            }
            false
        } else {
            true
        }
    }

    pub fn output_glyph(&mut self, glyph: u32) {
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
                return;
            }
        }

        if glyph == DELETED_GLYPH {
            self.buffer.scratch_flags |= HB_BUFFER_SCRATCH_FLAG_AAT_HAS_DELETED;
            let info = if at_end {
                self.buffer.prev_mut()
            } else {
                self.buffer.cur_mut(0)
            };
            info.set_aat_deleted();
        } else {
            if self.has_glyph_classes {
                let glyph_props = self.face.ot_tables.glyph_props(glyph.into());
                let info = if at_end {
                    self.buffer.prev_mut()
                } else {
                    self.buffer.cur_mut(0)
                };
                info.set_glyph_props(glyph_props);
            }
        }
        if !at_end {
            self.buffer.output_glyph(glyph);
        }
    }

    pub fn replace_glyph(&mut self, glyph: u32) {
        if glyph == DELETED_GLYPH {
            self.buffer.scratch_flags |= HB_BUFFER_SCRATCH_FLAG_AAT_HAS_DELETED;
            self.buffer.cur_mut(0).set_aat_deleted();
        }

        if self.using_buffer_glyph_set {
            self.buffer.glyph_set.insert(glyph);
        }
        if self.has_glyph_classes {
            self.buffer
                .cur_mut(0)
                .set_glyph_props(self.face.ot_tables.glyph_props(glyph.into()));
        }
        self.buffer.replace_glyph(glyph);
    }

    pub fn delete_glyph(&mut self) {
        self.buffer.scratch_flags |= HB_BUFFER_SCRATCH_FLAG_AAT_HAS_DELETED;
        self.buffer.cur_mut(0).set_aat_deleted();
        self.buffer.replace_glyph(DELETED_GLYPH);
    }

    pub fn replace_glyph_inplace(&mut self, i: usize, glyph: u32) {
        self.buffer.info[i].glyph_id = glyph;
        if glyph == DELETED_GLYPH {
            self.buffer.scratch_flags |= HB_BUFFER_SCRATCH_FLAG_AAT_HAS_DELETED;
            self.buffer.info[i].set_aat_deleted();
        }
        if self.using_buffer_glyph_set {
            self.buffer.glyph_set.insert(glyph);
        }
        if self.has_glyph_classes {
            self.buffer.info[i].set_glyph_props(self.face.ot_tables.glyph_props(glyph.into()));
        }
    }
}

/// Per-face acceleration for the drive loops' safe-to-break computation.
///
/// The data for every AAT state-machine subtable is stored in two packed
/// vectors. Each top-level subtable cache carries a compact descriptor for
/// its ranges. This avoids two `Vec` allocations in every subtable cache
/// while retaining O(classes) + O(states) storage.
#[derive(Default)]
pub(crate) struct SafeToBreakAccel {
    wouldbe: Vec<u16>,
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
    /// Per class: `entry(START_OF_TEXT, class)` packed as 15 bits of
    /// new state with the advance bit on top when present and
    /// non-actionable, `!0` otherwise — so condition 2c is a single
    /// compare. Real machines' state counts fit 15 bits.
    wouldbe: &'a [u16],
    /// Condition-3 bits for states 64 and up; states below 64 keep
    /// using the `start_end_safe_to_break` word. Empty for machines
    /// with at most 64 states.
    eot_tail: &'a [u64],
}

pub(crate) const WOULDBE_NONE: u16 = !0;

#[inline(always)]
pub(crate) fn pack_wouldbe(new_state: u16, advance: bool) -> u16 {
    // States fit 15 bits in any real font. A pathological new state
    // that doesn't can alias the sentinel and misreport condition 2c,
    // which only perturbs unsafe-to-break flags -- not worth runtime
    // checks on this path.
    new_state | ((advance as u16) << 15)
}

impl SafeToBreak<'_> {
    /// Condition 2c: would starting from the start state on this class
    /// take a non-actionable transition to the same state, advancing
    /// the same way?
    #[inline(always)]
    pub(crate) fn wouldbe_matches(&self, class: u16, next_state: u16, advance: bool) -> bool {
        let mut class = class as usize;
        if class >= self.n_classes as usize {
            class = class::OUT_OF_BOUNDS as usize;
        }
        self.wouldbe.get(class).copied() == Some(pack_wouldbe(next_state, advance))
    }

    /// Condition 3 for states 64 and up: no end-of-text action can fire
    /// out of this state.
    #[inline(always)]
    pub(crate) fn eot_safe_high(&self, state: u16) -> bool {
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
        // Classes are u16 values, so columns past 0x10000 are
        // unreachable and need no slots.
        let wouldbe_len = n_classes
            .max(class::OUT_OF_BOUNDS as usize + 1)
            .min(1 << 16);
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
                            machine.entry(state as u16, u16::from(class::END_OF_TEXT))
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
            n_classes,
            wouldbe_start,
            self.wouldbe.len(),
            eot_tail_start,
            self.eot_tail.len(),
        )
    }

    /// The legacy (`kern` Format1) counterpart of [`Self::build_extended`]:
    /// one-byte state cells and u8 classes.
    #[inline(never)]
    pub(crate) fn build_legacy(
        &mut self,
        machine: &StateTable,
        is_actionable: &dyn Fn(&StateEntry) -> bool,
        can_advance: &dyn Fn(&StateEntry) -> bool,
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
                        if let Ok(entry) = machine.entry(state as u16, class::END_OF_TEXT) {
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
            n_classes,
            wouldbe_start,
            self.wouldbe.len(),
            eot_tail_start,
            self.eot_tail.len(),
        )
    }
}

impl SafeToBreakSubtable {
    fn new(
        n_classes: usize,
        wouldbe_start: usize,
        wouldbe_end: usize,
        eot_tail_start: usize,
        eot_tail_end: usize,
    ) -> Self {
        let wouldbe_len = n_classes
            .max(class::OUT_OF_BOUNDS as usize + 1)
            .min(1 << 16);
        debug_assert_eq!(wouldbe_end - wouldbe_start, wouldbe_len);
        Self {
            wouldbe_start: wouldbe_start as u32,
            wouldbe_end: wouldbe_end as u32,
            eot_tail_start: eot_tail_start as u32,
            eot_tail_end: eot_tail_end as u32,
        }
    }
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
impl CollectGlyphs for Lookup2<'_> {
    fn collect_glyphs_filtered<T, F>(&self, set: &mut U32Set, _num_glyphs: u32, filter: F)
    where
        T: LookupValue,
        F: Fn(T) -> bool,
    {
        if let Ok(segments) = self.segments::<T>() {
            for segment in segments {
                let value = segment.value;
                if filter(value.get()) {
                    if segment.first_glyph.get() as u32 == DELETED_GLYPH {
                        continue;
                    }
                    set.insert_range(
                        segment.first_glyph.get() as u32..=segment.last_glyph.get() as u32,
                    );
                }
            }
        }
    }
}
impl CollectGlyphs for Lookup4<'_> {
    fn collect_glyphs<T>(&self, set: &mut U32Set, _num_glyphs: u32)
    where
        T: LookupValue,
    {
        for segment in self.segments() {
            if segment.first_glyph.get() as u32 == DELETED_GLYPH {
                continue;
            }
            set.insert_range(segment.first_glyph.get() as u32..=segment.last_glyph.get() as u32);
        }
    }
    fn collect_glyphs_filtered<T, F>(&self, set: &mut U32Set, _num_glyphs: u32, filter: F)
    where
        T: LookupValue,
        F: Fn(T) -> bool,
    {
        for (segment_idx, segment) in self.segments().iter().enumerate() {
            if segment.first_glyph.get() as u32 == DELETED_GLYPH {
                continue;
            }
            let segment_values = self.segment_values(segment_idx);
            if let Ok(segment_values) = segment_values {
                for (i, value) in segment_values.iter().enumerate() {
                    if filter(value.get()) {
                        set.insert(segment.first_glyph.get() as u32 + i as u32);
                    }
                }
            }
        }
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
    use crate::{Direction, FontRef, ShapePlan, ShaperData, UnicodeBuffer};
    use core::mem::size_of;

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
        assert!(empty.wouldbe.is_empty());
        assert!(empty.eot_tail.is_empty());

        let last = accel.subtable(last_subtable).unwrap();
        assert!(last.wouldbe_matches(99, 9, false));
        assert!(!last.eot_safe_high(64));
        assert!(last.eot_safe_high(65));
    }

    #[test]
    fn output_deleted_glyph_at_end_of_text_marks_output() {
        let font_data = include_bytes!("../../../tests/fonts/text-rendering-tests/TestMORXOne.ttf");
        let font = FontRef::new(font_data).unwrap();
        let shaper_data = ShaperData::new(&font);
        let shaper = shaper_data.shaper(&font).build();
        let plan = ShapePlan::new(&shaper, Direction::LeftToRight, None, None, &[]);

        let mut unicode_buffer = UnicodeBuffer::new();
        unicode_buffer.add('A', 0);
        let mut buffer = unicode_buffer.0;
        buffer.clear_output();
        buffer.next_glyph();

        let mut context = AatApplyContext::new(&plan, &shaper, Scale::default(), &mut buffer);
        context.output_glyph(DELETED_GLYPH);

        assert_eq!(context.buffer.out_len, 2);
        assert_eq!(context.buffer.prev().glyph_id, DELETED_GLYPH);
        assert!(context.buffer.prev().is_aat_deleted());
    }
}

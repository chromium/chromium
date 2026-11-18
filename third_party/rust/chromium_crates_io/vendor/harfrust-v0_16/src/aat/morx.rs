use super::layout::*;
use super::map::{budget_spend, AatMap, AatMapBuilder, RangeFlags};
use super::AatData;
use crate::aat::common::{
    extended_class_count, set_digest, set_heap_bytes, AatApplyContext, ClassCache,
    SafeToBreakAccel, SafeToBreakSubtable, TypedCollectGlyphs, START_OF_TEXT,
};
use crate::ot::layout::MAX_CONTEXT_LENGTH;
use crate::set_digest::SetDigest;
use crate::tag::lang_matches;
use crate::Language;
use crate::U32Set;
use alloc::{vec, vec::Vec};
use core::mem::size_of;
use read_fonts::tables::aat::{self, ExtendedStateTable, NoPayload, StateEntry, StateTable};
use read_fonts::tables::{mort, morx};
use read_fonts::types::{BigEndian, FixedSize, GlyphId, GlyphId16};
use read_fonts::ReadError;

trait MorphChain {
    fn default_flags(&self) -> u32;
    fn feature_count(&self) -> usize;
    fn for_each_feature(&self, f: impl FnMut(u16, u16, u32, u32));
}

impl MorphChain for morx::Chain<'_> {
    fn default_flags(&self) -> u32 {
        self.default_flags()
    }

    fn feature_count(&self) -> usize {
        self.features().len()
    }

    fn for_each_feature(&self, mut f: impl FnMut(u16, u16, u32, u32)) {
        for feature in self.features() {
            f(
                feature.feature_type(),
                feature.feature_settings(),
                feature.enable_flags(),
                feature.disable_flags(),
            );
        }
    }
}

impl MorphChain for mort::Chain<'_> {
    fn default_flags(&self) -> u32 {
        self.default_flags()
    }

    fn feature_count(&self) -> usize {
        self.features().len()
    }

    fn for_each_feature(&self, mut f: impl FnMut(u16, u16, u32, u32)) {
        for feature in self.features() {
            f(
                feature.feature_type(),
                feature.feature_settings(),
                feature.enable_flags(),
                feature.disable_flags(),
            );
        }
    }
}

// Chain::compile_flags in harfbuzz
pub fn compile_flags(aat: &AatData, builder: &AatMapBuilder, map: &mut AatMap) -> Option<()> {
    let has_feature = |kind: u16, setting: u16| {
        builder
            .current_features
            .binary_search_by(|probe| {
                if probe.kind != kind {
                    probe.kind.cmp(&kind)
                } else {
                    probe.setting.cmp(&setting)
                }
            })
            .is_ok()
    };
    let language_matches = |setting: u16| {
        let Some(index) = setting.checked_sub(1) else {
            return false;
        };
        let Some(requested) = builder.language.as_ref() else {
            return false;
        };
        let Some(ltag) = aat.ltag.as_ref() else {
            return false;
        };
        // Index the record array directly, as harfbuzz does. `Ltag::tag_indices`
        // slices and validates every preceding string, and this runs once per
        // language-tag feature entry of every chain.
        let Some(range) = ltag.tag_ranges().get(usize::from(index)) else {
            return false;
        };
        // A tag longer than the requested language cannot be a prefix of it.
        let len = usize::from(range.length());
        if len > requested.as_bytes().len() {
            return false;
        }
        let start = usize::from(range.offset());
        let Some(tag) = ltag
            .offset_data()
            .as_bytes()
            .get(start..start + len)
            .and_then(|bytes| core::str::from_utf8(bytes).ok())
        else {
            return false;
        };
        let Some(language) = Language::new(tag) else {
            return false;
        };
        lang_matches(requested.as_bytes(), language.as_bytes())
    };

    fn compile_chain(
        chain: &impl MorphChain,
        has_feature: &impl Fn(u16, u16) -> bool,
        language_matches: &impl Fn(u16) -> bool,
        chain_flags: &mut Vec<RangeFlags>,
        builder: &AatMapBuilder,
        budget: &mut i64,
    ) {
        // Charge this chain's feature records; the caller charges the base
        // per-chain cost for every chain it visits and stops compiling further
        // ranges once the map's budget is exhausted.
        budget_spend(budget, 1, chain.feature_count() as i64);

        let mut flags = chain.default_flags();
        chain.for_each_feature(
            |feature_type, feature_setting, enable_flags, disable_flags| {
                // Check whether this type/setting pair was requested in the map,
                // and if so, apply its flags.
                if has_feature(feature_type, feature_setting) {
                    flags &= disable_flags;
                    flags |= enable_flags;
                } else if feature_type == FEATURE_TYPE_LETTER_CASE as u16
                    && feature_setting == u16::from(FEATURE_SELECTOR_SMALL_CAPS)
                {
                    // Deprecated. https://github.com/harfbuzz/harfbuzz/issues/1342
                    let ok = has_feature(
                        FEATURE_TYPE_LOWER_CASE as u16,
                        u16::from(FEATURE_SELECTOR_LOWER_CASE_SMALL_CAPS),
                    );
                    if ok {
                        flags &= disable_flags;
                        flags |= enable_flags;
                    }
                } else if feature_type == FEATURE_TYPE_LANGUAGE_TAG_TYPE as u16
                    && language_matches(feature_setting)
                {
                    flags &= disable_flags;
                    flags |= enable_flags;
                }
            },
        );

        // Ranges are compiled in order and are contiguous. Extend the last
        // entry instead of starting a new one when this chain does not tell
        // the two ranges apart; the map would otherwise hold one entry per
        // chain per range, a product the font and the caller's features do
        // not bound. Consumers only read flags per cluster, so this is
        // output-equivalent.
        match chain_flags.last_mut() {
            Some(last) if last.flags == flags => {
                last.cluster_last = builder.range_last as u32;
            }
            _ => {
                // Stored ranges cost memory; charge them more than visited ones.
                budget_spend(budget, 16, 1);
                chain_flags.push(RangeFlags {
                    flags,
                    cluster_first: builder.range_first as u32,
                    cluster_last: builder.range_last as u32,
                });
            }
        }
    }

    let AatMap {
        chain_flags: map_chain_flags,
        budget,
    } = map;

    if let Some(morx) = aat.morx.as_ref() {
        let chains = morx.chains();
        map_chain_flags.resize(chains.iter().count(), vec![]);
        for (chain, chain_flags) in chains.iter().zip(map_chain_flags.iter_mut()) {
            // Charge every chain the iterator yields, even one too malformed to
            // parse, so a run of unparseable chains cannot make map compilation
            // quadratic in the number of ranges without spending any budget.
            budget_spend(budget, 1, 1);
            if let Ok(chain) = chain {
                compile_chain(
                    &chain,
                    &has_feature,
                    &language_matches,
                    chain_flags,
                    builder,
                    budget,
                );
            }
        }
    } else {
        let chains = aat.mort.as_ref()?.chains();
        map_chain_flags.resize(chains.iter().count(), vec![]);
        for (chain, chain_flags) in chains.iter().zip(map_chain_flags.iter_mut()) {
            // Charge every chain the iterator yields, even one too malformed to
            // parse, so a run of unparseable chains cannot make map compilation
            // quadratic in the number of ranges without spending any budget.
            budget_spend(budget, 1, 1);
            if let Ok(chain) = chain {
                compile_chain(
                    &chain,
                    &has_feature,
                    &language_matches,
                    chain_flags,
                    builder,
                    budget,
                );
            }
        }
    }

    Some(())
}

// Chain::apply in harfbuzz
pub fn apply<'a>(c: &mut AatApplyContext<'a, '_, '_>, map: &'a AatMap) -> Option<()> {
    c.buffer.unsafe_to_concat(None, None);

    c.setup_buffer_glyph_set();

    let cache = if c.layout.aat.morx.is_some() {
        c.layout.aat.morx_cache()?
    } else {
        c.layout.aat.mort_cache()?
    };
    c.setup_buffer_digest(cache.descriptors.len());
    apply_table(
        c,
        map,
        &cache.safe_to_break,
        &cache.subtables,
        &cache.descriptors,
    )
}

fn apply_table<'a>(
    c: &mut AatApplyContext<'a, '_, '_>,
    map: &'a AatMap,
    safe_to_break: &'a SafeToBreakAccel,
    subtable_caches: &'a [MorphSubtableCache<'a>],
    descriptors: &'a [MorphSubtableDescriptor],
) -> Option<()> {
    let mut last_chain_index = u32::MAX;
    let mut chain_flags = None;

    for (subtable_idx, desc) in descriptors.iter().enumerate() {
        if desc.chain_index != last_chain_index {
            // Chain boundary: restore buffer order, load this chain's flags.
            if c.buffer_is_reversed {
                c.reverse_buffer();
            }
            last_chain_index = desc.chain_index;
            chain_flags = map.chain_flags.get(desc.chain_index as usize);
        }
        let Some(chain_flags) = chain_flags else {
            continue;
        };
        c.range_flags = Some(chain_flags.as_slice());

        if chain_flags.len() == 1 && (desc.sub_feature_flags & chain_flags[0].flags == 0) {
            continue;
        }

        let coverage = desc.coverage;
        let is_all_directions = coverage & 0x2000_0000 != 0;
        let is_vertical = coverage & 0x8000_0000 != 0;
        if !is_all_directions && c.buffer.direction.is_vertical() != is_vertical {
            continue;
        }

        let subtable_cache = &subtable_caches[subtable_idx];
        c.subtable_flags = desc.sub_feature_flags;
        c.first_set = Some(&subtable_cache.glyph_set);
        c.first_digest = subtable_cache.glyph_digest.clone();
        c.machine_class_cache = Some(&subtable_cache.class_cache);
        c.start_end_safe_to_break = subtable_cache.start_end_safe_to_break;
        c.safe_to_break = safe_to_break.subtable(subtable_cache.safe_to_break)?;

        if !c.buffer_intersects_machine() {
            message!(
                c,
                "skipped chainsubtable {} because no glyph matches",
                subtable_idx
            );
            continue;
        }

        message_continue!(c, "start chainsubtable {}", subtable_idx);

        // Buffer contents is always in logical direction.  Determine if
        // we need to reverse before applying this subtable.  We reverse
        // back after if we did reverse indeed.
        //
        // See the coverage bits table in the `morx` spec for the meaning
        // of is_logical/is_backwards here.
        let is_logical = coverage & 0x1000_0000 != 0;
        let is_backwards = coverage & 0x4000_0000 != 0;
        let reverse = if is_logical {
            is_backwards
        } else {
            is_backwards != c.buffer.direction.is_backward()
        };

        if reverse != c.buffer_is_reversed {
            c.reverse_buffer();
        }

        match &subtable_cache.kind {
            Some(MorphSubtableKind::Morx(kind)) => apply_morx_subtable(kind, c),
            Some(MorphSubtableKind::Mort(kind)) => apply_mort_subtable(kind, c),
            None => {}
        }
        message!(c, "end chainsubtable {}", subtable_idx);
    }
    if c.buffer_is_reversed {
        c.reverse_buffer();
    }

    Some(())
}

trait MorphStateTable<T>
where
    T: FixedSize + bytemuck::AnyBitPattern,
{
    /// The number of classes that can have entries in the machine at the
    /// start of `data`.
    fn n_classes(&self, data: &[u8]) -> usize;
    fn class(&self, glyph_id: GlyphId) -> Result<u16, ReadError>;
    fn entry(&self, state: i32, class: u16) -> Result<StateEntry<T>, ReadError>;
    fn collect_glyphs_filtered(
        &self,
        glyphs: &mut U32Set,
        num_glyphs: u32,
        filter: impl Fn(u16) -> bool,
    );
    fn build_safe_to_break(
        &self,
        accel: &mut SafeToBreakAccel,
        data: &[u8],
        is_actionable: &dyn Fn(&StateEntry<T>) -> bool,
        can_advance: &dyn Fn(&StateEntry<T>) -> bool,
    ) -> SafeToBreakSubtable;
}

impl<T> MorphStateTable<T> for ExtendedStateTable<'_, T>
where
    T: FixedSize + bytemuck::AnyBitPattern,
{
    fn n_classes(&self, data: &[u8]) -> usize {
        extended_class_count(self.n_classes, data)
    }

    fn class(&self, glyph_id: GlyphId) -> Result<u16, ReadError> {
        self.class(glyph_id)
    }

    fn entry(&self, state: i32, class: u16) -> Result<StateEntry<T>, ReadError> {
        self.entry(state, class)
    }

    fn collect_glyphs_filtered(
        &self,
        glyphs: &mut U32Set,
        num_glyphs: u32,
        filter: impl Fn(u16) -> bool,
    ) {
        self.class_table
            .collect_glyphs_filtered(glyphs, num_glyphs, filter);
    }

    fn build_safe_to_break(
        &self,
        accel: &mut SafeToBreakAccel,
        data: &[u8],
        is_actionable: &dyn Fn(&StateEntry<T>) -> bool,
        can_advance: &dyn Fn(&StateEntry<T>) -> bool,
    ) -> SafeToBreakSubtable {
        accel.build_extended(self, data, is_actionable, can_advance)
    }
}

impl<T> MorphStateTable<T> for StateTable<'_, T>
where
    T: FixedSize + bytemuck::AnyBitPattern,
{
    fn n_classes(&self, _data: &[u8]) -> usize {
        self.n_classes
    }

    fn class(&self, glyph_id: GlyphId) -> Result<u16, ReadError> {
        let glyph_id = u16::try_from(glyph_id.to_u32()).map_err(|_| ReadError::OutOfBounds)?;
        self.class(GlyphId16::new(glyph_id)).map(u16::from)
    }

    fn entry(&self, state: i32, class: u16) -> Result<StateEntry<T>, ReadError> {
        let class = u8::try_from(class).unwrap_or(aat::class::OUT_OF_BOUNDS);
        self.entry(state, class)
    }

    fn collect_glyphs_filtered(
        &self,
        glyphs: &mut U32Set,
        num_glyphs: u32,
        filter: impl Fn(u16) -> bool,
    ) {
        let (first_glyph, classes) = self.class_mappings();
        for (i, class) in classes.iter().copied().enumerate() {
            let glyph = u32::from(first_glyph).saturating_add(i as u32);
            if glyph < num_glyphs && filter(u16::from(class)) {
                glyphs.insert(glyph);
            }
        }
    }

    fn build_safe_to_break(
        &self,
        accel: &mut SafeToBreakAccel,
        _data: &[u8],
        is_actionable: &dyn Fn(&StateEntry<T>) -> bool,
        can_advance: &dyn Fn(&StateEntry<T>) -> bool,
    ) -> SafeToBreakSubtable {
        accel.build_legacy(self, is_actionable, can_advance)
    }
}

fn get_morph_class<T, M>(machine: &M, glyph_id: GlyphId, cache: &ClassCache) -> u16
where
    T: FixedSize + bytemuck::AnyBitPattern,
    M: MorphStateTable<T>,
{
    if let Some(class) = cache.get(glyph_id.to_u32()) {
        return class as u16;
    }
    let class = machine
        .class(glyph_id)
        .unwrap_or(u16::from(aat::class::OUT_OF_BOUNDS));
    cache.set(glyph_id.to_u32(), u32::from(class));
    class
}

fn collect_initial_glyphs<T, M, Ctx>(machine: &M, data: &[u8], glyphs: &mut U32Set, num_glyphs: u32)
where
    T: FixedSize + bytemuck::AnyBitPattern,
    M: MorphStateTable<T>,
    Ctx: DriverContext<T>,
{
    let mut classes = U32Set::default();

    // Classes are u16 values, so columns past 0x10000 are unreachable.
    for i in 0..machine.n_classes(data) {
        if let Ok(entry) = machine.entry(START_OF_TEXT, i as u16) {
            if entry.new_state == START_OF_TEXT
                && !Ctx::is_action_initiable(&entry)
                && !Ctx::is_actionable(&entry)
            {
                continue;
            }
            classes.insert(i as u32);
        }
    }

    // And glyphs in those classes.

    let filter = crate::aat::common::class_set_filter(&classes);

    if filter(aat::class::DELETED_GLYPH as u16) {
        glyphs.insert(DELETED_GLYPH);
    }

    machine.collect_glyphs_filtered(glyphs, num_glyphs, filter);
}

fn collect_start_end_safe_to_break<T, M, Ctx>(machine: &M) -> u64
where
    T: FixedSize + bytemuck::AnyBitPattern,
    M: MorphStateTable<T>,
    Ctx: DriverContext<T>,
{
    let mut result = 0u64;
    for state in 0..64 {
        let bit = if let Ok(entry) = machine.entry(state, aat::class::END_OF_TEXT as u16) {
            !Ctx::is_actionable(&entry)
        } else {
            true
        };
        if bit {
            result |= 1 << state;
        }
    }
    result
}

pub(crate) trait DriverContext<T> {
    fn in_place() -> bool;
    fn can_advance(entry: &StateEntry<T>) -> bool;
    fn is_action_initiable(entry: &StateEntry<T>) -> bool;
    fn is_actionable(entry: &StateEntry<T>) -> bool;
    fn transition(&mut self, entry: &StateEntry<T>, ac: &mut AatApplyContext) -> Option<()>;
}

/// Returns the index of the range that holds `cluster`, given the range of
/// the previous glyph. The ranges are sorted and contiguous; the first starts
/// at zero and the last ends at `Feature::GLOBAL_END`.
#[inline(always)]
fn range_for_cluster(range_flags: &[RangeFlags], mut range: usize, cluster: u32) -> usize {
    // Monotone clusters stay in the range or move to a neighbour. Clusters
    // need not be monotone, though (non-monotone cluster levels, or caller
    // supplied cluster values): search for a far range, don't walk to it.
    for _ in 0..4 {
        if cluster < range_flags[range].cluster_first {
            range -= 1;
        } else if cluster > range_flags[range].cluster_last {
            range += 1;
        } else {
            return range;
        }
    }
    range_flags.partition_point(|range| range.cluster_last < cluster)
}

fn drive<T, M, Ctx>(machine: &M, c: &mut Ctx, ac: &mut AatApplyContext) -> Option<()>
where
    T: bytemuck::AnyBitPattern + FixedSize + core::fmt::Debug,
    M: MorphStateTable<T>,
    Ctx: DriverContext<T>,
{
    if !Ctx::in_place() {
        ac.buffer.clear_output();
    }

    let mut state = START_OF_TEXT;
    let mut last_range = ac.range_flags.as_ref().and_then(|rf| {
        if rf.len() > 1 {
            rf.first().map(|_| 0usize)
        } else {
            // If there's only one range, we already checked the flag.
            None
        }
    });
    // Condition 3 below, precomputed for the start-of-text state: no
    // end-of-text action can fire if we stop while in the start state.
    let start_state_safe_to_break_eot = (ac.start_end_safe_to_break & (1 << START_OF_TEXT)) != 0;
    ac.buffer.idx = 0;
    'drive: loop {
        // This block copied from NoncontextualSubtable::apply. Keep in sync.
        if let Some(range_flags) = ac.range_flags.as_ref() {
            if let Some(last_range) = last_range.as_mut() {
                let mut range = *last_range;
                if ac.buffer.idx < ac.buffer.len {
                    let cluster = ac.buffer.cur(0)?.cluster;
                    range = range_for_cluster(range_flags, range, cluster);
                    *last_range = range;
                }

                if range_flags[range].flags & ac.subtable_flags == 0 {
                    if ac.buffer.idx == ac.buffer.len || !ac.buffer.successful {
                        break;
                    }

                    state = START_OF_TEXT;

                    ac.buffer.next_glyph();
                    continue;
                }
            }
        }

        let class = if ac.buffer.idx < ac.buffer.len {
            get_morph_class::<T, _>(
                machine,
                ac.buffer.cur(0)?.as_glyph(),
                ac.machine_class_cache.unwrap(),
            )
        } else {
            u16::from(aat::class::END_OF_TEXT)
        };

        let Ok(entry) = machine.entry(state, class) else {
            break;
        };

        let next_state = entry.new_state;

        // Fast path for when transitioning from start-state to start-state with
        // no action and advancing. Do so as long as the class remains the same.
        // This is common with runs of non-actionable glyphs.
        if last_range.is_none()
            && state == START_OF_TEXT
            && next_state == START_OF_TEXT
            && start_state_safe_to_break_eot
            && !Ctx::is_actionable(&entry)
            && Ctx::can_advance(&entry)
        {
            let old_class = class;
            loop {
                c.transition(&entry, ac);
                if ac.buffer.idx >= ac.buffer.len || !ac.buffer.successful {
                    break 'drive;
                }
                ac.buffer.next_glyph();

                let new_class = if ac.buffer.idx < ac.buffer.len {
                    get_morph_class::<T, _>(
                        machine,
                        ac.buffer.cur(0)?.as_glyph(),
                        ac.machine_class_cache.unwrap(),
                    )
                } else {
                    u16::from(aat::class::END_OF_TEXT)
                };
                if new_class != old_class {
                    break;
                }
            }
            if ac.buffer.idx >= ac.buffer.len || !ac.buffer.successful {
                break 'drive;
            }
            continue 'drive;
        }

        // Conditions under which it's guaranteed safe-to-break before current glyph:
        //
        // 1. There was no action in this transition; and
        //
        // 2. If we break before current glyph, the results will be the same. That
        //    is guaranteed if:
        //
        //    2a. We were already in start-of-text state; or
        //
        //    2b. We are epsilon-transitioning to start-of-text state; or
        //
        //    2c. Starting from start-of-text state seeing current glyph:
        //
        //        2c'. There won't be any actions; and
        //
        //        2c". We would end up in the same state that we were going to end up
        //             in now, including whether epsilon-transitioning.
        //
        //    and
        //
        // 3. If we break before current glyph, there won't be any end-of-text action
        //    after previous glyph.
        //
        // This triples the transitions we need to look up, but is worth returning
        // granular unsafe-to-break results. See eg.:
        //
        //   https://github.com/harfbuzz/harfbuzz/issues/2860

        let is_safe_to_break =
            // 1
            !Ctx::is_actionable(&entry) &&

            // 2
            (
                state == START_OF_TEXT
                || (!Ctx::can_advance(&entry) && next_state == START_OF_TEXT)
                // 2c, 2c', 2c"
                || ac.safe_to_break.wouldbe_matches(class, next_state, Ctx::can_advance(&entry))
            ) &&

            // 3
            (
                if state < 0 {
                    machine.entry(state, aat::class::END_OF_TEXT as u16)
                        .map_or(true, |entry| !Ctx::is_actionable(&entry))
                } else if state < 64 {
                    (ac.start_end_safe_to_break & (1 << state)) != 0
                } else {
                    ac.safe_to_break.eot_safe_high(state)
                }
            )
        ;

        if !is_safe_to_break && ac.buffer.backtrack_len() > 0 && ac.buffer.idx < ac.buffer.len {
            ac.buffer.unsafe_to_break_from_outbuffer(
                Some(ac.buffer.backtrack_len() - 1),
                Some(ac.buffer.idx + 1),
            );
        }

        c.transition(&entry, ac);

        state = next_state;

        if ac.buffer.idx >= ac.buffer.len || !ac.buffer.successful {
            break;
        }

        if Ctx::can_advance(&entry) {
            ac.buffer.next_glyph();
        } else {
            if ac.buffer.max_ops <= 0 {
                ac.buffer.next_glyph();
            }
            ac.buffer.max_ops -= 1;
        }
    }

    if !Ctx::in_place() {
        ac.buffer.sync();
    }
    Some(())
}

fn apply_morx_subtable<'a>(kind: &'a morx::SubtableKind<'a>, ac: &mut AatApplyContext<'a, '_, '_>) {
    match kind {
        morx::SubtableKind::Rearrangement(table) => {
            let mut c = RearrangementCtx { start: 0, end: 0 };
            if drive(table, &mut c, ac).is_none() {
                ac.buffer.index_failed();
            }
        }
        morx::SubtableKind::Contextual(table) => {
            let mut c = ContextualCtx {
                mark_set: false,
                mark: 0,
                table: table.clone(),
            };
            if drive(&table.state_table, &mut c, ac).is_none() {
                ac.buffer.index_failed();
            }
        }
        morx::SubtableKind::Ligature(table) => {
            let mut c = LigatureCtx {
                table: table.clone(),
                match_length: 0,
                match_positions: [0; LIGATURE_MAX_MATCHES],
            };
            if drive(&table.state_table, &mut c, ac).is_none() {
                ac.buffer.index_failed();
            }
        }
        morx::SubtableKind::NonContextual(lookup) => {
            if apply_noncontextual(lookup, ac).is_none() {
                ac.buffer.index_failed();
            }
        }
        morx::SubtableKind::Insertion(table) => {
            let mut c = InsertionCtx {
                mark: 0,
                glyphs: table.glyphs,
            };
            if drive(&table.state_table, &mut c, ac).is_none() {
                ac.buffer.index_failed();
            }
        }
    }
}

fn apply_mort_subtable<'a>(kind: &'a mort::SubtableKind<'a>, ac: &mut AatApplyContext<'a, '_, '_>) {
    match kind {
        mort::SubtableKind::Rearrangement(table) => {
            let mut c = RearrangementCtx { start: 0, end: 0 };
            if drive(table, &mut c, ac).is_none() {
                ac.buffer.index_failed();
            }
        }
        mort::SubtableKind::Contextual(table) => {
            let mut c = ContextualCtx {
                mark_set: false,
                mark: 0,
                table: table.clone(),
            };
            if drive(&table.state_table, &mut c, ac).is_none() {
                ac.buffer.index_failed();
            }
        }
        mort::SubtableKind::Ligature(table) => {
            let mut c = LigatureCtx {
                table: table.clone(),
                match_length: 0,
                match_positions: [0; LIGATURE_MAX_MATCHES],
            };
            if drive(&table.state_table, &mut c, ac).is_none() {
                ac.buffer.index_failed();
            }
        }
        mort::SubtableKind::NonContextual(lookup) => {
            if apply_noncontextual(lookup, ac).is_none() {
                ac.buffer.index_failed();
            }
        }
        mort::SubtableKind::Insertion(table) => {
            let mut c = InsertionCtx {
                mark: 0,
                glyphs: table.glyphs,
            };
            if drive(&table.state_table, &mut c, ac).is_none() {
                ac.buffer.index_failed();
            }
        }
    }
}

fn apply_noncontextual(lookup: &aat::LookupU16, ac: &mut AatApplyContext) -> Option<()> {
    let mut last_range = ac.range_flags.as_ref().and_then(|rf| {
        if rf.len() > 1 {
            rf.first().map(|_| 0usize)
        } else {
            // If there's only one range, we already checked the flag.
            None
        }
    });

    for i in 0..ac.buffer.len {
        // This block copied from the state-table driver. Keep in sync.
        if let Some(range_flags) = ac.range_flags.as_ref() {
            if let Some(last_range) = last_range.as_mut() {
                let mut range = *last_range;
                if ac.buffer.idx < ac.buffer.len {
                    let cluster = ac.buffer.cur(0)?.cluster;
                    range = range_for_cluster(range_flags, range, cluster);
                    *last_range = range;
                }

                if range_flags[range].flags & ac.subtable_flags == 0 {
                    continue;
                }
            }
        }

        if let Some(glyph) = ac.buffer.info.get(i).and_then(|info| info.as_gid16()) {
            if let Ok(replacement) = lookup.value(glyph.to_u16()) {
                if ac.replace_glyph_inplace(i, replacement.into()).is_none() {
                    ac.buffer.index_failed();
                }
            }
        }
    }
    Some(())
}

struct RearrangementCtx {
    start: usize,
    end: usize,
}

impl RearrangementCtx {
    const MARK_FIRST: u16 = 0x8000;
    const DONT_ADVANCE: u16 = 0x4000;
    const MARK_LAST: u16 = 0x2000;
    const VERB: u16 = 0x000F;

    #[inline(never)]
    fn perform_action(&mut self, flags: u16, ac: &mut AatApplyContext) -> Option<()> {
        let buffer = &mut *ac.buffer;
        // The following map has two nibbles, for start-side
        // and end-side. Values of 0,1,2 mean move that many
        // to the other side. Value of 3 means move 2 and
        // flip them.
        static MAP: [u8; 16] = [
            0x00, // 0  no change
            0x10, // 1  Ax => xA
            0x01, // 2  xD => Dx
            0x11, // 3  AxD => DxA
            0x20, // 4  ABx => xAB
            0x30, // 5  ABx => xBA
            0x02, // 6  xCD => CDx
            0x03, // 7  xCD => DCx
            0x12, // 8  AxCD => CDxA
            0x13, // 9  AxCD => DCxA
            0x21, // 10 ABxD => DxAB
            0x31, // 11 ABxD => DxBA
            0x22, // 12 ABxCD => CDxAB
            0x32, // 13 ABxCD => CDxBA
            0x23, // 14 ABxCD => DCxAB
            0x33, // 15 ABxCD => DCxBA
        ];

        let m = MAP[usize::from(flags & Self::VERB)];
        let l = 2.min(m >> 4) as usize;
        let r = 2.min(m & 0x0F) as usize;
        let reverse_l = 3 == (m >> 4);
        let reverse_r = 3 == (m & 0x0F);

        if (self.end - self.start >= l + r) && (self.end - self.start <= MAX_CONTEXT_LENGTH) {
            buffer.merge_clusters(self.start, (buffer.idx + 1).min(buffer.len));
            buffer.merge_clusters(self.start, self.end);

            // Swap the first l glyphs with the last r: rotating the run
            // left by l puts the l glyphs at the end, then rotating the
            // rest right by r brings the r glyphs to the front.
            let run = buffer.info.get_mut(self.start..self.end)?;
            let len = run.len();
            run.rotate_left(l);
            run.get_mut(..len - l)?.rotate_right(r);

            if reverse_l {
                run.get_mut(len - 2..)?.reverse();
            }

            if reverse_r {
                run.get_mut(..2)?.reverse();
            }
        }
        Some(())
    }
}

impl DriverContext<NoPayload> for RearrangementCtx {
    fn in_place() -> bool {
        true
    }

    fn can_advance(entry: &StateEntry) -> bool {
        entry.flags & Self::DONT_ADVANCE == 0
    }

    fn is_action_initiable(entry: &StateEntry) -> bool {
        entry.flags & Self::MARK_FIRST != 0
    }

    fn is_actionable(entry: &StateEntry) -> bool {
        entry.flags & Self::VERB != 0
    }

    #[inline(always)]
    fn transition(&mut self, entry: &StateEntry, ac: &mut AatApplyContext) -> Option<()> {
        let buffer = &mut *ac.buffer;
        let flags = entry.flags;

        if flags & Self::MARK_FIRST != 0 {
            self.start = buffer.idx;
        }

        if flags & Self::MARK_LAST != 0 {
            self.end = (buffer.idx + 1).min(buffer.len);
        }

        if flags & Self::VERB != 0 && self.start < self.end {
            return self.perform_action(flags, ac);
        }

        Some(())
    }
}

trait ContextualActions<P> {
    fn mark_action(payload: &P) -> bool;
    fn current_action(payload: &P) -> bool;
    fn mark_replacement(&self, payload: &P, glyph: GlyphId16) -> Option<GlyphId16>;
    fn current_replacement(&self, payload: &P, glyph: GlyphId16) -> Option<GlyphId16>;
}

impl ContextualActions<morx::ContextualEntryData> for morx::ContextualSubtable<'_> {
    fn mark_action(payload: &morx::ContextualEntryData) -> bool {
        payload.mark_index.get() != 0xFFFF
    }

    fn current_action(payload: &morx::ContextualEntryData) -> bool {
        payload.current_index.get() != 0xFFFF
    }

    fn mark_replacement(
        &self,
        payload: &morx::ContextualEntryData,
        glyph: GlyphId16,
    ) -> Option<GlyphId16> {
        self.lookup(usize::from(payload.mark_index.get()))?
            .value(glyph.to_u16())
            .ok()
            .map(GlyphId16::new)
    }

    fn current_replacement(
        &self,
        payload: &morx::ContextualEntryData,
        glyph: GlyphId16,
    ) -> Option<GlyphId16> {
        self.lookup(usize::from(payload.current_index.get()))?
            .value(glyph.to_u16())
            .ok()
            .map(GlyphId16::new)
    }
}

impl ContextualActions<mort::ContextualEntryData> for mort::ContextualSubtable<'_> {
    fn mark_action(payload: &mort::ContextualEntryData) -> bool {
        payload.mark_offset.get() != 0
    }

    fn current_action(payload: &mort::ContextualEntryData) -> bool {
        payload.current_offset.get() != 0
    }

    fn mark_replacement(
        &self,
        payload: &mort::ContextualEntryData,
        glyph: GlyphId16,
    ) -> Option<GlyphId16> {
        self.substitution(payload.mark_offset.get(), glyph)
            .ok()
            .filter(|glyph| *glyph != GlyphId16::NOTDEF)
    }

    fn current_replacement(
        &self,
        payload: &mort::ContextualEntryData,
        glyph: GlyphId16,
    ) -> Option<GlyphId16> {
        self.substitution(payload.current_offset.get(), glyph)
            .ok()
            .filter(|glyph| *glyph != GlyphId16::NOTDEF)
    }
}

struct ContextualCtx<T> {
    mark_set: bool,
    mark: usize,
    table: T,
}

impl<T> ContextualCtx<T> {
    const SET_MARK: u16 = 0x8000;
    const DONT_ADVANCE: u16 = 0x4000;

    #[inline(never)]
    fn perform_action<P>(&mut self, entry: &StateEntry<P>, ac: &mut AatApplyContext)
    where
        T: ContextualActions<P>,
    {
        let mut replacement = None;

        if T::mark_action(&entry.payload) {
            if let Some(gid) = ac
                .buffer
                .info
                .get(self.mark)
                .and_then(|info| info.as_gid16())
            {
                replacement = self.table.mark_replacement(&entry.payload, gid);
            }
        }

        if let Some(replacement) = replacement {
            let end = (ac.buffer.idx + 1).min(ac.buffer.len);
            // unsafe_to_break() visits every glyph from the mark to here
            // (it gives up past 255), and an entry that doesn't advance can
            // ask for that walk again on the same glyph. Charge the walk to
            // the budget like move_to() and merge_clusters() charge their
            // ranges, so the driver forces an advance once it is spent.
            let walk = end.wrapping_sub(self.mark);
            if walk <= 255 {
                ac.buffer.max_ops -= walk as i32;
            }
            ac.buffer.unsafe_to_break(Some(self.mark), Some(end));
            if ac
                .replace_glyph_inplace(self.mark, replacement.into())
                .is_none()
            {
                ac.buffer.index_failed();
            }
        }

        replacement = None;
        let idx = ac.buffer.idx.min(ac.buffer.len - 1);
        if T::current_action(&entry.payload) {
            if let Some(gid) = ac.buffer.info.get(idx).and_then(|info| info.as_gid16()) {
                replacement = self.table.current_replacement(&entry.payload, gid);
            }
        }

        if let Some(replacement) = replacement {
            if ac.replace_glyph_inplace(idx, replacement.into()).is_none() {
                ac.buffer.index_failed();
            }
        }
    }
}

impl<P, T> DriverContext<P> for ContextualCtx<T>
where
    T: ContextualActions<P>,
{
    fn in_place() -> bool {
        true
    }

    fn can_advance(entry: &StateEntry<P>) -> bool {
        entry.flags & Self::DONT_ADVANCE == 0
    }

    fn is_action_initiable(entry: &StateEntry<P>) -> bool {
        entry.flags & Self::SET_MARK != 0
    }

    fn is_actionable(entry: &StateEntry<P>) -> bool {
        T::mark_action(&entry.payload) || T::current_action(&entry.payload)
    }

    #[inline(always)]
    fn transition(&mut self, entry: &StateEntry<P>, ac: &mut AatApplyContext) -> Option<()> {
        // Looks like CoreText applies neither mark nor current substitution for
        // end-of-text if mark was not explicitly set.
        if ac.buffer.idx == ac.buffer.len && !self.mark_set {
            return Some(());
        }

        if Self::is_actionable(entry) {
            self.perform_action(entry, ac);
        }

        if entry.flags & Self::SET_MARK != 0 {
            self.mark_set = true;
            self.mark = ac.buffer.idx;
        }

        Some(())
    }
}

struct InsertionCtx<'a> {
    mark: u32,
    glyphs: &'a [BigEndian<GlyphId16>],
}

trait InsertionPayload {
    fn current_insert_index(&self) -> u16;
    fn marked_insert_index(&self) -> u16;
}

impl InsertionPayload for morx::InsertionEntryData {
    fn current_insert_index(&self) -> u16 {
        self.current_insert_index.get()
    }

    fn marked_insert_index(&self) -> u16 {
        self.marked_insert_index.get()
    }
}

impl InsertionPayload for mort::InsertionEntryData {
    fn current_insert_index(&self) -> u16 {
        self.current_insert_index.get()
    }

    fn marked_insert_index(&self) -> u16 {
        self.marked_insert_index.get()
    }
}

impl InsertionCtx<'_> {
    const SET_MARK: u16 = 0x8000;
    const DONT_ADVANCE: u16 = 0x4000;
    const CURRENT_INSERT_BEFORE: u16 = 0x0800;
    const MARKED_INSERT_BEFORE: u16 = 0x0400;
    const CURRENT_INSERT_COUNT: u16 = 0x03E0;
    const MARKED_INSERT_COUNT: u16 = 0x001F;

    #[inline(never)]
    fn perform_action<P: InsertionPayload>(
        &mut self,
        entry: &StateEntry<P>,
        ac: &mut AatApplyContext,
    ) -> Option<()> {
        let flags = entry.flags;
        let mark_loc = ac.buffer.out_len;

        if entry.payload.marked_insert_index() != 0xFFFF {
            let count = flags & Self::MARKED_INSERT_COUNT;
            ac.buffer.max_ops -= i32::from(count);
            if ac.buffer.max_ops <= 0 {
                return Some(());
            }

            let start = entry.payload.marked_insert_index();
            let before = flags & Self::MARKED_INSERT_BEFORE != 0;

            let end = ac.buffer.out_len;
            if !ac.buffer.move_to(self.mark as usize) {
                return Some(());
            }

            if ac.buffer.idx < ac.buffer.len && !before {
                ac.buffer.copy_glyph();
            }

            // TODO We ignore KashidaLike setting.
            for i in 0..count {
                let i = usize::from(start) + usize::from(i);
                if ac
                    .output_glyph(u32::from(self.glyphs.get(i)?.get().to_u16()))
                    .is_none()
                {
                    ac.buffer.index_failed();
                }
            }

            if ac.buffer.idx < ac.buffer.len && !before {
                ac.buffer.skip_glyph();
            }

            if !ac.buffer.move_to(end + usize::from(count)) {
                return Some(());
            }

            ac.buffer.unsafe_to_break_from_outbuffer(
                Some(self.mark as usize),
                Some((ac.buffer.idx + 1).min(ac.buffer.len)),
            );
        }

        if flags & Self::SET_MARK != 0 {
            self.mark = mark_loc as u32;
        }

        if entry.payload.current_insert_index() != 0xFFFF {
            let count = (flags & Self::CURRENT_INSERT_COUNT) >> 5;
            ac.buffer.max_ops -= i32::from(count);
            if ac.buffer.max_ops < 0 {
                return Some(());
            }

            let start = entry.payload.current_insert_index();
            let before = flags & Self::CURRENT_INSERT_BEFORE != 0;
            let end = ac.buffer.out_len;

            if ac.buffer.idx < ac.buffer.len && !before {
                ac.buffer.copy_glyph();
            }

            // TODO We ignore KashidaLike setting.
            for i in 0..count {
                let i = usize::from(start) + usize::from(i);
                if ac
                    .output_glyph(u32::from(self.glyphs.get(i)?.get().to_u16()))
                    .is_none()
                {
                    ac.buffer.index_failed();
                }
            }

            if ac.buffer.idx < ac.buffer.len && !before {
                ac.buffer.skip_glyph();
            }

            // Humm. Not sure where to move to. There's this wording under
            // DontAdvance flag:
            //
            // "If set, don't update the glyph index before going to the new state.
            // This does not mean that the glyph pointed to is the same one as
            // before. If you've made insertions immediately downstream of the
            // current glyph, the next glyph processed would in fact be the first
            // one inserted."
            //
            // This suggests that if DontAdvance is NOT set, we should move to
            // end+count. If it *was*, then move to end, such that newly inserted
            // glyphs are now visible.
            //
            // https://github.com/harfbuzz/harfbuzz/issues/1224#issuecomment-427691417
            if !ac.buffer.move_to(if flags & Self::DONT_ADVANCE != 0 {
                end
            } else {
                end + usize::from(count)
            }) {
                return Some(());
            }
        }

        Some(())
    }
}

impl<P: InsertionPayload> DriverContext<P> for InsertionCtx<'_> {
    fn in_place() -> bool {
        false
    }

    fn can_advance(entry: &StateEntry<P>) -> bool {
        entry.flags & Self::DONT_ADVANCE == 0
    }

    fn is_action_initiable(entry: &StateEntry<P>) -> bool {
        entry.flags & Self::SET_MARK != 0
    }

    fn is_actionable(entry: &StateEntry<P>) -> bool {
        (entry.flags & (Self::CURRENT_INSERT_COUNT | Self::MARKED_INSERT_COUNT) != 0)
            && (entry.payload.current_insert_index() != 0xFFFF
                || entry.payload.marked_insert_index() != 0xFFFF)
    }

    #[inline(always)]
    fn transition(&mut self, entry: &StateEntry<P>, ac: &mut AatApplyContext) -> Option<()> {
        if entry.payload.marked_insert_index() != 0xFFFF
            || entry.payload.current_insert_index() != 0xFFFF
        {
            return self.perform_action(entry, ac);
        }
        if entry.flags & Self::SET_MARK != 0 {
            self.mark = ac.buffer.out_len as u32;
        }
        Some(())
    }
}

const LIGATURE_MAX_MATCHES: usize = 64;

trait LigatureActions<P> {
    fn action_start(entry: &StateEntry<P>) -> Option<usize>;
    fn action(&self, index: usize) -> Option<u32>;
    fn next_action(index: usize) -> usize;
    fn component(&self, glyph: u32, offset: i32) -> Option<u16>;
    fn ligature(&self, index: u32) -> Option<GlyphId16>;
}

impl LigatureActions<BigEndian<u16>> for morx::LigatureSubtable<'_> {
    fn action_start(entry: &StateEntry<BigEndian<u16>>) -> Option<usize> {
        (entry.flags & 0x2000 != 0).then(|| usize::from(entry.payload.get()))
    }

    fn action(&self, index: usize) -> Option<u32> {
        self.ligature_actions.get(index).map(|action| action.get())
    }

    fn next_action(index: usize) -> usize {
        index + 1
    }

    fn component(&self, glyph: u32, offset: i32) -> Option<u16> {
        let index = usize::try_from(i64::from(glyph) + i64::from(offset)).ok()?;
        self.components.get(index).map(|component| component.get())
    }

    fn ligature(&self, index: u32) -> Option<GlyphId16> {
        self.ligatures.get(index as usize).map(|glyph| glyph.get())
    }
}

impl LigatureActions<NoPayload> for mort::LigatureSubtable<'_> {
    fn action_start(entry: &StateEntry) -> Option<usize> {
        let offset = entry.flags & 0x3FFF;
        (offset != 0).then_some(usize::from(offset))
    }

    fn action(&self, index: usize) -> Option<u32> {
        self.ligature_action(index).ok()
    }

    fn next_action(index: usize) -> usize {
        index + u32::RAW_BYTE_LEN
    }

    fn component(&self, glyph: u32, offset: i32) -> Option<u16> {
        let word_offset = i32::try_from(glyph).ok()?.checked_add(offset)?;
        self.component(word_offset).ok()
    }

    fn ligature(&self, index: u32) -> Option<GlyphId16> {
        self.ligature(index as usize).ok()
    }
}

struct LigatureCtx<T> {
    table: T,
    match_length: usize,
    match_positions: [usize; LIGATURE_MAX_MATCHES],
}

impl<T> LigatureCtx<T> {
    const SET_COMPONENT: u16 = 0x8000;
    const DONT_ADVANCE: u16 = 0x4000;

    const LIG_ACTION_LAST: u32 = 0x8000_0000;
    const LIG_ACTION_STORE: u32 = 0x4000_0000;
    const LIG_ACTION_OFFSET: u32 = 0x3FFF_FFFF;

    #[inline(never)]
    fn perform_action<P>(
        &mut self,
        mut ligature_actions_index: usize,
        ac: &mut AatApplyContext,
    ) -> Option<()>
    where
        T: LigatureActions<P>,
    {
        let end = ac.buffer.out_len;

        if self.match_length == 0 {
            return Some(());
        }

        if ac.buffer.idx >= ac.buffer.len {
            return Some(()); // TODO: Work on previous instead?
        }

        let mut cursor = self.match_length;

        let mut ligature_idx = 0u32;
        loop {
            if cursor == 0 {
                // Stack underflow. Clear the stack.
                self.match_length = 0;
                break;
            }

            cursor -= 1;
            if !ac
                .buffer
                .move_to(self.match_positions[cursor % LIGATURE_MAX_MATCHES])
            {
                return Some(());
            }

            // We cannot use ? in this loop, because we must call
            // ac.buffer.move_to(end) in the end.
            let Some(action) = self.table.action(ligature_actions_index) else {
                break;
            };

            let mut uoffset = action & Self::LIG_ACTION_OFFSET;
            if uoffset & 0x2000_0000 != 0 {
                uoffset |= 0xC000_0000; // Sign-extend.
            }

            let offset = uoffset as i32;
            let component = match self.table.component(ac.buffer.cur(0)?.glyph_id, offset) {
                Some(component) => u32::from(component),
                None => break,
            };
            let Some(next_ligature_idx) = ligature_idx.checked_add(component) else {
                break;
            };
            ligature_idx = next_ligature_idx;

            if (action & (Self::LIG_ACTION_STORE | Self::LIG_ACTION_LAST)) != 0 {
                let Some(lig) = self.table.ligature(ligature_idx) else {
                    break;
                };

                if ac.replace_glyph(u32::from(lig.to_u16())).is_none() {
                    ac.buffer.index_failed();
                }

                let lig_end =
                    self.match_positions[(self.match_length - 1) % LIGATURE_MAX_MATCHES] + 1;
                // Now go and delete all subsequent components.
                while self.match_length - 1 > cursor {
                    self.match_length -= 1;
                    if !ac
                        .buffer
                        .move_to(self.match_positions[self.match_length % LIGATURE_MAX_MATCHES])
                    {
                        return Some(());
                    }
                    if ac.delete_glyph().is_none() {
                        ac.buffer.index_failed();
                    }
                }

                if !ac.buffer.move_to(lig_end) {
                    return Some(());
                }
                ac.buffer.merge_out_clusters(
                    self.match_positions[cursor % LIGATURE_MAX_MATCHES],
                    ac.buffer.out_len,
                );
            }

            ligature_actions_index = T::next_action(ligature_actions_index);

            if action & Self::LIG_ACTION_LAST != 0 {
                break;
            }
        }

        if !ac.buffer.move_to(end) {
            return Some(());
        }
        Some(())
    }
}

impl<P, T> DriverContext<P> for LigatureCtx<T>
where
    T: LigatureActions<P>,
{
    fn in_place() -> bool {
        false
    }

    fn can_advance(entry: &StateEntry<P>) -> bool {
        entry.flags & Self::DONT_ADVANCE == 0
    }

    fn is_action_initiable(entry: &StateEntry<P>) -> bool {
        entry.flags & Self::SET_COMPONENT != 0
    }

    fn is_actionable(entry: &StateEntry<P>) -> bool {
        T::action_start(entry).is_some()
    }

    #[inline(always)]
    fn transition(&mut self, entry: &StateEntry<P>, ac: &mut AatApplyContext) -> Option<()> {
        if entry.flags & Self::SET_COMPONENT != 0 {
            // Never mark same index twice, in case DONT_ADVANCE was used...
            if self.match_length != 0
                && self.match_positions[(self.match_length - 1) % LIGATURE_MAX_MATCHES]
                    == ac.buffer.out_len
            {
                self.match_length -= 1;
            }

            self.match_positions[self.match_length % LIGATURE_MAX_MATCHES] = ac.buffer.out_len;
            self.match_length += 1;
        }

        if let Some(ligature_actions_index) = T::action_start(entry) {
            return self.perform_action::<P>(ligature_actions_index, ac);
        }

        Some(())
    }
}

/// Flat, packed per-subtable filter state: everything the per-buffer walk
/// needs to decide whether a subtable applies, in one small contiguous
/// array that stays cache-resident. The heavy state (glyph set, class
/// cache, parts) lives in [MorphSubtableCache] and is only touched once a
/// subtable passes these filters.
pub(crate) struct MorphSubtableDescriptor {
    pub(crate) chain_index: u32,
    pub(crate) coverage: u32,
    pub(crate) sub_feature_flags: u32,
}

enum MorphSubtableKind<'a> {
    Morx(morx::SubtableKind<'a>),
    Mort(mort::SubtableKind<'a>),
}

pub(crate) struct MorphSubtableCache<'a> {
    start_end_safe_to_break: u64,
    safe_to_break: SafeToBreakSubtable,
    glyph_set: U32Set,
    glyph_digest: SetDigest,
    class_cache: ClassCache,
    /// The parsed subtable, or `None` if it is unreadable.
    kind: Option<MorphSubtableKind<'a>>,
}

fn accelerate_state_machine<T, M, Ctx>(
    machine: &M,
    data: &[u8],
    num_glyphs: u32,
    safe_to_break: &mut SafeToBreakAccel,
    start_end_safe_to_break: &mut u64,
    safe_to_break_subtable: &mut SafeToBreakSubtable,
    glyph_set: &mut U32Set,
) where
    T: FixedSize + bytemuck::AnyBitPattern,
    M: MorphStateTable<T>,
    Ctx: DriverContext<T>,
{
    *start_end_safe_to_break = collect_start_end_safe_to_break::<T, M, Ctx>(machine);
    *safe_to_break_subtable =
        machine.build_safe_to_break(safe_to_break, data, &Ctx::is_actionable, &Ctx::can_advance);
    collect_initial_glyphs::<T, M, Ctx>(machine, data, glyph_set, num_glyphs);
}

impl<'a> MorphSubtableCache<'a> {
    pub(crate) fn descriptor_morx(
        chain_index: usize,
        subtable: &morx::Subtable,
    ) -> MorphSubtableDescriptor {
        MorphSubtableDescriptor {
            chain_index: chain_index as u32,
            coverage: subtable.coverage(),
            sub_feature_flags: subtable.sub_feature_flags(),
        }
    }

    pub(crate) fn descriptor_mort(
        chain_index: usize,
        subtable: &mort::Subtable,
    ) -> MorphSubtableDescriptor {
        MorphSubtableDescriptor {
            chain_index: chain_index as u32,
            // Normalize legacy coverage flags to the `morx` bit positions
            // consumed by the shared application loop.
            coverage: u32::from(subtable.coverage()) << 16,
            sub_feature_flags: subtable.sub_feature_flags(),
        }
    }

    pub(crate) fn new_morx(
        subtable: &morx::Subtable<'a>,
        num_glyphs: u32,
        safe_to_break: &mut SafeToBreakAccel,
    ) -> Self {
        let mut start_end_safe_to_break = 0u64;
        let mut safe_to_break_subtable = safe_to_break.empty_subtable();
        let mut glyph_set = U32Set::default();
        let kind = subtable.kind().ok();
        if let Some(kind) = &kind {
            match kind {
                morx::SubtableKind::Rearrangement(table) => {
                    accelerate_state_machine::<_, _, RearrangementCtx>(
                        table,
                        subtable.data(),
                        num_glyphs,
                        safe_to_break,
                        &mut start_end_safe_to_break,
                        &mut safe_to_break_subtable,
                        &mut glyph_set,
                    );
                }
                morx::SubtableKind::Contextual(table) => {
                    accelerate_state_machine::<_, _, ContextualCtx<morx::ContextualSubtable<'_>>>(
                        &table.state_table,
                        subtable.data(),
                        num_glyphs,
                        safe_to_break,
                        &mut start_end_safe_to_break,
                        &mut safe_to_break_subtable,
                        &mut glyph_set,
                    );
                }
                morx::SubtableKind::Ligature(table) => {
                    accelerate_state_machine::<_, _, LigatureCtx<morx::LigatureSubtable<'_>>>(
                        &table.state_table,
                        subtable.data(),
                        num_glyphs,
                        safe_to_break,
                        &mut start_end_safe_to_break,
                        &mut safe_to_break_subtable,
                        &mut glyph_set,
                    );
                }
                morx::SubtableKind::NonContextual(ref lookup) => {
                    lookup.collect_glyphs(&mut glyph_set, num_glyphs);
                }
                morx::SubtableKind::Insertion(table) => {
                    accelerate_state_machine::<_, _, InsertionCtx<'_>>(
                        &table.state_table,
                        subtable.data(),
                        num_glyphs,
                        safe_to_break,
                        &mut start_end_safe_to_break,
                        &mut safe_to_break_subtable,
                        &mut glyph_set,
                    );
                }
            }
        }
        MorphSubtableCache {
            start_end_safe_to_break,
            safe_to_break: safe_to_break_subtable,
            glyph_digest: set_digest(&glyph_set),
            glyph_set,
            class_cache: ClassCache::new(),
            kind: kind.map(MorphSubtableKind::Morx),
        }
    }

    pub(crate) fn new_mort(
        subtable: &mort::Subtable<'a>,
        num_glyphs: u32,
        safe_to_break: &mut SafeToBreakAccel,
    ) -> Self {
        let mut start_end_safe_to_break = 0u64;
        let mut safe_to_break_subtable = safe_to_break.empty_subtable();
        let mut glyph_set = U32Set::default();
        let kind = subtable.kind().ok();
        if let Some(kind) = &kind {
            match kind {
                mort::SubtableKind::Rearrangement(table) => {
                    accelerate_state_machine::<_, _, RearrangementCtx>(
                        table,
                        subtable.data(),
                        num_glyphs,
                        safe_to_break,
                        &mut start_end_safe_to_break,
                        &mut safe_to_break_subtable,
                        &mut glyph_set,
                    );
                }
                mort::SubtableKind::Contextual(table) => {
                    accelerate_state_machine::<_, _, ContextualCtx<mort::ContextualSubtable<'_>>>(
                        &table.state_table,
                        subtable.data(),
                        num_glyphs,
                        safe_to_break,
                        &mut start_end_safe_to_break,
                        &mut safe_to_break_subtable,
                        &mut glyph_set,
                    );
                }
                mort::SubtableKind::Ligature(table) => {
                    accelerate_state_machine::<_, _, LigatureCtx<mort::LigatureSubtable<'_>>>(
                        &table.state_table,
                        subtable.data(),
                        num_glyphs,
                        safe_to_break,
                        &mut start_end_safe_to_break,
                        &mut safe_to_break_subtable,
                        &mut glyph_set,
                    );
                }
                mort::SubtableKind::NonContextual(ref lookup) => {
                    lookup.collect_glyphs(&mut glyph_set, num_glyphs);
                }
                mort::SubtableKind::Insertion(table) => {
                    accelerate_state_machine::<_, _, InsertionCtx<'_>>(
                        &table.state_table,
                        subtable.data(),
                        num_glyphs,
                        safe_to_break,
                        &mut start_end_safe_to_break,
                        &mut safe_to_break_subtable,
                        &mut glyph_set,
                    );
                }
            }
        }
        MorphSubtableCache {
            start_end_safe_to_break,
            safe_to_break: safe_to_break_subtable,
            glyph_digest: set_digest(&glyph_set),
            glyph_set,
            class_cache: ClassCache::new(),
            kind: kind.map(MorphSubtableKind::Mort),
        }
    }

    /// The bytes this entry keeps for the life of the font. Its glyph set
    /// and safe-to-break data can be far larger than the subtable.
    pub(crate) fn retained_bytes(&self) -> usize {
        size_of::<Self>() + set_heap_bytes(&self.glyph_set) + self.safe_to_break.accel_bytes()
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use read_fonts::{FontData, FontRead};

    #[test]
    fn legacy_negative_states_rearrange_and_check_end_of_text() {
        let font = crate::Font::new(
            include_bytes!("../../tests/fonts/text-rendering-tests/TestMORXOne.ttf").to_vec(),
            0,
        )
        .unwrap();
        let shaper_font = crate::ShaperFont::new(&font);
        let plan = crate::ShapePlan::new(&font, crate::Direction::LeftToRight, None, None, &[]);

        for eot_entry in [0, 2] {
            #[rustfmt::skip]
            let data = [
                0, 6, 0, 8, 0, 20, 0, 26, // classes, class/state/entry offsets
                0, 1, 0, 2, 4, 5, // glyphs 1 and 2 have classes 4 and 5
                eot_entry, 0, 0, 0, 0, 2, // state -1, before the initial state
                0, 0, 0, 0, 1, 0, // state 0
                0, 20, 0, 0, // entry 0: return to state 0, no action
                0, 14, 0x80, 0, // entry 1: mark first, transition to state -1
                0, 14, 0x20, 3, // entry 2: swap first/last, stay in state -1
            ];
            let machine = StateTable::<NoPayload>::read(FontData::new(&data)).unwrap();
            let mut accel = SafeToBreakAccel::default();
            let subtable = accel.build_legacy(
                &machine,
                &RearrangementCtx::is_actionable,
                &RearrangementCtx::can_advance,
            );
            let class_cache = ClassCache::new();
            let mut buffer = crate::Buffer::new();
            for glyph in 1..=3 {
                buffer.push(glyph, glyph - 1);
            }
            let mut ac = AatApplyContext::new(
                &plan,
                shaper_font.layout(),
                crate::Scale::default(),
                &mut buffer,
                #[cfg(feature = "tracing")]
                &shaper_font,
            );
            ac.machine_class_cache = Some(&class_cache);
            ac.start_end_safe_to_break =
                collect_start_end_safe_to_break::<_, _, RearrangementCtx>(&machine);
            ac.safe_to_break = accel.subtable(subtable).unwrap();

            drive(
                &machine,
                &mut RearrangementCtx { start: 0, end: 0 },
                &mut ac,
            )
            .unwrap();

            let infos = ac.buffer.glyph_infos();
            assert_eq!(
                infos.iter().map(|info| info.glyph_id).collect::<Vec<_>>(),
                [2, 1, 3]
            );
            // Breaking before glyph 3 is unsafe only when the negative
            // state's end-of-text transition would rearrange the prefix.
            assert_eq!(infos[2].flags().is_unsafe_to_break(), eot_entry == 2);
        }
    }

    #[test]
    fn noncontextual_caches_collect_overlapping_lookup_segments() {
        // Both non-contextual cache constructors use the unfiltered collector.
        // All 4096 segments cover the same range and share the value array.
        let count = 4096u16;
        let values_offset = 12 + 6 * count;
        let mut words = vec![4u16, 6, count, 0, 0, 0];
        for _ in 0..count {
            words.extend([65534, 0, values_offset]);
        }
        words.extend([1u16; 65535]);
        let lookup: Vec<u8> = words.iter().flat_map(|word| word.to_be_bytes()).collect();
        let mut bytes = Vec::new();
        bytes.extend(((12 + lookup.len()) as u32).to_be_bytes());
        bytes.extend(4u32.to_be_bytes()); // Non-contextual coverage.
        bytes.extend(0u32.to_be_bytes());
        bytes.extend_from_slice(&lookup);
        let subtable = morx::Subtable::read(FontData::new(&bytes)).unwrap();
        let mut accel = SafeToBreakAccel::default();
        let cache = MorphSubtableCache::new_morx(&subtable, 65535, &mut accel);
        assert_eq!(cache.glyph_set.len(), 65535);
        assert!(cache.glyph_set.contains(0));
        assert!(cache.glyph_set.contains(65534));
        assert!(!cache.glyph_set.contains(65535));

        // Legacy mort uses a 16-bit subtable length, so use the same segment
        // pattern over a smaller glyph range and value array.
        let mut legacy_lookup = lookup[..usize::from(values_offset)].to_vec();
        for segment in legacy_lookup[12..].chunks_exact_mut(6) {
            segment[..2].copy_from_slice(&999u16.to_be_bytes());
        }
        legacy_lookup.extend((0..1000).flat_map(|_| 1u16.to_be_bytes()));
        let mut bytes = Vec::new();
        bytes.extend(((8 + legacy_lookup.len()) as u16).to_be_bytes());
        bytes.extend(4u16.to_be_bytes());
        bytes.extend(0u32.to_be_bytes());
        bytes.extend(legacy_lookup);
        let subtable = mort::Subtable::read(FontData::new(&bytes)).unwrap();
        let cache = MorphSubtableCache::new_mort(&subtable, 1000, &mut accel);
        assert_eq!(cache.glyph_set.len(), 1000);
        assert!((0..1000).all(|glyph| cache.glyph_set.contains(glyph)));
        assert!(!cache.glyph_set.contains(1000));
    }

    #[test]
    fn collect_initial_glyphs_bounds_extended_classes() {
        #[rustfmt::skip]
        let mut data = vec![
            0, 0, 0, 0, // nClasses, set below
            0, 0, 0, 16, // class table offset
            0, 0, 0, 32, // state array offset
            0, 0, 0, 24, // entry table offset
            0, 8, 0, 1, 0, 1, 0xFF, 0xFF, // lookup format 8: glyph 1 -> class 65535
            0, 0, 0, 0, // entry 0: no action
            0, 0, 0x80, 0, // entry 1: mark first
        ];
        // Only the highest reachable class uses entry 1.
        data.resize(32 + (1 << 16) * 2, 0);
        *data.last_mut().unwrap() = 1;

        for n_classes in [1u32 << 16, u32::MAX] {
            data[..4].copy_from_slice(&n_classes.to_be_bytes());
            let machine = ExtendedStateTable::<NoPayload>::read(FontData::new(&data)).unwrap();
            let mut glyphs = U32Set::default();
            collect_initial_glyphs::<_, _, RearrangementCtx>(&machine, &data, &mut glyphs, 2);
            assert_eq!(glyphs.iter().collect::<Vec<_>>(), [1]);
        }
    }

    #[test]
    fn range_for_cluster_from_any_range() {
        // Ranges of one, two, ... clusters; the last one runs to the end.
        let mut ranges: Vec<RangeFlags> = (0..20u32)
            .map(|i| RangeFlags {
                flags: 0,
                cluster_first: i * (i + 1) / 2,
                cluster_last: (i + 1) * (i + 2) / 2 - 1,
            })
            .collect();
        ranges.last_mut().unwrap().cluster_last = crate::Feature::GLOBAL_END;
        for last_range in 0..ranges.len() {
            for (range, flags) in ranges.iter().enumerate() {
                for cluster in [flags.cluster_first, flags.cluster_last] {
                    assert_eq!(range_for_cluster(&ranges, last_range, cluster), range);
                }
            }
        }
    }

    /// A `feat` table exposing feature type 1 (ligatures) with settings 2 (on)
    /// and 3 (off), so `liga` ranges can be added to a map builder. Mirrors the
    /// `feat` bytes from `create_morx_face` in HB's test-aat-layout.c.
    #[rustfmt::skip]
    const LIGA_FEAT: [u8; 32] = [
        0,1,0,0,   0,1,   0,0,   0,0,0,0,        // version 1.0, one feature name
        0,1,   0,2,   0,0,0,0x18,   0,0,   1,0,  // feature 1, 2 settings at offset 24
        0,2, 1,0,   0,3, 1,0,                    // settings 2 and 3
    ];

    /// Builds an `AatMap` for the given morx bytes by adding a `liga` range for
    /// each of `length` clusters, on when `i % period != 0`.
    fn compile_liga_ranges(morx_bytes: &[u8], length: u32, period: u32) -> AatMap {
        use crate::aat::{AatData, CachedAatTables};
        use read_fonts::tables::feat::Feat;

        let morx = morx::Morx::read(FontData::new(morx_bytes)).unwrap();
        let feat = Feat::read(FontData::new(&LIGA_FEAT)).unwrap();
        let tables = CachedAatTables {
            morx: Some(morx),
            feat: Some(feat),
            ..CachedAatTables::EMPTY
        };
        let aat = AatData {
            cache: None,
            tables: &tables,
        };

        let liga = crate::Tag::new(b"liga");
        let mut builder = AatMapBuilder::new(None);
        for i in 0..length {
            builder.add_feature(
                &aat,
                &crate::Feature {
                    tag: liga,
                    value: (i % period != 0) as u32,
                    start: i,
                    end: i + 1,
                },
            );
        }
        let mut map = AatMap::default();
        builder.compile(&aat, &mut map);
        map
    }

    /// Compiles an AAT feature map for a morx table with one chain that turns
    /// glyph 1 into glyph 2 when common ligatures are on (off by default),
    /// followed by `empty_chains` chains that react to no feature. Each of the
    /// `length` clusters gets its own `liga` range, on when `i % period != 0`.
    /// Mirrors `create_morx_face`/`shape_ranged_features` from HB's
    /// test-aat-layout.c.
    fn compile_ranged_liga_map(empty_chains: usize, length: u32, period: u32) -> AatMap {
        #[rustfmt::skip]
        const CHAIN: [u8; 60] = [
            0,0,0,0,            // defaultFlags = 0
            0,0,0,0x3C,         // chainLength = 60
            0,0,0,2,            // nFeatureEntries = 2
            0,0,0,1,            // nSubtables = 1
            0,1, 0,2, 0,0,0,1, 0xFF,0xFF,0xFF,0xFF, // type 1, setting 2 (on)
            0,1, 0,3, 0,0,0,0, 0xFF,0xFF,0xFF,0xFE, // type 1, setting 3 (off)
            0,0,0,0x14,         // subtable length = 20
            0x20,0,0,4,         // coverage: all directions, noncontextual
            0,0,0,1,            // subFeatureFlags = 1
            0,8, 0,1, 0,1, 0,2, // lookup format 8: glyph 1 -> glyph 2
        ];

        let chain_count = (1 + empty_chains) as u32;
        let mut morx_bytes = vec![0, 2, 0, 0]; // version 2, reserved
        morx_bytes.extend_from_slice(&chain_count.to_be_bytes());
        morx_bytes.extend_from_slice(&CHAIN);
        for _ in 0..empty_chains {
            // defaultFlags 0, chainLength 16, 0 feature entries, 0 subtables.
            morx_bytes.extend_from_slice(&[0, 0, 0, 0, 0, 0, 0, 16, 0, 0, 0, 0, 0, 0, 0, 0]);
        }

        compile_liga_ranges(&morx_bytes, length, period)
    }

    #[test]
    fn equal_flag_ranges_merge_per_chain() {
        let length = 100u32;
        let period = 3u32;
        let empty_chains = 5usize;
        let map = compile_ranged_liga_map(empty_chains, length, period);

        assert_eq!(map.chain_flags.len(), 1 + empty_chains);

        // Chains that react to nothing collapse to a single entry spanning the
        // whole text, instead of one entry per feature range.
        for chain in &map.chain_flags[1..] {
            assert_eq!(chain.len(), 1);
            assert_eq!(chain[0].cluster_first, 0);
            assert_eq!(chain[0].cluster_last, crate::Feature::GLOBAL_END);
        }

        // The reacting chain keeps distinct ranges but still merges equal-flag
        // neighbours, so it has more than one entry and fewer than one per range.
        let reacting = &map.chain_flags[0];
        assert!(reacting.len() > 1);
        assert!((reacting.len() as u32) < length);
        assert_eq!(
            reacting.last().unwrap().cluster_last,
            crate::Feature::GLOBAL_END
        );

        // Output-equivalence: the flag seen at every cluster matches the
        // feature the caller set there (common ligatures on when i % period).
        let mut range = 0;
        for i in 0..length {
            range = range_for_cluster(reacting, range, i);
            assert_eq!(reacting[range].flags != 0, i % period != 0);
        }

        // Well within budget.
        assert!(map.budget >= 0);

        // The empty chains do not perturb the reacting chain's compiled ranges.
        let baseline = compile_ranged_liga_map(0, length, period);
        assert_eq!(baseline.chain_flags[0].len(), reacting.len());
        for (a, b) in baseline.chain_flags[0].iter().zip(reacting.iter()) {
            assert_eq!(
                (a.flags, a.cluster_first, a.cluster_last),
                (b.flags, b.cluster_first, b.cluster_last)
            );
        }
    }

    #[test]
    fn budget_bounds_chains_times_ranges() {
        // The product of chains and ranges is what must not be paid in full,
        // in time or memory. Without the merge and budget this compiles roughly
        // 20000 chains x 2000 ranges of RangeFlags.
        let length = 2000u32;
        let period = 2u32;
        let empty_chains = 20_000usize;
        let map = compile_ranged_liga_map(empty_chains, length, period);

        assert_eq!(map.chain_flags.len(), 1 + empty_chains);

        // The budget was exhausted, so compilation stopped before the last
        // ranges and the reacting chain holds fewer entries than there are
        // ranges.
        assert!(map.budget < 0);
        assert!((map.chain_flags[0].len() as u32) < length);

        // The ranges compiled before the budget ran out are still correct, and
        // the fix-up extends the last one to the end of the text.
        let reacting = &map.chain_flags[0];
        assert_eq!(
            reacting.last().unwrap().cluster_last,
            crate::Feature::GLOBAL_END
        );
        let mut range = 0;
        for i in 0..100u32 {
            range = range_for_cluster(reacting, range, i);
            assert_eq!(reacting[range].flags != 0, i % period != 0);
        }

        // Empty chains still collapse to a single entry, however many ranges.
        assert_eq!(map.chain_flags[1].len(), 1);
    }

    #[test]
    fn budget_bounds_unparseable_chains() {
        // A morx whose chains are all too short to parse (chainLength 4, below
        // Chain::MIN_SIZE): the chain iterator advances four bytes and yields
        // an error for each. They compile nothing, but are still visited once
        // per range, so they must be charged or compilation is quadratic in the
        // number of ranges at no cost.
        let n_chains = 20_000usize;
        let length = 2000u32;

        let mut morx_bytes = vec![0u8, 2, 0, 0]; // version 2, reserved
        morx_bytes.extend_from_slice(&(n_chains as u32).to_be_bytes());
        // Every four-byte word reads as 4, so each chain's length field (the
        // u32 after its four-byte defaultFlags) is 4. A couple of extra words
        // keep the final length read in bounds.
        for _ in 0..(n_chains + 2) {
            morx_bytes.extend_from_slice(&4u32.to_be_bytes());
        }

        let map = compile_liga_ranges(&morx_bytes, length, 2);

        // The unparseable chains were visited and charged, exhausting the
        // budget before all ranges were compiled.
        assert!(map.chain_flags.len() > 1000);
        assert!(map.budget < 0);
        // None of them store any range, so the map stays empty.
        assert!(map.chain_flags.iter().all(|chain| chain.is_empty()));
    }
}

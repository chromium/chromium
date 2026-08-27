use super::layout::*;
use super::map::{AatMap, AatMapBuilder, RangeFlags};
use crate::hb::aat::layout_common::{
    AatApplyContext, ClassCache, SafeToBreakAccel, SafeToBreakSubtable, TypedCollectGlyphs,
    START_OF_TEXT,
};
use crate::hb::ot_layout::MAX_CONTEXT_LENGTH;
use crate::hb::tag::lang_matches;
use crate::hb::{hb_font_t, GlyphInfo, Language};
use crate::U32Set;
use alloc::{vec, vec::Vec};
use read_fonts::tables::aat::{self, ExtendedStateTable, NoPayload, StateEntry, StateTable};
use read_fonts::tables::{mort, morx};
use read_fonts::types::{BigEndian, FixedSize, GlyphId, GlyphId16};
use read_fonts::{FontData, ReadError};

trait MorphChain {
    fn default_flags(&self) -> u32;
    fn for_each_feature(&self, f: impl FnMut(u16, u16, u32, u32));
}

impl MorphChain for morx::Chain<'_> {
    fn default_flags(&self) -> u32 {
        self.default_flags()
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
pub fn compile_flags(face: &hb_font_t, builder: &AatMapBuilder, map: &mut AatMap) -> Option<()> {
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
        let Some(ltag) = face.aat_tables.ltag.as_ref() else {
            return false;
        };
        let Some(tag) = ltag
            .tag_indices()
            .find_map(|(tag_index, tag)| (tag_index == u32::from(index)).then_some(tag))
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
    ) {
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

        chain_flags.push(RangeFlags {
            flags,
            cluster_first: builder.range_first as u32,
            cluster_last: builder.range_last as u32,
        });
    }

    if let Some((morx, _, _)) = face.aat_tables.morx.as_ref() {
        let chains = morx.chains();
        map.chain_flags.resize(chains.iter().count(), vec![]);
        for (chain, chain_flags) in chains.iter().zip(map.chain_flags.iter_mut()) {
            if let Ok(chain) = chain {
                compile_chain(
                    &chain,
                    &has_feature,
                    &language_matches,
                    chain_flags,
                    builder,
                );
            }
        }
    } else {
        let chains = face.aat_tables.mort.as_ref()?.0.chains();
        map.chain_flags.resize(chains.iter().count(), vec![]);
        for (chain, chain_flags) in chains.iter().zip(map.chain_flags.iter_mut()) {
            if let Ok(chain) = chain {
                compile_chain(
                    &chain,
                    &has_feature,
                    &language_matches,
                    chain_flags,
                    builder,
                );
            }
        }
    }

    Some(())
}

// Chain::apply in harfbuzz
pub fn apply<'a>(c: &mut AatApplyContext<'a>, map: &'a AatMap) -> Option<()> {
    c.buffer.unsafe_to_concat(None, None);

    c.setup_buffer_glyph_set();

    let safe_to_break = c.face.aat_tables.safe_to_break?;
    if let Some((morx, subtable_caches, descriptors)) = c.face.aat_tables.morx.as_ref() {
        apply_table(
            c,
            map,
            safe_to_break,
            morx.offset_data().as_bytes(),
            subtable_caches,
            descriptors,
        )?;
    } else {
        let (mort, subtable_caches, descriptors) = c.face.aat_tables.mort.as_ref()?;
        apply_table(
            c,
            map,
            safe_to_break,
            mort.offset_data().as_bytes(),
            subtable_caches,
            descriptors,
        )?;
    }

    Some(())
}

fn apply_table<'a>(
    c: &mut AatApplyContext<'a>,
    map: &'a AatMap,
    safe_to_break: &'a SafeToBreakAccel,
    table_bytes: &'a [u8],
    subtable_caches: &'a [MorphSubtableCache],
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
        c.machine_class_cache = Some(&subtable_cache.class_cache);
        c.start_end_safe_to_break = subtable_cache.start_end_safe_to_break;
        c.safe_to_break = safe_to_break.subtable(subtable_cache.safe_to_break)?;

        if !c.buffer_intersects_machine() {
            continue;
        }

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

        let Some(data) = table_bytes.get(desc.data_start as usize..desc.data_end as usize) else {
            continue;
        };
        match &subtable_cache.parts {
            MorphSubtableParts::Morx(parts) => {
                if let Ok(kind) = morx::SubtableKind::from_parts(FontData::new(data), parts) {
                    apply_morx_subtable(kind, c);
                }
            }
            MorphSubtableParts::Mort(parts) => {
                if let Ok(kind) = mort::SubtableKind::from_parts(FontData::new(data), parts) {
                    apply_mort_subtable(kind, c);
                }
            }
        }
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
    fn n_classes(&self) -> usize;
    fn class(&self, glyph_id: GlyphId) -> Result<u16, ReadError>;
    fn entry(&self, state: u16, class: u16) -> Result<StateEntry<T>, ReadError>;
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
    fn n_classes(&self) -> usize {
        self.n_classes
    }

    fn class(&self, glyph_id: GlyphId) -> Result<u16, ReadError> {
        self.class(glyph_id)
    }

    fn entry(&self, state: u16, class: u16) -> Result<StateEntry<T>, ReadError> {
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
    fn n_classes(&self) -> usize {
        self.n_classes
    }

    fn class(&self, glyph_id: GlyphId) -> Result<u16, ReadError> {
        let glyph_id = u16::try_from(glyph_id.to_u32()).map_err(|_| ReadError::OutOfBounds)?;
        self.class(GlyphId16::new(glyph_id)).map(u16::from)
    }

    fn entry(&self, state: u16, class: u16) -> Result<StateEntry<T>, ReadError> {
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

fn collect_initial_glyphs<T, M, Ctx>(machine: &M, glyphs: &mut U32Set, num_glyphs: u32)
where
    T: FixedSize + bytemuck::AnyBitPattern,
    M: MorphStateTable<T>,
    Ctx: DriverContext<T>,
{
    let mut classes = U32Set::default();

    for i in 0..machine.n_classes() {
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

    let filter = |class: u16| classes.contains(class as u32);

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

fn drive<T, M, Ctx>(machine: &M, c: &mut Ctx, ac: &mut AatApplyContext)
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
                    let cluster = ac.buffer.cur(0).cluster;
                    while cluster < range_flags[range].cluster_first {
                        range -= 1;
                    }

                    while cluster > range_flags[range].cluster_last {
                        range += 1;
                    }

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
                ac.buffer.cur(0).as_glyph(),
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
                        ac.buffer.cur(0).as_glyph(),
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
                if state < 64 {
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
}

fn apply_morx_subtable<'a>(kind: morx::SubtableKind<'a>, ac: &mut AatApplyContext<'a>) {
    match kind {
        morx::SubtableKind::Rearrangement(table) => {
            let mut c = RearrangementCtx { start: 0, end: 0 };
            drive(&table, &mut c, ac);
        }
        morx::SubtableKind::Contextual(table) => {
            let mut c = ContextualCtx {
                mark_set: false,
                mark: 0,
                table: table.clone(),
            };
            drive(&table.state_table, &mut c, ac);
        }
        morx::SubtableKind::Ligature(table) => {
            let mut c = LigatureCtx {
                table: table.clone(),
                match_length: 0,
                match_positions: [0; LIGATURE_MAX_MATCHES],
            };
            drive(&table.state_table, &mut c, ac);
        }
        morx::SubtableKind::NonContextual(ref lookup) => apply_noncontextual(lookup, ac),
        morx::SubtableKind::Insertion(table) => {
            let mut c = InsertionCtx {
                mark: 0,
                glyphs: table.glyphs,
            };
            drive(&table.state_table, &mut c, ac);
        }
    }
}

fn apply_mort_subtable<'a>(kind: mort::SubtableKind<'a>, ac: &mut AatApplyContext<'a>) {
    match kind {
        mort::SubtableKind::Rearrangement(table) => {
            let mut c = RearrangementCtx { start: 0, end: 0 };
            drive(&table, &mut c, ac);
        }
        mort::SubtableKind::Contextual(table) => {
            let mut c = ContextualCtx {
                mark_set: false,
                mark: 0,
                table: table.clone(),
            };
            drive(&table.state_table, &mut c, ac);
        }
        mort::SubtableKind::Ligature(table) => {
            let mut c = LigatureCtx {
                table: table.clone(),
                match_length: 0,
                match_positions: [0; LIGATURE_MAX_MATCHES],
            };
            drive(&table.state_table, &mut c, ac);
        }
        mort::SubtableKind::NonContextual(ref lookup) => apply_noncontextual(lookup, ac),
        mort::SubtableKind::Insertion(table) => {
            let mut c = InsertionCtx {
                mark: 0,
                glyphs: table.glyphs,
            };
            drive(&table.state_table, &mut c, ac);
        }
    }
}

fn apply_noncontextual(lookup: &aat::LookupU16, ac: &mut AatApplyContext) {
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
                    let cluster = ac.buffer.cur(0).cluster;
                    while cluster < range_flags[range].cluster_first {
                        range -= 1;
                    }

                    while cluster > range_flags[range].cluster_last {
                        range += 1;
                    }

                    *last_range = range;
                }

                if range_flags[range].flags & ac.subtable_flags == 0 {
                    continue;
                }
            }
        }

        if let Some(glyph) = ac.buffer.info[i].as_gid16() {
            if let Ok(replacement) = lookup.value(glyph.to_u16()) {
                ac.replace_glyph_inplace(i, replacement.into());
            }
        }
    }
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

                let mut buf = [GlyphInfo::default(); 4];

                for (i, glyph_info) in buf[..l].iter_mut().enumerate() {
                    *glyph_info = buffer.info[self.start + i];
                }

                for i in 0..r {
                    buf[i + 2] = buffer.info[self.end - r + i];
                }

                if l > r {
                    for i in 0..(self.end - self.start - l - r) {
                        buffer.info[self.start + r + i] = buffer.info[self.start + l + i];
                    }
                } else if l < r {
                    for i in (0..(self.end - self.start - l - r)).rev() {
                        buffer.info[self.start + r + i] = buffer.info[self.start + l + i];
                    }
                }

                for i in 0..r {
                    buffer.info[self.start + i] = buf[2 + i];
                }

                for i in 0..l {
                    buffer.info[self.end - l + i] = buf[i];
                }

                if reverse_l {
                    buffer.info.swap(self.end - 1, self.end - 2);
                }

                if reverse_r {
                    buffer.info.swap(self.start, self.start + 1);
                }
            }
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
        self.lookups
            .get(usize::from(payload.mark_index.get()))
            .ok()?
            .value(glyph.to_u16())
            .ok()
            .map(GlyphId16::new)
    }

    fn current_replacement(
        &self,
        payload: &morx::ContextualEntryData,
        glyph: GlyphId16,
    ) -> Option<GlyphId16> {
        self.lookups
            .get(usize::from(payload.current_index.get()))
            .ok()?
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

        let mut replacement = None;

        if T::mark_action(&entry.payload) {
            if let Some(gid) = ac.buffer.info[self.mark].as_gid16() {
                replacement = self.table.mark_replacement(&entry.payload, gid);
            }
        }

        if let Some(replacement) = replacement {
            ac.buffer.unsafe_to_break(
                Some(self.mark),
                Some((ac.buffer.idx + 1).min(ac.buffer.len)),
            );
            ac.replace_glyph_inplace(self.mark, replacement.into());
        }

        replacement = None;
        let idx = ac.buffer.idx.min(ac.buffer.len - 1);
        if T::current_action(&entry.payload) {
            if let Some(gid) = ac.buffer.info[idx].as_gid16() {
                replacement = self.table.current_replacement(&entry.payload, gid);
            }
        }

        if let Some(replacement) = replacement {
            ac.replace_glyph_inplace(idx, replacement.into());
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
                let i = usize::from(start + i);
                ac.output_glyph(u32::from(self.glyphs.get(i)?.get().to_u16()));
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
                let i = usize::from(start + i);
                ac.output_glyph(u32::from(self.glyphs.get(i)?.get().to_u16()));
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

        if let Some(mut ligature_actions_index) = T::action_start(entry) {
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
                let component = match self.table.component(ac.buffer.cur(0).glyph_id, offset) {
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

                    ac.replace_glyph(u32::from(lig.to_u16()));

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
                        ac.delete_glyph();
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
    pub(crate) data_start: u32,
    pub(crate) data_end: u32,
}

enum MorphSubtableParts {
    Morx(morx::SubtableParts),
    Mort(mort::SubtableParts),
}

pub(crate) struct MorphSubtableCache {
    start_end_safe_to_break: u64,
    safe_to_break: SafeToBreakSubtable,
    glyph_set: U32Set,
    class_cache: ClassCache,
    /// Pre-resolved subtable layout, so per-application dispatch rebuilds
    /// the kind without re-reading headers. An unreadable subtable stores
    /// an invalid format, which makes from_parts fail like the full read
    /// did.
    parts: MorphSubtableParts,
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
    collect_initial_glyphs::<T, M, Ctx>(machine, glyph_set, num_glyphs);
}

impl MorphSubtableCache {
    pub(crate) fn descriptor_morx(
        chain_index: usize,
        subtable: &morx::Subtable,
        morx_base: usize,
    ) -> MorphSubtableDescriptor {
        let data = subtable.data();
        let start = data.as_ptr() as usize - morx_base;
        MorphSubtableDescriptor {
            chain_index: chain_index as u32,
            coverage: subtable.coverage(),
            sub_feature_flags: subtable.sub_feature_flags(),
            data_start: start as u32,
            data_end: (start + data.len()) as u32,
        }
    }

    pub(crate) fn descriptor_mort(
        chain_index: usize,
        subtable: &mort::Subtable,
        mort_base: usize,
    ) -> MorphSubtableDescriptor {
        let data = subtable.data();
        let start = data.as_ptr() as usize - mort_base;
        MorphSubtableDescriptor {
            chain_index: chain_index as u32,
            // Normalize legacy coverage flags to the `morx` bit positions
            // consumed by the shared application loop.
            coverage: u32::from(subtable.coverage()) << 16,
            sub_feature_flags: subtable.sub_feature_flags(),
            data_start: start as u32,
            data_end: (start + data.len()) as u32,
        }
    }

    pub(crate) fn new_morx(
        subtable: &morx::Subtable,
        num_glyphs: u32,
        safe_to_break: &mut SafeToBreakAccel,
    ) -> Self {
        let mut start_end_safe_to_break = 0u64;
        let mut safe_to_break_subtable = safe_to_break.empty_subtable();
        let mut glyph_set = U32Set::default();
        if let Ok(kind) = subtable.kind() {
            match &kind {
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
        let parts = morx::SubtableKind::parts(FontData::new(subtable.data()), subtable.coverage())
            .unwrap_or(morx::SubtableParts {
                format: 0xFF,
                ..Default::default()
            });
        MorphSubtableCache {
            start_end_safe_to_break,
            safe_to_break: safe_to_break_subtable,
            glyph_set,
            class_cache: ClassCache::new(),
            parts: MorphSubtableParts::Morx(parts),
        }
    }

    pub(crate) fn new_mort(
        subtable: &mort::Subtable,
        num_glyphs: u32,
        safe_to_break: &mut SafeToBreakAccel,
    ) -> Self {
        let mut start_end_safe_to_break = 0u64;
        let mut safe_to_break_subtable = safe_to_break.empty_subtable();
        let mut glyph_set = U32Set::default();
        if let Ok(kind) = subtable.kind() {
            match &kind {
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
        let parts = mort::SubtableKind::parts(FontData::new(subtable.data()), subtable.coverage())
            .unwrap_or(mort::SubtableParts {
                format: 0xFF,
                ..Default::default()
            });
        MorphSubtableCache {
            start_end_safe_to_break,
            safe_to_break: safe_to_break_subtable,
            glyph_set,
            class_cache: ClassCache::new(),
            parts: MorphSubtableParts::Mort(parts),
        }
    }
}

use super::{coverage_binary_cached, coverage_index, covered, glyph_class};
use crate::hb::buffer::GlyphInfo;
use crate::hb::ot::{ClassDefInfo, CoverageInfo};
use crate::hb::ot_layout_gsubgpos::OT::hb_ot_apply_context_t;
use crate::hb::ot_layout_gsubgpos::{
    apply_lookup, match_always, match_backtrack, match_glyph, match_input, match_lookahead,
    may_skip_t, skipping_iterator_t, Apply, BinaryCache, ChainContextFormat2Cache,
    ContextFormat2Cache, SubtableExternalCache, SubtableExternalCacheMode, WouldApply,
    WouldApplyContext,
};
use read_fonts::tables::gsub::ClassDef;
use read_fonts::tables::layout::{
    ChainedClassSequenceRule, ChainedSequenceContextFormat1, ChainedSequenceContextFormat2,
    ChainedSequenceContextFormat3, ChainedSequenceRule, ClassSequenceRule, SequenceContextFormat1,
    SequenceContextFormat2, SequenceContextFormat3, SequenceLookupRecord, SequenceRule,
};
use read_fonts::types::{BigEndian, FixedSize, GlyphId, Offset16};
use read_fonts::FontData;

impl WouldApply for SequenceContextFormat1<'_> {
    fn would_apply(&self, ctx: &WouldApplyContext) -> bool {
        coverage_index(self.coverage(), ctx.glyphs[0])
            .and_then(|index| {
                self.seq_rule_sets()
                    .get(index as usize)
                    .transpose()
                    .ok()
                    .flatten()
            })
            .is_some_and(|set| {
                set.seq_rules().iter().any(|rule| {
                    rule.is_ok_and(|rule| {
                        let input = rule.input_sequence();
                        ctx.glyphs.len() == input.len() + 1
                            && input.iter().enumerate().all(|(i, value)| {
                                let mut info = GlyphInfo {
                                    glyph_id: ctx.glyphs[i + 1].into(),
                                    ..GlyphInfo::default()
                                };
                                match_glyph(&mut info, value.get().to_u32())
                            })
                    })
                })
            })
    }
}

impl Apply for SequenceContextFormat1<'_> {
    fn apply(&self, ctx: &mut hb_ot_apply_context_t) -> Option<()> {
        let glyph = ctx.buffer.cur(0).as_glyph();
        let index = self.coverage().ok()?.get(glyph)? as usize;
        let set = self.seq_rule_sets().get(index)?.ok()?;
        apply_context_rules(ctx, set.offset_data(), set.seq_rule_offsets(), match_glyph)
    }
}

impl WouldApply for SequenceContextFormat2<'_> {
    fn would_apply(&self, ctx: &WouldApplyContext) -> bool {
        let class_def = self.class_def().ok();
        let match_fn = &match_class(&class_def);
        let class = glyph_class(self.class_def(), ctx.glyphs[0]);
        self.class_seq_rule_sets()
            .get(class as usize)
            .transpose()
            .ok()
            .flatten()
            .is_some_and(|set| {
                set.class_seq_rules().iter().any(|rule| {
                    rule.is_ok_and(|rule| {
                        let input = rule.input_sequence();
                        ctx.glyphs.len() == input.len() + 1
                            && input.iter().enumerate().all(|(i, value)| {
                                let mut info = GlyphInfo {
                                    glyph_id: ctx.glyphs[i + 1].into(),
                                    ..GlyphInfo::default()
                                };
                                match_fn(&mut info, value.get() as u32)
                            })
                    })
                })
            })
    }
}

impl Apply for SequenceContextFormat2<'_> {
    fn apply_with_external_cache(
        &self,
        ctx: &mut hb_ot_apply_context_t,
        external_cache: &SubtableExternalCache,
    ) -> Option<()> {
        let glyph = ctx.buffer.cur(0).as_glyph();
        let SubtableExternalCache::ContextFormat2Cache(cache) = external_cache else {
            return None;
        };
        let offset_data = self.offset_data();
        coverage_binary_cached(
            |gid| cache.coverage.index(&offset_data, gid),
            glyph,
            &cache.coverage_cache,
        )?;
        let input_class = |gid| cache.input.class(&offset_data, gid);
        let index = input_class(glyph) as usize;
        let set = self.class_seq_rule_sets().get(index)?.ok()?;
        apply_context_rules(
            ctx,
            set.offset_data(),
            set.class_seq_rule_offsets(),
            |info, value| u32::from(input_class(info.as_glyph())) == value,
        )
    }

    fn apply_cached(
        &self,
        ctx: &mut hb_ot_apply_context_t,
        external_cache: &SubtableExternalCache,
    ) -> Option<()> {
        let glyph = ctx.buffer.cur(0).as_glyph();
        let SubtableExternalCache::ContextFormat2Cache(cache) = external_cache else {
            return None;
        };
        let offset_data = self.offset_data();
        coverage_binary_cached(
            |gid| cache.coverage.index(&offset_data, gid),
            glyph,
            &cache.coverage_cache,
        )?;
        let input_class = |gid| cache.input.class(&offset_data, gid);
        let index = get_class_cached(&input_class, &mut ctx.buffer.info[ctx.buffer.idx]) as usize;
        let set = self.class_seq_rule_sets().get(index)?.ok()?;
        apply_context_rules(
            ctx,
            set.offset_data(),
            set.class_seq_rule_offsets(),
            match_class_cached(&input_class),
        )
    }

    fn cache_cost(&self) -> u32 {
        self.class_def()
            .ok()
            .map_or(0, |class_def| class_def.cost())
    }

    fn external_cache_create(&self, _mode: SubtableExternalCacheMode) -> SubtableExternalCache {
        let data = self.offset_data();
        SubtableExternalCache::ContextFormat2Cache(ContextFormat2Cache {
            coverage_cache: BinaryCache::new(),
            coverage: CoverageInfo::new(&data, self.coverage_offset().to_u32() as u16)
                .unwrap_or_default(),
            input: ClassDefInfo::new(&data, self.class_def_offset().to_u32() as u16)
                .unwrap_or_default(),
        })
    }
}

impl WouldApply for SequenceContextFormat3<'_> {
    fn would_apply(&self, ctx: &WouldApplyContext) -> bool {
        let coverages = self.coverages();
        ctx.glyphs.len() == coverages.len() + 1
            && coverages
                .iter()
                .enumerate()
                .all(|(i, coverage)| covered(coverage, ctx.glyphs[i + 1]))
    }
}

impl Apply for SequenceContextFormat3<'_> {
    fn apply(&self, ctx: &mut hb_ot_apply_context_t) -> Option<()> {
        let glyph = ctx.buffer.cur(0).as_glyph();
        let input_coverages = self.coverages();
        input_coverages.get(0).ok()?.get(glyph)?;
        let input = |info: &mut GlyphInfo, index: u32| {
            input_coverages
                .get(index as usize + 1)
                .is_ok_and(|cov| cov.get(info.glyph_id).is_some())
        };
        let mut match_end = 0;
        if match_input(
            ctx,
            input_coverages.len() as u16 - 1,
            input,
            &mut match_end,
            None,
        ) {
            ctx.buffer
                .unsafe_to_break(Some(ctx.buffer.idx), Some(match_end));
            apply_lookup(
                ctx,
                input_coverages.len() - 1,
                match_end,
                self.seq_lookup_records(),
            );
            Some(())
        } else {
            ctx.buffer
                .unsafe_to_concat(Some(ctx.buffer.idx), Some(match_end));
            None
        }
    }
}

impl WouldApply for ChainedSequenceContextFormat1<'_> {
    fn would_apply(&self, ctx: &WouldApplyContext) -> bool {
        coverage_index(self.coverage(), ctx.glyphs[0])
            .and_then(|index| {
                self.chained_seq_rule_sets()
                    .get(index as usize)
                    .transpose()
                    .ok()
                    .flatten()
            })
            .is_some_and(|set| {
                set.chained_seq_rules().iter().any(|rule| {
                    rule.is_ok_and(|rule| {
                        let input = rule.input_sequence();
                        (!ctx.zero_context
                            || (rule.backtrack_glyph_count() == 0
                                && rule.lookahead_glyph_count() == 0))
                            && ctx.glyphs.len() == input.len() + 1
                            && input.iter().enumerate().all(|(i, value)| {
                                let mut info = GlyphInfo {
                                    glyph_id: ctx.glyphs[i + 1].into(),
                                    ..GlyphInfo::default()
                                };
                                match_glyph(&mut info, value.get().to_u32())
                            })
                    })
                })
            })
    }
}

impl Apply for ChainedSequenceContextFormat1<'_> {
    fn apply(&self, ctx: &mut hb_ot_apply_context_t) -> Option<()> {
        let glyph = ctx.buffer.cur(0).as_glyph();
        let index = self.coverage().ok()?.get(glyph)? as usize;
        let set = self.chained_seq_rule_sets().get(index)?.ok()?;
        apply_chain_context_rules(
            ctx,
            set.offset_data(),
            set.chained_seq_rule_offsets(),
            (match_glyph, match_glyph, match_glyph),
        )
    }
}

impl WouldApply for ChainedSequenceContextFormat2<'_> {
    fn would_apply(&self, ctx: &WouldApplyContext) -> bool {
        let class_def = self.input_class_def().ok();
        let match_fn = &match_class(&class_def);
        let class = glyph_class(self.input_class_def(), ctx.glyphs[0]);
        self.chained_class_seq_rule_sets()
            .get(class as usize)
            .transpose()
            .ok()
            .flatten()
            .is_some_and(|set| {
                set.chained_class_seq_rules().iter().any(|rule| {
                    rule.is_ok_and(|rule| {
                        let input = rule.input_sequence();
                        (!ctx.zero_context
                            || (rule.backtrack_glyph_count() == 0
                                && rule.lookahead_glyph_count() == 0))
                            && ctx.glyphs.len() == input.len() + 1
                            && input.iter().enumerate().all(|(i, value)| {
                                let mut info = GlyphInfo {
                                    glyph_id: ctx.glyphs[i + 1].into(),
                                    ..GlyphInfo::default()
                                };
                                match_fn(&mut info, value.get() as u32)
                            })
                    })
                })
            })
    }
}

/// Value represents glyph class.
fn match_class<'a>(
    class_def: &'a Option<ClassDef<'a>>,
) -> impl Fn(&mut GlyphInfo, u32) -> bool + 'a {
    |&mut info, value| {
        class_def
            .as_ref()
            .is_some_and(|class_def| u32::from(class_def.get(info.as_glyph())) == value)
    }
}

fn get_class_cached(class_def: &impl Fn(GlyphId) -> u16, info: &mut GlyphInfo) -> u16 {
    let mut klass = info.syllable() as u16;
    if klass < 255 {
        return klass;
    }
    klass = class_def(info.as_glyph());
    if klass < 255 {
        info.set_syllable(klass as u8);
    }

    klass
}

fn match_class_cached<'a>(
    class_def: impl Fn(GlyphId) -> u16 + 'a,
) -> impl Fn(&mut GlyphInfo, u32) -> bool + 'a {
    move |info: &mut GlyphInfo, value| u32::from(get_class_cached(&class_def, info)) == value
}

fn get_class_cached1(class_def: &impl Fn(GlyphId) -> u16, info: &mut GlyphInfo) -> u16 {
    let mut klass = (info.syllable() & 0x0F) as u16;
    if klass < 15 {
        return klass;
    }

    klass = class_def(info.as_glyph());

    if klass < 15 {
        info.set_syllable((info.syllable() & 0xF0) | klass as u8);
    }

    klass
}

fn match_class_cached1<'a>(
    class_def: impl Fn(GlyphId) -> u16 + 'a,
) -> impl Fn(&mut GlyphInfo, u32) -> bool + 'a {
    move |info: &mut GlyphInfo, value| u32::from(get_class_cached1(&class_def, info)) == value
}

fn get_class_cached2(class_def: &impl Fn(GlyphId) -> u16, info: &mut GlyphInfo) -> u16 {
    let mut klass = (info.syllable() & 0xF0) as u16 >> 4;
    if klass < 15 {
        return klass;
    }
    klass = class_def(info.as_glyph());
    if klass < 15 {
        info.set_syllable((info.syllable() & 0x0F) | ((klass as u8) << 4));
    }
    klass
}

fn match_class_cached2<'a>(
    class_def: impl Fn(GlyphId) -> u16 + 'a,
) -> impl Fn(&mut GlyphInfo, u32) -> bool + 'a {
    move |info: &mut GlyphInfo, value| u32::from(get_class_cached2(&class_def, info)) == value
}

impl Apply for ChainedSequenceContextFormat2<'_> {
    fn apply_with_external_cache(
        &self,
        ctx: &mut hb_ot_apply_context_t,
        external_cache: &SubtableExternalCache,
    ) -> Option<()> {
        let glyph = ctx.buffer.cur(0).as_glyph();
        let SubtableExternalCache::ChainContextFormat2Cache(cache) = external_cache else {
            return None;
        };
        let offset_data = self.offset_data();
        coverage_binary_cached(
            |gid| cache.coverage.index(&offset_data, gid),
            glyph,
            &cache.coverage_cache,
        )?;
        let index = cache.input.class(&offset_data, glyph) as usize;
        let set = self.chained_class_seq_rule_sets().get(index)?.ok()?;
        apply_chain_context_rules(
            ctx,
            set.offset_data(),
            set.chained_class_seq_rule_offsets(),
            (
                |info, val| u32::from(cache.backtrack.class(&offset_data, info.as_glyph())) == val,
                |info, val| u32::from(cache.input.class(&offset_data, info.as_glyph())) == val,
                |info, val| u32::from(cache.lookahead.class(&offset_data, info.as_glyph())) == val,
            ),
        )
    }
    fn apply_cached(
        &self,
        ctx: &mut hb_ot_apply_context_t,
        external_cache: &SubtableExternalCache,
    ) -> Option<()> {
        let glyph = ctx.buffer.cur(0).as_glyph();
        let SubtableExternalCache::ChainContextFormat2Cache(cache) = external_cache else {
            return None;
        };
        let offset_data = self.offset_data();
        coverage_binary_cached(
            |gid| cache.coverage.index(&offset_data, gid),
            glyph,
            &cache.coverage_cache,
        )?;
        let input_class = |gid| cache.input.class(&offset_data, gid);
        let lookahead_class = |gid| cache.lookahead.class(&offset_data, gid);
        let index = get_class_cached2(&input_class, &mut ctx.buffer.info[ctx.buffer.idx]) as usize;
        let set = self.chained_class_seq_rule_sets().get(index)?.ok()?;
        apply_chain_context_rules(
            ctx,
            set.offset_data(),
            set.chained_class_seq_rule_offsets(),
            (
                |info, val| u32::from(cache.backtrack.class(&offset_data, info.as_glyph())) == val,
                match_class_cached2(&input_class),
                match_class_cached1(&lookahead_class),
            ),
        )
    }
    fn cache_cost(&self) -> u32 {
        self.input_class_def()
            .ok()
            .map_or(0, |class_def| class_def.cost())
            + self
                .lookahead_class_def()
                .ok()
                .map_or(0, |class_def| class_def.cost())
    }

    fn external_cache_create(&self, _mode: SubtableExternalCacheMode) -> SubtableExternalCache {
        let data = self.offset_data();
        SubtableExternalCache::ChainContextFormat2Cache(ChainContextFormat2Cache {
            coverage_cache: BinaryCache::new(),
            coverage: CoverageInfo::new(&data, self.coverage_offset().to_u32() as u16)
                .unwrap_or_default(),
            backtrack: ClassDefInfo::new(&data, self.backtrack_class_def_offset().to_u32() as u16)
                .unwrap_or_default(),
            input: ClassDefInfo::new(&data, self.input_class_def_offset().to_u32() as u16)
                .unwrap_or_default(),
            lookahead: ClassDefInfo::new(&data, self.lookahead_class_def_offset().to_u32() as u16)
                .unwrap_or_default(),
        })
    }
}

impl WouldApply for ChainedSequenceContextFormat3<'_> {
    fn would_apply(&self, ctx: &WouldApplyContext) -> bool {
        let input_coverages = self.input_coverages();
        (!ctx.zero_context
            || (self.backtrack_coverage_offsets().is_empty()
                && self.lookahead_coverage_offsets().is_empty()))
            && (ctx.glyphs.len() == input_coverages.len()
                && input_coverages
                    .iter()
                    .skip(1)
                    .enumerate()
                    .all(|(i, coverage)| {
                        coverage.is_ok_and(|cov| cov.get(ctx.glyphs[i + 1]).is_some())
                    }))
    }
}

impl Apply for ChainedSequenceContextFormat3<'_> {
    fn apply(&self, ctx: &mut hb_ot_apply_context_t) -> Option<()> {
        let glyph = ctx.buffer.cur(0).as_glyph();

        let input_coverages = self.input_coverages();
        input_coverages.get(0).ok()?.get(glyph)?;

        let backtrack_coverages = self.backtrack_coverages();
        let lookahead_coverages = self.lookahead_coverages();

        let back = |info: &mut GlyphInfo, index: u32| {
            backtrack_coverages
                .get(index as usize)
                .is_ok_and(|cov| cov.get(info.glyph_id).is_some())
        };

        let ahead = |info: &mut GlyphInfo, index: u32| {
            lookahead_coverages
                .get(index as usize)
                .is_ok_and(|cov| cov.get(info.glyph_id).is_some())
        };

        let input = |info: &mut GlyphInfo, index: u32| {
            input_coverages
                .get(index as usize + 1)
                .is_ok_and(|cov| cov.get(info.glyph_id).is_some())
        };

        let mut end_index = ctx.buffer.idx;
        let mut match_end = 0;

        let input_matches = match_input(
            ctx,
            input_coverages.len() as u16 - 1,
            input,
            &mut match_end,
            None,
        );

        if input_matches {
            end_index = match_end;
        }

        if !(input_matches
            && match_lookahead(
                ctx,
                lookahead_coverages.len() as u16,
                ahead,
                match_end,
                &mut end_index,
            ))
        {
            ctx.buffer
                .unsafe_to_concat(Some(ctx.buffer.idx), Some(end_index));
            return None;
        }

        let mut start_index = ctx.buffer.out_len;

        if !match_backtrack(
            ctx,
            backtrack_coverages.len() as u16,
            back,
            &mut start_index,
        ) {
            ctx.buffer
                .unsafe_to_concat_from_outbuffer(Some(start_index), Some(end_index));
            return None;
        }

        ctx.buffer
            .unsafe_to_break_from_outbuffer(Some(start_index), Some(end_index));
        apply_lookup(
            ctx,
            input_coverages.len() - 1,
            match_end,
            self.seq_lookup_records(),
        );

        Some(())
    }
}

/// All of a context rule's fields, parsed in a single pass.
///
/// The generated getters re-derive the positions of all preceding fields on
/// every call, so fetching fields individually in the rule-matching loops
/// re-reads the leading counts many times over; this parses the whole rule
/// once instead. Glyph ids and classes are both read as raw u16s.
#[derive(Clone, Copy, Default)]
struct ParsedRule<'a> {
    backtrack: &'a [BigEndian<u16>],
    input: &'a [BigEndian<u16>],
    lookahead: &'a [BigEndian<u16>],
    records: &'a [SequenceLookupRecord],
}

impl<'a> ParsedRule<'a> {
    /// Parse a SequenceRule or ClassSequenceRule.
    fn from_rule_data(data: FontData<'a>) -> Option<Self> {
        let glyph_count = usize::from(data.read_at::<u16>(0).ok()?);
        let record_count = usize::from(data.read_at::<u16>(2).ok()?);
        let input_end = 4 + glyph_count.saturating_sub(1) * u16::RAW_BYTE_LEN;
        let records_end = input_end + record_count * SequenceLookupRecord::RAW_BYTE_LEN;
        Some(ParsedRule {
            input: data.read_array(4..input_end).ok()?,
            records: data.read_array(input_end..records_end).ok()?,
            ..ParsedRule::default()
        })
    }

    /// Parse a ChainedSequenceRule or ChainedClassSequenceRule.
    fn from_chain_rule_data(data: FontData<'a>) -> Option<Self> {
        let backtrack_count = usize::from(data.read_at::<u16>(0).ok()?);
        let backtrack_end = 2 + backtrack_count * u16::RAW_BYTE_LEN;
        let input_count = usize::from(data.read_at::<u16>(backtrack_end).ok()?);
        let input_end = backtrack_end + 2 + input_count.saturating_sub(1) * u16::RAW_BYTE_LEN;
        let lookahead_count = usize::from(data.read_at::<u16>(input_end).ok()?);
        let lookahead_end = input_end + 2 + lookahead_count * u16::RAW_BYTE_LEN;
        let record_count = usize::from(data.read_at::<u16>(lookahead_end).ok()?);
        let records_end = lookahead_end + 2 + record_count * SequenceLookupRecord::RAW_BYTE_LEN;
        Some(ParsedRule {
            backtrack: data.read_array(2..backtrack_end).ok()?,
            input: data.read_array(backtrack_end + 2..input_end).ok()?,
            lookahead: data.read_array(input_end + 2..lookahead_end).ok()?,
            records: data.read_array(lookahead_end + 2..records_end).ok()?,
        })
    }

    /// Match this rule's input sequence and apply its lookup records.
    ///
    /// Backtrack/lookahead are not consulted; chain rules go through
    /// [`apply_chain_with_sequences`] instead.
    fn apply(
        &self,
        ctx: &mut hb_ot_apply_context_t,
        match_func: &impl Fn(&mut GlyphInfo, u32) -> bool,
    ) -> Option<()> {
        let inputs = self.input;
        let match_func = |info: &mut GlyphInfo, index| {
            inputs
                .get(index as usize)
                .is_some_and(|value| match_func(info, value.get() as u32))
        };

        let mut match_end = 0;

        if match_input(ctx, inputs.len() as _, match_func, &mut match_end, None) {
            ctx.buffer
                .unsafe_to_break(Some(ctx.buffer.idx), Some(match_end));
            apply_lookup(ctx, inputs.len(), match_end, self.records);
            return Some(());
        }
        None
    }
}

/// `first_input` for SequenceRule/ClassSequenceRule.
///
/// The glyph count is at offset 0 and the input sequence starts at offset 4,
/// after the lookup record count. (The input sequence's first entry covers
/// the *second* glyph of the matched sequence.)
fn plain_rule_first_input(data: &FontData) -> Option<u16> {
    let glyph_count: u16 = data.read_at(0).ok()?;
    if glyph_count <= 1 {
        return None;
    }
    data.read_at(4).ok()
}

/// `second_input` for SequenceRule/ClassSequenceRule; see
/// [`plain_rule_first_input`] for the layout.
fn plain_rule_second_input(data: &FontData) -> Option<u16> {
    let glyph_count: u16 = data.read_at(0).ok()?;
    if glyph_count <= 2 {
        return None;
    }
    data.read_at(6).ok()
}

/// `first_input` for ChainedSequenceRule/ChainedClassSequenceRule.
///
/// The input glyph count follows the variable-length backtrack sequence, and
/// the input sequence follows it directly.
fn chain_rule_first_input(data: &FontData) -> Option<u16> {
    let backtrack_count = usize::from(data.read_at::<u16>(0).ok()?);
    let count_pos = 2 + backtrack_count * u16::RAW_BYTE_LEN;
    let input_count: u16 = data.read_at(count_pos).ok()?;
    if input_count <= 1 {
        return None;
    }
    data.read_at(count_pos + 2).ok()
}

/// Positional probes into a chain rule's raw data, reading only the values
/// the rule pre-match needs instead of parsing the whole rule.
struct ChainRuleProbe<'a> {
    data: FontData<'a>,
    count_pos: usize,
    input_end: usize,
    input_count: u16,
}

impl<'a> ChainRuleProbe<'a> {
    fn new(data: FontData<'a>) -> Option<Self> {
        let backtrack_count = usize::from(data.read_at::<u16>(0).ok()?);
        let count_pos = 2 + backtrack_count * u16::RAW_BYTE_LEN;
        let input_count: u16 = data.read_at(count_pos).ok()?;
        let input_end =
            count_pos + 2 + usize::from(input_count).saturating_sub(1) * u16::RAW_BYTE_LEN;
        Some(Self {
            data,
            count_pos,
            input_end,
            input_count,
        })
    }

    /// Matches `ParsedRule`: `input.len() + 1`, where the input array holds
    /// `input_count - 1` values (saturating).
    fn len_p1(&self) -> usize {
        usize::from(self.input_count).max(1)
    }

    /// The input value at `index`. Callers must keep `index` within the
    /// input array (`index + 1 < input_count`).
    fn input(&self, index: usize) -> Option<u16> {
        self.data
            .read_at(self.count_pos + 2 + index * u16::RAW_BYTE_LEN)
            .ok()
    }

    fn lookahead_len(&self) -> usize {
        usize::from(self.data.read_at::<u16>(self.input_end).ok().unwrap_or(0))
    }

    /// The lookahead value at `index`. Callers must keep `index` within
    /// `lookahead_len()`.
    fn lookahead(&self, index: usize) -> Option<u16> {
        self.data
            .read_at(self.input_end + 2 + index * u16::RAW_BYTE_LEN)
            .ok()
    }
}

const _: () = assert!(SequenceRule::MIN_SIZE == ClassSequenceRule::MIN_SIZE);
const _: () = assert!(ChainedSequenceRule::MIN_SIZE == ChainedClassSequenceRule::MIN_SIZE);

/// The raw data of the rule at `off` within a rule set, gated the same
/// way the generated rule table's read would gate it. Working on the raw
/// data lets the rule walks below probe rules without constructing rule
/// tables for the ones they discard, which is most of them.
#[inline]
fn plain_rule_data_at<'a>(
    set_data: FontData<'a>,
    off: &BigEndian<Offset16>,
) -> Option<FontData<'a>> {
    let data = set_data.split_off(off.get().to_u32() as usize)?;
    (data.len() >= SequenceRule::MIN_SIZE).then_some(data)
}

#[inline]
fn parse_plain_rule_at<'a>(
    set_data: FontData<'a>,
    off: &BigEndian<Offset16>,
) -> Option<ParsedRule<'a>> {
    plain_rule_data_at(set_data, off).map(|d| ParsedRule::from_rule_data(d).unwrap_or_default())
}

/// [plain_rule_data_at] for chained rules.
#[inline]
fn chain_rule_data_at<'a>(
    set_data: FontData<'a>,
    off: &BigEndian<Offset16>,
) -> Option<FontData<'a>> {
    let data = set_data.split_off(off.get().to_u32() as usize)?;
    (data.len() >= ChainedSequenceRule::MIN_SIZE).then_some(data)
}

#[inline]
fn parse_chain_rule_at<'a>(
    set_data: FontData<'a>,
    off: &BigEndian<Offset16>,
) -> Option<ParsedRule<'a>> {
    chain_rule_data_at(set_data, off)
        .map(|d| ParsedRule::from_chain_rule_data(d).unwrap_or_default())
}

fn apply_context_rules(
    ctx: &mut hb_ot_apply_context_t,
    set_data: FontData<'_>,
    rule_offsets: &[BigEndian<Offset16>],
    match_func: impl Fn(&mut GlyphInfo, u32) -> bool,
) -> Option<()> {
    // HarfBuzz bypasses the first/second-component pre-match below for rule
    // sets of at most 4 rules, because its pre-match setup costs more than
    // it saves there. For us the pre-match loop rejects rules on one or two
    // cheap probes without parsing them, and measures faster than direct
    // application at every rule-set size, so there is no bypass.
    //
    // This version is optimized for speed by matching the first & second
    // components of the rule here, instead of calling into the matching code.
    //
    // We use the iter_context instead of iter_input, to avoid skipping
    // default-ignorables and such.
    //
    // Related: https://github.com/harfbuzz/harfbuzz/issues/4813
    let mut skippy_iter = skipping_iterator_t::with_match_fn(ctx, true, Some(match_always));
    skippy_iter.reset(skippy_iter.buffer.idx);
    skippy_iter.set_glyph_data(0);
    let mut unsafe_to = None;
    let unsafe_to1;
    let mut unsafe_to2 = 0;
    let mut second = None;
    let first = if skippy_iter.next(None) {
        let g1 = skippy_iter.index();
        if skippy_iter.may_skip(&skippy_iter.buffer.info[g1]) != may_skip_t::SKIP_NO {
            // Can't use the fast path if eg. the next char is a default-ignorable
            // or other skippable.
            for off in rule_offsets {
                let Some(rule) = parse_plain_rule_at(set_data, off) else {
                    continue;
                };
                if rule.apply(ctx, &match_func).is_some() {
                    return Some(());
                }
            }
            return None;
        }
        unsafe_to1 = skippy_iter.index() + 1;
        g1
    } else {
        // Failed to match a next glyph. Only try applying rules that have no
        // further impact.
        for off in rule_offsets {
            let Some(rule) = parse_plain_rule_at(set_data, off) else {
                continue;
            };
            if rule.input.len() <= 1 && rule.apply(ctx, &match_func).is_some() {
                return Some(());
            }
        }
        return None;
    };
    let matched = skippy_iter.next(None);
    let g2 = skippy_iter.index();
    if matched {
        second = Some(g2);
        unsafe_to2 = skippy_iter.index() + 1;
        if skippy_iter.may_skip(&skippy_iter.buffer.info[g2]) != may_skip_t::SKIP_NO {
            // Can't use the fast path if eg. the next char is a default-ignorable
            // or other skippable.
            for off in rule_offsets {
                let Some(rule) = parse_plain_rule_at(set_data, off) else {
                    continue;
                };
                if rule.apply(ctx, &match_func).is_some() {
                    return Some(());
                }
            }
            return None;
        }
    }
    let mut i = 0;
    while let Some(off) = rule_offsets.get(i) {
        let Some(data) = plain_rule_data_at(set_data, off) else {
            i += 1;
            continue;
        };
        // Probe the first two input values without parsing the whole rule;
        // most visited rules are rejected on these probes and never pay for
        // a full parse — nor for constructing a rule table.
        let first_value = plain_rule_first_input(&data);
        if first_value.is_none_or(|v| match_func(&mut ctx.buffer.info[first], u32::from(v))) {
            if second.is_none()
                || plain_rule_second_input(&data)
                    .is_none_or(|v| match_func(&mut ctx.buffer.info[second.unwrap()], u32::from(v)))
            {
                if ParsedRule::from_rule_data(data)
                    .unwrap_or_default()
                    .apply(ctx, &match_func)
                    .is_some()
                {
                    if let Some(unsafe_to) = unsafe_to {
                        ctx.buffer
                            .unsafe_to_concat(Some(ctx.buffer.idx), Some(unsafe_to));
                    }
                    return Some(());
                }
            } else {
                unsafe_to = Some(unsafe_to2);
            }
            i += 1;
        } else {
            if unsafe_to.is_none() {
                unsafe_to = Some(unsafe_to1);
            }

            // Skip ahead to next possible first glyph match.
            let first_glyph_value = first_value.unwrap();
            loop {
                i += 1;
                let Some(off) = rule_offsets.get(i) else {
                    break;
                };
                let Some(d) = plain_rule_data_at(set_data, off) else {
                    continue;
                };
                if plain_rule_first_input(&d) != Some(first_glyph_value) {
                    break;
                }
            }
        }
    }
    if let Some(unsafe_to) = unsafe_to {
        ctx.buffer
            .unsafe_to_concat(Some(ctx.buffer.idx), Some(unsafe_to));
    }
    None
}

fn apply_chain_with_sequences<
    F1: Fn(&mut GlyphInfo, u32) -> bool,
    F2: Fn(&mut GlyphInfo, u32) -> bool,
    F3: Fn(&mut GlyphInfo, u32) -> bool,
>(
    ctx: &mut hb_ot_apply_context_t,
    rule: &ParsedRule<'_>,
    match_funcs: &(F1, F2, F3),
) -> Option<()> {
    let input = rule.input;
    let f3 = |info: &mut GlyphInfo, index| {
        input
            .get(index as usize)
            .is_some_and(|value| match_funcs.1(info, value.get() as u32))
    };

    let mut end_index = ctx.buffer.idx;
    let mut match_end = 0;

    let input_matches = match_input(ctx, input.len() as u16, f3, &mut match_end, None);

    if input_matches {
        end_index = match_end;
    } else {
        ctx.buffer
            .unsafe_to_concat(Some(ctx.buffer.idx), Some(end_index));
        return None;
    }

    let lookahead = rule.lookahead;
    let f2 = |info: &mut GlyphInfo, index| {
        lookahead
            .get(index as usize)
            .is_some_and(|value| match_funcs.2(info, value.get() as u32))
    };

    if !match_lookahead(ctx, lookahead.len() as u16, f2, match_end, &mut end_index) {
        ctx.buffer
            .unsafe_to_concat(Some(ctx.buffer.idx), Some(end_index));
        return None;
    }

    let mut start_index = ctx.buffer.out_len;

    let backtrack = rule.backtrack;
    let f1 = |info: &mut GlyphInfo, index| {
        backtrack
            .get(index as usize)
            .is_some_and(|value| match_funcs.0(info, value.get() as u32))
    };

    if !match_backtrack(ctx, backtrack.len() as u16, f1, &mut start_index) {
        ctx.buffer
            .unsafe_to_concat_from_outbuffer(Some(start_index), Some(end_index));
        return None;
    }

    ctx.buffer
        .unsafe_to_break_from_outbuffer(Some(start_index), Some(end_index));
    apply_lookup(ctx, input.len(), match_end, rule.records);

    Some(())
}

fn apply_chain_context_rules<
    F1: Fn(&mut GlyphInfo, u32) -> bool,
    F2: Fn(&mut GlyphInfo, u32) -> bool,
    F3: Fn(&mut GlyphInfo, u32) -> bool,
>(
    ctx: &mut hb_ot_apply_context_t,
    set_data: FontData<'_>,
    rule_offsets: &[BigEndian<Offset16>],
    match_funcs: (F1, F2, F3),
) -> Option<()> {
    // No small-rule-set bypass here either; see apply_context_rules.
    //
    // This version is optimized for speed by matching the first & second
    // components of the rule here, instead of calling into the matching code.
    //
    // We use the iter_context instead of iter_input, to avoid skipping
    // default-ignorables and such.
    //
    // Related: https://github.com/harfbuzz/harfbuzz/issues/4813
    let mut skippy_iter = skipping_iterator_t::with_match_fn(ctx, true, Some(match_always));
    skippy_iter.reset(skippy_iter.buffer.idx);
    skippy_iter.set_glyph_data(0);
    let mut unsafe_to = None;
    let unsafe_to1;
    let mut unsafe_to2 = 0;
    let mut second = None;
    let first = if skippy_iter.next(None) {
        let g1 = skippy_iter.index();
        if skippy_iter.may_skip(&skippy_iter.buffer.info[g1]) != may_skip_t::SKIP_NO {
            // Can't use the fast path if eg. the next char is a default-ignorable
            // or other skippable.
            for off in rule_offsets {
                let Some(rule) = parse_chain_rule_at(set_data, off) else {
                    continue;
                };
                if apply_chain_with_sequences(ctx, &rule, &match_funcs).is_some() {
                    return Some(());
                }
            }
            return None;
        }
        unsafe_to1 = skippy_iter.index() + 1;
        g1
    } else {
        // Failed to match a next glyph. Only try applying rules that have no
        // further impact.
        for off in rule_offsets {
            let Some(rule) = parse_chain_rule_at(set_data, off) else {
                continue;
            };
            if rule.input.len() <= 1
                && rule.lookahead.is_empty()
                && apply_chain_with_sequences(ctx, &rule, &match_funcs).is_some()
            {
                return Some(());
            }
        }
        return None;
    };
    let matched = skippy_iter.next(None);
    let g2 = skippy_iter.index();
    if matched {
        second = Some(g2);
        unsafe_to2 = skippy_iter.index() + 1;
        if skippy_iter.may_skip(&skippy_iter.buffer.info[g2]) != may_skip_t::SKIP_NO {
            // Can't use the fast path if eg. the next char is a default-ignorable
            // or other skippable.
            for off in rule_offsets {
                let Some(rule) = parse_chain_rule_at(set_data, off) else {
                    continue;
                };
                if apply_chain_with_sequences(ctx, &rule, &match_funcs).is_some() {
                    return Some(());
                }
            }
            return None;
        }
    }
    let mut i = 0;
    while let Some(off) = rule_offsets.get(i) {
        let Some(data) = chain_rule_data_at(set_data, off) else {
            i += 1;
            continue;
        };
        // Probe the values the pre-match needs without parsing the whole
        // rule; most visited rules are rejected on these probes and never
        // pay for a full parse — nor for constructing a rule table.
        let Some(probe) = ChainRuleProbe::new(data) else {
            // Unreadable rule header; the full parse treats it as an empty
            // rule, same as the parse-first code did.
            let rule = ParsedRule::from_chain_rule_data(data).unwrap_or_default();
            if apply_chain_with_sequences(ctx, &rule, &match_funcs).is_some() {
                if let Some(unsafe_to) = unsafe_to {
                    ctx.buffer
                        .unsafe_to_concat(Some(ctx.buffer.idx), Some(unsafe_to));
                }
                return Some(());
            }
            i += 1;
            continue;
        };
        let len_p1 = probe.len_p1();
        let matched_first = if len_p1 > 1 {
            probe
                .input(0)
                .is_some_and(|v| match_funcs.1(&mut ctx.buffer.info[first], u32::from(v)))
        } else {
            probe.lookahead_len() == 0
                || probe
                    .lookahead(0)
                    .is_some_and(|v| match_funcs.2(&mut ctx.buffer.info[first], u32::from(v)))
        };
        if matched_first {
            let matched_second = if let Some(second) = second {
                if len_p1 > 2 {
                    probe
                        .input(1)
                        .is_some_and(|v| match_funcs.1(&mut ctx.buffer.info[second], u32::from(v)))
                } else {
                    (probe.lookahead_len() <= 2 - len_p1)
                        || probe.lookahead(2 - len_p1).is_some_and(|v| {
                            match_funcs.2(&mut ctx.buffer.info[second], u32::from(v))
                        })
                }
            } else {
                true
            };
            if matched_second {
                let rule = ParsedRule::from_chain_rule_data(data).unwrap_or_default();
                if apply_chain_with_sequences(ctx, &rule, &match_funcs).is_some() {
                    if let Some(unsafe_to) = unsafe_to {
                        ctx.buffer
                            .unsafe_to_concat(Some(ctx.buffer.idx), Some(unsafe_to));
                    }
                    return Some(());
                }
            } else {
                unsafe_to = Some(unsafe_to2);
            }

            i += 1;
        } else {
            if unsafe_to.is_none() {
                unsafe_to = Some(unsafe_to1);
            }

            let skip_key = if len_p1 > 1 { probe.input(0) } else { None };
            if let Some(first_glyph_value) = skip_key {
                // Skip ahead to next possible first glyph match.
                loop {
                    i += 1;
                    let Some(off) = rule_offsets.get(i) else {
                        break;
                    };
                    let Some(d) = chain_rule_data_at(set_data, off) else {
                        continue;
                    };
                    if chain_rule_first_input(&d) != Some(first_glyph_value) {
                        break;
                    }
                }
            } else {
                i += 1;
            }
        }
    }
    if let Some(unsafe_to) = unsafe_to {
        ctx.buffer
            .unsafe_to_concat(Some(ctx.buffer.idx), Some(unsafe_to));
    }
    None
}
